import csv
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('world_gate', Path(__file__).resolve().parents[1] / 'tools/check_world_performance.py')
gate = importlib.util.module_from_spec(spec); spec.loader.exec_module(gate)


class PerformanceGate(unittest.TestCase):
    def fixture(self, directory, intervals):
        path = Path(directory) / 'run.json'
        meta = dict(frames=len(intervals), warmup_frames=2, samples=len(intervals)-2,
                    dropped_samples=0, renderer='AMD Radeon synthetic test fixture', gpu_stage_timing_available=False)
        path.write_text(json.dumps(meta))
        with Path(str(path)+'.csv').open('w') as stream:
            writer=csv.writer(stream); writer.writerow(['frame','frame_ms','raw_update_ms','raw_render_cpu_ms','present_ms'])
            for index, interval in enumerate(intervals,1): writer.writerow([index,interval,1,2,12])
        return path

    def test_strict_spike_gate_and_explicit_relaxation(self):
        with tempfile.TemporaryDirectory() as directory:
            path=self.fixture(directory,[90,90]+[16.67]*999+[25])
            strict,_,_=gate.audit(path)
            self.assertGreater(strict['mean_fps'],59)
            self.assertGreater(strict['one_percent_low_fps'],45)
            self.assertFalse(strict['pass']); self.assertFalse(strict['every_frame_within_budget'])
            relaxed,_,_=gate.audit(path,allowed_misses=1)
            self.assertTrue(relaxed['pass']); self.assertEqual(relaxed['allowed_budget_misses'],1)
            self.assertEqual(strict['frames_over_budget'],1)

    def test_missing_frame_and_nonfinite_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path=self.fixture(directory,[16.67]*200)
            csvpath=Path(str(path)+'.csv'); original=csvpath.read_text()
            lines=original.splitlines(); csvpath.write_text('\n'.join(lines[:55]+lines[56:])+'\n')
            with self.assertRaises(ValueError):gate.audit(path)
            csvpath.write_text(original.replace('55,16.67','55,nan'))
            with self.assertRaises(ValueError):gate.audit(path)

    def test_software_and_world_errors_cannot_pass(self):
        with tempfile.TemporaryDirectory() as directory:
            path=self.fixture(directory,[16.67]*200)
            meta=json.loads(path.read_text());meta['renderer']='llvmpipe';path.write_text(json.dumps(meta))
            self.assertFalse(gate.audit(path)[0]['pass'])
            meta['renderer']='AMD Radeon synthetic test fixture';path.write_text(json.dumps(meta))
            Path(str(path)+'.world.json').write_text(json.dumps(dict(gl_error=1282,remaining_texture_references=3)))
            report,_,_=gate.audit(path);self.assertFalse(report['pass']);self.assertEqual(len(report['correctness_errors']),2)

    def test_trace_is_honest_and_publication_protected(self):
        with tempfile.TemporaryDirectory() as directory:
            path=self.fixture(directory,[16.67]*200)
            _,rows,meta=gate.audit(path);trace=gate.trace_events(rows,meta)
            self.assertFalse(any(event['ph']=='X' for event in trace['traceEvents']))
            self.assertEqual(sum(event['ph']=='C' for event in trace['traceEvents']),200)
            output=Path(directory)/'trace.json';gate.save_new(output,trace);before=output.read_bytes()
            with self.assertRaises(FileExistsError):gate.save_new(output,dict(replaced=True))
            self.assertEqual(before,output.read_bytes())

    def test_only_initial_warmup_interval_may_be_zero(self):
        with tempfile.TemporaryDirectory() as directory:
            path=self.fixture(directory,[0,16.67]+[16.67]*198)
            self.assertTrue(gate.audit(path)[0]['pass'])
            path=self.fixture(directory,[0,16.67]+[0]+[16.67]*197)
            with self.assertRaises(ValueError):gate.audit(path)


if __name__=='__main__':unittest.main()
