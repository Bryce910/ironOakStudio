#!/usr/bin/env python3
"""Run the existing normal-input QTE review after the full apartment opening.

The private window must already be at FleeToAmbush. This wrapper changes only
where input review begins; all real missed-duck, retry, fresh duck, F taps,
stab contact and durable receipt checks come from the unchanged QTE driver.
No game inputs are sent until run() is explicitly invoked by the root reviewer.
"""
from pathlib import Path
import argparse
import importlib.util
import json
import sys

N=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('normal_qte_review',N/'tools/play_battlefield_qte.py')
base=importlib.util.module_from_spec(spec);spec.loader.exec_module(base)

class FromPlaying(base.Driver):
    def continue_checkpoint(self):
        row=self.observe()
        if row.get('_stage')!='flee_to_ambush' or row.get('screen')!=1 or row.get('loading') or row.get('paused'):
            raise base.DriverError('Start this wrapper at actual unpaused FleeToAmbush after the apartment opening.')
        self.release_all();self.sleep(.30);self.shot('01-before-ambush-from-full-opening')
        self.log('start_from_existing_normal_play',observer=row)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--review',default=str(N/'review/live-full'))
    p.add_argument('--output',default='qte-after-apartment-driver')
    p.add_argument('--timeout',type=float,default=120)
    p.add_argument('--record',action='store_true')
    args=p.parse_args()
    if not 30<=args.timeout<=120:p.error('--timeout must be30–120 seconds')
    if Path(args.output).name!=args.output:p.error('--output must be a simple folder name')
    driver=None
    try:
        driver=FromPlaying(args);driver.run();return driver.finish()
    except BaseException as exc:
        if driver is not None:return driver.finish(exc)
        print(json.dumps({'passed':False,'error':str(exc),'inputs_sent':False}),file=sys.stderr);return 1

if __name__=='__main__':raise SystemExit(main())
