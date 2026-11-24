#!/usr/bin/env python3
"""Decode purchased BEP scene/terrain/prefab evidence offline with UnityPy.

Run after recover_bep_unity_sources.py --recover-serialized. Source coordinates
remain Unity metres, Y up; array row zero is source terrain Z zero (unflipped).
No imported source scripts, Unity editor, renderer or production project runs.
"""
from pathlib import Path
import collections
import json
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / '.local-tools/unity-source/pythondeps'))
import UnityPy
import numpy as np
from scipy.spatial.transform import Rotation

OUT = ROOT / '.local-tools/bep-source'


def save(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2) + '\n')


class Sources:
    def __init__(self):
        self.rows = {}
        for p in sorted(OUT.glob('*/inventory.json')):
            for row in json.loads(p.read_text())['assets']:
                row = dict(row, package=p.parent.name)
                self.rows[row['guid']] = row

    def path(self, guid):
        row = self.rows[guid]
        p = OUT / row['package'] / 'guid' / guid / 'asset'
        if not p.exists():
            for candidate in OUT.glob('*/guid/' + guid + '/asset'):
                return candidate
        return p

    def load(self, guid):
        return UnityPy.load(str(self.path(guid)))

    def ref(self, obj, ref):
        file_id, path_id = ref['m_FileID'], ref['m_PathID']
        if not file_id:
            return dict(file_id=path_id)
        ext = obj.assets_file.externals[file_id - 1]
        # Unity's serialized GUID bytes contain low nibble before high nibble.
        raw = ext.guid.hex()
        guid = ''.join(raw[i + 1] + raw[i] for i in range(0, 32, 2))
        return dict(guid=guid, path=self.rows.get(guid, {}).get('path'), file_id=path_id)

    def refs(self, obj, value):
        if isinstance(value, dict):
            if 'm_FileID' in value and 'm_PathID' in value:
                return self.ref(obj, value)
            return {k:self.refs(obj, v) for k,v in value.items()}
        if isinstance(value, (list, tuple)):
            return [self.refs(obj, v) for v in value]
        if isinstance(value, bytes):
            return dict(bytes=len(value))
        return value


def trs(data):
    p = data.get('m_LocalPosition', dict(x=0,y=0,z=0))
    q = data.get('m_LocalRotation', dict(x=0,y=0,z=0,w=1))
    s = data.get('m_LocalScale', dict(x=1,y=1,z=1))
    matrix = np.eye(4)
    matrix[:3,:3] = Rotation.from_quat([q[k] for k in 'xyzw']).as_matrix() @ np.diag([s[k] for k in 'xyz'])
    matrix[:3,3] = [p[k] for k in 'xyz']
    return matrix


def scene(source, guid):
    env = source.load(guid)
    objects = {o.path_id:o for o in env.objects}
    data = {o.path_id:o.read_typetree() for o in env.objects}
    transforms = {pid:d for pid,d in data.items() if objects[pid].type.name=='Transform'}
    by_go = {d['m_GameObject']['m_PathID']:pid for pid,d in transforms.items() if 'm_GameObject' in d}
    cache = {}
    def world(pid, visiting=()):
        if pid in cache:return cache[pid]
        if pid in visiting:raise ValueError('Transform hierarchy cycle')
        d = transforms[pid]
        if 'm_LocalPosition' not in d:
            raise ValueError(f'Stripped transform {pid} requires prefab expansion')
        parent = d['m_Father']['m_PathID']
        value = (world(parent, visiting+(pid,)) if parent else np.eye(4)) @ trs(d)
        cache[pid] = value
        return value
    named = []
    cameras, terrain, lights, renderers, instances = [], [], [], [], []
    for pid,d in data.items():
        typ = objects[pid].type.name
        if typ=='GameObject' and 'm_Name' in d:
            row = dict(id=pid,name=d['m_Name'],active=d.get('m_IsActive'))
            t = by_go.get(pid)
            if t:
                try:row['world_matrix_unity'] = world(t).tolist()
                except ValueError as e:row['unresolved'] = str(e)
            named.append(row)
        if typ in {'Camera','Terrain','Light','MeshRenderer','MeshFilter'}:
            row = dict(id=pid,type=typ,data=source.refs(objects[pid],d))
            go = d.get('m_GameObject',{}).get('m_PathID',0)
            row['name'] = data.get(go,{}).get('m_Name')
            t = by_go.get(go)
            if t:
                try:row['world_matrix_unity'] = world(t).tolist()
                except ValueError as e:row['unresolved'] = str(e)
            {'Camera':cameras,'Terrain':terrain,'Light':lights,'MeshRenderer':renderers,'MeshFilter':renderers}[typ].append(row)
        if typ=='PrefabInstance':
            row = dict(id=pid,source=source.ref(objects[pid],d['m_SourcePrefab']),
                       modification=source.refs(objects[pid],d['m_Modification']))
            instances.append(row)
    report = dict(source=source.rows[guid],coordinate_system='Unity X,Y,Z metres; Y up; no native Z reflection',
                  object_counts=dict(collections.Counter(o.type.name for o in env.objects)),
                  terrain=terrain,cameras=cameras,lights=lights,named_objects=named,
                  renderers=renderers,prefab_instances=instances,
                  settings=[dict(type=objects[pid].type.name,data=source.refs(objects[pid],d)) for pid,d in data.items()
                            if objects[pid].type.name in {'RenderSettings','LightmapSettings','WindZone','LightingSettings'}])
    save(OUT/'scene-Isle_01.json',report)
    return report


def terrain(source, guid, scene_report=None):
    env = source.load(guid)
    obj = next(o for o in env.objects if o.type.name=='TerrainData')
    d = obj.read_typetree()
    height, detail, splat = d['m_Heightmap'], d['m_DetailDatabase'], d['m_SplatDatabase']
    n = height['m_Resolution']
    raw = np.asarray(height['m_Heights'],dtype=np.uint16).reshape(n,n)
    scale = height['m_Scale']
    size = np.array([scale['x']*(n-1),scale['y'],scale['z']*(n-1)])
    meters = raw.astype(np.float32) * np.float32(scale['y']/32767.)
    # Serialized Terrain heights are UNORM15; validate against authored tree Y.
    trees = detail['m_TreeInstances']
    tree_positions = np.array([[t['position'][k] for k in 'xyz'] for t in trees],dtype=np.float32).reshape(-1,3)
    if len(trees):
        ix=np.rint(tree_positions[:,0]*(n-1)).astype(int);iz=np.rint(tree_positions[:,2]*(n-1)).astype(int)
        error=np.abs(meters[iz,ix]-tree_positions[:,1]*size[1])
        tree_validation=dict(count=len(trees),median_abs_error_m=float(np.median(error)),p95_abs_error_m=float(np.percentile(error,95)))
    else:tree_validation=dict(count=0)
    textures={o.path_id:o for o in env.objects if o.type.name=='Texture2D'}
    alphas=[]
    for ref in splat['m_AlphaTextures']:
        t=textures[ref['m_PathID']].read_typetree()
        assert t['m_TextureFormat']==4, 'Expected source RGBA32 alpha texture'
        a=np.frombuffer(t['image data'],dtype=np.uint8,count=t['m_Width']*t['m_Height']*4).reshape(t['m_Height'],t['m_Width'],4)
        alphas.append(a)
    alpha=np.concatenate(alphas,axis=2)[:,:,:len(splat['m_TerrainLayers'])]
    patch_count, samples = detail['m_PatchCount'], detail['m_PatchSamples']
    densities=np.zeros((len(detail['m_DetailPrototypes']),patch_count*samples,patch_count*samples),dtype=np.uint8)
    assert len(detail['m_Patches'])==patch_count**2
    for index,patch in enumerate(detail['m_Patches']):
        layers=patch['layerIndices']; counts=np.asarray(patch['numberOfObjects'],dtype=np.uint8)
        assert len(counts)==len(layers)*samples*samples
        pz,px=divmod(index,patch_count)
        for i,layer in enumerate(layers):
            densities[layer,pz*samples:(pz+1)*samples,px*samples:(px+1)*samples]=counts[i*samples*samples:(i+1)*samples*samples].reshape(samples,samples)
    out=OUT/('terrain-'+d['m_Name']+'.npz')
    holes=np.asarray(height['m_Holes'],dtype=np.uint8)
    if len(holes):holes=holes.reshape(n-1,n-1)
    np.savez_compressed(out,height_raw_unorm15_zx=raw,height_meters_zx=meters,
                        holes_zx=holes,
                        alpha_u8_zxl=alpha,detail_density_lzx=densities,
                        tree_position_normalized_xyz=tree_positions,tree_position_local_xyz=tree_positions*size,
                        tree_prototype_index=np.array([t['index']for t in trees],dtype=np.int32),
                        tree_width_scale=np.array([t['widthScale']for t in trees],dtype=np.float32),
                        tree_height_scale=np.array([t['heightScale']for t in trees],dtype=np.float32),
                        tree_rotation_radians=np.array([t['rotation']for t in trees],dtype=np.float32),
                        tree_color_rgba_packed=np.array([t['color']['rgba']for t in trees],dtype=np.uint32))
    report=dict(source=source.rows[guid],name=d['m_Name'],arrays=str(out.relative_to(ROOT)),
                resolution=n,size_unity_xyz_m=size.tolist(),height_range_m=[float(meters.min()),float(meters.max())],
                height_encoding='Source UNORM15 /32767 * scaleY, validated against tree normalized Y; row0 sourceZ0',
                array_orientation='All arrays unflipped. z increases with row, x with column. Alpha channels preserve source TerrainLayer order.',
                tree_validation=tree_validation,detail_resolution=patch_count*samples,
                detail_total_by_prototype=densities.sum(axis=(1,2),dtype=np.uint64).tolist(),
                detail_prototypes=source.refs(obj,detail['m_DetailPrototypes']),
                tree_prototypes=source.refs(obj,detail['m_TreePrototypes']),
                layers=source.refs(obj,splat['m_TerrainLayers']),
                alpha_resolution=splat['m_AlphamapResolution'],alpha_sum_range=[int(alpha.sum(2).min()),int(alpha.sum(2).max())],
                alpha_mean_by_layer=alpha.mean(axis=(0,1)).tolist(),
                grass_settings={k:v for k,v in detail.items() if k.startswith('m_Waving') or k=='WavingGrassTint'})
    if scene_report:
        report['scene_terrain_components']=scene_report['terrain']
    save(OUT/('terrain-'+d['m_Name']+'.json'),report)
    print('READY',out,report['resolution'],report['size_unity_xyz_m'],len(trees),'trees',flush=True)
    return report


def prefabs(source):
    rows=[];materials=[];errors=[]
    for guid,record in source.rows.items():
        path=record.get('path','')
        if not path.endswith(('.prefab','.mat','.terrainlayer')):continue
        try:
            env=source.load(guid)
            objects=[]
            for o in env.objects:
                if o.type.name not in {'GameObject','Transform','LODGroup','MeshFilter','MeshRenderer','Material','TerrainLayer','SkinnedMeshRenderer'}:continue
                objects.append(dict(id=o.path_id,type=o.type.name,data=source.refs(o,o.read_typetree())))
            row=dict(source=record,objects=objects)
            (materials if path.endswith('.mat') else rows).append(row)
        except Exception as e:errors.append(dict(guid=guid,path=path,error=str(e)))
    save(OUT/'prefabs.json',dict(prefabs_and_layers=rows,errors=errors))
    save(OUT/'materials.json',dict(materials=materials,errors=errors))
    print('READY',len(rows),'prefabs/layers',len(materials),'materials',len(errors),'errors',flush=True)


def main():
    source=Sources()
    existing=OUT/'scene-Isle_01.json'
    report=json.loads(existing.read_text()) if existing.exists() else scene(source,'a4225b790cc5c64418f465e5b73ec58c')
    if not (OUT/'terrain-Isle_01.json').exists():terrain(source,'68e7d593f94e01d428732769d7deef18',report)
    terrain(source,'c7e82a1d0e8a7774e8ef8caaf6226705')
    prefabs(source)


if __name__=='__main__':main()
