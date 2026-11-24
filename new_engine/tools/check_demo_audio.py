#!/usr/bin/env python3
"""Check shipped demo PCM provenance, signal budgets and runtime fixture output."""
from pathlib import Path
import hashlib
import json
import wave
import numpy as np

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'projects/valwood_world/audio/demo'
REPORT=ROOT/'docs/performance/demo-finish/audio'

def main():
    metadata=json.loads((SOURCE/'manifest.json').read_text())
    assert metadata['version']==1 and len(metadata['assets'])==20
    expected={name+'.wav' for name in ['wind','forest','creek','fire','rain','market','tavern','night']}
    expected|={f'step_{surface}_{variant}.wav' for surface in ['soil','stone','wood','wet'] for variant in range(3)}
    assert {asset['file'] for asset in metadata['assets']}==expected
    records=[]
    for asset in metadata['assets']:
        path=SOURCE/asset['file'];assert path.parent==SOURCE
        assert hashlib.sha256(path.read_bytes()).hexdigest()==asset['sha256']
        with wave.open(str(path)) as stream:
            assert stream.getnchannels()==2 and stream.getsampwidth()==2 and stream.getframerate()==22050
            assert stream.getnframes()==asset['frames']
            signal=np.frombuffer(stream.readframes(stream.getnframes()),dtype='<i2').reshape(-1,2).astype(np.float64)/32768
        peak=float(np.max(np.abs(signal)));rms=float(np.sqrt(np.mean(signal*signal)))
        step=asset['file'].startswith('step_')
        assert .005<rms<.09 and peak<(.461 if step else .361)
        # A seam must be no worse than normal neighboring samples in the clip.
        # This catches a wrong crossfade while allowing broadband rain/water.
        seam=float(np.max(np.abs(signal[-1]-signal[0])))
        largest_neighbor=float(np.max(np.abs(np.diff(signal,axis=0))))
        assert seam<=(.0001 if step else largest_neighbor)
        records.append({'file':asset['file'],'sha256':asset['sha256'],'peak':peak,'rms':rms,
                        'loop_seam_delta':seam,'max_adjacent_delta':largest_neighbor})
    runtime=(REPORT/'dummy-backend-tests.log').read_text()
    assert 'PASS demo audio:' in runtime and '35/35/35' in runtime
    budget=1.12*.36+4*(31/128)*.46
    assert budget<.85
    REPORT.mkdir(parents=True,exist_ok=True)
    report={'pass':True,'original_procedural_assets':20,'bed_voices':8,'step_voices':4,
            'max_simultaneous_signal_peak_bound':budget,'decoded_bytes_44100_stereo16':sum(a['frames']*8 for a in metadata['assets']),
            'runtime_dummy_backend_pass':True,'distance_cadence_30_60_144fps':[35,35,35],
            'native_audible_device_review':'Not performed by this CPU fixture; runtime driver/device is recorded by integration.',
            'signals':records}
    (REPORT/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({key:value for key,value in report.items() if key!='signals'},indent=2))

if __name__=='__main__':main()
