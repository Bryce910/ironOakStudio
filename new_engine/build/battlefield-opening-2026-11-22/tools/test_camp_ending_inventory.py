#!/usr/bin/env python3
"""Exercise the exact proposed ending verifier via AST, without live-control imports."""
import ast
import copy
import shlex
import tempfile
from pathlib import Path
import unittest

source=Path(__file__).with_name('play_camp_quest_review.py')
parsed=ast.parse(source.read_text())
helpers=[n for n in parsed.body if isinstance(n,ast.FunctionDef) and n.name in ('questless_inventory','fingerprint','inventory_receipt')]
cls=next(n for n in parsed.body if isinstance(n,ast.ClassDef) and n.name=='CampQuestReview')
methods=[n for n in cls.body if isinstance(n,ast.FunctionDef) and n.name in ('verify_ending','profile')]
module=ast.Module(body=[*helpers,ast.ClassDef(name='EndingVerifier',bases=[],keywords=[],body=methods,decorator_list=[])],type_ignores=[])
namespace={'DriverError':RuntimeError,'shlex':shlex}
exec(compile(ast.fix_missing_locations(module),str(source),'exec'),namespace)

class EndingInventoryChecks(unittest.TestCase):
    def setUp(self):
        counts=[0]*16;counts[1]=counts[11]=counts[15]=1
        self.inventory={'counts':counts,'weapon':1,'receipts':29}
        self.saved={'stage':7,'camp':7,'progress':[1]*5,'reward':[12,1,1],'inventory':self.inventory}
        self.profile={'story_coins':12,'inventory':namespace['questless_inventory'](self.inventory)}
        self.row={'screen':5};self.shots=[]
        self.driver=namespace['EndingVerifier']()
        self.driver.require_live=lambda:self.row
        self.driver.snapshot=lambda:copy.deepcopy(self.saved)
        self.driver.profile=lambda:copy.deepcopy(self.profile)
        self.driver.reward_profile={'story_coins':12}
        self.driver.ending_seen=False;self.driver.receipts={}
        self.driver.shot=lambda name:self.shots.append(name)
    def verify(self):self.driver.verify_ending()
    def test_actual_story_letter_and_questless_durable_contract_passes(self):
        self.verify();self.assertTrue(self.driver.ending_seen)
        self.assertEqual(self.shots,['thanks-for-playing-real-ending'])
        self.assertEqual(self.driver.receipts['opening']['inventory']['counts'][15],1)
        self.assertEqual(self.driver.receipts['durable_profile']['inventory']['counts'][15],0)
        self.assertEqual(self.inventory['counts'][15],1)
    def native_profile(self,checksum='3dc218483350588a'):
        # Golden bytes from native world_player_profile::encode, including its
        # nested arena checksum. No current game files are read or written.
        return ('VALWOOD_PLAYER_PROFILE 1\nrevision 5\nimports 1 0\nstory 12\ninventory "VALWOOD_INVENTORY 1\n'
                'items 0 1 0 0 0 0 0 0 0 0 0 1 0 0 0 0\nequipment 1 0 0 0 0\nreceipts 29\nspent 0\nend\n"\n'
                'arena "VALWOOD_ARENA_PROGRESS 1\nselection 8 1\nledger 0 0 0 1 0\nactive 0 8 1\nchecksum 9f1a39d7fdc108b4\n"\n'
                'checksum '+checksum+'\n')
    def read_profile(self,payload):
        with tempfile.TemporaryDirectory() as directory:
            holder=namespace['EndingVerifier']();holder.project=Path(directory)
            (holder.project/'saves').mkdir();(holder.project/'saves/player.progress').write_text(payload)
            return holder.profile()
    def test_actual_native_profile_checksum_and_projection_read(self):
        self.assertEqual(self.read_profile(self.native_profile()),self.profile)
    def test_corrupt_durable_checksum_is_refused(self):
        with self.assertRaisesRegex(RuntimeError,'durable player profile checksum'):
            self.read_profile(self.native_profile('0'*16))
    def test_legacy_quest_checksum_basis_cannot_validate_profile(self):
        body,_,_=self.native_profile().rpartition('checksum ')
        payload=body+'checksum '+format(namespace['fingerprint'](body),'016x')+'\n'
        with self.assertRaisesRegex(RuntimeError,'durable player profile checksum'):self.read_profile(payload)
    def test_quest_tokens_in_durable_are_refused(self):
        for index in (14,15):
            with self.subTest(index=index):
                self.profile['inventory']['counts'][index]=1
                with self.assertRaisesRegex(RuntimeError,'Durable non-quest'):self.verify()
                self.profile['inventory']['counts'][index]=0
    def test_missing_or_duplicate_nonquest_items_refused(self):
        for index,count in ((1,0),(1,2),(11,0),(11,2),(7,1)):
            with self.subTest(index=index,count=count):
                before=self.profile['inventory']['counts'][index]
                self.profile['inventory']['counts'][index]=count
                with self.assertRaisesRegex(RuntimeError,'Durable non-quest'):self.verify()
                self.profile['inventory']['counts'][index]=before
    def test_durable_equipment_or_receipts_cannot_differ(self):
        self.profile['inventory']['weapon']=0
        with self.assertRaisesRegex(RuntimeError,'Durable non-quest'):self.verify()
        self.profile['inventory']['weapon']=1;self.profile['inventory']['receipts']=13
        with self.assertRaisesRegex(RuntimeError,'Durable non-quest'):self.verify()
    def test_opening_letter_must_exist(self):
        self.inventory['counts'][15]=0
        with self.assertRaisesRegex(RuntimeError,'sword/bread/satchel/letter'):self.verify()
    def test_bread_cannot_be_lost_in_both_scopes(self):
        self.inventory['counts'][11]=self.profile['inventory']['counts'][11]=0
        with self.assertRaisesRegex(RuntimeError,'sword/bread/satchel/letter'):self.verify()
    def test_sword_cannot_be_duplicated_in_both_scopes(self):
        self.inventory['counts'][1]=self.profile['inventory']['counts'][1]=2
        with self.assertRaisesRegex(RuntimeError,'sword/bread/satchel/letter'):self.verify()
    def test_battlefield_receipt_cannot_be_lost_in_both_scopes(self):
        self.inventory['receipts']=self.profile['inventory']['receipts']=13
        with self.assertRaisesRegex(RuntimeError,'sword/bread/satchel/letter'):self.verify()
    def test_twelve_coin_reward_and_complete_journey_remain_required(self):
        self.profile['story_coins']=11
        with self.assertRaises(RuntimeError):self.verify()
        self.profile['story_coins']=12;self.saved['stage']=6
        with self.assertRaises(RuntimeError):self.verify()
    def test_ending_screen_and_single_reward_still_required(self):
        self.row['screen']=1
        with self.assertRaisesRegex(RuntimeError,'Thanks for Playing'):self.verify()
        self.row['screen']=5;self.driver.reward_profile['story_coins']=24
        with self.assertRaisesRegex(RuntimeError,'duplicated or lost'):self.verify()

if __name__=='__main__':unittest.main()
