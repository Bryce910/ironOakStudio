#!/usr/bin/env python3
"""Prepare portable cutscene narration; originals and existing assets stay intact."""
from pathlib import Path
import argparse
import hashlib
import json
import math
import os
import subprocess
import tempfile
import wave


def digest(path):
    with Path(path).open('rb') as handle:
        return hashlib.file_digest(handle, 'sha256').hexdigest()


def prepare(source, project):
    source = Path(source).expanduser().resolve(strict=True)
    project = Path(project).expanduser().resolve(strict=True)
    if not source.is_file() or not (project / 'project.valwood').is_file():
        raise ValueError('Choose an audio file and a Valwood project folder.')
    result = subprocess.run(['ffprobe', '-v', 'error', '-show_streams', '-show_format',
                             '-of', 'json', str(source)], capture_output=True, text=True,
                            check=True, timeout=30)
    info = json.loads(result.stdout)
    if not any(s.get('codec_type') == 'audio' for s in info.get('streams', [])):
        raise ValueError('The selected file has no narration/audio track.')
    duration = float(info.get('format', {}).get('duration', 0))
    if not math.isfinite(duration) or not .1 <= duration <= 180:
        raise ValueError('Use narration between 0.1 and 180 seconds; split longer scenes into sequences.')
    source_hash = digest(source)
    folder = project / 'cinematics' / 'audio'
    for candidate in (project / 'cinematics', folder):
        if candidate.is_symlink():
            raise ValueError('The project cinematic/audio directories must be real folders.')
        candidate.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.voice-', dir=folder) as working:
        working = Path(working)
        wav = working / 'narration.wav'
        command = ['ffmpeg', '-v', 'error', '-nostdin', '-threads', '2', '-i', str(source),
                   '-map', '0:a:0', '-vn', '-af',
                   f'aresample=48000:async=1:first_pts=0,apad,atrim=duration={duration:.9f}',
                   '-t', f'{duration:.9f}', '-ac', '2', '-ar', '48000', '-c:a', 'pcm_s16le', str(wav)]
        subprocess.run(command, capture_output=True, check=True, timeout=180)
        with wave.open(str(wav), 'rb') as audio:
            actual = audio.getnframes() / audio.getframerate()
            if (audio.getnchannels(), audio.getsampwidth(), audio.getframerate()) != (2, 2, 48000):
                raise ValueError('Prepared narration has an unsupported audio format.')
            if abs(actual - duration) > 1 / 48000 + 1e-6:
                raise ValueError('Prepared narration timing changed.')
        if digest(source) != source_hash:
            raise ValueError('The source changed during import; no asset was installed.')
        audio_hash = digest(wav)
        relative = Path('cinematics/audio') / (audio_hash + '.wav')
        destination = project / relative
        if destination.exists() or destination.is_symlink():
            if destination.is_symlink() or digest(destination) != audio_hash:
                raise ValueError('Existing narration asset differs; no asset was overwritten.')
            reused = True
        else:
            # Atomic, exclusive install prevents concurrent imports from replacing an asset.
            try:
                os.link(wav, destination)
                reused = False
            except FileExistsError:
                if destination.is_symlink() or digest(destination) != audio_hash:
                    raise ValueError('Concurrent import produced a different asset.')
                reused = True
        return {'installed': True, 'asset': relative.as_posix(), 'duration': actual,
                'bytes': destination.stat().st_size, 'sha256': audio_hash, 'reused': reused,
                'source_unchanged': True,
                'next_step': 'Paste asset into the Cinematics narration cue; set cue and timeline duration.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--project', type=Path, required=True)
    args = parser.parse_args()
    try:
        print(json.dumps(prepare(args.source, args.project), indent=2))
    except (OSError, ValueError, json.JSONDecodeError, subprocess.SubprocessError) as error:
        parser.exit(1, f'Voice import failed: {error}\n')


if __name__ == '__main__':
    main()
