import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parents[1]/'tools'; sys.path.insert(0, str(TOOLS))
spec = importlib.util.spec_from_file_location('check_combat_arena', TOOLS/'check_combat_arena.py')
runner = importlib.util.module_from_spec(spec); spec.loader.exec_module(runner)


def world():
    return dict(arena_qa=dict(enabled=True, passed=True, checks=len(runner.CHECKS), events=1500,
        loss_hits=6, total_hits=40, total_parries=16, captures=len(runner.CAPTURES),
        initial_purse=0,final_purse=90,wallet_disk_verified=True,combo_pose_witnesses=7,
        concurrent_damage_sources=4,final_total=8,final_simultaneous=8,
        check_labels=sorted(runner.CHECKS), capture_names=sorted(runner.CAPTURES),
        combat_state_mutations=0, player_pose_writes=0, story_state_writes=0),
        gl_error=0, remaining_texture_references=0, asset_streaming=dict(failed=0),
        arena_camera=dict(player_follow_frames=12000,multiple_opponent_frames=120,locked_frames=24,lock_toggles=2,
            maximum_heading_error=0.,maximum_lateral_error=0.,maximum_pitch_error=0.),
        volumetric_blood=dict(enabled=True,provenance_loaded=True,variants=3,source_frames=250,
            source_vertices=5000,source_triangles=9900,geometry_bytes=12000000,texture_bytes=1000000,
            mesh_draws=900,stain_draws=4000,frame_transitions=800,failures=0,spawned=52,
            counter_rebases=8,resets=2,peak_meshes=4,peak_stains=16),
        combat_blood_intensity=dict(fluid_scale=.48,initial_particles=32,impacts=40,secondary_sprays=40,
            secondary_draws=200,finisher_events=9,finisher_draws=50,initial_finisher_bursts=9,max_strength=1.5),
        ground_blood_persistence=dict(enabled=True,capacity=128,witness_checked=True,witness_age=43.,
            witness_serial=14,stains_spawned=52,stains_replaced=0),
        combat_blood_motion=dict(playback_rate=3,maximum_mesh_seconds=.97,gravity=9.81,
            mist_lifetime=.1,settled_checked=True,settle_elapsed=1.05,settle_stains=5),
        character_polish=dict(enabled=True,pose_assets=344,reaction_assets=40,death_assets=24,
            keep_defeated=True,fallen_peak=8,death_starts=8,directional_reactions=10,
            fall_checked=True,grounded_checked=True,grounded_age=1.9),
        combat_feedback=dict(body_bursts=40,metal_bursts=16,body_audio=40,metal_audio=16,
                             cut_windup_seconds=.18,cut_cycle_seconds=.51,parry_window_seconds=.24))


def capture(name):
    state = dict(name=name, frame=500, state=2, weapon=0, third_person=True,
        target_locked=False,lock_target=8,lock_toggles=0,camera_heading_error=0.,camera_lateral_error=0.,camera_pitch_error=0.,
        drawing=False, draw=0., shots=1, arrow_hits=1, flying_arrows=0,
        waiting=7, entering=0, active=1, yielded=0, defeated=0, current=0, health=92.,
        body_bursts=1,metal_bursts=1,material=1,blood_particles=28,metal_particles=18,
        contact_kind=1,damage_cue=18.,body_audio=1,metal_audio=1,
        blood_loaded=True,blood_variants=3,blood_source_frames=250,blood_meshes=1,blood_stains=1,
        blood_frame=4,blood_frame_transitions=60,blood_mesh_draws=70,blood_stain_draws=100,
        blood_elapsed=4.,blood_paused=False,blood_failures=0,blood_scale=.48,blood_secondary_draws=50,
        blood_finisher_events=1,blood_finisher_draws=20,blood_max_strength=1.5,blood_live_finishers=0,
        blood_persistent_mode=True,blood_stain_capacity=128,blood_minimum_stain_fade=1.,
        blood_oldest_stain_age=3.,blood_persistent_stains_drawn=1,blood_playback_rate=3,
        blood_max_air_seconds=.97,blood_staged_meshes=1,blood_settle_elapsed=0.,
        motion_polished=True,motion_pose_assets=344,reaction_assets=40,death_assets=24,
        fallen_count=0,first_defeat_age=0.,first_defeat_phase=0.,first_defeat_anchored=False)
    state.update(total_opponents=8,simultaneous_opponents=1,unused=0,ui_phase=1,purse=80,
        initial_purse=0,run_id=2,saved_reward=0,reward_pending=False,attack_serial=0,
        combo_links=0,combo_count=0,stamina=100.,draft_total=8,draft_simultaneous=1,
        payout_preview=80,combo_pending=False,concurrent_damage_sources=0,player_pose=5)
    if name in {'arena-eight', 'arena-retry', 'arena-reset'}:
        state.update(state=1, shots=0, arrow_hits=0, waiting=7, entering=1, active=0, health=100.,blood_meshes=0,blood_stains=0)
    if name == 'arena-defeat': state.update(state=5, health=0.)
    if name in {'bow-first-draw', 'bow-third-draw'}:
        state.update(weapon=1, drawing=True, draw=.85, third_person=name == 'bow-third-draw')
    if name == 'arrow-flight': state.update(flying_arrows=1)
    if name == 'queue-progress': state.update(waiting=5, entering=1, active=0, yielded=2, defeated=2, current=2)
    if name == 'blood-paused': state.update(blood_paused=True)
    if name == 'blood-stain': state.update(waiting=6,entering=0,active=1,yielded=1,defeated=1,current=1)
    if name == 'blood-persistent':
        state.update(waiting=3,entering=0,active=1,yielded=4,defeated=4,current=4,
            blood_witness_serial=14,blood_witness_age=43.,blood_witness_anchored=True,
            blood_oldest_stain_age=43.,blood_persistent_stains_drawn=20,blood_stains=20)
    if name == 'blood-settled':
        state.update(state=3,waiting=7,entering=0,active=0,yielded=1,defeated=1,current=0,
            blood_particles=0,blood_meshes=0,blood_staged_meshes=0,blood_settle_elapsed=1.05)
    if name == 'gore-impact': state.update(blood_meshes=2)
    if name == 'finisher-blood': state.update(blood_meshes=2,blood_live_finishers=2)
    if name == 'arena-victory': state.update(state=4, waiting=0, active=0, yielded=8, defeated=8, current=7,saved_reward=80,ui_phase=2)
    if name == 'arena-setup': state.update(state=0,waiting=8,entering=0,active=0,purse=0,ui_phase=0,run_id=0)
    if name == 'arena-one-setup': state.update(state=1,waiting=0,entering=1,active=0,ui_phase=1,
        total_opponents=1,simultaneous_opponents=1,unused=7,draft_total=1,payout_preview=10,health=100.,shots=0,arrow_hits=0)
    if name in {'combo-first','combo-second','combo-third','combo-held'}:
        swing={'combo-first':1,'combo-second':2,'combo-third':3,'combo-held':3}[name]
        state.update(total_opponents=1,unused=7,waiting=0,active=1,attack_serial=swing,
            combo_links=swing-1,combo_count=swing,stamina=100.-16*swing,draft_total=1,payout_preview=10)
    if name == 'arena-one-victory': state.update(state=4,total_opponents=1,unused=7,waiting=0,active=0,
        yielded=1,defeated=1,saved_reward=10,purse=90,ui_phase=2,draft_total=1,payout_preview=10)
    if name == 'arena-many-setup': state.update(state=1,total_opponents=8,simultaneous_opponents=8,unused=0,waiting=0,entering=8,active=0,
        ui_phase=1,draft_simultaneous=8,purse=90,health=100.,shots=0,arrow_hits=0)
    if name in {'arena-many-active','arena-lock-on'}: state.update(simultaneous_opponents=8,waiting=0,active=8,draft_simultaneous=8,purse=90)
    if name=='arena-lock-on':state.update(target_locked=True,lock_target=0,current=0,lock_toggles=1)
    if name == 'arena-many-defeat': state.update(state=5,simultaneous_opponents=8,waiting=0,active=8,
        draft_simultaneous=8,purse=90,health=0.,concurrent_damage_sources=4,ui_phase=2)
    if name == 'defeat-fall':
        state.update(state=3,waiting=7,active=0,yielded=1,defeated=1,fallen_count=1,
            first_defeat_age=.95,first_defeat_phase=.53,first_defeat_anchored=True)
    if name == 'defeat-grounded':
        state.update(state=1,waiting=6,entering=1,active=0,yielded=1,defeated=1,current=1,
            fallen_count=1,first_defeat_age=1.9,first_defeat_phase=1.,first_defeat_anchored=True)
    if name in {'metal-block','metal-parry'}: state.update(material=2,contact_kind=4 if name=='metal-parry' else 3,damage_cue=0.)
    return state


class ArenaEvidence(unittest.TestCase):
    def test_configured_contract_retains_combat_and_adds_menu_combo_and_wallet_checks(self):
        self.assertEqual(len(runner.CHECKS),60)
        self.assertEqual(len(runner.CAPTURES),33)
    def test_actual_player_camera_and_explicit_lock_evidence_required(self):
        for key,value in [('locked_frames',0),('multiple_opponent_frames',0),('maximum_lateral_error',1.05),('maximum_heading_error',.5)]:
            state=world();state['arena_camera'][key]=value;self.assertTrue(runner.qa_failures(state),key)
        for name,key,value in [('arena-many-active','target_locked',True),('arena-lock-on','lock_target',8),
                ('arena-lock-on','target_locked',False),('arena-lock-on','current',1),('body-hit','camera_lateral_error',1.05)]:
            state=capture(name);state[key]=value;self.assertTrue(runner.capture_failures(name,state),(name,key))
    def test_configured_wallet_and_concurrency_cannot_pass_without_evidence(self):
        for key,value in [('wallet_disk_verified',False),('initial_purse',None),('final_purse',180),
                          ('combo_pose_witnesses',3),('concurrent_damage_sources',1),('final_total',7),('final_simultaneous',1)]:
            state=world();state['arena_qa'][key]=value
            self.assertTrue(runner.qa_failures(state),(key,value))
        state=world();state['arena_qa']['check_labels'].append(next(iter(runner.CHECKS)))
        self.assertTrue(runner.qa_failures(state))
    def test_combo_payout_and_actual_concurrent_captures_are_not_replaceable_with_counters(self):
        for name,key,value in [('arena-one-setup','draft_total',8),('arena-many-setup','draft_simultaneous',1),
            ('combo-second','attack_serial',1),('combo-third','stamina',80.),('combo-held','combo_pending',True),
            ('arena-one-victory','saved_reward',80),('arena-one-victory','purse',180),
            ('arena-many-active','active',1),('arena-many-defeat','concurrent_damage_sources',1)]:
            state=capture(name);state[key]=value
            self.assertTrue(runner.capture_failures(name,state),(name,key,value))
    def test_setup_commit_requires_exact_fresh_roster_and_receipt(self):
        for name, selected, gain in [('arena-one-setup',1,80),('arena-many-setup',8,90)]:
            for key,value in [('ui_phase',0),('ui_phase',2),('state',0),('state',2),
                ('draft_total',2),('draft_simultaneous',2),('payout_preview',999),
                ('total_opponents',2),('simultaneous_opponents',2),('active',1),
                ('entering',0),('waiting',1),('yielded',1),('defeated',1),
                ('health',99),('health',float('nan')),('stamina',99),('state',1.),('weapon',1),('shots',1),('arrow_hits',1),
                ('flying_arrows',1),('attack_serial',1),('combo_links',1),('combo_count',1),
                ('saved_reward',10),('run_id',0),('run_id',True),('reward_pending',True),
                ('drawing',True),('purse',gain+10),('initial_purse',None)]:
                state=capture(name);state[key]=value
                self.assertTrue(runner.capture_failures(name,state),(name,key,value))
            self.assertEqual(runner.capture_failures(name,capture(name)),[])
    def test_initial_setup_remains_a_real_idle_menu(self):
        for key,value in [('ui_phase',1),('state',1),('draft_total',1),('draft_simultaneous',8)]:
            state=capture('arena-setup');state[key]=value
            self.assertTrue(runner.capture_failures('arena-setup',state),(key,value))
    def test_full_observed_rehearsal_passes(self): self.assertEqual(runner.qa_failures(world()), [])
    def test_success_flag_without_evidence_fails(self): self.assertTrue(runner.qa_failures({'arena_qa': {'enabled': True, 'passed': True}}))
    def test_each_behavior_required(self):
        for label in runner.CHECKS:
            value=world(); value['arena_qa']['check_labels'].remove(label)
            self.assertTrue(runner.qa_failures(value), label)
    def test_state_bypasses_rejected(self):
        for key in ('combat_state_mutations', 'player_pose_writes', 'story_state_writes'):
            value=world(); value['arena_qa'][key]=1; self.assertTrue(runner.qa_failures(value))
    def test_all_actual_capture_states_pass(self):
        for name in runner.CAPTURES: self.assertEqual(runner.capture_failures(name,capture(name)), [], name)
    def test_actor_count_and_single_active_enforced(self):
        for name in runner.CAPTURES:
            value=capture(name); value['waiting']=8; value['active']=2
            self.assertTrue(runner.capture_failures(name,value),name)
    def test_bow_camera_and_draw_enforced(self):
        for name in ('bow-first-draw','bow-third-draw'):
            value=capture(name);value['third_person']=not value['third_person'];self.assertTrue(runner.capture_failures(name,value))
            value=capture(name);value['drawing']=False;self.assertTrue(runner.capture_failures(name,value))
    def test_projectile_and_eight_victory_evidence_enforced(self):
        for name,key,value in [('arrow-flight','flying_arrows',0),('arrow-hit','arrow_hits',0),('arena-victory','defeated',7),('arena-victory','health',0),('arena-defeat','health',1)]:
            state=capture(name);state[key]=value;self.assertTrue(runner.capture_failures(name,state))
    def test_material_particle_damage_and_sound_evidence_required(self):
        for name,key,value in [('body-hit','blood_particles',0),('arrow-hit','material',2),
                               ('body-hit','damage_cue',0),('body-hit','body_audio',0),
                               ('metal-block','metal_particles',0),('metal-parry','material',1),
                               ('metal-block','damage_cue',5),('metal-parry','metal_audio',0)]:
            state=capture(name);state[key]=value;self.assertTrue(runner.capture_failures(name,state),(name,key))
    def test_faster_timing_fair_parry_and_distinct_feedback_required(self):
        for key,value in [('cut_windup_seconds',.31),('cut_cycle_seconds',.83),('parry_window_seconds',.1),
                          ('body_bursts',0),('metal_bursts',0),('body_audio',0),('metal_audio',0)]:
            state=world();state['combat_feedback'][key]=value;self.assertTrue(runner.qa_failures(state),key)
    def test_purchased_source_animation_and_budgets_required(self):
        for key in world()['volumetric_blood']:
            state=world();state['volumetric_blood'].pop(key);self.assertTrue(runner.qa_failures(state),key)
        for key,value in [('peak_meshes',9),('peak_stains',129),('failures',1),('provenance_loaded',False)]:
            state=world();state['volumetric_blood'][key]=value;self.assertTrue(runner.qa_failures(state),key)
        state=world();state['volumetric_blood']['peak_stains']=128
        self.assertEqual(runner.qa_failures(state),[])
    def test_purchased_capture_animation_pause_stains_and_reset_required(self):
        for name,key,value in [('arrow-hit','blood_meshes',0),('blood-fluid','blood_frame',0),
                ('blood-paused','blood_paused',False),('blood-stain','blood_stains',0),
                ('arena-reset','blood_stains',1),('body-hit','blood_loaded',False)]:
            state=capture(name);state[key]=value;self.assertTrue(runner.capture_failures(name,state),(name,key))
    def test_heavy_sprays_and_real_finishers_required(self):
        for key,value in [('fluid_scale',.2),('initial_particles',12),('secondary_sprays',0),
                ('secondary_draws',0),('finisher_events',0),('finisher_draws',0),('max_strength',1)]:
            state=world();state['combat_blood_intensity'][key]=value;self.assertTrue(runner.qa_failures(state),key)
        for name,key,value in [('gore-impact','blood_meshes',1),('finisher-blood','blood_live_finishers',0),
                ('finisher-blood','blood_finisher_draws',0),('body-hit','blood_particles',12)]:
            state=capture(name);state[key]=value;self.assertTrue(runner.capture_failures(name,state),(name,key))
    def test_persistence_profile_requires_retained_original_witness(self):
        for key in world()['ground_blood_persistence']:
            state=world();state['ground_blood_persistence'].pop(key)
            self.assertTrue(runner.qa_failures(state),key)
        for key,value in [('enabled',False),('capacity',16),('capacity',129),('witness_checked',False),
                ('witness_age',29.99),('witness_age',float('nan')),('witness_age',float('inf')),
                ('witness_age',True),('witness_serial',0),('witness_serial',True),('stains_spawned',0),
                ('stains_replaced',1),('stains_replaced',False)]:
            state=world();state['ground_blood_persistence'][key]=value
            self.assertTrue(runner.qa_failures(state),(key,value))
    def test_every_capture_requires_actual_persistent_renderer_metadata(self):
        for name in runner.CAPTURES:
            for key in ('blood_persistent_mode','blood_stain_capacity','blood_minimum_stain_fade',
                    'blood_oldest_stain_age','blood_persistent_stains_drawn'):
                state=capture(name);state.pop(key)
                self.assertTrue(runner.capture_failures(name,state),(name,key))
        for key,value in [('blood_persistent_mode',False),('blood_stain_capacity',16),
                ('blood_minimum_stain_fade',float('nan')),('blood_minimum_stain_fade',1.01),
                ('blood_oldest_stain_age',float('inf')),('blood_oldest_stain_age',-1),
                ('blood_persistent_stains_drawn',129),('blood_persistent_stains_drawn',False)]:
            state=capture('body-hit');state[key]=value
            self.assertTrue(runner.capture_failures('body-hit',state),key)
    def test_persistence_capture_rejects_expired_moved_or_invisible_witness(self):
        for key in ('blood_witness_serial','blood_witness_age','blood_witness_anchored'):
            state=capture('blood-persistent');state.pop(key)
            self.assertTrue(runner.capture_failures('blood-persistent',state),key)
        for key,value in [('blood_witness_serial',0),('blood_witness_anchored',False),
                ('blood_witness_age',25.),('blood_witness_age',float('nan')),
                ('blood_oldest_stain_age',.5),('blood_minimum_stain_fade',0.),
                ('blood_minimum_stain_fade',.99),('blood_persistent_stains_drawn',0),('defeated',3)]:
            state=capture('blood-persistent');state[key]=value
            self.assertTrue(runner.capture_failures('blood-persistent',state),(key,value))
    def test_blood_motion_profile_requires_short_physical_airborne_effects(self):
        for key in world()['combat_blood_motion']:
            state=world();state['combat_blood_motion'].pop(key)
            self.assertTrue(runner.qa_failures(state),key)
        for key,value in [('playback_rate',1),('maximum_mesh_seconds',1.01),('maximum_mesh_seconds',0),
                ('maximum_mesh_seconds',float('nan')),('gravity',1.),('mist_lifetime',.25),
                ('mist_lifetime',float('inf')),('settled_checked',False),('settle_elapsed',.99),
                ('settle_elapsed',float('nan')),('settle_stains',0),('settle_stains',129)]:
            state=world();state['combat_blood_motion'][key]=value
            self.assertTrue(runner.qa_failures(state),(key,value))
    def test_every_capture_requires_motion_timing_and_actual_live_mesh_count(self):
        for name in runner.CAPTURES:
            for key in ('blood_playback_rate','blood_max_air_seconds','blood_staged_meshes','blood_settle_elapsed'):
                state=capture(name);state.pop(key)
                self.assertTrue(runner.capture_failures(name,state),(name,key))
        for key,value in [('blood_playback_rate',1),('blood_max_air_seconds',1.01),
                ('blood_max_air_seconds',float('nan')),('blood_staged_meshes',9),
                ('blood_staged_meshes',False),('blood_settle_elapsed',float('inf'))]:
            state=capture('blood-fluid');state[key]=value
            self.assertTrue(runner.capture_failures('blood-fluid',state),(key,value))
    def test_settled_capture_rejects_lingering_blood_or_missing_ground(self):
        for key,value in [('blood_particles',1),('blood_meshes',1),('blood_staged_meshes',1),
                ('blood_stains',0),('blood_persistent_stains_drawn',0),('blood_minimum_stain_fade',.9),
                ('blood_settle_elapsed',.99),('blood_settle_elapsed',float('nan')),('defeated',0),('defeated',2)]:
            state=capture('blood-settled');state[key]=value
            self.assertTrue(runner.capture_failures('blood-settled',state),(key,value))
    def test_character_profile_requires_real_reactions_collapse_and_eight_retained_fighters(self):
        for key in world()['character_polish']:
            state=world();state['character_polish'].pop(key)
            self.assertTrue(runner.qa_failures(state),key)
        for key,value in [('enabled',False),('pose_assets',216),('pose_assets',True),
                ('reaction_assets',0),('death_assets',0),('keep_defeated',False),
                ('fallen_peak',7),('fallen_peak',9),('death_starts',7),('death_starts',True),
                ('directional_reactions',7),('directional_reactions',True),
                ('fall_checked',False),('grounded_checked',False),('grounded_age',1.79),
                ('grounded_age',float('nan')),('grounded_age',float('inf')),('grounded_age',True)]:
            state=world();state['character_polish'][key]=value
            self.assertTrue(runner.qa_failures(state),(key,value))
    def test_every_capture_requires_rendered_character_motion_metadata(self):
        keys=('motion_polished','motion_pose_assets','reaction_assets','death_assets',
              'fallen_count','first_defeat_age','first_defeat_phase','first_defeat_anchored')
        for name in runner.CAPTURES:
            for key in keys:
                state=capture(name);state.pop(key)
                self.assertTrue(runner.capture_failures(name,state),(name,key))
        for key,value in [('motion_polished',False),('motion_pose_assets',216),
                ('motion_pose_assets',True),('reaction_assets',39),('death_assets',23),
                ('fallen_count',-1),('fallen_count',9),('fallen_count',False),
                ('first_defeat_age',-1),('first_defeat_age',float('nan')),
                ('first_defeat_age',float('inf')),('first_defeat_age',True),
                ('first_defeat_phase',-1),('first_defeat_phase',1.01),
                ('first_defeat_phase',float('nan')),('first_defeat_phase',float('inf')),
                ('first_defeat_phase',True),('first_defeat_anchored',0)]:
            state=capture('body-hit');state[key]=value
            self.assertTrue(runner.capture_failures('body-hit',state),(key,value))
    def test_collapse_capture_rejects_endpoint_poses_wrong_time_or_moving_root(self):
        for key,value in [('fallen_count',0),('first_defeat_age',.74),('first_defeat_age',1.31),
                ('first_defeat_phase',0),('first_defeat_phase',1),('first_defeat_anchored',False)]:
            state=capture('defeat-fall');state[key]=value
            self.assertTrue(runner.capture_failures('defeat-fall',state),(key,value))
        for age in (.75,1.3):
            state=capture('defeat-fall');state['first_defeat_age']=age
            self.assertEqual(runner.capture_failures('defeat-fall',state),[])
    def test_grounded_capture_requires_completed_anchored_collapse_during_next_duel(self):
        for key,value in [('fallen_count',0),('first_defeat_age',1.79),
                ('first_defeat_phase',.99),('first_defeat_anchored',False),
                ('current',0),('current',True),('current',8)]:
            state=capture('defeat-grounded');state[key]=value
            self.assertTrue(runner.capture_failures('defeat-grounded',state),(key,value))
        state=capture('defeat-grounded');state['first_defeat_age']=1.8
        self.assertEqual(runner.capture_failures('defeat-grounded',state),[])
    def test_private_rehearsal_preserves_required_character_marker(self):
        with tempfile.TemporaryDirectory() as directory:
            base=Path(directory);source=base/'source';source.mkdir();bank=source/'arena_assets';bank.mkdir()
            for name in ('arena.config','playtest.route','CHARACTER_POLISH.json'):
                (source/name).write_text('{"private":true}\n')
            for kind in ('body','arms'):
                for pose in range(3): (bank/f'bow_{kind}_{pose:03d}.glb').write_bytes(b'ownedbow')
            blood=source/'blood_assets';blood.mkdir();(blood/'blood.bank').write_bytes(b'ownedblood')
            record=dict(bank=str(blood),files_sha256={'blood.bank':runner.sha(blood/'blood.bank')})
            from tests.character_polish_assets_test import fixture as character_fixture
            character_fixture(source/'models'/'longsword_mocap')
            def empty_isolation(original,target):
                target.mkdir();(target/'models').symlink_to(source/'models',target_is_directory=True);return {}
            with patch.object(runner,'isolate_project',side_effect=empty_isolation), \
                    patch.object(runner,'blood_asset_record',return_value=record):
                target=base/'private';inputs=runner.isolate(source,target)
                marker=target/'CHARACTER_POLISH.json'
                self.assertFalse(marker.is_symlink())
                self.assertEqual(marker.read_bytes(),(source/marker.name).read_bytes())
                self.assertEqual(inputs[marker.name],runner.sha(source/marker.name))
                self.assertIn('models/longsword_mocap/combat_body_171.glb',inputs)
                self.assertIn('models/longsword_mocap/combat_weapon_sockets.bin',inputs)
                (source/marker.name).unlink()
                with self.assertRaisesRegex(ValueError,'CHARACTER_POLISH.json'):
                    runner.isolate(source,base/'missing-marker')
                (source/marker.name).symlink_to(marker)
                with self.assertRaisesRegex(ValueError,'CHARACTER_POLISH.json'):
                    runner.isolate(source,base/'linked-marker')


if __name__ == '__main__': unittest.main()
