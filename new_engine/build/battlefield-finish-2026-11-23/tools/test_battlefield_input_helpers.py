#!/usr/bin/env python3
"""CPU-only input-harness feedback checks. Never connects to X11 or a game."""
from pathlib import Path
import importlib.util
import math
import json
import tempfile
import types
import unittest
from unittest.mock import patch

path=Path(__file__).with_name('play_battlefield_escape.py')
spec=importlib.util.spec_from_file_location('escape_input_harness_test',path)
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class Clock:
    now=0.
    def monotonic(self):return self.now
    def sleep(self,seconds):self.now+=seconds

class CameraFeedback:
    def __init__(self,clock,stale=False):self.clock=clock;self.events=[(-1.,0.)];self.stale=stale
    def yaw(self,at=None):
        at=self.clock.now if at is None else at
        return next(yaw for when,yaw in reversed(self.events) if when<=at)
    def read(self):
        # Reproduce a buffered trace flush every700ms containing samples that
        # were already150ms old at the flush. This used to cause overshoot.
        sampled=0. if self.stale else max(0.,math.floor(self.clock.now/.7)*.7-.15)
        return {'frame':int(sampled*60),'eye':[0,2,0,self.yaw(sampled),0,1.]}

class AlignmentTests(unittest.TestCase):
    def test_warp_relative_camera_uses_guarded_absolute_window_motion(self):
        driver=m.EscapeDriver.__new__(m.EscapeDriver);calls=[]
        driver.bounds=(3040,302,1600,900);driver.focus_guard=lambda:calls.append(('guard',))
        driver.log=lambda *args,**kwargs:None
        driver.m=types.SimpleNamespace(D='private-display',T=types.SimpleNamespace(XTestFakeMotionEvent=lambda *args:calls.append(args)),X=types.SimpleNamespace(XFlush=lambda *_:None))
        driver.relative(100,-20)
        self.assertEqual(calls,[('guard',),('private-display',-1,3940,732,0)])
        driver.bounds=(0,0,640,360)
        with self.assertRaises(m.DriverError):driver.relative(600,0)
        self.assertEqual(len(calls),3) # Second guard, but no out-of-window motion.

    def test_frame_tail_seeds_recent_complete_rows_and_handles_append(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'frames.jsonl'
            with path.open('w') as stream:
                for frame in range(4000):stream.write(json.dumps({'frame':frame,'eye':[0,2,0,0,0,1],'padding':'x'*35})+'\n')
            reader=m.FrameTail(path);self.assertEqual(reader.read()['frame'],3999)
            self.assertGreater(reader.position,65536)
            with path.open('a') as stream:stream.write('{"frame":4000')
            self.assertEqual(reader.read()['frame'],3999)
            with path.open('a') as stream:stream.write('}\n')
            self.assertEqual(reader.read()['frame'],4000)
            path.write_text('{"frame":2}\n');self.assertEqual(reader.read()['frame'],2)

    def driver(self,logical=False,stale=False):
        clock=Clock();feedback=CameraFeedback(clock,stale)
        driver=m.EscapeDriver.__new__(m.EscapeDriver)
        driver.args=types.SimpleNamespace(sensitivity=1.)
        driver.deadline=30.;driver.frames=feedback;driver.observation_version=0
        sent=[]
        def observe():
            driver.observation_version+=1
            row={'_stage':'escape','root':[0,0,0],'enemy':-1}
            if logical:row['yaw']=feedback.yaw()
            return row
        def relative(pixels):
            sent.append((clock.now,pixels));feedback.events.append((clock.now,feedback.yaw()+pixels*.0022))
        driver.require_live=observe;driver.playing=lambda _:True
        driver.sleep=clock.sleep;driver.relative=relative
        return driver,clock,feedback,sent

    def test_buffered_camera_waits_for_turn_and_settle(self):
        driver,clock,feedback,sent=self.driver()
        target=(10*math.sin(.8),-10*math.cos(.8))
        with patch.object(m.time,'monotonic',clock.monotonic):driver.aligned(target)
        self.assertEqual(len(sent),2)
        self.assertGreater(sent[1][0]-sent[0][0],1.0)
        self.assertLess(abs(m.angle_delta(feedback.yaw(),.8)),.045)

    def test_flushed_logical_yaw_does_not_wait_for_render_trace(self):
        driver,clock,feedback,sent=self.driver(logical=True,stale=True)
        with patch.object(m.time,'monotonic',clock.monotonic):driver.aligned((10*math.sin(.8),-10*math.cos(.8)))
        self.assertEqual(len(sent),2);self.assertLess(clock.now,1.)
        self.assertLess(abs(m.angle_delta(feedback.yaw(),.8)),.045)

    def test_stale_trace_stops_without_camera_input(self):
        driver,clock,_,sent=self.driver(stale=True)
        with patch.object(m.time,'monotonic',clock.monotonic):
            with self.assertRaises(m.DriverError):driver.aligned((10,0))
        self.assertEqual(sent,[])

    def test_exact_enemy_root_replaces_stale_spawn_distance(self):
        driver,_,_,_=self.driver()
        row={'root':[-12078,12,5144],'enemy':0,'enemy_root':[-12076.2,12,5144]}
        self.assertAlmostEqual(driver.enemy_distance(row,0),1.8)
        row.pop('enemy_root');self.assertAlmostEqual(driver.enemy_distance(row,0),8.)

if __name__=='__main__':unittest.main()
