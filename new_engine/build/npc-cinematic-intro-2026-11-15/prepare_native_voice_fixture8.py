#!/usr/bin/env python3
"""A clearly synthetic MP3 fixture for the owned real-time voice-over pipeline."""
from pathlib import Path
import hashlib
import json
import shutil
import re
import subprocess

N = Path(__file__).resolve().parent
D = N / 'review/native-voice-fixture8'


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    D.mkdir(exist_ok=True)
    mp3 = D / 'synthetic-tone.mp3'
    require = lambda value, message: None if value else (_ for _ in ()).throw(RuntimeError(message))
    require(not mp3.exists(), 'Existing synthetic fixture is immutable; choose a new version')
    command = ['/usr/bin/ffmpeg', '-hide_banner', '-loglevel', 'error', '-nostdin',
               '-f', 'lavfi', '-i', 'sine=frequency=523.25:sample_rate=48000:duration=8',
               '-af', 'afade=t=in:d=0.05,afade=t=out:st=7.8:d=0.2',
               '-ac', '2', '-ar', '48000', '-c:a', 'libmp3lame', '-b:a', '96k', '-threads', '2', str(mp3)]
    subprocess.run(command, check=True)
    srt = D / 'supplied-story-captions.srt'
    original=(N/'NPC_INTRO_SCRIPT.srt').read_text()
    def scaled(match):
        h,m,sec,ms=map(int,match.groups());value=round((((h*60+m)*60+sec)*1000+ms)*8/36)
        return f'{value//3600000:02d}:{value//60000%60:02d}:{value//1000%60:02d},{value%1000:03d}'
    srt.write_text(re.sub(r'(\d{2}):(\d{2}):(\d{2}),(\d{3})',scaled,original))
    probe = json.loads(subprocess.check_output(['/usr/bin/ffprobe', '-v', 'error', '-show_streams', '-show_format', '-of', 'json', str(mp3)], text=True))
    require(any(row['codec_name'] == 'mp3' and row['codec_type'] == 'audio' for row in probe['streams']), 'Fixture is not actual MP3')
    report = {'schema': 'valwood.npc-intro.synthetic-mp3-fixture.v1', 'passed': True,
              'duration_seconds':8,'source_story_duration_seconds':36,'caption_time_scaling':8/36,
              'actual_mp3_source': True, 'synthetic_test_tone': True, 'recorded_actor_voice': False,
              'narrative_captions_from_authored_script': True, 'command': command, 'probe': probe,
              'inputs': {str(Path(__file__).resolve()): sha(__file__), str(N / 'NPC_INTRO_SCRIPT.srt'): sha(N / 'NPC_INTRO_SCRIPT.srt')},
              'outputs': {str(mp3): sha(mp3), str(srt): sha(srt)}}
    (D / 'FIXTURE.json').write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
    print('PASS actual synthetic MP3 source and supplied story captions:', D)


if __name__ == '__main__':
    main()
