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
    def __init__(self,clock,stale=False):
        self.clock=clock;self.events=[(-1.,0.)];self.stale=stale
        self.playable_after=0.;self.include_readiness=True
    def yaw(self,at=None):
        at=self.clock.now if at is None else at
        return next(yaw for when,yaw in reversed(self.events) if when<=at)
    def read(self):
        # Reproduce a buffered trace flush every700ms containing samples that
        # were already150ms old at the flush. This used to cause overshoot.
        sampled=0. if self.stale else max(0.,math.floor(self.clock.now/.7)*.7-.15)
        row={'frame':int(sampled*60),'eye':[0,2,0,self.yaw(sampled),0,1.],
             'third_person':True}
        if self.include_readiness:row['camera_safety_playable']=self.clock.now>=self.playable_after
        return row

def feedback_driver(logical=False,stale=False):
    clock=Clock();feedback=CameraFeedback(clock,stale)
    driver=m.EscapeDriver.__new__(m.EscapeDriver)
    driver.args=types.SimpleNamespace(sensitivity=1.,melee_free_look=False)
    driver.deadline=30.;driver.frames=feedback;driver.observation_version=0
    sent=[]
    def observe():
        driver.observation_version+=1
        row={'_stage':'escape','root':[0,0,0],'enemy':-1,'frame':int(clock.now*60)}
        if logical:row['yaw']=feedback.yaw()
        return row
    def relative(pixels):
        sent.append((clock.now,pixels));feedback.events.append((clock.now,feedback.yaw()+pixels*.0022))
    driver.require_live=observe;driver.playing=lambda _:True
    driver.sleep=clock.sleep;driver.relative=relative
    return driver,clock,feedback,sent

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
        return feedback_driver(logical,stale)

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
            with self.assertRaisesRegex(m.DriverError,'feedback did not become fresh and settled'):driver.aligned((10,0))
        self.assertEqual(sent,[])

    def test_camera_readiness_is_required_even_with_fresh_logical_yaw(self):
        for missing in (False,True):
            with self.subTest(missing_readiness_field=missing):
                driver,clock,feedback,sent=self.driver(logical=True)
                feedback.playable_after=10.;feedback.include_readiness=not missing
                with patch.object(m.time,'monotonic',clock.monotonic):
                    with self.assertRaisesRegex(m.DriverError,'fresh playable third-person view'):
                        driver.aligned((10,0))
                self.assertEqual(sent,[]);self.assertGreaterEqual(clock.now,5.)

    def test_playable_camera_older_than_battlefield_frame_is_rejected(self):
        driver,clock,_,sent=self.driver(logical=True)
        observe=driver.require_live
        driver.require_live=lambda:dict(observe(),frame=1000)
        with patch.object(m.time,'monotonic',clock.monotonic):
            with self.assertRaisesRegex(m.DriverError,'fresh playable third-person view'):
                driver.aligned((10,0))
        self.assertEqual(sent,[])

    def test_camera_turn_waits_until_dialogue_readiness_returns(self):
        driver,clock,feedback,sent=self.driver(logical=True)
        feedback.playable_after=.65
        with patch.object(m.time,'monotonic',clock.monotonic):
            driver.aligned((10*math.sin(.8),-10*math.cos(.8)))
        self.assertTrue(sent);self.assertGreaterEqual(sent[0][0],.65)
        self.assertLess(abs(m.angle_delta(feedback.yaw(),.8)),.045)

    def test_exact_enemy_root_replaces_stale_spawn_distance(self):
        driver,_,_,_=self.driver()
        row={'root':[-12078,12,5144],'enemy':0,'enemy_root':[-12076.2,12,5144]}
        self.assertAlmostEqual(driver.enemy_distance(row,0),1.8)
        row.pop('enemy_root');self.assertAlmostEqual(driver.enemy_distance(row,0),8.)

class MeleeFacingTests(unittest.TestCase):
    def driver(self):
        driver,clock,feedback,sent=feedback_driver(logical=True)
        driver.args.melee_free_look=True;events=[]
        state={'_stage':'escape','root':[0,0,0],'enemy':0,
               'enemy_root':[2*math.sin(.8),0,-2*math.cos(.8)],'health':61.,'enemy_health':90.,'wins':0}
        def observe():
            driver.observation_version+=1
            return dict(state,yaw=feedback.yaw(),frame=int(clock.now*60))
        driver.require_live=observe
        driver.button=lambda button,down:events.append(('button',button,down))
        driver.key=lambda key,down:events.append(('key',key,down))
        driver.log=lambda name,**data:events.append(('log',name,data))
        return driver,clock,feedback,sent,events,state

    def assert_released(self,events):
        self.assertEqual(events[0],('button',3,False))
        self.assertEqual([e for e in events if e[0]=='key'],[('key','q',True),('key','q',False)])
        self.assertFalse(any(e[0]=='button' and e[2] for e in events))

    def test_disabled_opt_in_sends_nothing_and_reads_no_game_state(self):
        driver,_,_,sent,events,_=self.driver();driver.args.melee_free_look=False
        driver.require_live=lambda:self.fail('Opt-out must not read or drive the encounter')
        driver.face_melee_enemy()
        self.assertEqual(events,[]);self.assertEqual(sent,[])

    def test_normal_q_mouse_uses_bounded_feedback_and_preserves_combat_state(self):
        driver,clock,feedback,sent,events,state=self.driver();before=json.dumps(state,sort_keys=True)
        with patch.object(m.time,'monotonic',clock.monotonic):driver.face_melee_enemy()
        self.assert_released(events)
        self.assertTrue(sent);self.assertLessEqual(len(sent),16)
        self.assertTrue(all(abs(pixels)<=160 for _,pixels in sent))
        self.assertLess(abs(m.angle_delta(feedback.yaw(),.8)),.07)
        self.assertEqual(json.dumps(state,sort_keys=True),before)
        self.assertTrue(any(e[:2]==('log','documented_melee_free_look_aligned') for e in events))

    def test_gone_or_paused_encounter_releases_q_without_motion(self):
        for gone in (False,True):
            with self.subTest(enemy_gone=gone):
                driver,clock,_,sent,events,state=self.driver()
                if gone:state['enemy']=-1
                else:driver.playing=lambda _:False
                with patch.object(m.time,'monotonic',clock.monotonic):driver.face_melee_enemy()
                self.assert_released(events);self.assertEqual(sent,[])

    def test_missing_actual_enemy_or_yaw_fails_closed_and_releases_q(self):
        for missing in ('enemy_root','yaw'):
            with self.subTest(missing=missing):
                driver,clock,_,sent,events,state=self.driver()
                if missing=='enemy_root':state.pop('enemy_root')
                else:driver.orientation=lambda _:None
                with patch.object(m.time,'monotonic',clock.monotonic):
                    with self.assertRaises(m.DriverError):driver.face_melee_enemy()
                self.assert_released(events);self.assertEqual(sent,[])

    def test_guarded_errors_release_q_and_preserve_original_failure(self):
        for fault in ('initial_wait','observation','motion','turn_feedback'):
            with self.subTest(fault=fault):
                driver,clock,_,_,events,_=self.driver()
                def fail(*args,**kwargs):raise m.DriverError('guard rejected '+fault)
                if fault=='initial_wait':driver.sleep=fail
                elif fault=='observation':driver.require_live=fail
                elif fault=='motion':driver.relative=fail
                else:driver.wait_orientation=fail
                with patch.object(m.time,'monotonic',clock.monotonic):
                    with self.assertRaisesRegex(m.DriverError,'guard rejected '+fault):driver.face_melee_enemy()
                self.assert_released(events)

    def test_unobserved_turn_never_fabricates_alignment_and_is_bounded(self):
        driver,clock,_,sent,events,state=self.driver();before=json.dumps(state,sort_keys=True)
        # A motion request is logged, but native feedback never acknowledges it.
        driver.relative=lambda pixels:sent.append((clock.now,pixels))
        driver.wait_orientation=lambda *args:None
        with patch.object(m.time,'monotonic',clock.monotonic):
            with self.assertRaisesRegex(m.DriverError,'did not align actual combat aim'):
                driver.face_melee_enemy()
        self.assert_released(events);self.assertEqual(len(sent),16)
        self.assertTrue(all(abs(pixels)<=160 for _,pixels in sent))
        self.assertEqual(json.dumps(state,sort_keys=True),before)

if __name__=='__main__':unittest.main()
