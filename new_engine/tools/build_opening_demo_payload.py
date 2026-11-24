#!/usr/bin/env python3
"""Materialize the accepted opening's Windows demo data without source assets.

Only a fresh release/project and a project payload manifest are published.
The caller supplies the Windows executable/launcher; this tool never runs it.
"""
from pathlib import Path, PurePosixPath
from dataclasses import dataclass
import argparse, hashlib, json, os, re, shutil, stat, struct, tempfile
from urllib.parse import unquote, urlsplit
from build_world_package import _publish_directory
from build_windows_package import windows_names

ROOT=Path(__file__).resolve().parents[1]
@dataclass(frozen=True)
class Inputs:
    project: Path
    origin: Path
    origin_sha256: str
    runtime_ready: Path
    windows_executable: Path
    runtime_closure: Path
    output: Path
    audit_dir: Path
    previous_manifest: Path | None = None
    reuse_immutable_from: Path | None = None

    def validate(self):
        assert self.project.is_absolute() and not self.project.is_symlink()
        assert self.project.parent.parent == ROOT/'build/journey-opening'
        assert re.fullmatch(r'candidate-v[1-9][0-9]*', self.project.parent.name)
        assert self.project.name == 'project' and self.project.is_dir()
        assert self.origin == self.project.parent/'local-origin.json'
        assert re.fullmatch('[a-f0-9]{64}',self.origin_sha256)
        assert sha(self.origin)==self.origin_sha256,'Source origin differs from explicitly reviewed hash'
        for p in (self.runtime_ready,self.windows_executable,self.runtime_closure):
            assert p.is_absolute() and p.resolve(strict=True).is_relative_to(ROOT) and p.is_file(),str(p)
        assert self.output.is_absolute() and self.output.parent.resolve()==ROOT/'releases'
        assert re.fullmatch(r'windows-demo-[0-9]{4}-[0-9]{2}-[0-9]{2}-v[1-9][0-9]*',self.output.name)
        assert not self.output.is_symlink()
        assert self.audit_dir.is_absolute() and self.audit_dir.is_relative_to(ROOT/'docs/windows-demo')
        assert not self.audit_dir.is_symlink()
        if self.previous_manifest:
            assert self.previous_manifest.is_file() and self.previous_manifest.resolve().is_relative_to(ROOT/'releases')
            assert self.previous_manifest != self.output/'project-payload-manifest.json'
        if self.reuse_immutable_from:
            old = self.reuse_immutable_from
            assert old.is_absolute() and old.parent == ROOT/'releases' and old != self.output
            assert not old.is_symlink() and old.is_dir() and (old/'project-payload-manifest.json').is_file()
            assert re.fullmatch(r'windows-demo-[0-9]{4}-[0-9]{2}-[0-9]{2}-v[1-9][0-9]*', old.name)

ROOT_FILES=('project.valwood','world.studio','opening.config','opening.scene',
 'CHARACTER_POLISH.json','city_showcase.json','publisher_showcase.json',
 'city.scene','city.sky','city.physics','showcase.views','showcase.flames')
DIRS=('opening','scenery','ground','particles','fonts','shaders','audio','environments','ai')
ALLOWED={'.glb','.gltf','.bin','.png','.jpg','.jpeg','.tga','.bmp','.webp','.hdr',
 '.vfx','.wav','.ogg','.mp3','.ttf','.otf','.txt','.md','.json','.settings','.glsl',
 '.vs','.fs','.vert','.frag','.inc','.vwml','.vwmemory','.vwminds','.vwlooks',
 '.height','.coverage','.physics','.scene','.sky','.vbc7','.bank'}
GRAPHICS=b'''version=2
preset=Balanced
auto_quality=1
target_fps=60
render_scale=1
min_render_scale=0.7
shadow_side=2048
reflections=1
contact_shadows=1
volumetric_fog=1
clouds=1
reflection_scale=0.5
display_mode=Windowed
window_width=0
window_height=0
vsync=1
'''

def sha(p):
    with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def js(p,v):
    with p.open('x') as f:json.dump(v,f,indent=2,allow_nan=False);f.write('\n')
def safe_name(s):
    q=PurePosixPath(s)
    assert not q.is_absolute() and q.parts and all(x not in ('..','.','') and not x.startswith('.') for x in q.parts),s
    return q.as_posix()
def fingerprint(p):
    s=p.stat();assert stat.S_ISREG(s.st_mode)
    return s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns

def immutable_reuse_name(name):
    """Only read-only binary assets may share an inode with an older release."""
    p = PurePosixPath(safe_name(name))
    if len(p.parts) < 2 or 'saves' in p.parts:
        return False
    images = {'.png','.jpg','.jpeg','.tga','.bmp','.webp','.hdr','.exr','.vbc7'}
    suffix = p.suffix.lower()
    if p.parts[0] == 'models':
        return suffix in images | {'.glb','.bin'}
    if p.parts[0] in {'ground','textures','texture_blocks','environments','particles'}:
        return suffix in images
    return p.parts[0] == 'audio' and suffix in {'.wav','.ogg','.mp3'}

def preserved_file_state(path):
    # Creating a hardlink changes ctime/nlink. It must never change the bytes,
    # inode, size, mtime or mode of any existing release file.
    s = path.stat()
    assert stat.S_ISREG(s.st_mode) and not path.is_symlink()
    return s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,stat.S_IMODE(s.st_mode)

class ImmutableReuse:
    def __init__(self, package, inputs):
        self.package = package
        assert package.is_dir() and not package.is_symlink()
        paths = sorted(package.rglob('*'))
        assert not any(p.is_symlink() for p in paths), 'Reuse package contains a link'
        files = [p for p in paths if p.is_file()]
        assert len(files) <= 8192 and sum(p.stat().st_size for p in files) <= 64*1024**3
        self.before = {}
        cache = {}
        for path in files:
            fp = fingerprint(path)
            digest = cache.get(fp)
            if digest is None:
                digest = sha(path);cache[fp] = digest
            self.before[path.relative_to(package).as_posix()] = dict(state=preserved_file_state(path),sha256=digest)
        manifest_path = package/'project-payload-manifest.json'
        self.manifest_sha256 = self.before['project-payload-manifest.json']['sha256']
        manifest = json.loads(manifest_path.read_text())
        assert manifest.get('schema') == 'valwood.opening-demo-project-payload' and manifest.get('version') == 3
        assert manifest.get('project_directory') == 'project' and manifest.get('fresh_saves') is True
        self.rows = {}
        for row in manifest['files']:
            name = safe_name(row['path'])
            assert name not in self.rows and type(row['bytes']) is int and row['bytes'] >= 0
            assert re.fullmatch('[a-f0-9]{64}',row['sha256'])
            current = self.before['project/'+name]
            assert current['state'][2] == row['bytes'] and current['sha256'] == row['sha256'], 'Old payload is corrupt: '+name
            self.rows[name] = row
        actual = {p.relative_to(package/'project').as_posix() for p in (package/'project').rglob('*') if p.is_file()}
        assert actual == set(self.rows) == set(inputs)|{'graphics.settings'}, 'Reuse requires exactly the planned payload paths'
        assert not any((package/'project/saves').iterdir()), 'Reuse package saves are not fresh'
        assert self.rows['graphics.settings']['sha256'] == hashlib.sha256(GRAPHICS).hexdigest()
        source_cache = {}
        for name,item in inputs.items():
            source = Path(item['source'])
            assert Path(item['lexical']).resolve(strict=True) == source and fingerprint(source) == tuple(item['fingerprint'])
            fp = tuple(item['fingerprint']);digest = source_cache.get(fp)
            if digest is None:
                digest = sha(source);source_cache[fp] = digest
            row = self.rows[name]
            assert source.stat().st_size == row['bytes'] and digest == row['sha256'], 'New source differs from old payload: '+name
            if item['expected_sha256']:
                assert digest == item['expected_sha256'], 'New source differs from its reviewed origin: '+name
        self.linked = []

    def link(self, name, target):
        assert immutable_reuse_name(name), 'Mutable/private payload cannot be hardlinked: '+name
        source = self.package/'project'/name
        before = self.before['project/'+name]
        assert preserved_file_state(source) == tuple(before['state'])
        os.link(source,target,follow_symlinks=False)
        assert not target.is_symlink() and preserved_file_state(target) == tuple(before['state'])
        # Do not chmod this target: its inode belongs to the prior release too.
        self.linked.append(name)
        return self.rows[name]['sha256']

    def verify_preserved(self, verified):
        paths = sorted(self.package.rglob('*'))
        assert not any(p.is_symlink() for p in paths)
        actual = {p.relative_to(self.package).as_posix():p for p in paths if p.is_file()}
        assert set(actual) == set(self.before), 'Prior package inventory changed'
        for name,path in actual.items():
            before = self.before[name]
            assert preserved_file_state(path) == tuple(before['state']), 'Prior package metadata changed: '+name
            # Reused files have the exact inode just independently hashed via
            # the new output. Other prior files receive their own fresh hash.
            digest = verified.get(fingerprint(path))
            if digest is None:
                digest = sha(path)
            assert digest == before['sha256'], 'Prior package bytes changed: '+name
        assert sha(self.package/'project-payload-manifest.json') == self.manifest_sha256

    def record(self):
        return dict(source_payload_manifest_sha256=self.manifest_sha256,
            hardlinked_files=len(self.linked),hardlinked_bytes=sum(self.rows[n]['bytes'] for n in self.linked),
            prior_package_bytes_size_mtime_mode_unchanged=True,
            prior_inode_link_count_and_ctime_change_only=True,no_shared_chmod_or_writes=True,
            copied_private_files=len(self.rows)-len(self.linked),
            policy='Whitelisted model/image/audio binary assets only; no EXE/DLL/config/shader/JSON/save/font/blood/physics sharing.')

def plan(config):
    config.validate()
    SOURCE=config.project;ORIGIN=config.origin;READY=config.runtime_ready
    RUNTIME=config.windows_executable;RUNTIME_CLOSURE=config.runtime_closure
    origin=json.loads(ORIGIN.read_text());assert Path(origin['project'])==SOURCE
    ready=json.loads(READY.read_text())
    assert ready['ready'] is True and ready['compiled_both'] is True
    assert Path(ready['windows'])==RUNTIME and ready['windows_sha256']==sha(RUNTIME)
    # An explicit reviewed origin hash permits packaging a validated private
    # data revision without activating or modifying the user's current game.
    sources={};reason={};expected=dict(origin['generated_sha256'])
    for name,digest in origin['character_assets']['files_sha256'].items():expected['models/longsword_mocap/'+name]=digest
    for name,digest in origin['blood_assets']['files_sha256'].items():expected['blood_assets/'+name]=digest
    def add(name,why,source_path=None):
        name=safe_name(name);p=Path(source_path) if source_path is not None else SOURCE/name;real=p.resolve(strict=True)
        assert real.is_relative_to(ROOT) and real.is_file(),name
        assert not any(x in real.parts for x in ('.ssh','.aws','.codex','.git','Downloads','saves')),name
        assert p.suffix.lower() in ALLOWED or name in ROOT_FILES,name
        sources[name]=dict(source=str(real),lexical=str(p),fingerprint=fingerprint(real),expected_sha256=expected.get(name))
        reason.setdefault(name,set()).add(why)
    def tree(name,why):
        base=SOURCE/name;assert base.is_dir(),name
        def walk(p):
            for q in sorted(p.iterdir()):
                # Windows download ADS metadata is not a shader/runtime file.
                if q.name.endswith('Zone.Identifier'):continue
                assert not q.name.startswith('.') and q.name not in ('saves','captures','__pycache__'),str(q)
                if q.is_dir():walk(q)
                else:
                    assert q.suffix.lower() in ALLOWED and not q.name.endswith(('.qa.walk','.py','.cs','.fbx','.blend')),str(q)
                    add(q.relative_to(SOURCE).as_posix(),why)
        walk(base)
    for name in ROOT_FILES:add(name,'opening root runtime document/marker')
    for name in DIRS:tree(name,'opening runtime directory')
    # Ordinary initial map and attribution only; no staged authoring scenes.
    add('assets/map.jpg','atlas map')
    for q in sorted((SOURCE/'assets').rglob('CREDITS*')):
        if q.is_file() and q.suffix.lower() in ('.txt','.md'):add(q.relative_to(SOURCE).as_posix(),'asset attribution')
    # Conservative fallback for code-driven NPCs and legacy scenery. The
    # first-journey combat host explicitly loads models/longsword_mocap, so
    # the obsolete root-level108-pose bank is not a dependency of this demo.
    for p in sorted((SOURCE/'models').iterdir()):
        if p.is_file() and p.suffix.lower() in ('.glb','.gltf') and not re.fullmatch(r'combat_(?:body|arms)_\d{3}\.glb',p.name):
            add(p.relative_to(SOURCE).as_posix(),'hardcoded legacy scenery/NPC fallback')
    for name in ('models/opening','models/animations','models/watch_quarter','models/longsword_mocap'):
        tree(name,'immutable opening/actor runtime bank')
    origin_bank_fallbacks=[]
    if (SOURCE/'blood_assets').is_dir():
        tree('blood_assets','immutable opening/actor runtime bank')
    else:
        # A private art clone can omit its convenience bank symlink. The
        # reviewed origin still binds all three exact immutable runtime files.
        # Materialize them directly; never repair or mutate the source project.
        bank=Path(origin['blood_assets']['bank'])
        assert bank.is_absolute() and bank.resolve().is_relative_to(ROOT/'assets/purchased/volumetric_blood')
        assert set(origin['blood_assets']['files_sha256'])=={'blood.bank','BLOOD_ASSETS.json','provenance.json'}
        for leaf in sorted(origin['blood_assets']['files_sha256']):
            add('blood_assets/'+leaf,'exact immutable blood bank bound by reviewed origin',bank/leaf)
        origin_bank_fallbacks.append('blood_assets')
    aliases=0;studio_lods=set()
    for line in (SOURCE/'world.studio').read_text().splitlines():
        s=line.split()
        if not s or s[0]!='asset':continue
        assert len(s)==9
        aliases+=1;lods=int(s[3]);assert lods in (1,2,3)
        # Retain all existing Studio-declared paths for compatibility, then
        # add the exact configured production paths below. Existing built-in
        # aliases can retain named_near_file independently of vegetation.
        for lod in range(lods):
            suffix=('_near.glb','_mid.glb','_far.glb')[lod] if int(s[5])==2 else ('.glb','_mid.glb','_far.glb')[lod]
            name='models/'+s[2]+suffix;add(name,'all authored Studio aliases/LODs');studio_lods.add(name)
            # Keep already-cooked BC7 sidecars adjacent to any selected mesh.
            cache=(SOURCE/name).parent/'texture_blocks'
            if cache.is_dir():tree(cache.relative_to(SOURCE).as_posix(),'selected model compressed texture cache')
    assert 1<=aliases<=512 and len(studio_lods)<=1536,(aliases,len(studio_lods))
    proof=json.loads(RUNTIME_CLOSURE.read_text())
    assert proof['passed'] is True and proof['definitions']==aliases
    assert Path(proof['project'])==SOURCE,'Runtime registry proof is for another candidate'
    assert proof['studio_sha256']==sha(SOURCE/'world.studio') and proof['production_source_sha256']==sha(ROOT/'engine/world_scenery.cpp')
    required=sorted(set(proof['runtime_paths'])|set(proof['shadow_proxy_paths']))
    assert required
    for name in required:
        add(name,'actual configured production LOD/shadow path')
    assert all(safe_name(n) in sources and (SOURCE/n).is_file() for n in required)
    # Follow actual external glTF resource URIs; absolute/network URIs fail.
    inspected=set();external=[]
    while True:
        todo=[n for n in sources if n not in inspected and n.endswith(('.glb','.gltf'))]
        if not todo:break
        for name in todo:
            inspected.add(name);path=SOURCE/name
            with path.open('rb') as f:
                if path.suffix=='.glb':
                    h=f.read(20);assert len(h)==20
                    magic,version,total,count,kind=struct.unpack('<IIIII',h)
                    assert magic==0x46546c67 and version==2 and total==path.stat().st_size and kind==0x4e4f534a and count<=16*1024*1024,name
                    doc=json.loads(f.read(count))
                else:doc=json.load(f)
            for part in doc.get('buffers',[])+doc.get('images',[]):
                uri=part.get('uri')
                if uri is None or uri.startswith('data:'):continue
                u=urlsplit(uri);assert not(u.scheme or u.netloc or u.query or u.fragment),uri
                relative=PurePosixPath(name).parent/PurePosixPath(unquote(u.path))
                normalized=os.path.normpath(relative.as_posix()).replace(os.sep,'/')
                add(normalized,'external glTF buffer/image');external.append(dict(owner=name,path=normalized))
    windows_names(list(sources)+['graphics.settings'])
    assert all('saves' not in PurePosixPath(n).parts for n in sources)
    assert len([n for n in sources if re.fullmatch(r'models/longsword_mocap/combat_(body|arms)_\d{3}\.glb',n)])==344
    assert {'blood_assets/blood.bank','blood_assets/BLOOD_ASSETS.json','blood_assets/provenance.json'}<=sources.keys()
    return sources,dict(schema='valwood.opening-demo-payload-plan',version=3,
        source_reference=dict(candidate_origin_sha256=sha(ORIGIN),runtime_ready_sha256=sha(READY),windows_binary_sha256=sha(RUNTIME)),
        source_project=str(SOURCE),origin_bound_bank_fallbacks=origin_bank_fallbacks,files=len(sources)+1,bytes=sum(x['fingerprint'][2] for x in sources.values())+len(GRAPHICS),
        studio_aliases=aliases,studio_lod_paths=len(studio_lods),model_glbs=len(inspected),external_gltf_resources=external,
        runtime_alias_proof=dict(path=str(RUNTIME_CLOSURE),sha256=sha(RUNTIME_CLOSURE),runtime_paths=required),
        excluded=['user saves/preferences','root obsolete combat pose duplicates','unreferenced managed import revisions','unused dragon/jungle/visual upgrade sub-banks','editor tools/Python/source archives'],
        reasons={n:sorted(r)for n,r in reason.items()})

def build(config):
    config.validate();output=config.output
    SOURCE=config.project;ORIGIN=config.origin;READY=config.runtime_ready
    RUNTIME=config.windows_executable;RUNTIME_CLOSURE=config.runtime_closure
    output.parent.mkdir(exist_ok=True);output.mkdir(exist_ok=True)
    assert not output.is_symlink() and not (output/'project').exists() and not (output/'project-payload-manifest.json').exists()
    inputs,review=plan(config);reference=review['source_reference'];origin_before=reference['candidate_origin_sha256']
    reuse = ImmutableReuse(config.reuse_immutable_from,inputs) if config.reuse_immutable_from else None
    old=config.previous_manifest;old_manifest_sha256=sha(old) if old else None
    config.audit_dir.mkdir(parents=True,exist_ok=True)
    assert not any((config.audit_dir/n).exists() for n in ('INPUTS.json','READY.json'))
    stage=Path(tempfile.mkdtemp(prefix='.opening-demo-project-',dir=output.parent));entries=[];memo={}
    print(json.dumps({'copying':True,'files':review['files'],'bytes':review['bytes'],'project_destination':str(output/'project')}),flush=True)
    try:
        for i,(name,item) in enumerate(sorted(inputs.items())):
            source=Path(item['source']);lexical=Path(item['lexical']);assert lexical.resolve(strict=True)==source
            assert fingerprint(source)==tuple(item['fingerprint']),name
            target=stage/name;target.parent.mkdir(parents=True,exist_ok=True)
            key=tuple(item['fingerprint']);prior=memo.get(key)
            shared_prior = reuse is not None and immutable_reuse_name(name)
            if shared_prior:
                digest = reuse.link(name,target)
            elif prior:
                # Same-source alias dedup is internal to this new package only.
                # Ordinary local hardlinks retain self-contained Windows ZIP extraction.
                previous,digest=prior;os.link(previous,target)
            else:
                h=hashlib.sha256()
                with source.open('rb') as incoming,target.open('xb') as outgoing:
                    while data:=incoming.read(4*1024*1024):h.update(data);outgoing.write(data)
                digest=h.hexdigest();memo[key]=(target,digest)
            assert fingerprint(source)==tuple(item['fingerprint']),name
            if item['expected_sha256']:assert digest==item['expected_sha256'],name
            if not shared_prior:
                os.chmod(target,0o644)
            entries.append(dict(path=name,bytes=target.stat().st_size,sha256=digest,source_sha256=digest,generated=False))
            if i%100==0:print(json.dumps({'copied':i+1,'of':len(inputs),'path':name}),flush=True)
        (stage/'graphics.settings').write_bytes(GRAPHICS)
        entries.append(dict(path='graphics.settings',bytes=len(GRAPHICS),sha256=hashlib.sha256(GRAPHICS).hexdigest(),source_sha256=None,generated=True))
        (stage/'saves').mkdir(mode=0o755)
        # Independent output SHA verification, including all alias payloads.
        verified={}
        for row in entries:
            p=stage/row['path'];assert not p.is_symlink()
            fp=fingerprint(p);digest=verified.get(fp)
            if digest is None:digest=sha(p);verified[fp]=digest
            assert p.stat().st_size==row['bytes'] and digest==row['sha256'],row['path']
        for n,item in inputs.items():
            assert Path(item['lexical']).resolve(strict=True)==Path(item['source']) and fingerprint(Path(item['source']))==tuple(item['fingerprint']),n
        assert sha(ORIGIN)==origin_before and sha(READY)==reference['runtime_ready_sha256'] and sha(RUNTIME)==reference['windows_binary_sha256']
        assert not any(p.is_symlink() for p in stage.rglob('*'))
        assert old is None or sha(old)==old_manifest_sha256
        assert sha(RUNTIME_CLOSURE)==review['runtime_alias_proof']['sha256']
        assert all((stage/n).is_file() for n in review['runtime_alias_proof']['runtime_paths'])
        if reuse:
            reuse.verify_preserved(verified)
            review['immutable_reuse'] = dict(source_package=str(reuse.package),**reuse.record())
        manifest=dict(schema='valwood.opening-demo-project-payload',version=3,project_directory='project',files=sorted(entries,key=lambda x:x['path']),
            source_reference=reference,bytes=sum(x['bytes']for x in entries),fresh_saves=True,graphics_default='Balanced',
            studio_aliases=review['studio_aliases'],studio_lod_paths=review['studio_lod_paths'],pose_assets=344,blood_variant_count=3,no_symlinks=True,source_inputs_unchanged=True,
            runtime_alias_proof={k:review['runtime_alias_proof'][k] for k in ('sha256','runtime_paths')},previous_payload_manifest_sha256=old_manifest_sha256,
            origin_bound_bank_fallbacks=review['origin_bound_bank_fallbacks'],runtime_not_launched=True,gpu_used=False,closure=review['excluded'])
        if reuse:
            manifest['immutable_reuse'] = reuse.record()
        _publish_directory(stage,output/'project')
        js(output/'project-payload-manifest.json',manifest)
        audit=config.audit_dir;assert audit.is_dir() and not (audit/'READY.json').exists()
        js(audit/'INPUTS.json',dict(plan=review,inputs=inputs,tool_sha256=sha(Path(__file__))))
        if reuse:
            js(audit/'REUSE.json',dict(source_package=str(reuse.package),prior_files_before=reuse.before,**reuse.record()))
        result=dict(ready=True,project=str(output/'project'),manifest=str(output/'project-payload-manifest.json'),manifest_sha256=sha(output/'project-payload-manifest.json'),files=len(entries),bytes=manifest['bytes'],gib=manifest['bytes']/2**30,source_inputs_unchanged=True,no_symlinks=True,empty_saves=True,output_hashes_verified=True,gpu_used=False)
        js(audit/'READY.json',result);return result
    finally:
        if stage.exists():shutil.rmtree(stage)

def arguments():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('project','origin','runtime-ready','windows-executable','runtime-closure','output','audit-dir'):
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--origin-sha256',required=True,help='Exact SHA256 of the reviewed final candidate origin')
    p.add_argument('--previous-manifest',type=Path)
    p.add_argument('--reuse-immutable-from',type=Path,help='Opt-in exact-data reuse from an immutable prior release; all private files still copied')
    p.add_argument('--plan',action='store_true')
    a=p.parse_args()
    config=Inputs(a.project.absolute(),a.origin.absolute(),a.origin_sha256,
        a.runtime_ready.absolute(),a.windows_executable.absolute(),
        a.runtime_closure.absolute(),a.output.absolute(),a.audit_dir.absolute(),
        a.previous_manifest.absolute() if a.previous_manifest else None,
        a.reuse_immutable_from.absolute() if a.reuse_immutable_from else None)
    return config,a.plan

if __name__=='__main__':
    config,plan_only=arguments()
    if plan_only:
        _,r=plan(config);r.pop('reasons');print(json.dumps(r,indent=2))
    else:print(json.dumps(build(config),indent=2))
