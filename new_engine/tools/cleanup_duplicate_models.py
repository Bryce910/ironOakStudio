#!/usr/bin/env python3
"""Reclaim exact model duplicates without removing any game-visible path.

Equal-sized candidates inside saved builds/docs are first sampled, then SHA256
checked in full. Applying shares their inode with hard links on the same
filesystem; standalone packages retain every file and remain copyable. Editable
project/source models, archives, textures, saves and source code are excluded.
To edit a saved package copy independently, replace it atomically first.
"""
from pathlib import Path
import argparse,hashlib,json,os,stat,time
from collections import defaultdict

EXTENSIONS={'.glb','.gltf','.fbx','.obj','.blend','.dae','.3ds'}
SKIP={'.git','.codex','.agents','.aws','node_modules','__pycache__','third_party'}
def full_hash(path):
    h=hashlib.sha256()
    with path.open('rb')as f:
        for block in iter(lambda:f.read(4*1024*1024),b''):h.update(block)
    return h.hexdigest()
def sample_hash(path,size):
    h=hashlib.sha256()
    with path.open('rb')as f:
        for offset in sorted({0,max(0,size//2-32768),max(0,size-65536)}):
            f.seek(offset);h.update(f.read(65536))
    return h.digest()
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--report',type=Path,required=True);p.add_argument('--apply',action='store_true');a=p.parse_args()
    root=a.root.resolve();groups=defaultdict(list);seen=set();examined=0
    for directory,dirs,files in os.walk(root,followlinks=False):
        dirs[:]=[n for n in dirs if n not in SKIP and not Path(directory,n).is_symlink()]
        for name in files:
            path=Path(directory,name)
            if path.suffix.lower()not in EXTENSIONS or path.is_symlink():continue
            if not {'builds','docs'}.intersection(path.relative_to(root).parts):continue
            st=path.stat();examined+=1;inode=(st.st_dev,st.st_ino)
            if not stat.S_ISREG(st.st_mode)or st.st_size==0 or inode in seen:continue
            seen.add(inode);groups[(st.st_dev,st.st_size,stat.S_IMODE(st.st_mode))].append((path,st))
    print(f'Scanned {examined} model paths; {len(seen)} distinct nonempty inodes',flush=True)
    exact=defaultdict(list);sampled=0;hashed=0
    for key,files in groups.items():
        if len(files)<2:continue
        samples=defaultdict(list)
        for path,st in files:samples[sample_hash(path,st.st_size)].append((path,st));sampled+=1
        for candidates in samples.values():
            if len(candidates)<2:continue
            for path,st in candidates:
                digest=full_hash(path);after=path.stat()
                if (st.st_ino,st.st_size,st.st_mtime_ns)!=(after.st_ino,after.st_size,after.st_mtime_ns):raise RuntimeError(f'Model changed during audit: {path}')
                exact[(key,digest)].append((path,st));hashed+=1
                if hashed%1000==0:print(f'Fully verified {hashed} candidate models',flush=True)
    linked=[];recoverable=0;skipped=[]
    for (key,digest),files in exact.items():
        if len(files)<2:continue
        # Prefer the active project's source path, then the shallowest path.
        files.sort(key=lambda v:('/builds/'in str(v[0]),'/docs/'in str(v[0]),len(v[0].parts),str(v[0])))
        canonical,canonical_st=files[0]
        for path,st in files[1:]:
            if st.st_nlink!=1:
                skipped.append(str(path.relative_to(root)));continue
            row=dict(path=str(path.relative_to(root)),canonical=str(canonical.relative_to(root)),bytes=st.st_size,sha256=digest)
            if a.apply:
                now=path.stat();base=canonical.stat()
                if (now.st_ino,now.st_size,now.st_mtime_ns)!=(st.st_ino,st.st_size,st.st_mtime_ns)or(base.st_ino,base.st_size,base.st_mtime_ns)!=(canonical_st.st_ino,canonical_st.st_size,canonical_st.st_mtime_ns):raise RuntimeError(f'Model changed before replacement: {path}')
                temp=path.with_name(path.name+f'.dedup-{os.getpid()}')
                try:os.link(canonical,temp);os.replace(temp,path)
                finally:
                    if temp.exists():temp.unlink()
                if path.stat().st_ino!=canonical.stat().st_ino:raise RuntimeError(f'Hard-link verification failed: {path}')
            linked.append(row);recoverable+=st.st_blocks*512
    result=dict(applied=a.apply,root=str(root),scanned_paths=examined,fully_hashed_models=hashed,duplicates=len(linked),physical_bytes_recovered=recoverable if a.apply else 0,physical_bytes_recoverable=recoverable,already_shared_inodes_skipped=skipped,paths_preserved=True,source_archives_untouched=True,models=linked)
    a.report.parent.mkdir(parents=True,exist_ok=True);a.report.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items()if k not in ['models','already_shared_inodes_skipped']}),flush=True)
if __name__=='__main__':main()
