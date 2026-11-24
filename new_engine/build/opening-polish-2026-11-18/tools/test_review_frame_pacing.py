#!/usr/bin/env python3
"""Small evidence-contract tests: no silent hitch exclusions or false joins."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location('review_frame_pacing', HERE/'review_frame_pacing.py')
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class FramePacingEvidenceTests(unittest.TestCase):
    def test_capture_is_correlation_and_hitch_remains_in_overall(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root/'performance.json.csv').write_text('frame,frame_ms,update_ms\n1,0,2\n2,10,3\n3,1000,4\n4,20,5\n')
            (root/'performance.json').write_text(json.dumps({'warmup_frames':1}))
            (root/'performance.json.world.json').write_text(json.dumps({'travel_loading_frames':2,'longest_travel_loading_streak':2,'destination_readiness':{'overlapping_reasons':True}}))
            (root/'checkpoint.json').write_text(json.dumps({'frame':2,'name':'checkpoint','inventory_open':False}))
            (root/'checkpoint.bmp').write_bytes(b'fixture')
            (root/'unlabelled-opening.bmp').write_bytes(b'fixture')
            report = MODULE.build_report(root)
            overall = report['overall_csv_intervals']
            self.assertEqual(overall['all_positive_intervals']['samples'],3)
            self.assertAlmostEqual(overall['all_positive_intervals']['mean_ms'],1030/3)
            self.assertEqual(overall['all_positive_intervals']['max_ms'],1000)
            self.assertEqual(overall['after_declared_warmup']['max_ms'],1000)
            correlations = report['checkpoint_capture_correlations']
            row = correlations['records'][0]
            self.assertEqual(row['candidate_following_profiler_frame'],3)
            self.assertEqual(row['following_interval_ms'],1000)
            self.assertFalse(correlations['alignment_verified'])
            self.assertFalse(correlations['adjusted_statistics_produced'])
            self.assertFalse(report['playable_loading_split_available'])
            self.assertEqual(report['aggregate_loading']['travel_loading_frames'],2)
            self.assertEqual(len(correlations['native_bmp_files_without_frame_sidecars']),1)
            json.dumps(report,allow_nan=False)

    def test_duplicate_frames_and_bad_values_do_not_create_exact_join(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            (root/'custom.csv').write_text('frame,frame_ms\n1,10\n2,20\n2,30\n3,nan\n4,-1\n')
            (root/'capture.json').write_text('{"name":"capture","frame":1}')
            report=MODULE.build_report(root,profile='custom.csv')
            self.assertEqual(report['csv']['duplicate_frame_ids'],[2])
            self.assertEqual(len(report['csv']['invalid_rows']),2)
            self.assertIsNone(report['checkpoint_capture_correlations']['records'][0]['following_interval_ms'])
            self.assertIsNone(report['overall_csv_intervals']['after_declared_warmup'])
            self.assertFalse(report['playable_loading_split_available'])

    def test_missing_csv_is_explicit(self):
        with tempfile.TemporaryDirectory() as tmp:
            report=MODULE.build_report(tmp)
            self.assertFalse(report['available'])
            self.assertEqual(report['csv']['error'],'Timing CSV is missing')
            self.assertEqual(report['overall_csv_intervals']['all_positive_intervals']['samples'],0)
            self.assertFalse(report['aggregate_loading']['available'])
            json.dumps(report,allow_nan=False)


if __name__=='__main__':
    unittest.main()
