#!/usr/bin/env python3
"""CPU-only resume validation/copy checks; never launches or controls a game."""
from pathlib import Path
import json
import tempfile
import unittest
import private_review_resume as m

class ResumeChecks(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.base=Path(self.temp.name)
        self.review=self.base/'review';self.review.mkdir();self.source=self.review/'archived-full';self.project=self.source/'project';self.saves=self.project/'saves';self.saves.mkdir(parents=True)
        self.binary=self.base/'current-rpg';self.binary.write_bytes(b'test-fixture-current-binary')
        (self.project/'PRIVATE_CAPTURE_PROJECT.json').write_text('{"private":true}\n')
        (self.source/'state-preservation.json').write_text('{"exit_code":0,"live_state_unchanged":true}\n')
        (self.source/'launch.json').write_text(json.dumps({'binary_sha256':m.sha256(self.binary)}))
        (self.saves/'opening.progress').write_text('VALWOOD_OPENING 1\nfixture-native-opening-bytes\n')
        (self.saves/'player.progress').write_text('VALWOOD_PLAYER_PROFILE 1\nfixture-native-profile-bytes\n')
        (self.project/'graphics.settings').write_text('fixture-native-ui-selected-environment-medium\n')
    def tearDown(self):self.temp.cleanup()
    def validate(self):return m.validate_resume_source(self.source,self.review,self.binary,self.review/'live-full')
    def test_identical_bytes_and_graphics_copied_with_provenance(self):
        receipt=self.validate();destination=self.review/'live-full/project';destination.mkdir(parents=True)
        (destination/'graphics.settings').write_text('fresh-world-defaults')
        m.copy_resume_files(receipt,destination)
        self.assertTrue(receipt['normal_continue_required']);self.assertFalse(receipt['seeded_checkpoint_written'])
        for entry in receipt['files']:
            self.assertEqual((destination/entry['relative']).read_bytes(),(self.project/entry['relative']).read_bytes())
            self.assertEqual(m.sha256(destination/entry['relative']),entry['sha256'])
    def test_wrong_binary_rejected(self):
        self.binary.write_bytes(b'new-build');self.assertRaises(m.ResumeError,self.validate)
    def compatible_fixture(self,changed='engine/world_model_cpu.cpp'):
        old_hash=m.sha256(self.binary)
        sources={'engine/world_model_cpu.cpp':'1'*64,'engine/world_battlefield_prologue.h':'2'*64}
        (self.source/'BUILD_RECEIPT.json').write_text(json.dumps({'binary_sha256':old_hash,'source_sha256':sources}))
        self.binary.write_bytes(b'new-renderer-build')
        updated=dict(sources);updated[changed]='3'*64
        receipt=self.base/'current-build.json'
        receipt.write_text(json.dumps({'binary_sha256':m.sha256(self.binary),'source_sha256':updated}))
        return receipt
    def test_renderer_only_native_save_compatibility_is_explicit(self):
        receipt=self.compatible_fixture()
        self.assertRaises(m.ResumeError,self.validate)
        r=m.validate_resume_source(self.source,self.review,self.binary,render_build_receipt=receipt)
        self.assertTrue(r['render_build_compatibility']['gameplay_and_save_sources_identical'])
        self.assertFalse(r['seeded_checkpoint_written']);self.assertTrue(r['normal_continue_required'])
    def test_gameplay_change_cannot_use_renderer_resume(self):
        receipt=self.compatible_fixture('engine/world_battlefield_prologue.h')
        self.assertRaises(m.ResumeError,m.validate_resume_source,self.source,self.review,self.binary,None,receipt)
    def test_renderer_scratch_policy_compatibility_is_explicit(self):
        receipt=self.compatible_fixture()
        current=json.loads(receipt.read_text())
        current['source_sha256']['engine/world_scenery_streaming.inc']='4'*64
        receipt.write_text(json.dumps(current))
        self.assertRaises(m.ResumeError,self.validate)
        result=m.validate_resume_source(self.source,self.review,self.binary,render_build_receipt=receipt)
        self.assertEqual(result['render_build_compatibility']['engine_changes'],
                         ['engine/world_model_cpu.cpp','engine/world_scenery_streaming.inc'])
    def test_unrelated_engine_change_cannot_use_renderer_resume(self):
        receipt=self.compatible_fixture('engine/world_viewer.cpp')
        self.assertRaises(m.ResumeError,m.validate_resume_source,self.source,self.review,self.binary,None,receipt)
    def test_incorrect_previous_build_receipt_rejected(self):
        receipt=self.compatible_fixture();old=json.loads((self.source/'BUILD_RECEIPT.json').read_text());old['binary_sha256']='0'*64
        (self.source/'BUILD_RECEIPT.json').write_text(json.dumps(old))
        self.assertRaises(m.ResumeError,m.validate_resume_source,self.source,self.review,self.binary,None,receipt)
    def test_unclean_exit_rejected(self):
        (self.source/'state-preservation.json').write_text('{"exit_code":1,"live_state_unchanged":true}')
        self.assertRaises(m.ResumeError,self.validate)
    def test_changed_published_state_rejected(self):
        (self.source/'state-preservation.json').write_text('{"exit_code":0,"live_state_unchanged":false}')
        self.assertRaises(m.ResumeError,self.validate)
    def test_nonprivate_marker_rejected(self):
        (self.project/'PRIVATE_CAPTURE_PROJECT.json').write_text('{"private":false}')
        self.assertRaises(m.ResumeError,self.validate)
    def test_save_symlink_rejected(self):
        path=self.saves/'player.progress';data=path.read_bytes();path.unlink();outside=self.base/'outside-save';outside.write_bytes(data);path.symlink_to(outside)
        self.assertRaises(m.ResumeError,self.validate)
    def test_settings_symlink_rejected(self):
        path=self.project/'graphics.settings';path.unlink();target=self.base/'outside-settings';target.write_text('outside');path.symlink_to(target)
        self.assertRaises(m.ResumeError,self.validate)
    def test_save_subdirectory_rejected(self):
        (self.saves/'directory').mkdir();self.assertRaises(m.ResumeError,self.validate)
    def test_source_must_be_archived_below_review(self):
        self.assertRaises(m.ResumeError,m.validate_resume_source,self.base,self.review,self.binary)
        self.assertRaises(m.ResumeError,m.validate_resume_source,self.source,self.review,self.binary,self.source)
    def test_changed_source_after_validation_rejected(self):
        receipt=self.validate();(self.saves/'opening.progress').write_text('changed-after-validation')
        destination=self.review/'live-full/project';destination.mkdir(parents=True)
        self.assertRaises(m.ResumeError,m.copy_resume_files,receipt,destination)
    def test_missing_native_profile_rejected(self):
        (self.saves/'player.progress').unlink();self.assertRaises(m.ResumeError,self.validate)
    def test_symlink_destination_rejected(self):
        receipt=self.validate();destination=self.review/'live-full/project';destination.mkdir(parents=True)
        target=self.base/'outside-target';target.write_text('unchanged');(destination/'graphics.settings').symlink_to(target)
        self.assertRaises(m.ResumeError,m.copy_resume_files,receipt,destination);self.assertEqual(target.read_text(),'unchanged')
    def test_profile_prefix_future_path_allowed_only_below_new_review(self):
        new=self.review/'live-full';prefix=new/'profile/stationary'
        self.assertEqual(m.validate_profile_prefix(prefix,new),prefix)
        self.assertFalse(new.exists())
        self.assertRaises(m.ResumeError,m.validate_profile_prefix,'relative/profile',new)
        self.assertRaises(m.ResumeError,m.validate_profile_prefix,self.base/'outside',new)
        self.assertRaises(m.ResumeError,m.validate_profile_prefix,new,new)

if __name__=='__main__':unittest.main()
