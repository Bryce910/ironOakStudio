#!/usr/bin/env python3
"""CPU-only evidence-validator tests in a temporary synthetic fixture.

These fixtures never create actual native acceptance or modify a game project.
"""
from pathlib import Path
import json
import shlex
import tempfile
import unittest
import report_battlefield_acceptance as m


class AcceptanceChecks(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
        self.n=self.root/'candidate';self.review=self.n/'review';self.session=self.review/'private-session'
        self.project=self.session/'project';self.project.mkdir(parents=True)
        self.write(self.project/'PRIVATE_CAPTURE_PROJECT.json',{'private':True})
        self.write(self.session/'state-preservation.json',{'exit_code':0,'live_state_unchanged':True})
        self.pid=99999999;self.put(self.session/'pid',str(self.pid))
        self.binary=self.n/'runtime/rpg';self.put(self.binary,'synthetic-unit-test-binary')
        self.source=self.n/'source-v1/engine/frozen.h';self.put(self.source,'synthetic-unit-test-source')
        self.build=self.n/'runtime-native-v1/BUILD_RECEIPT.json'
        self.write(self.build,{'binary_sha256':m.digest(self.binary),'source_sha256':{'engine/frozen.h':m.digest(self.source)}})
        self.live=self.root/'published';self.put(self.live/'world.studio','published-world');self.put(self.live/'graphics.settings','published-settings')
        self.launch=self.session/'launch.json'
        self.write(self.launch,{'binary_sha256':m.digest(self.binary),'command':[str(self.binary),'--project',str(self.project),'--play'],
            'resume':None,'protected':{str(p):m.digest(p) for p in self.live.iterdir()}})
        files=[]
        for relative in sorted(m.SCENE):
            path=self.n/'world-project'/relative;self.put(path,'synthetic-scene:'+relative)
            self.put(self.project/relative,path.read_text())
            files.append({'relative_path':relative,'candidate':{'sha256':m.digest(path)}})
        self.plan=self.review/'PUBLISH_PLAN.json';self.write(self.plan,{'files':files})
        counts=[0]*16;counts[1]=counts[11]=counts[15]=1
        self.inv={'counts':counts,'weapon':1,'receipts':29}
        self.durable=m.questless_inventory(self.inv)
        inv='VALWOOD_INVENTORY 1\nitems '+' '.join(map(str,counts))+'\nequipment 1 0 0 0 0\nreceipts 29\nspent 0\nend\n'
        quest_body='layout 123\nstage 7\ncamp 7\nprogress 1 1 1 1 1\nreward 12 1 1\nend\n'
        quest='VALWOOD_FIRST_JOURNEY_PROGRESS 1 '+str(m.fnv(quest_body))+'\n'+quest_body
        bf='VALWOOD_BATTLEFIELD 1\ncheckpoint 3\ndifficulty 1\nsword 1\nwins 3\n'
        quoted=lambda value:'"'+value.replace('\\','\\\\').replace('"','\\"')+'"'
        self.opening=self.project/'saves/opening.progress'
        self.put(self.opening,'VALWOOD_OPENING 1\ncheckpoint 2\ncamp_spoken 7\nfirst_journey '+quoted(quest)+
                 '\ninventory '+quoted(inv)+'\nbattlefield_progress '+quoted(bf)+'\n')
        self.profile=self.project/'saves/player.progress'
        durable_inv=inv.replace('items '+' '.join(map(str,counts)),'items '+' '.join(map(str,self.durable['counts'])))
        body='VALWOOD_PLAYER_PROFILE 1\nrevision 5\nimports 1 0\nstory 12\ninventory '+quoted(durable_inv)+'\n'
        self.put(self.profile,body+'checksum '+format(m.fnv(body,14695981039346656037),'016x')+'\n')
        journey={'stage':7,'camp':7,'progress':[1]*5,'reward':[12,1,1],'checksum_verified':True,'inventory':self.inv}
        base={'passed':True,'error':None,'private_project':str(self.project),'pid':self.pid,'normal_physical_inputs_only':True,
              'game_state_injected':False,'saved_progress_written_by_driver':False,'screenshots':[]}
        self.results={key:self.session/(key+'-result.json') for key in ('apartment','qte','escape','camp')}
        self.write(self.results['apartment'],dict(base,review='fresh_apartment_to_battlefield_normal_inputs',apartment_started=True,
            bills_read=True,console_interacted=True,new_game_confirmed=True,battlefield_arrival_observed=True,cutscenes_skipped=False,
            receipts={'opening_checkpoint':2,'battlefield_checkpoint':0,'sword':0,'wins':0}))
        qinv={'sword_count':1,'weapon':1,'receipts':16}
        self.write(self.results['qte'],dict(base,intentional_missed_duck_death=True,local_retry=True,fresh_duck_success=True,
            tap_struggle_success=True,receipts={'opening':qinv,'durable_profile':qinv,'battlefield':{'checkpoint':1,'sword':1}}))
        self.write(self.results['escape'],dict(base,review='battlefield_escape_normal_inputs',
            duels=[{'soldier':i,'wins':i,'attempts':5,'last_attack_serial':5,'first_attack_serial':0} for i in (1,2,3)],
            receipts={'battlefield_checkpoint':3,'real_wins':3,'sword':1,'inventory':qinv},
            bridge_samples=[{'root':[-12027,12.1,5160]},{'root':[-12020,12.1,5163]},{'root':[-12007,12.1,5168]}]))
        image=self.session/'thanks-for-playing-real-ending.png';self.put(image,'synthetic-screenshot-fixture')
        self.write(self.results['camp'],dict(base,review='camp_first_quest_normal_inputs',raider_real_victory_reviewed=True,
            thanks_for_playing_observed=True,raider_attacks=9,last_observer={'screen':5},screenshots=[str(image)],
            receipts={'opening':journey,'durable_profile':{'story_coins':12,'inventory':self.durable}}))
        stages=['inactive','dazed','recover','flee_to_ambush','duck','dead','duck','tackle','struggle','stab','stand','escape','escape','escape','forest','campsite']
        self.trace=self.session/'battlefield.jsonl';rows=[]
        for index,stage in enumerate(stages):
            rows.append({'frame':index,'stage':stage,'wins':0 if index<12 else index-11 if index<14 else 3,
                         'sword':index>=9,'screen':5 if stage=='campsite' else 1})
        self.put(self.trace,''.join(json.dumps(row)+'\n' for row in rows))
        settings=self.session/'benchmark/graphics.settings';self.put(settings,'synthetic-measured-settings')
        self.bench=self.session/'benchmark/benchmark.json'
        self.write(self.bench,{'passed':True,'error':None,'collector_read_only':True,'sent_inputs':False,'launched_game':False,
            'private_project':str(self.project),'process':{'pid':self.pid,'command':[str(self.binary),'--project',str(self.project),'--play']},'requested_seconds':30.,'actual_seconds':30.,
            'frames':630,'timing':{'observed_coverage_seconds':29.9,'mean_fps':21.,'p95_ms':55.},
            'graphics_settings_sha256':m.digest(settings),'graphics_settings_end_sha256':m.digest(settings)})
    def tearDown(self):
        self.temp.cleanup()
    def put(self,path,value):
        path.parent.mkdir(parents=True,exist_ok=True);path.write_text(value)
    def write(self,path,value):
        self.put(path,json.dumps(value))
    def change(self,path,**values):
        data=json.loads(path.read_text());data.update(values);self.write(path,data)
    def validate(self):
        return m.validate(self.n,self.session,self.results,[self.bench])
    def test_valid_functional_fixture_never_calls_21fps_a_target_or_AAA_pass(self):
        result=self.validate();self.assertTrue(result['passed'])
        self.assertEqual(result['performance']['status'],'target_unmet')
        self.assertFalse(result['performance']['fps_target_passed'])
        self.assertEqual(result['performance']['quality_certification'],'not_assessed')
        self.assertFalse((self.review/'NATIVE_ACCEPTANCE.json').exists())
    def set_durable_inventory(self,value):
        payload='VALWOOD_INVENTORY 1\nitems '+' '.join(map(str,value['counts']))+'\nequipment '+str(value['weapon'])+' 0 0 0 0\nreceipts '+str(value['receipts'])+'\nspent 0\nend\n'
        quoted=lambda text:'"'+text.replace('\\','\\\\').replace('"','\\"')+'"'
        body='VALWOOD_PLAYER_PROFILE 1\nrevision 5\nimports 1 0\nstory 12\ninventory '+quoted(payload)+'\n'
        self.put(self.profile,body+'checksum '+format(m.fnv(body,14695981039346656037),'016x')+'\n')
    def test_quest_letter_is_local_and_all_nonquest_items_are_durable(self):
        saved,_=m.final_saves(self.project)
        self.assertEqual(saved['inventory']['counts'][15],1)
        self.assertEqual(saved['durable_inventory']['counts'][14:16],[0,0])
        self.assertEqual(saved['inventory']['counts'][:14],saved['durable_inventory']['counts'][:14])
    def test_durable_checksum_uses_production_basis_only(self):
        body,_,_=self.profile.read_text().rpartition('checksum ')
        self.put(self.profile,body+'checksum '+format(m.fnv(body),'016x')+'\n')
        with self.assertRaisesRegex(m.Refusal,'durable player profile checksum'):self.validate()
    def test_durable_quest_tokens_refused_with_valid_checksum(self):
        for item in (14,15):
            with self.subTest(item=item):
                value=dict(self.durable,counts=self.durable['counts'][:]);value['counts'][item]=1
                self.set_durable_inventory(value)
                with self.assertRaisesRegex(m.Refusal,'non-quest inventory'):self.validate()
    def test_durable_nonquest_item_loss_or_duplication_refused(self):
        for item,count in ((1,0),(1,2),(11,0),(11,2),(7,1)):
            with self.subTest(item=item,count=count):
                value=dict(self.durable,counts=self.durable['counts'][:]);value['counts'][item]=count
                self.set_durable_inventory(value)
                with self.assertRaisesRegex(m.Refusal,'non-quest inventory'):self.validate()
    def test_durable_weapon_or_receipt_differences_refused(self):
        for value in (dict(self.durable,weapon=0),dict(self.durable,receipts=13)):
            self.set_durable_inventory(value)
            with self.assertRaisesRegex(m.Refusal,'non-quest inventory'):self.validate()
    def test_letter_cannot_be_removed_from_opening_checkpoint(self):
        text=self.opening.read_text();counts=self.inv['counts'][:];counts[15]=0
        text=text.replace('items '+' '.join(map(str,self.inv['counts'])),'items '+' '.join(map(str,counts)))
        self.put(self.opening,text)
        with self.assertRaisesRegex(m.Refusal,'sword/bread/satchel/letter'):self.validate()
    def test_bread_cannot_be_removed_from_both_ledgers(self):
        text=self.opening.read_text();counts=self.inv['counts'][:];counts[11]=0
        self.put(self.opening,text.replace('items '+' '.join(map(str,self.inv['counts'])),'items '+' '.join(map(str,counts))))
        value=dict(self.durable,counts=self.durable['counts'][:]);value['counts'][11]=0;self.set_durable_inventory(value)
        with self.assertRaisesRegex(m.Refusal,'sword/bread/satchel/letter'):self.validate()
    def test_ending_receipt_must_use_actual_questless_durable_inventory(self):
        value=json.loads(self.results['camp'].read_text());value['receipts']['durable_profile']['inventory']=self.inv
        self.write(self.results['camp'],value)
        with self.assertRaisesRegex(m.Refusal,'ending result'):self.validate()
    def test_current_binary_change_refused(self):
        self.put(self.binary,'new-binary');self.assertRaises(m.Refusal,self.validate)
    def test_old_launch_binary_refused(self):
        self.change(self.launch,binary_sha256='0'*64);self.assertRaises(m.Refusal,self.validate)
    def test_stale_build_refused(self):
        self.change(self.build,binary_sha256='0'*64);self.assertRaises(m.Refusal,self.validate)
    def test_uncompiled_source_change_refused(self):
        self.put(self.source,'uncompiled-change');self.assertRaises(m.Refusal,self.validate)
    def test_unrecorded_added_source_refused(self):
        self.put(self.source.parent/'new.h','new-source');self.assertRaises(m.Refusal,self.validate)
    def test_scene_plan_change_refused(self):
        self.put(self.n/'world-project/world.studio','new-scene');self.assertRaises(m.Refusal,self.validate)
    def test_reviewed_scene_change_refused(self):
        self.put(self.project/'world.studio','other-scene');self.assertRaises(m.Refusal,self.validate)
    def test_missing_twenty_third_scene_refused(self):
        data=json.loads(self.plan.read_text());data['files']=[r for r in data['files'] if r['relative_path']!='models/battlefield/discarded_arms_mid.glb'];self.write(self.plan,data);self.assertRaises(m.Refusal,self.validate)
    def test_unreviewed_mid_coverage_refused(self):
        for relative in ('models/battlefield/barricade_mid.glb','models/battlefield/discarded_arms_mid.glb'):
            with self.subTest(relative=relative):
                path=self.project/relative;previous=path.read_bytes()
                path.write_bytes(b'unreviewed-coverage');self.assertRaises(m.Refusal,self.validate)
                path.write_bytes(previous)
    def test_uncompiled_candidate_mid_coverage_refused(self):
        for relative in ('models/battlefield/barricade_mid.glb','models/battlefield/discarded_arms_mid.glb'):
            with self.subTest(relative=relative):
                path=self.n/'world-project'/relative;previous=path.read_bytes()
                path.write_bytes(b'post-review-coverage');self.assertRaises(m.Refusal,self.validate)
                path.write_bytes(previous)
    def test_unreviewed_road_lanes_refused(self):
        self.put(self.project/'opening/road-traffic.settings','unreviewed-road-lanes');self.assertRaises(m.Refusal,self.validate)
    def test_no_clean_close_refused(self):
        (self.session/'state-preservation.json').unlink();self.assertRaises(m.Refusal,self.validate)
    def test_unclean_close_refused(self):
        self.change(self.session/'state-preservation.json',exit_code=1);self.assertRaises(m.Refusal,self.validate)
    def test_live_state_changed_refused(self):
        self.put(self.live/'graphics.settings','changed-live-settings');self.assertRaises(m.Refusal,self.validate)
    def test_nonprivate_project_refused(self):
        self.change(self.project/'PRIVATE_CAPTURE_PROJECT.json',private=False);self.assertRaises(m.Refusal,self.validate)
    def test_resumed_instead_of_fresh_apartment_refused(self):
        self.change(self.launch,resume={'source':'old-save'});self.assertRaises(m.Refusal,self.validate)
    def test_failed_escape_is_not_accepted_despite_three_wins(self):
        self.change(self.results['escape'],passed=False,error='stuck at campsite');self.assertRaises(m.Refusal,self.validate)
    def test_mixed_pid_refused(self):
        self.change(self.results['qte'],pid=123);self.assertRaises(m.Refusal,self.validate)
    def test_mixed_project_refused(self):
        self.change(self.results['camp'],private_project=str(self.root/'different-project'));self.assertRaises(m.Refusal,self.validate)
    def test_injected_or_written_state_refused(self):
        self.change(self.results['escape'],game_state_injected=True);self.assertRaises(m.Refusal,self.validate)
    def test_missing_actual_duel_refused(self):
        data=json.loads(self.results['escape'].read_text());data['duels'].pop();self.write(self.results['escape'],data);self.assertRaises(m.Refusal,self.validate)
    def test_no_actual_attack_serial_refused(self):
        data=json.loads(self.results['escape'].read_text());data['duels'][1]['last_attack_serial']=0;self.write(self.results['escape'],data);self.assertRaises(m.Refusal,self.validate)
    def test_duplicate_sword_grant_refused(self):
        data=json.loads(self.results['qte'].read_text());data['receipts']['opening']['sword_count']=2;self.write(self.results['qte'],data);self.assertRaises(m.Refusal,self.validate)
    def test_missing_thanks_screen_refused(self):
        self.change(self.results['camp'],last_observer={'screen':1});self.assertRaises(m.Refusal,self.validate)
    def test_corrupt_quest_checksum_refused(self):
        self.put(self.opening,self.opening.read_text().replace('progress 1 1 1 1 1','progress 1 0 1 1 1'));self.assertRaises(m.Refusal,self.validate)
    def test_corrupt_durable_profile_refused(self):
        self.put(self.profile,self.profile.read_text().replace('story 12','story 24'));self.assertRaises(m.Refusal,self.validate)
    def test_receipts_disagree_with_final_saves_refused(self):
        data=json.loads(self.results['camp'].read_text());data['receipts']['durable_profile']['story_coins']=24;self.write(self.results['camp'],data);self.assertRaises(m.Refusal,self.validate)
    def test_missing_real_win_in_observer_refused(self):
        rows=[json.loads(line) for line in self.trace.read_text().splitlines()];rows=[dict(row,wins=0 if row['wins']==2 else row['wins']) for row in rows]
        self.put(self.trace,''.join(json.dumps(row)+'\n' for row in rows));self.assertRaises(m.Refusal,self.validate)
    def test_observer_frame_reset_refused(self):
        self.put(self.trace,self.trace.read_text()+self.trace.read_text());self.assertRaises(m.Refusal,self.validate)
    def test_short_benchmark_refused(self):
        self.change(self.bench,requested_seconds=10.,actual_seconds=10.);self.assertRaises(m.Refusal,self.validate)
    def test_other_pid_benchmark_refused(self):
        self.change(self.bench,process={'pid':123});self.assertRaises(m.Refusal,self.validate)
    def test_changed_benchmark_settings_refused(self):
        self.change(self.bench,graphics_settings_end_sha256='0'*64);self.assertRaises(m.Refusal,self.validate)
    def test_no_implicit_or_overwriting_receipt_write(self):
        result=self.validate();path=self.review/'unit-fixture-report.json'
        m.write_receipt(result,path,self.n);self.assertTrue(path.is_file())
        self.assertRaises(FileExistsError,m.write_receipt,result,path,self.n)
        self.assertRaises(m.Refusal,m.write_receipt,result,self.project/'forbidden.json',self.n)


if __name__=='__main__':
    unittest.main()
