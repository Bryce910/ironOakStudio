"""Verify a scene/art derivative with an exact native build and reviewed shader override."""
from pathlib import Path
import hashlib, importlib.util, json, shlex, subprocess, sys
sys.dont_write_bytecode = True
N = Path(__file__).resolve().parent
B = N.with_name('woodland-detail-2026-11-19')
E = N.parents[1]
L = E/'projects/valwood_current'
IMMUTABLE = ('runtime/rpg', 'source-v1', 'shared/shaders', 'shared/character-textures', 'assets', 'motion')


def need(ok, message):
    if not ok: raise RuntimeError(message)


def sha(path):
    with Path(path).open('rb') as f: return hashlib.file_digest(f, 'sha256').hexdigest()


def read(path): return json.loads(Path(path).read_text())


def stat(path):
    s=Path(path).stat()
    return [s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns]


def inventory(root):
    root=Path(root)
    return {str(p.relative_to(root)):sha(p) for p in sorted(root.rglob('*'))
            if p.is_file() and '__pycache__' not in p.parts and p.suffix not in ('.pyc','.pyo')}


def shader_override():
    path=N/'review/SHADER_OVERRIDE.json'
    if not path.exists(): return None
    owned_project_leaf(N,'review/SHADER_OVERRIDE.json');doc=read(path)
    need(doc.get('schema')=='valwood.reviewed-shader-override.v1' and doc.get('reviewed') is True and
         doc.get('root_freeze_authorized') is True and doc.get('runtime_reused') is True and
         doc.get('CXX_source_reused') is True and doc.get('shader_bank_reused') is False and
         doc.get('shader_count')==204 and doc.get('baseline_shader_bank')==str(B/'shared/shaders') and
         doc.get('candidate_shader_bank')==str(N/'shared/shaders'),'Shader override lacks exact root authorization')
    bank=N/'shared/shaders';need(bank.is_dir() and not bank.is_symlink() and not N.is_symlink() and
         all(not p.is_symlink() for p in bank.parents if p.is_relative_to(N)),
         'Override shader bank must be owned')
    for p in bank.rglob('*'):
        need(not p.is_symlink() and (p.is_dir() or p.is_file()),'Override shader bank has an unsafe alias/leaf')
    old,new=inventory(B/'shared/shaders'),inventory(bank)
    need(len(old)==len(new)==204 and set(old)==set(new),'Override requires exact original 204 shader paths')
    changed=[dict(path=p,before_sha256=old[p],after_sha256=new[p]) for p in sorted(old) if old[p]!=new[p]]
    need(len(changed)==1 and changed[0]['path']=='world_surface_library.fs.glsl' and
         doc.get('changes')==changed,'Only the reviewed terrain surface fragment override is allowed')
    return dict(path=str(path),sha256=sha(path),changes=changed)


def shader_delivery():
    override=shader_override()
    return dict(shader_bank_reused=override is None,shader_changes=override['changes'] if override else [],
                shader_override=override)


def verify_reuse(*, inherited=True):
    baseline=read(N/'BASELINE.json')
    need(baseline.get('baseline')==str(B) and baseline.get('runtime_reused') is True and
         baseline.get('source_reused') is True and baseline.get('shader_bank_reused') is True and
         baseline.get('Windows_work_performed') is False,'Wrong scene-only baseline scope')
    need(sha(B/'UNIFIED_READY.json')==baseline['ready_sha256'],'Published baseline receipt changed')
    override=shader_override()
    for relative in IMMUTABLE:
        if relative=='shared/shaders' and override:continue
        p=N/relative
        need(p.is_symlink() and p.exists() and p.resolve()==(B/relative).resolve(),
             'Reused immutable alias changed: '+relative)
    need(sha(N/'runtime/rpg')==sha(B/'runtime/rpg')==baseline['binary_sha256'],'Reused binary changed')
    bank=inventory(N/'shared/shaders')
    original=inventory(B/'shared/shaders')
    need(len(bank)==204 and original==baseline['shader_sha256'] and (override or bank==original),
         'Exact published 204-shader bank changed')
    if inherited:
        # Separate process prevents the baseline's generic `from launch` from
        # accidentally importing this generation's launch module.
        helper=N/'tools/verify_baseline.py' if catalog_transition() else B/'check_unified.py'
        result=subprocess.run([sys.executable,str(helper)],cwd=E,
                              capture_output=True,text=True)
        need(result.returncode==0,'Published baseline integrity failed: '+result.stderr+result.stdout)
    return baseline['binary_sha256'],bank


def mutable(relative):
    p=Path(relative)
    return (any(x in ('saves','settings','__pycache__') for x in p.parts) or
            p.name in ('graphics.settings','studio-layout.ini','world.studio.workspace','player-profile.config') or
            p.name.startswith('arena_') and p.suffix=='.cfg' or '.recovery' in p.name or
            p.suffix in ('.log','.lock','.pyc','.pyo','.progress'))


def tree(root):
    """Record owned leaves and aliases without following directory aliases."""
    root=Path(root);result={}
    need(root.is_dir() and not root.is_symlink(),'Project root must be owned: '+str(root))
    def walk(directory):
        for p in sorted(directory.iterdir()):
            rel=str(p.relative_to(root))
            if mutable(rel): continue
            if p.is_symlink():
                need(p.exists(),'Dangling project alias: '+str(p))
                result[rel]=dict(kind='alias',target=str(p.resolve()),sha256=sha(p) if p.is_file() else None)
            elif p.is_dir(): walk(p)
            else:
                need(p.is_file(),'Unsupported project leaf: '+str(p))
                result[rel]=dict(kind='file',sha256=sha(p))
    walk(root)
    return result


def owned_project_leaf(root,relative):
    root,relative=Path(root),Path(relative)
    need(not relative.is_absolute() and '..' not in relative.parts,'Escaping authoring path')
    p=root/relative
    need(p.is_file() and not p.is_symlink() and not root.is_symlink() and
         all(not q.is_symlink() for q in p.parents if q.is_relative_to(root)),
         'Authoring leaf/parent must be owned: '+str(p))
    return p


def allowed_scene_path(relative):
    return relative in ('world.studio','collision/structures/catalog') or relative in {
        str(p.relative_to(B/'world-project')) for p in (B/'world-project/scenery').glob('*.txt')}


def art_document():
    path=N/'review/MODEL_ADDITIONS.json'
    if not path.exists(): return {}
    owned_project_leaf(N,'review/MODEL_ADDITIONS.json')
    doc=read(path)
    need(doc.get('schema')=='valwood.scene-art.model-additions.v1' and
         doc.get('reviewed') is True and doc.get('root_freeze_authorized') is True and
         isinstance(doc.get('collision_catalog_unchanged'),bool),'Model additions lack explicit reviewed scope')
    return doc


def new_art_rows(rows,*,collision=False,resource=False):
    seen=set()
    for row in rows:
        rel=Path(row['path'])
        need(not rel.is_absolute() and '..' not in rel.parts and len(rel.parts)>1 and
             ((rel.parts[:2]==('collision','structures') and rel.suffix=='.vwc') if collision else
              ((rel.parts[0]=='environments' and rel.suffix=='.hdr') or
               (rel.parts[0]=='surfaces' and rel.suffix=='.png')) if resource else
              (rel.parts[0]=='models' and rel.suffix=='.glb')) and str(rel) not in seen,
             'Invalid/duplicate new model path')
        seen.add(str(rel));p=owned_project_leaf(N/'world-project',rel)
        need(sha(p)==row['sha256'] and not (B/'world-project'/rel).exists() and
             not (B/'world-project'/rel).is_symlink(),'Art addition must be a new owned reviewed resource')
    return rows


def model_additions(): return new_art_rows(art_document().get('additions',[]))


def collision_additions(): return new_art_rows(art_document().get('collision_additions',[]),collision=True)


def resource_additions(): return new_art_rows(art_document().get('resource_additions',[]),resource=True)


def resource_overlays():
    """Allow only byte-preserving owned expansions of two inherited resource aliases.

    LIVE's resource parents are owned. The candidate may copy their immutable
    original leaves so it can stage new resources without writing through N's
    aliases into LIVE. This is a logical comparison, not permission to replace
    an existing resource during publication.
    """
    approved={row['path']:row['sha256'] for row in resource_additions()}
    result={}
    def leaves(root,prefix):
        values={}
        def walk(directory):
            for p in sorted(directory.iterdir()):
                relative=str(Path(prefix)/p.relative_to(root))
                need(not p.is_symlink() or not p.is_dir(),
                     'Nested resource directory alias is not an approved overlay: '+str(p))
                if p.is_dir():walk(p)
                elif p.is_file():values[relative]=dict(kind='file',sha256=sha(p))
                elif p.is_symlink():raise RuntimeError('Dangling resource overlay leaf: '+str(p))
                else:raise RuntimeError('Unsupported resource overlay leaf: '+str(p))
        walk(root);return values
    for name in ('surfaces','environments'):
        old,new=B/'world-project'/name,N/'world-project'/name
        if not (old.is_symlink() and new.is_dir() and not new.is_symlink()):continue
        need(old.is_dir(),'Inherited resource directory alias is dangling: '+name)
        original,current=leaves(old,name),leaves(new,name)
        for relative,row in original.items():
            need(current.get(relative)==row,'Existing resource overlay leaf changed/missing: '+relative)
        extra=set(current)-set(original)
        need(all(relative in approved and current[relative]==dict(kind='file',sha256=approved[relative])
                 for relative in extra),'Unreviewed resource overlay addition')
        result[name]=dict(original_alias=str(old.resolve()),original_leaves=original,
                          additions={relative:approved[relative] for relative in sorted(extra)})
    return result


def catalog_transition():
    doc=art_document()
    if not doc or doc.get('collision_catalog_unchanged') is True:
        need(not doc.get('collision_additions'),'Collision additions require an explicit catalog transition')
        return None
    binding=doc.get('tree_review',{});relative=Path(binding.get('path',''))
    need(not relative.is_absolute() and '..' not in relative.parts and relative.parts[:1]==('review',),
         'Invalid tree review path')
    report_path=owned_project_leaf(N,relative);report=read(report_path)
    need(sha(report_path)==binding.get('sha256') and report.get('schema')=='valwood.camp-tree-variants.v1' and
         report.get('passed') is True and report.get('production_catalog_and_cache_original_pass') is True,
         'Tree catalog/cache review missing, stale or failed')
    before=Path(report.get('preimage_path',''));after=owned_project_leaf(N/'world-project','collision/structures/catalog')
    need(before.is_relative_to(N/'review') and before.resolve().is_relative_to((N/'review').resolve()) and
         not before.is_symlink() and before.is_file() and
         sha(before)==report.get('before_sha256')==sha(B/'world-project/collision/structures/catalog') and
         sha(after)==report.get('after_sha256') and report.get('catalog_relative_path')=='collision/structures/catalog',
         'Catalog transition differs from exact before/after')
    old=before.read_bytes();new=after.read_bytes()
    need(old.endswith(b'\n') and new.startswith(old),'Existing catalog rows changed')
    rows=new[len(old):].decode().splitlines()
    need(len(rows)==8 and rows==report.get('added_catalog_rows'),'Exactly eight reviewed catalog rows may append')
    models={row['path']:row['sha256'] for row in model_additions()}
    tree_models={row['path']:row['sha256'] for row in report.get('model_additions',[])}
    collisions={row['path']:row['sha256'] for row in collision_additions()}
    expected={'nm_camp_variant_oak02','nm_camp_variant_oak04','nm_camp_variant_beech01','nm_camp_variant_beech02'}
    need(set(report.get('changed_aliases',[]))==expected and len(tree_models)==8 and len(collisions)==4 and
         all(models.get(name)==h for name,h in tree_models.items()),
         'Expected four reviewed tree variants with eight GLBs/four collision caches')
    aliases=set()
    for mesh_line,revision_line in zip(rows[::2],rows[1::2]):
        mesh,revision=shlex.split(mesh_line),shlex.split(revision_line)
        need(len(mesh)==len(revision)==4 and mesh[0]=='mesh' and revision[0]=='model' and
             mesh[1]==revision[1] and mesh[1] in expected and mesh[1] not in aliases and
             mesh[2] in tree_models and mesh[3] in collisions and
             int(revision[2])==(N/'world-project'/mesh[2]).stat().st_size and
             len(revision[3])==16 and int(revision[3],16)>0,'Invalid appended tree binding')
        aliases.add(mesh[1])
    for group in ('input_sha256','source_inputs_sha256'):
        for name,h in report.get(group,{}).items():
            need(Path(name).is_file() and sha(name)==h,'Tree review input changed: '+name)
    return dict(path=str(L/'collision/structures/catalog'),before_sha256=sha(before),after_sha256=sha(after),
                preimage_path=str(before),review_path=str(report_path),review_sha256=sha(report_path))


def scene_scope():
    overlays=resource_overlays()
    original,current=tree(B/'world-project'),tree(N/'world-project')
    if shader_override():
        need(original.get('shaders',{}).get('target')==str((B/'shared/shaders').resolve()) and
             current.get('shaders',{}).get('target')==str((N/'shared/shaders').resolve()) and
             original['shaders']['kind']==current['shaders']['kind']=='alias',
             'Candidate project shader alias differs from approved bank')
        original['shaders']=current['shaders']
    for name,overlay in overlays.items():
        original.pop(name)
        current={p:row for p,row in current.items() if not p.startswith(name+'/')}
        original.update(overlay['original_leaves']);current.update(overlay['original_leaves'])
        current.update({p:dict(kind='file',sha256=h) for p,h in overlay['additions'].items()})
    added=model_additions()+collision_additions()+resource_additions();addition_paths={r['path'] for r in added}
    delta={p:dict(before=original.get(p),after=current.get(p)) for p in set(original)|set(current)
           if original.get(p)!=current.get(p)}
    for name,row in delta.items():
        if name in addition_paths: continue
        need(allowed_scene_path(name) and row['before'] and row['after'] and
             row['before']['kind']==row['after']['kind']=='file','Unreviewed scene/model scope: '+name)
        owned_project_leaf(N/'world-project',name)
    transition=catalog_transition()
    need(transition or sha(N/'world-project/collision/structures/catalog')==sha(B/'world-project/collision/structures/catalog'),
         'Unreviewed shared collision catalog change')
    return delta,added


def check_inputs(manifest):
    binary,bank=verify_reuse()
    delta,models=scene_scope()
    delivery=shader_delivery()
    need(manifest.get('schema')=='valwood.scene-art.changeset.v1' and manifest.get('reviewed') is True and
         manifest.get('runtime_reused') is True and manifest.get('Windows_work_performed') is False and
         manifest.get('source_reused') is True and manifest.get('shader_bank_reused') is delivery['shader_bank_reused'] and
         manifest.get('source_changes')==[] and manifest.get('shader_changes')==delivery['shader_changes'] and
         manifest.get('shader_override')==delivery['shader_override'] and
         manifest.get('binary_sha256')==binary and manifest.get('shader_sha256')==bank and
         manifest.get('scene_delta')==delta and manifest.get('model_additions')==models and
         manifest.get('catalog_transition')==catalog_transition(),
         'Wrong/stale scene-only delivery scope')
    overlays=resource_overlays()
    need(manifest.get('resource_overlays',{})==overlays,
         'Wrong/stale preserved resource overlays')
    for p,h in manifest['pins'].items(): need(Path(p).is_file() and sha(p)==h,'Reviewed input changed: '+p)
    for p,target in manifest['aliases'].items():
        need(Path(p).is_symlink() and Path(p).exists() and str(Path(p).resolve())==target,'Reviewed alias changed: '+p)
    return binary


def evidence(manifest):
    binary=check_inputs(manifest)
    for p,h in manifest['evidence'].items():
        need(sha(p)==h and read(p).get('passed') is True,'Review receipt changed/failed: '+p)
    final=read(N/'review/FINAL_REVIEW.json')
    need(final.get('passed') is True and final.get('visual_review_performed') is True and
         final.get('AAA_quality_certified') is False,'Missing scoped final visual review')
    return binary


def check():
    ready=read(N/'UNIFIED_READY.json')
    need(ready.get('ready') is True and ready.get('generation')==str(N) and ready.get('runtime_reused') is True,
         'Scene-only generation is not published')
    verify_reuse()
    delivery=shader_delivery()
    need(ready.get('shader_bank_reused') is delivery['shader_bank_reused'] and
         ready.get('shader_changes',[])==delivery['shader_changes'] and
         ready.get('shader_override')==delivery['shader_override'],'Published shader delivery changed')
    for p,target in ready['aliases'].items():
        need(Path(p).is_symlink() and Path(p).exists() and str(Path(p).resolve())==target,'Runtime alias changed: '+p)
    for p,h in ready['pins'].items():
        need(Path(p).is_file(),'Missing runtime dependency: '+p)
        current=stat(p)
        if current!=ready['stats'].get(p): need(sha(p)==h and stat(p)==current,'Runtime input changed: '+p)
    spec=importlib.util.spec_from_file_location('scene_art_launch',N/'launch.py')
    launch=importlib.util.module_from_spec(spec);spec.loader.exec_module(launch)
    for mode in launch.MODES:
        command,env,project=launch.configuration(mode)
        need(command[0]==str(N/'runtime/rpg') and Path(command[0]).resolve()==(B/'runtime/rpg').resolve(),
             'Mode uses another executable')
        need((project/'shaders').resolve()==(N/'shared/shaders').resolve(),'Mode shader mismatch')
        need((project/'character-textures').resolve()==(B/'shared/character-textures').resolve(),'Mode character bank mismatch')
    for project,_ in launch.AREAS.values():
        need((project/'shaders').resolve()==(N/'shared/shaders').resolve(),'Area shader mismatch')
    expected={str(project/'shaders'):str((N/'shared/shaders').resolve())
              for project in [L,*[p for p,_ in launch.AREAS.values()]]} if delivery['shader_override'] else {}
    need(ready.get('live_shader_bindings',{})==expected,'Published shared shader binding scope changed')
    for name in ready['live_scene_sha256']: need((L/name).is_file(),'Working authoring missing: '+name)
    for name,target in ready.get('live_model_additions',{}).items():
        need(Path(name).is_symlink() and str(Path(name).resolve())==target,'Published model addition binding changed')
    for name,overlay in ready.get('resource_overlays',{}).items():
        need(name in ('surfaces','environments') and (L/name).is_dir() and not (L/name).is_symlink() and
             str((L/name).resolve())==overlay['original_alias'],'Published original resource parent changed')
        for relative,row in overlay['original_leaves'].items():
            p=L/relative
            need(Path(relative).parts[0]==name and p.is_file() and sha(p)==row['sha256'],
                 'Published original resource changed: '+relative)
    return ready


if __name__=='__main__':
    check()
    print('Scene/art derivative and five shared launch modes verified; exact native runtime and approved shader bank.')
