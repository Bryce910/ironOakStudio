#!/usr/bin/env python3
"""Run the actual versioned installer on tiny fixture data under bundled Windows Python/Wine."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess
import sys
import zipfile

sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'));sys.path.insert(0,str(ROOT/'tests'))
from windows_playtest_overlay_test import WindowsPlaytestOverlayTest

CHECK=r'''
import hashlib,json,sys
from pathlib import Path
sys.dont_write_bytecode=True
bundle=Path(sys.argv[1]);before=json.loads(Path(sys.argv[2]).read_text())
sys.path.insert(0,str(bundle))
import Install_Playtest_Bundle as installer
first=bundle.parent/'new-playtest-one';second=bundle.parent/'new-playtest-two'
one=installer.install(bundle,'city',first)
two=installer.install(bundle,'city',second)
assert one['independent_files'] and two['independent_files']
for seed in (first,second):
 for path in seed.rglob('*'):
  assert not path.is_symlink(),str(path)
  if path.is_file():assert path.stat().st_nlink==1,str(path)
original=(second/'project/playtest.route').read_bytes()
(first/'project/playtest.route').write_bytes(b'isolated edit')
assert (second/'project/playtest.route').read_bytes()==original
try:installer.install(bundle,'city',first)
except ValueError as error:assert 'already exists' in str(error)
else:raise AssertionError('Existing seed replacement was accepted')
outside=bundle.parent/'outside';outside.mkdir()
hidden=outside/'..'/bundle.name/'must-not-install'
try:installer.install(bundle,'city',hidden)
except ValueError as error:assert 'outside the extracted bundle' in str(error)
else:raise AssertionError('Dotdot containment bypass was accepted')
assert not (bundle/'must-not-install').exists()
for name,digest in before.items():
 assert hashlib.sha256((bundle/name).read_bytes()).hexdigest()==digest,name
print(json.dumps(dict(passed=True,platform=sys.platform,python=sys.version,installer=installer.__file__,
 independent_seed_installs=2,existing_destination_rejected=True,dotdot_containment_rejected=True,
 base_files_unchanged=len(before),file_links_created=False,game_or_sdl_launched=False)))
'''


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--python',type=Path,default=ROOT/'build/windows/editor/bin/python/python.exe')
    parser.add_argument('--wine-prefix',type=Path,default=ROOT/'build/windows/wine-prefix')
    args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    fixture=WindowsPlaytestOverlayTest();fixture.setUp()
    try:
        add_on,_=fixture.build();bundle,before=fixture.extracted(add_on)
        fixture.fixture.engine.rename(fixture.root/'source-away')
        script=fixture.root/'windows-check.py';script.write_text(CHECK)
        proof=fixture.root/'base-hashes.json';proof.write_text(json.dumps(before))
        win=lambda path:'Z:'+str(path.resolve()).replace('/','\\')
        command=['/usr/lib/wine/wine64',win(args.python),'-B',win(script),win(bundle),win(proof)]
        env=os.environ.copy();env.update(WINEPREFIX=str(args.wine_prefix.resolve()),WINEDEBUG='-all',WINEDLLOVERRIDES='mscoree,mshtml=')
        result=subprocess.run(command,env=env,capture_output=True,text=True,timeout=120)
        (out/'installer.log').write_text(result.stdout+result.stderr)
        report=dict(passed=False,exit_code=result.returncode,command=command,
                    installer_sha256=hashlib.sha256((ROOT/'tools/install_windows_playtest_bundle.py').read_bytes()).hexdigest(),
                    scope='Actual bundled Windows Python and versioned standalone installer; tiny real base+overlay package, no game/SDL/GPU or production seed extraction.')
        if result.returncode==0:
            report.update(json.loads(result.stdout));report['passed']=report.get('passed')is True
        else:report['diagnostic']=result.stdout+result.stderr
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(report,indent=2));return 0 if report['passed']else 1
    finally:fixture.tearDown()


if __name__=='__main__':raise SystemExit(main())
