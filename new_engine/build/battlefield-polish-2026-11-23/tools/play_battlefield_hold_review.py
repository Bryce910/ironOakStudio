#!/usr/bin/env python3
"""Normal-input review of the optional continuous-hold struggle control.

The root reviewer must first enable the ordinary pause-menu checkbox
"Hold A / F instead of repeated taps", then run this in the unlocked private
window at FleeToAmbush, Duck or a newly entered Struggle. This helper never
changes settings directly. It ducks normally if needed, holds F continuously,
and requires the actual stab/sword/checkpoint result without repeated taps.
"""
from pathlib import Path
import argparse
import importlib.util
import json
import sys
import time

N=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('hold_qte_review',N/'tools/play_battlefield_qte.py')
base=importlib.util.module_from_spec(spec);spec.loader.exec_module(base)

class HoldReview(base.Driver):
    def __init__(self,args):
        super().__init__(args);self.hold_wall_seconds=0.;self.hold_started_elapsed=0.;self.maximum_tap_count=0

    def run(self):
        self.setup_controls();self.start_recording();row=self.observe()
        if row.get('screen')!=1 or row.get('paused') or row.get('loading'):
            raise base.DriverError('The private game must be playing and unpaused.')
        if row.get('_stage')=='flee_to_ambush':self.walk_to_duck('01-hold-review-duck')
        if self.stage()=='duck':
            end=min(self.deadline,time.monotonic()+4)
            while time.monotonic()<end:
                row=self.observe()
                if row.get('_stage')!='duck':raise base.DriverError('The duck window ended before the normal Q press.')
                if float(row.get('elapsed',0))>=.38:break
                self.sleep(.025)
            self.tap('q',.10);self.wait_stage('tackle',timeout=4);self.ducked=True
            self.wait_stage('struggle',timeout=10)
        if self.stage()!='struggle':raise base.DriverError('Start hold review at FleeToAmbush, Duck or a newly entered Struggle.')
        self.key('f',False);self.sleep(.20);row=self.observe()
        self.hold_started_elapsed=float(row.get('elapsed',0));self.shot('02-continuous-hold-start')
        started=time.monotonic();end=min(self.deadline,started+20)
        self.key('f',True)
        try:
            while time.monotonic()<end:
                row=self.observe();stage=row.get('_stage')
                self.maximum_tap_count=max(self.maximum_tap_count,int(row.get('struggle_presses',0)))
                if stage=='stab':self.stole=True;break
                if stage=='dead':raise base.DriverError('Continuous hold did not win. Verify the real pause-menu hold checkbox was enabled.')
                if stage!='struggle' or row.get('paused') or row.get('loading'):
                    raise base.DriverError('Hold review lost QTE ownership; releasing input instead of carrying it through a pause.')
                self.sleep(.035)
            else:raise base.DriverError('A continuous real F hold did not produce the actual stab within20 seconds.')
        finally:
            self.key('f',False);self.hold_wall_seconds=time.monotonic()-started
        if self.maximum_tap_count!=0:raise base.DriverError('Struggle recorded tap progress: this was not the continuous-hold alternative.')
        self.shot('03-hold-won-stab');row=self.wait_stage('escape',timeout=20)
        if not row.get('sword'):raise base.DriverError('The actual hold-success stab did not grant the recovered sword.')
        self.shot('04-hold-escape-with-sword');self.verify_receipts()
        self.log('hold_review_complete',hold_seconds=self.hold_wall_seconds,maximum_tap_count=self.maximum_tap_count,receipts=self.receipts)

    def finish(self,error=None):
        code=super().finish(error);path=self.output/'result.json';result=json.loads(path.read_text())
        for key in ('intentional_missed_duck_death','local_retry','fresh_duck_success','tap_struggle_success','f_taps'):result.pop(key,None)
        result.update({'review':'battlefield_continuous_hold_normal_inputs','hold_enabled_through_normal_pause_menu_by_operator':True,
                       'continuous_hold_seconds':self.hold_wall_seconds,'hold_started_at_story_elapsed':self.hold_started_elapsed,
                       'maximum_struggle_taps':self.maximum_tap_count,'repeated_taps_sent':0})
        path.write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps({'result':str(path),'passed':result['passed'],'error':result['error'],'held_seconds':self.hold_wall_seconds},indent=2),flush=True)
        return code

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--review',default=str(N/'review/live-battlefield'))
    p.add_argument('--output',default='hold-alternative-driver')
    p.add_argument('--timeout',type=float,default=120)
    p.add_argument('--record',action='store_true')
    p.add_argument('--hold-already-enabled',action='store_true',required=True,help='Confirm the operator enabled the normal pause-menu hold checkbox.')
    args=p.parse_args()
    if not 45<=args.timeout<=150:p.error('--timeout must be45–150 seconds')
    if Path(args.output).name!=args.output:p.error('--output must be a simple folder name')
    driver=None
    try:
        driver=HoldReview(args);driver.run();return driver.finish()
    except BaseException as exc:
        if driver is not None:return driver.finish(exc)
        print(json.dumps({'passed':False,'error':str(exc),'inputs_sent':False}),file=sys.stderr);return 1

if __name__=='__main__':raise SystemExit(main())
