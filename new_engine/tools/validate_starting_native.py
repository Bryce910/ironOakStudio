#!/usr/bin/env python3
"""Run compiled GL correctness fixtures on the actual Radeon (not a benchmark).

Build fixtures with `make test` first; this does not compile or profile.
"""
import argparse,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'docs/performance/starting-area'
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=OUT,metavar='DIRECTORY',help='Native fixture log directory (default: %(default)s).')
    args=parser.parse_args();output=args.output.expanduser()
    fixtures=['water','foliage','lighting','post','scenery','temporal','models','sky','occlusion','weather','shadow-filter','village-population']
    for name in fixtures:
        binary=Path('/tmp/valwood-world-'+name+'-gl-test');assert binary.is_file(),f'Run make test first: {binary}'
    output.mkdir(exist_ok=True,parents=True);env=os.environ.copy();env['VALWOOD_TEST_NATIVE_GL']='1';env['SDL_AUDIODRIVER']='dummy'
    for name in fixtures:
        binary=Path('/tmp/valwood-world-'+name+'-gl-test')
        log=output/(name+'-native.log')
        with log.open('w') as stream:subprocess.run([str(binary),str(ROOT)],cwd=ROOT,env=env,stdout=stream,stderr=subprocess.STDOUT,check=True)
        text=log.read_text();assert 'AMD Radeon RX 5300' in text,(name,text[:500])
        print('Native Radeon correctness PASS: '+name,flush=True)
if __name__=='__main__':main()
