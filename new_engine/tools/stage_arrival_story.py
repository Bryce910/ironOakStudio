#!/usr/bin/env python3
"""Install the reviewed first-contact story into an isolated arrival runtime.

Accepted engine sources and prior runtime binaries are never modified. Root's
build helper owns the remaining viewer/renderer changes and both builds.
"""
from pathlib import Path
import argparse,hashlib,json,os,tempfile
ROOT=Path(__file__).resolve().parents[1]
STORY=ROOT/'docs/arrival-campsite/2026-11-04/story'
FILES=('world_opening_arrival_camp.h','world_opening_prologue.h','world_opening_prologue_runtime.inc')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--runtime-root',type=Path,default=ROOT/'build/arrival-campsite/runtime-source');args=parser.parse_args()
    target=args.runtime_root.resolve()
    if not target.is_relative_to((ROOT/'build/arrival-campsite').resolve()) or target==ROOT:
        raise SystemExit('Only an isolated build/arrival-campsite runtime is a valid target')
    directory=target/'engine'
    if not directory.is_dir():raise SystemExit('Create the private runtime source tree first')
    staged=STORY/'runtime/engine'
    review=json.loads((STORY/'CPU_STORY_REVIEW.json').read_text())
    guard={ROOT/record['accepted_source_path']:record['accepted_sha256'] for record in review['files'] if record['accepted_sha256']}
    if any(sha(path)!=expected for path,expected in guard.items()):
        raise SystemExit('Accepted source baseline changed; review and regenerate the isolated patch first')
    records=[]
    # Read all payloads before making the first change.
    payloads=[(name,(staged/name).read_bytes()) for name in FILES]
    for name,data in payloads:
        path=directory/name
        fd,tmp=tempfile.mkstemp(prefix='.'+name+'-',dir=directory)
        try:
            with os.fdopen(fd,'wb') as stream:stream.write(data);stream.flush();os.fsync(stream.fileno())
            os.chmod(tmp,0o644);os.replace(tmp,path)
        finally:
            if os.path.exists(tmp):os.unlink(tmp)
        records.append(dict(path=str(path.relative_to(ROOT)),sha256=sha(path)))
    if any(sha(path)!=expected for path,expected in guard.items()):
        raise SystemExit('Accepted source baseline unexpectedly changed during private staging')
    report=dict(passed=True,runtime_root=str(target.relative_to(ROOT)),files=records,accepted_sources_unchanged=True)
    (STORY/'PRIVATE_SOURCE_STAGE.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
if __name__=='__main__':main()
