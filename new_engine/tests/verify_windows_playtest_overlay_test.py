#!/usr/bin/env python3
"""Small real base/add-on transport checks, including stale patch-source rejection."""
import hashlib
from pathlib import Path
import sys
import unittest
from unittest import mock
import zipfile

sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'));sys.path.insert(0,str(ROOT/'tests'))
import windows_playtest_overlay_test as fixtures
import verify_windows_playtest_overlay as verifier


class VerifyPlaytestOverlayTest(unittest.TestCase):
    def setUp(self):
        self.fixture=fixtures.WindowsPlaytestOverlayTest();self.fixture.setUp()
        self.path,_=self.fixture.build()

    def tearDown(self):self.fixture.tearDown()

    def verify(self,**kwargs):
        return verifier.verify_overlay(self.fixture.base,self.path,self.fixture.spec,self.fixture.verification,**kwargs)

    def test_actual_fixture_every_overlay_member_once_without_extraction(self):
        calls={};original=zipfile.ZipFile.open
        def counted(archive,name,*args,**kwargs):
            if archive.filename==str(self.path):
                key=name.filename if isinstance(name,zipfile.ZipInfo)else name
                calls[key]=calls.get(key,0)+1
            return original(archive,name,*args,**kwargs)
        with mock.patch.object(zipfile.ZipFile,'open',counted), \
             mock.patch.object(zipfile.ZipFile,'extractall',side_effect=AssertionError('extraction')):
            report=self.verify()
        with zipfile.ZipFile(self.path)as archive:names=archive.namelist()
        self.assertTrue(report['verified']);self.assertEqual(report['base_payloads_extracted'],0)
        self.assertEqual(calls,{name:1 for name in names})
        self.assertEqual(report['original_transport_members_overwritten'],0)

    def test_stale_patch_source_data_cannot_certify_older_addon(self):
        (self.fixture.sources/'route.txt').write_bytes(b'Changed route after candidate build')
        with self.assertRaisesRegex(ValueError,'Patch source/transport mismatch'):self.verify()

    def test_corrupt_payload_with_fresh_crc_and_archive_sidecar_fails_sha(self):
        with zipfile.ZipFile(self.path)as archive:
            payloads={row.filename:archive.read(row)for row in archive.infolist()}
        digest=hashlib.sha256((self.fixture.sources/'rpg-next.exe').read_bytes()).hexdigest()
        member='blobs/'+digest[:2]+'/'+digest;payloads[member]=b'X'*len(payloads[member])
        with zipfile.ZipFile(self.path,'w')as archive:
            for name,raw in payloads.items():archive.writestr(name,raw)
        Path(str(self.path)+'.sha256').write_text(hashlib.sha256(self.path.read_bytes()).hexdigest()+'  '+self.path.name+'\n')
        with self.assertRaisesRegex(ValueError,'payload SHA'):self.verify()

    def test_failed_runtime_acceptance_cannot_certify_transport_as_tested(self):
        with self.assertRaisesRegex(ValueError,'acceptance/source guards'):
            self.verify(opening=dict(passed=False,source_state_unchanged=True,binary_unchanged=True))

    def test_tested_grandstone_hall_binds_hall_route_not_approach(self):
        approach=self.fixture.sources/'approach.route';hall=self.fixture.sources/'hall.route'
        approach.write_text('VALWOOD_PLAYTEST_ROUTE 1\ncount 3\n')
        hall.write_text('VALWOOD_PLAYTEST_ROUTE 1\ncount 7\n')
        spec={'patches':{'project/playtest.route':str(approach),'project/playtest-hall.route':str(hall)}}
        self.assertEqual(verifier.runtime_route('grandstone',{'playtest_slice':{'section':1,'nodes':7}},spec),
                         (1,'project/playtest-hall.route',7))
        self.assertEqual(verifier.runtime_route('grandstone',{'playtest_slice':{'section':0,'nodes':3}},spec),
                         (0,'project/playtest.route',3))
        with self.assertRaisesRegex(ValueError,'node count'):
            verifier.runtime_route('grandstone',{'playtest_slice':{'section':1,'nodes':3}},spec)

    def test_unsupported_or_malformed_tested_sections_reject(self):
        spec={'patches':{'project/playtest.route':str(self.fixture.sources/'route.txt')}}
        for label,section in [('city',1),('jungle',1),('grandstone',2),('grandstone',-1),
                              ('city',False),('grandstone','1')]:
            with self.subTest(label=label,section=section), self.assertRaisesRegex(ValueError,'section'):
                verifier.runtime_route(label,{'playtest_slice':{'section':section,'nodes':3}},spec)
        with self.assertRaisesRegex(ValueError,'Missing tested section route'):
            verifier.runtime_route('grandstone',{'playtest_slice':{'section':1,'nodes':7}},spec)


if __name__=='__main__':unittest.main()
