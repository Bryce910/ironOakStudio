#!/usr/bin/env python3
"""Normal Continue into a genuine saved private checkpoint; no state edits."""
from pathlib import Path
import argparse, importlib.util, json, subprocess, time, types
N = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('private_continue_controls', N/'tools/play_battlefield_qte.py')
base = importlib.util.module_from_spec(spec); spec.loader.exec_module(base)
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--review', default=str(N/'review/live-full'))
p.add_argument('--output', default='normal-continue')
p.add_argument('--expect-stage', default='escape')
a = p.parse_args()
d = base.Driver(types.SimpleNamespace(review=a.review, output=a.output, record=False, timeout=200))
try:
    end=time.monotonic()+30
    while time.monotonic()<end:
        rows=subprocess.check_output(['wmctrl','-lp'],text=True).splitlines()
        if any(base.TITLE in row and int(row.split()[2])==d.pid for row in rows): break
        d.sleep(.10)
    else: raise RuntimeError('Private native window did not appear.')
    d.setup_controls()
    end = time.monotonic()+85
    while time.monotonic()<end:
        row=d.observe()
        if row.get('screen')==0 and not row.get('loading',False): break
        d.sleep(.06)
    else: raise RuntimeError('Private title did not become ready.')
    d.tap('Down',.15); d.sleep(.25); d.tap('Return',.30)
    end=time.monotonic()+105
    while time.monotonic()<end:
        row=d.observe(); d.playing(row)
        if row.get('_stage')==a.expect_stage and not row.get('loading') and not row.get('paused'): break
        d.sleep(.06)
    else: raise RuntimeError('Normal Continue did not restore expected stage '+a.expect_stage)
    d.shot('restored-checkpoint')
    (d.output/'result.json').write_text(json.dumps({'passed':True,'normal_inputs_only':True,'observer':row},indent=2)+'\n')
    print('Private Continue ready:', json.dumps(row), flush=True)
finally:
    d.release_all(); d.logfile.close()
