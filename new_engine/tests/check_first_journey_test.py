import importlib.util
import json
from pathlib import Path
import re
import shlex
import struct
import sys
import tempfile
import unittest

TOOLS=Path(__file__).resolve().parents[1]/'tools';sys.path.insert(0,str(TOOLS))
spec=importlib.util.spec_from_file_location('check_first_journey',TOOLS/'check_first_journey.py')
runner=importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)
CINEMATIC=dict(shot_count=2,duration=10)


def world():
    return dict(first_journey_qa=dict(enabled=True,passed=True,phase=28,checks=sorted(runner.CHECKS),
        events=5000,movement_frames=4000,walked_m=350.,loss_enemy_hits=7,win_hits=5,win_parries=2,
        win_blocks=1,retries=1,restores=1,captures=12,capture_names=sorted(runner.CAPTURES),
        qa_player_pose_writes=0,qa_story_state_writes=0,qa_health_writes=0,unexpected_walk_discontinuities=0,
        loan_grants=1,reward_grants=1,save_verified=True,restore_verified=True),
        first_journey=dict(enabled=True,stage='complete',camp_mask=7,loaned=True,supplies=True,rewarded=True,complete=True),
        opening_prologue_qa=dict(checks=sorted(runner.INHERITED_CHECKS),qa_player_pose_writes=0,
            qa_story_state_writes=0,capture_failures=0),
        opening_prologue=dict(arrivals=1),opening_game_start=dict(new_games=1),
        opening_entry_qa=dict(enabled=True,shot_count=2,shot_mask=3,matched_handoff=True,handoff_max_error=0.),
        gl_error=0,remaining_texture_references=0,asset_streaming=dict(failed=0))


def capture(name):
    state=dict(name=name,frame=1000,journey_stage=runner.CAPTURE_STAGES[name],camp_mask=7,
        grounded=True,loaned=name!='journey-loan',supplies=name in {'journey-satchel','journey-reward','journey-town','journey-restored'},
        rewarded=name in {'journey-reward','journey-town','journey-restored'},complete=name in {'journey-town','journey-restored'},
        reward_grants=1 if name in {'journey-reward','journey-town','journey-restored'} else 0,
        combat_state=4 if name=='journey-victory' else 2,combat_health=90.,enemy_health=0. if name=='journey-victory' else 70.,
        hits=5,enemy_hits=0,parries=1,blocks=0)
    if name=='journey-defeat':state.update(combat_state=5,combat_health=0.,enemy_hits=7)
    if name=='journey-retry':state.update(combat_state=0,combat_health=100.)
    return state


class JourneyEvidence(unittest.TestCase):
    def test_native_replay_matches_runner_check_and_capture_contract(self):
        source=(TOOLS.parent/'engine/world_first_journey_qa.inc').read_text()
        self.assertEqual(set(re.findall(r'first_journey_qa_pass\("([^"\n]+)"\)',source)),runner.CHECKS)
        self.assertEqual(set(re.findall(r'first_journey_qa_capture_request\("([^"\n]+)"\)',source)),runner.CAPTURES)
    def test_native_replay_uses_inputs_instead_of_mutating_gameplay(self):
        source=(TOOLS.parent/'engine/world_first_journey_qa.inc').read_text()
        self.assertFalse(re.search(r'cam\.(?:x|y|z|yaw|pitch)\s*=(?!=)',source))
        self.assertFalse(re.search(r'fight\.(?:state|health|stamina|enemy_health|player_phase|enemy_phase)\s*=(?!=)',source))
        self.assertFalse(re.search(r'first_journey_state\.(?:tick|restore|reset|configure)\s*\(',source))
        self.assertFalse(re.search(r'fight\.(?:attack|choose|set_guard|start|start_threat|start_arena)\s*\(',source))
    def test_exact_contract_has_twenty_two_checks_and_twelve_captures(self):
        self.assertEqual((len(runner.CHECKS),len(runner.CAPTURES)),(22,12))
        self.assertEqual(set(runner.CAPTURE_STAGES),runner.CAPTURES)
    def test_observed_end_to_end_evidence_passes(self):self.assertEqual(runner.qa_failures(world(),CINEMATIC),[])
    def test_success_flags_without_evidence_fail(self):self.assertTrue(runner.qa_failures({'first_journey_qa':{'passed':True}},CINEMATIC))
    def test_each_journey_checkpoint_and_check_is_required(self):
        for label in runner.CHECKS:
            data=world();data['first_journey_qa']['checks'].remove(label);self.assertTrue(runner.qa_failures(data,CINEMATIC),label)
        for name in runner.CAPTURES:
            data=world();data['first_journey_qa']['capture_names'].remove(name);self.assertTrue(runner.qa_failures(data,CINEMATIC),name)
    def test_duplicates_and_malformed_lists_rejected(self):
        for key,value in [('checks',[{}]),('capture_names',[{}]),('checks',sorted(runner.CHECKS)+['fresh_loan_interaction'])]:
            data=world();data['first_journey_qa'][key]=value;self.assertTrue(runner.qa_failures(data,CINEMATIC))
    def test_pose_health_and_story_bypasses_are_rejected(self):
        for key in ('qa_player_pose_writes','qa_story_state_writes','qa_health_writes','unexpected_walk_discontinuities'):
            data=world();data['first_journey_qa'][key]=1;self.assertTrue(runner.qa_failures(data,CINEMATIC),key)
    def test_real_fighting_movement_retry_and_restore_evidence_required(self):
        for key,value in [('loss_enemy_hits',0),('win_hits',0),('walked_m',0),('movement_frames',0),('retries',0),('restores',0),('save_verified',False),('restore_verified',False)]:
            data=world();data['first_journey_qa'][key]=value;self.assertTrue(runner.qa_failures(data,CINEMATIC),key)
        data=world();data['first_journey_qa'].update(win_parries=0,win_blocks=0);self.assertTrue(runner.qa_failures(data,CINEMATIC))
    def test_single_loan_reward_and_final_progress_required(self):
        for key in ('loan_grants','reward_grants'):
            for value in (0,2):
                data=world();data['first_journey_qa'][key]=value;self.assertTrue(runner.qa_failures(data,CINEMATIC))
        for key in ('loaned','supplies','rewarded','complete'):
            data=world();data['first_journey'][key]=False;self.assertTrue(runner.qa_failures(data,CINEMATIC))
    def test_inherited_apartment_controls_camp_and_cinematic_required(self):
        for key,value in [('checks',[]),('qa_player_pose_writes',1),('capture_failures',1)]:
            data=world();data['opening_prologue_qa'][key]=value;self.assertTrue(runner.qa_failures(data,CINEMATIC))
        data=world();data['opening_entry_qa']['matched_handoff']=False;self.assertTrue(runner.qa_failures(data,CINEMATIC))
    def test_optional_room_lights_need_all_existing_checks(self):
        data=world();self.assertTrue(runner.qa_failures(data,CINEMATIC,True));data['opening_prologue_qa']['checks']+=sorted(runner.ROOM_LIGHT_CHECKS)
        self.assertEqual(runner.qa_failures(data,CINEMATIC,True),[])
    def test_each_actual_capture_state_passes(self):
        for name in runner.CAPTURES:self.assertEqual(runner.capture_failures(name,capture(name)),[],name)
    def test_capture_does_not_accept_early_rewards_or_future_stage(self):
        for name in runner.CAPTURES-{'journey-reward','journey-town','journey-restored'}:
            state=capture(name);state['rewarded']=True;self.assertTrue(runner.capture_failures(name,state))
        state=capture('journey-road');state['journey_stage']='complete';self.assertTrue(runner.capture_failures('journey-road',state))
    def test_capture_requires_real_damage_guard_and_actual_victory(self):
        for name,key,value in [('journey-defeat','combat_health',1),('journey-defeat','enemy_hits',0),
                ('journey-block','parries',None),('journey-hit','hits',0),('journey-victory','enemy_health',1),('journey-retry','combat_health',0)]:
            state=capture(name);state[key]=value;self.assertTrue(runner.capture_failures(name,state),(name,key))


class SealedPaths(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.project=Path(self.tmp.name);(self.project/'opening').mkdir()
        self.path=self.project/'opening/first-journey.routes.json'
        self.data=dict(schema='valwood.first-journey-routes',version=1,paths={name:[[0,0,0],[1,0,1]] for name in runner.PATH_NAMES})
        self.write()
    def tearDown(self):self.tmp.cleanup()
    def write(self):self.path.write_text(json.dumps(self.data))
    def test_seals_all_five_authored_paths_with_stable_hash(self):
        digest=runner.write_paths(self.project);out=self.project/'opening/first-journey.qa.walk'
        self.assertEqual(digest,runner.sha(out));self.assertTrue(out.read_text().startswith('VALWOOD_FIRST_JOURNEY_QA 1\nloan 2\n'))
        self.assertEqual(runner.write_paths(self.project),digest)
    def test_rejects_missing_path_nonfinite_and_teleport_segments(self):
        self.data['paths'].pop('town');self.write()
        with self.assertRaises(ValueError):runner.authored_paths(self.project)
        self.data['paths']['town']=[[0,0,float('nan')]];self.write()
        with self.assertRaises(ValueError):runner.authored_paths(self.project)
        self.data['paths']['town']=[[0,0,0],[26,0,0]];self.write()
        with self.assertRaises(ValueError):runner.authored_paths(self.project)
    def test_rejects_linked_route_and_linked_sealed_output(self):
        saved=self.project/'actual.json';self.path.rename(saved);self.path.symlink_to(saved)
        with self.assertRaises(ValueError):runner.authored_paths(self.project)
        self.path.unlink();saved.rename(self.path);(self.project/'opening/first-journey.qa.walk').symlink_to(self.path)
        with self.assertRaises(ValueError):runner.write_paths(self.project)


class AtomicCheckpoint(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.project=Path(self.tmp.name);(self.project/'opening').mkdir();(self.project/'saves').mkdir()
        self.settings=self.project/'opening/first-journey.settings';self.path=self.project/'saves/opening.progress'
        values=[];rows=['VALWOOD_FIRST_JOURNEY 1']
        for i,name in enumerate(('camp','rowan','bram','encounter','supplies','retry','town')):
            point=[float(i),0.,float(i)];values+=point;rows.append(name+' '+' '.join(str(v) for v in point))
        values += [2.7,4.,2.1,3.2];rows+=['radii 2.7 4 2.1 3.2','end'];self.settings.write_text('\n'.join(rows)+'\n')
        self.layout=runner.fnv64(b''.join(struct.pack('<d',v) for v in values));self.write()
    def tearDown(self):self.tmp.cleanup()
    def write(self,stage=7,reward='12 1 1',wrong_hash=False):
        body=f'layout {self.layout}\nstage {stage}\ncamp 7\nprogress 1 1 1 1 1\nreward {reward}\nend\n'
        nested=f'VALWOOD_FIRST_JOURNEY_PROGRESS 1 {runner.fnv64(body.encode())+(1 if wrong_hash else 0)}\n'+body
        self.path.write_text('VALWOOD_OPENING 1\ncheckpoint 3\nplayer 0 0 0 0 0\nroom_lights 15\ncamp_spoken 7\nfirst_journey "'+nested+'"\n')
    def test_reads_single_atomic_embedded_complete_save_and_ledger(self):
        result=runner.checkpoint_evidence(self.project);self.assertTrue(result['atomic_embedded_checkpoint']);self.assertEqual((result['coins'],result['food'],result['reputation']),(12,1,1));self.assertEqual(result['layout_signature'],self.layout)
    def test_checksum_layout_reward_and_complete_stage_are_required(self):
        for options in (dict(stage=2),dict(reward='24 2 2'),dict(wrong_hash=True)):
            self.write(**options)
            with self.assertRaises(ValueError):runner.checkpoint_evidence(self.project)
        self.write();self.settings.write_text(self.settings.read_text().replace('radii 2.7','radii 2.8'))
        with self.assertRaises(ValueError):runner.checkpoint_evidence(self.project)
    def test_rejects_incomplete_atomic_write_and_symlink(self):
        (self.project/'saves/opening.progress.tmp-incomplete').write_text('incomplete')
        with self.assertRaises(ValueError):runner.checkpoint_evidence(self.project)
        (self.project/'saves/opening.progress.tmp-incomplete').unlink();saved=self.project/'actual.save';self.path.rename(saved);self.path.symlink_to(saved)
        with self.assertRaises(ValueError):runner.checkpoint_evidence(self.project)


if __name__=='__main__':unittest.main()
