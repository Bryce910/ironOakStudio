#!/usr/bin/env python3
"""Recover the purchased UE4.26 publisher map as data, without running Unreal.

Only tagged properties and explicit actor/component transforms are interpreted.
Opaque native instance buffers remain identified by their exact source spans.
This does not modify a live project, source archive, game, or GPU resource.
"""
from pathlib import Path
import argparse, collections, functools, hashlib, json, math, struct
import numpy as np
from audit_purchased_village_source import Package

ROOT = Path(__file__).resolve().parents[1]
PRIVATE = ROOT.parent/'.local-tools/village-source'
SOURCE = PRIVATE/'source-project/Medieval_Village/Content/Medieval_Village/Maps/Medieval_Village.umap'

class ScenePackage(Package):
    def __init__(self, path):
        super().__init__(path)
        self.pos = 20
        custom = self.i32(); self.pos += custom*20
        self.i32(); self.string(); self.pos += 4
        self.i32(); self.i32(); self.string(); self.pos += 8
        ec, eo, ic, io = self.i32(), self.i32(), self.i32(), self.i32()
        assert ec == len(self.exports) and ic == len(self.imports)
        self.import_records = []
        self.pos = io
        for _ in range(ic):
            package, kind, outer, name = self.name(), self.name(), self.i32(), self.name()
            self.pos += 8
            self.import_records.append(dict(package=package, kind=kind, outer=outer, name=name))
        for i, entry in enumerate(self.exports):
            self.pos = eo+104*i
            entry.update(index=i+1, cls=self.i32(), super=self.i32(), template=self.i32(), outer=self.i32())
        self.property_cache = {}

    @functools.lru_cache(maxsize=None)
    def object_path(self, index):
        if not index: return None
        entry = self.import_records[-index-1] if index < 0 else self.exports[index-1]
        parent = self.object_path(entry['outer'])
        return parent+'.'+entry['name'] if parent else entry['name']

    def ref(self, index):
        return None if not index else dict(index=index, name=Package.object(self,index), path=self.object_path(index))

    def properties(self):
        result = {}
        for _ in range(20000):
            tag = self.header_tag()
            if tag is None: return result
            name, kind, size, meta, start = (tag[k] for k in ['name','kind','size','meta','start'])
            end = start+size
            opaque = dict(type=meta or kind, bytes=size, source_offset=start)
            if kind == 'FloatProperty': value = struct.unpack_from('<f',self.data,start)[0]
            elif kind == 'DoubleProperty': value = struct.unpack_from('<d',self.data,start)[0]
            elif kind == 'BoolProperty': value = meta
            elif kind == 'IntProperty': value = self.i32()
            elif kind == 'UInt32Property': value = struct.unpack_from('<I',self.data,start)[0]
            elif kind in {'ObjectProperty','WeakObjectProperty'} and size == 4: value = self.ref(self.i32())
            elif kind in {'NameProperty','EnumProperty'}: value = self.name()
            elif kind == 'ByteProperty': value = self.data[start] if size == 1 else self.name()
            elif kind == 'StrProperty': value = self.string()
            elif kind == 'StructProperty':
                if meta in {'Vector','Rotator'} and size == 12:
                    value = list(struct.unpack_from('<3f',self.data,start))
                elif meta == 'Vector2D' and size == 8: value = list(struct.unpack_from('<2f',self.data,start))
                elif meta in {'Quat','Vector4','LinearColor'} and size == 16: value = list(struct.unpack_from('<4f',self.data,start))
                elif meta == 'Color' and size == 4:
                    b,g,r,a = self.data[start:end]; value = dict(r=r,g=g,b=b,a=a)
                elif meta == 'Guid' and size == 16: value = self.data[start:end].hex()
                elif meta == 'IntPoint' and size == 8: value = list(struct.unpack_from('<2i',self.data,start))
                else:
                    try:
                        value = self.properties()
                        if self.pos > end: value = opaque
                    except (AssertionError,IndexError,ValueError,struct.error,UnicodeDecodeError): value = opaque
            elif kind == 'ArrayProperty':
                count = self.i32(); assert 0 <= count < 10000000
                if meta == 'ObjectProperty': value = [self.ref(self.i32()) for _ in range(count)]
                elif meta == 'IntProperty': value = list(struct.unpack_from('<'+str(count)+'i',self.data,self.pos)) if count < 2000 else dict(count=count,**opaque)
                else: value = dict(count=count,**opaque)
            else: value = opaque
            result[name] = value
            self.pos = end
        raise ValueError('Property count exceeds bound')

    def props(self, index):
        if index not in self.property_cache:
            entry = self.exports[index-1]
            self.pos = entry['offset']; values = self.properties()
            self.property_cache[index] = dict(values=values, native_tail_offset=self.pos,
                native_tail_bytes=entry['offset']+entry['size']-self.pos)
        return self.property_cache[index]['values']

def rotation(rotator):
    # Unreal FRotationMatrix is row-vector based. Return its column-vector
    # transpose, retaining UnrealX-forward,Y-right,Z-up and centimetres.
    p,y,r = np.radians(rotator); sp,cp,sy,cy,sr,cr = math.sin(p),math.cos(p),math.sin(y),math.cos(y),math.sin(r),math.cos(r)
    return np.array([[cp*cy,sr*sp*cy-cr*sy,-(cr*sp*cy+sr*sy)],
                     [cp*sy,sr*sp*sy+cr*cy,cy*sr-cr*sp*sy], [sp,-sr*cp,cr*cp]])

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=SOURCE)
    parser.add_argument('--output',type=Path,default=PRIVATE/'publisher-scene.json')
    args = parser.parse_args(); package = ScenePackage(args.source)
    classes = collections.Counter(Package.object(package,e['cls']) for e in package.exports)
    errors = []; worlds = {}; computing = set()

    def world(index):
        if index in worlds: return worlds[index]
        if index in computing: raise ValueError('Cyclic component attachment')
        computing.add(index); props = package.props(index)
        location = props.get('RelativeLocation',[0,0,0]); angles = props.get('RelativeRotation',[0,0,0]); scale = props.get('RelativeScale3D',[1,1,1])
        matrix = np.eye(4); matrix[:3,:3] = rotation(angles)@np.diag(scale); matrix[:3,3] = location
        parent = props.get('AttachParent')
        if parent:
            assert parent['index'] > 0, ('External component attachment',parent)
            parent_matrix = world(parent['index']); combined = parent_matrix@matrix
            if props.get('bAbsoluteLocation'): combined[:3,3] = location
            if props.get('bAbsoluteRotation') or props.get('bAbsoluteScale'):
                raise ValueError('Absolute component rotation/scale requires explicit decomposition')
            matrix = combined
        computing.remove(index); worlds[index] = matrix
        return matrix

    placements = []; lighting = []; cameras = []; native_instances = []; actors = {}
    for entry in package.exports:
        cls = Package.object(package,entry['cls'])
        try: props = package.props(entry['index'])
        except (AssertionError,IndexError,ValueError,struct.error,UnicodeDecodeError) as error:
            errors.append(dict(index=entry['index'],name=entry['name'],cls=cls,error=type(error).__name__+': '+str(error))); continue
        if 'ActorLabel' in props: actors[entry['index']] = props
        if cls in {'StaticMeshComponent','SplineMeshComponent','FoliageInstancedStaticMeshComponent','InstancedStaticMeshComponent'}:
            record = dict(export_index=entry['index'],name=entry['name'],class_name=cls,hierarchy=package.object_path(entry['index']),
                owner_index=entry['outer'],mesh=props.get('StaticMesh'),properties=props,**package.property_cache[entry['index']])
            record.pop('values')
            try:
                matrix = world(entry['index']); record.update(world_matrix_unreal_cm_column_major=matrix.T.flatten().tolist(),
                    position_unreal_m=(matrix[:3,3]/100).tolist(),world_scale=np.linalg.norm(matrix[:3,:3],axis=0).tolist())
            except (AssertionError,ValueError) as error: record['transform_error'] = str(error)
            if cls in {'FoliageInstancedStaticMeshComponent','InstancedStaticMeshComponent'}: native_instances.append(record)
            else: placements.append(record)
        if any(word in cls for word in ['Light','Fog','PostProcess','Atmosphere','ReflectionCapture']) or entry['name'].startswith('BP_Sky'):
            record = dict(export_index=entry['index'],name=entry['name'],class_name=cls,hierarchy=package.object_path(entry['index']),properties=props)
            if any(key in props for key in ['RelativeLocation','RelativeRotation','AttachParent']):
                try: record['world_matrix_unreal_cm_column_major'] = world(entry['index']).T.flatten().tolist()
                except (AssertionError,ValueError) as error: record['transform_error'] = str(error)
            lighting.append(record)
        if cls in {'CameraActor','CameraComponent'}:
            cameras.append(dict(export_index=entry['index'],name=entry['name'],class_name=cls,hierarchy=package.object_path(entry['index']),properties=props))
    for record in placements+native_instances:
        owner = actors.get(record['owner_index'],{})
        record['actor_label'] = owner.get('ActorLabel'); record['actor_folder'] = owner.get('FolderPath')
    cameras_resolved = []
    basis = np.array([[1,0,0],[0,0,1],[0,-1,0]],dtype=float)
    for camera in cameras:
        if camera['class_name'] != 'CameraActor': continue
        actor_props = camera['properties']; component_ref = actor_props.get('CameraComponent')
        if not component_ref: continue
        ci = component_ref['index']; cp = package.props(ci); matrix = world(ci)
        native = np.eye(4); native[:3,:3] = basis@matrix[:3,:3]@basis.T; native[:3,3] = basis@matrix[:3,3]/100
        forward = basis@matrix[:3,0]; forward /= np.linalg.norm(forward)
        yaw = math.atan2(forward[0],-forward[2]); pitch = math.atan2(forward[1],math.hypot(forward[0],forward[2]))
        cameras_resolved.append(dict(name=camera['name'],actor_index=camera['export_index'],component_index=ci,
            world_matrix_unreal_cm_column_major=matrix.T.flatten().tolist(),
            native_matrix_column_major=native.T.flatten().tolist(),position_native_m=native[:3,3].tolist(),
            forward_native=forward.tolist(),yaw_native_radians=yaw,pitch_native_radians=pitch,
            horizontal_fov_degrees=cp.get('FieldOfView',90.0),horizontal_fov_explicit='FieldOfView' in cp,
            aspect_ratio=cp.get('AspectRatio',1.777778),aspect_ratio_explicit='AspectRatio' in cp,
            constrain_aspect_ratio=cp.get('bConstrainAspectRatio',False),properties=cp))
    by_mesh = collections.Counter(p['mesh']['name'] for p in placements if p['mesh'])
    positions = np.array([p['position_unreal_m'] for p in placements if 'position_unreal_m' in p])
    result = dict(source=str(args.source.resolve()),source_sha256=hashlib.sha256(package.data).hexdigest(),
        source_unmodified=True,format='UE4.26 package522',coordinates='Unreal centimetres, Xforward Yright Zup, column-vector matrices',
        explicit_properties_only=True,engine_defaults_not_invented=True,classes=dict(classes),actors=len(actors),
        placements=placements,mesh_counts=dict(by_mesh),native_instance_components=native_instances,lighting=lighting,cameras=cameras,cameras_resolved=cameras_resolved,
        bounds_actor_origins_unreal_m=[positions.min(0).tolist(),positions.max(0).tolist()],errors=errors,
        limitations=['Native foliage/rope instance buffers identified, not decoded in this first data-only pass.',
                     'World transforms retain original asset pivots; native bottom-centred mesh pivot corrections are not yet applied.',
                     'No production placement, terrain, interaction, collision or save file changed.'])
    args.output.parent.mkdir(parents=True,exist_ok=True); args.output.write_text(json.dumps(result,indent=2,allow_nan=False)+'\n')
    print(json.dumps(dict(output=str(args.output),static_components=len(placements),mesh_families=len(by_mesh),instance_components=len(native_instances),
        actors=len(actors),lighting_records=len(lighting),camera_records=len(cameras),errors=errors,bounds=result['bounds_actor_origins_unreal_m']),indent=2))

if __name__ == '__main__': main()
