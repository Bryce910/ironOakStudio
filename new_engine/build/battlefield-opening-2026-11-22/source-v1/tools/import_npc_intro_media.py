#!/usr/bin/env python3
"""Prepare supplied NPC-intro video/voice offline; runtime needs no FFmpeg.

Installs a new immutable generation, then atomically switches media.cfg. A
failed conversion never replaces the last working configuration. The supplied
source file is read only. All subprocess arguments are passed without a shell.
"""
from __future__ import annotations
import argparse
from fractions import Fraction
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import uuid
import wave

MAX_DURATION = 300.0
MAX_PACK = 8 * 1024**3
MAX_JPEG = 32 * 1024**2


def sha(path: Path) -> str:
    with path.open('rb') as file:
        return hashlib.file_digest(file, 'sha256').hexdigest()


def quoted(text: str) -> str:
    return '"' + text.replace('\\', '\\\\').replace('"', '\\"') + '"'


def finite(value, label):
    try:
        value = float(value)
    except (TypeError, ValueError) as error:
        raise ValueError('Missing ' + label) from error
    if not math.isfinite(value):
        raise ValueError('Nonfinite ' + label)
    return value


def regular(path: Path):
    if path.is_symlink() or not path.is_file():
        raise ValueError('Expected a regular local source file: ' + str(path))


def safe_directory(path: Path, create=False):
    path = path.absolute()
    for part in [*reversed(path.parents), path]:
        if part.is_symlink():
            raise ValueError('Symbolic links are not allowed in the install path: ' + str(part))
        if part.exists() and not part.is_dir():
            raise ValueError('Install path component is not a directory: ' + str(part))
    if create:
        path.mkdir(parents=True, exist_ok=True)
    return path


def probe(source: Path):
    result = subprocess.run(['ffprobe', '-v', 'error', '-show_streams', '-show_format',
                             '-of', 'json', str(source)], check=True,
                            capture_output=True, text=True, timeout=60)
    document = json.loads(result.stdout)
    if len(document.get('streams', [])) > 64:
        raise ValueError('Too many media streams')
    return document


def subtitle_cues(path: Path, duration: float):
    regular(path)
    if path.stat().st_size > 2 * 1024**2:
        raise ValueError('SRT exceeds 2 MiB')
    text = path.read_text(encoding='utf-8-sig').replace('\r\n', '\n').replace('\r', '\n').strip()
    stamp = r'(\d{1,2}):(\d{2}):(\d{2})[,.](\d{3})'
    timing = re.compile(r'^' + stamp + r'\s*-->\s*' + stamp + r'\s*$')
    cues = []
    for block in re.split(r'\n\s*\n', text):
        lines = block.splitlines()
        if lines and lines[0].strip().isdigit():
            lines.pop(0)
        if len(lines) < 2:
            raise ValueError('Each SRT cue needs a timestamp and text')
        match = timing.fullmatch(lines.pop(0).strip())
        if not match:
            raise ValueError('Invalid SRT timestamp')
        numbers = list(map(int, match.groups()))
        for offset in (0, 4):
            if numbers[offset + 1] >= 60 or numbers[offset + 2] >= 60:
                raise ValueError('Invalid SRT minute/second')
        start, end = [(numbers[o]*3600 + numbers[o+1]*60 + numbers[o+2] + numbers[o+3]/1000)
                      for o in (0, 4)]
        body = '\n'.join(lines).strip()
        if not body or len(body.encode('utf8')) > 2048 or any(ord(c) < 32 and c != '\n' for c in body):
            raise ValueError('Invalid or oversized SRT text')
        if not 0 <= start < end <= duration + .001 or (cues and start < cues[-1][1]):
            raise ValueError('SRT cues must be ordered, nonoverlapping and inside the clip')
        cues.append((start, min(end, duration), body))
    if not 0 < len(cues) <= 1000:
        raise ValueError('SRT needs 1–1000 cues')
    return cues


def run_audio(source, target, duration, has_audio, stderr):
    command = ['ffmpeg', '-v', 'error', '-nostdin', '-y', '-threads', '2']
    if has_audio:
        command += ['-copyts', '-start_at_zero', '-i', str(source), '-map', '0:a:0', '-vn',
                    '-af', f'aresample=48000:async=1:first_pts=0,apad,atrim=duration={duration:.9f}']
    else:
        command += ['-f', 'lavfi', '-i', 'anullsrc=r=48000:cl=stereo']
    command += ['-t', f'{duration:.9f}', '-ac', '2', '-ar', '48000', '-c:a', 'pcm_s16le', str(target)]
    subprocess.run(command, check=True, stdout=subprocess.DEVNULL, stderr=stderr, timeout=900)
    with wave.open(str(target), 'rb') as wav:
        if wav.getnchannels() != 2 or wav.getsampwidth() != 2 or wav.getframerate() != 48000:
            raise ValueError('Unexpected converted audio format')
        actual = wav.getnframes() / 48000
        if abs(actual - duration) > 1/48000 + 1e-8:
            raise ValueError('Converted audio duration changed')
    return command


def run_video(source, folder, stream, duration, args, stderr):
    width, height = int(stream['width']), int(stream['height'])
    if not 1 <= width <= 16384 or not 1 <= height <= 16384:
        raise ValueError('Source dimensions exceed decode bounds')
    rotation = 0
    for item in stream.get('side_data_list', []):
        if 'rotation' in item:
            rotation = int(round(finite(item['rotation'], 'rotation'))) % 360
    rotation = int(stream.get('tags', {}).get('rotate', rotation)) % 360
    sar = Fraction(stream.get('sample_aspect_ratio', '1:1').replace(':', '/'))
    if sar <= 0 or not .1 <= float(sar) <= 10:
        raise ValueError('Invalid source pixel aspect')
    display_width, display_height = width * float(sar), float(height)
    if rotation in (90, 270):
        display_width, display_height = display_height, display_width
    elif rotation != 0 and rotation != 180:
        raise ValueError('Only right-angle video orientation is supported')
    scale = min(1., args.max_width/display_width, args.max_height/display_height)
    out_width = max(2, int(display_width*scale)//2*2)
    out_height = max(2, int(display_height*scale)//2*2)
    rate = Fraction(stream.get('avg_frame_rate') or stream.get('r_frame_rate') or '0/1')
    if rate <= 0 or rate > 1000:
        raise ValueError('Source video has no valid frame rate')
    rate = min(rate, Fraction(str(args.fps)), Fraction(30))
    count_bound = min(9000, math.ceil(duration*float(rate) + 1e-7))
    video_filter = (f'scale={out_width}:{out_height}:flags=lanczos,setsar=1,'
                    f'tpad=stop_mode=clone:stop_duration={duration:.9f},'
                    f'fps=fps={rate.numerator}/{rate.denominator}:start_time=0:round=near')
    command = ['ffmpeg', '-v', 'error', '-nostdin', '-threads', '2', '-copyts', '-start_at_zero',
               '-i', str(source), '-map', f'0:{int(stream["index"])}', '-an', '-vf', video_filter,
               '-t', f'{duration:.9f}', '-frames:v', str(count_bound), '-c:v', 'mjpeg',
               '-q:v', str(args.jpeg_quality), '-pix_fmt', 'yuvj420p', '-threads', '2',
               '-f', 'image2pipe', 'pipe:1']
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=stderr)
    frames = []; pending = bytearray(); pack_size = 0
    try:
        with (folder/'frames.pack').open('xb') as pack:
            while True:
                chunk = process.stdout.read(65536)
                if not chunk:
                    break
                pending.extend(chunk)
                while True:
                    end = pending.find(b'\xff\xd9')
                    if end < 0:
                        break
                    jpeg = bytes(pending[:end+2]); del pending[:end+2]
                    if not jpeg.startswith(b'\xff\xd8') or len(jpeg) > MAX_JPEG:
                        raise ValueError('Invalid or oversized converted JPEG')
                    if len(frames) >= count_bound or pack_size + len(jpeg) > MAX_PACK:
                        raise ValueError('Converted video exceeds frame/pack budget')
                    frames.append((len(frames), len(frames)/float(rate), pack_size, len(jpeg)))
                    pack.write(jpeg); pack_size += len(jpeg)
                if len(pending) > MAX_JPEG:
                    raise ValueError('Unbounded converted JPEG')
            if process.wait(timeout=30) != 0 or pending or not frames:
                raise ValueError('Video conversion did not complete')
            pack.flush(); os.fsync(pack.fileno())
    except BaseException:
        process.kill(); process.wait(); raise
    finally:
        process.stdout.close()
    if len(frames) < max(1, math.floor(duration*float(rate))):
        raise ValueError('Video conversion lost the clip tail')
    lines = ['VALWOOD_NPC_INTRO_FRAMES 1', f'size {out_width} {out_height}',
             f'fps {rate.numerator} {rate.denominator}', f'count {len(frames)}', f'pack_size {pack_size}']
    lines += [f'frame {i} {pts:.12f} {offset} {size}' for i, pts, offset, size in frames]
    lines += ['end', '']
    (folder/'frames.idx').write_text('\n'.join(lines), encoding='utf8')
    return out_width, out_height, rate, frames, command


def import_media(args):
    source = args.source.absolute(); regular(source)
    project = safe_directory(args.project)
    if not project.is_dir():
        raise ValueError('Project directory does not exist')
    if not 2 <= args.max_width <= 3840 or not 2 <= args.max_height <= 2160 or not 0 < args.fps <= 30 or not 2 <= args.jpeg_quality <= 10:
        raise ValueError('Limits: <=3840×2160,0<fps<=30,JPEG quality2–10')
    document = probe(source)
    videos = [s for s in document['streams'] if s.get('codec_type') == 'video' and not s.get('disposition', {}).get('attached_pic')]
    audios = [s for s in document['streams'] if s.get('codec_type') == 'audio']
    duration = finite(document.get('format', {}).get('duration'), 'container duration')
    if not .1 <= duration <= MAX_DURATION:
        raise ValueError('Supplied intro duration must be0.1–300 seconds')
    if args.mode == 'video' and not videos:
        raise ValueError('Video mode needs a video stream')
    if args.mode == 'voice' and not audios:
        raise ValueError('Voice mode needs an audio stream')
    # A single, explicit SDR preparation path avoids silently washing HDR out.
    if args.mode == 'video' and videos[0].get('color_transfer') in ('smpte2084', 'arib-std-b67'):
        raise ValueError('Supply an SDR Rec.709 export; automatic HDR tone mapping is not applied')
    cues = subtitle_cues(args.subtitles, duration) if args.subtitles else []
    install = safe_directory(project/'opening'/'npc_intro', create=True)
    generation = 'media-' + uuid.uuid4().hex
    staging = Path(tempfile.mkdtemp(prefix='.prepare-', dir=install))
    renamed = False
    try:
        with (staging/'ffmpeg.log').open('wb') as errors:
            audio_command = run_audio(source, staging/'audio.wav', duration, bool(audios), errors)
            width = height = 0; rate = Fraction(0); frames = []; video_command = None
            if args.mode == 'video':
                width, height, rate, frames, video_command = run_video(source, staging, videos[0], duration, args, errors)
        if cues:
            lines = ['VALWOOD_NPC_INTRO_SUBTITLES 1']
            lines += [f'cue {start:.9f} {end:.9f} {quoted(text)}' for start, end, text in cues]
            (staging/'subtitles.vst').write_text('\n'.join(lines + ['end', '']), encoding='utf8')
        files = {p.name: {'bytes': p.stat().st_size, 'sha256': sha(p)} for p in staging.iterdir() if p.is_file()}
        metadata = {'schema': 'valwood.npc-intro.import.v1', 'mode': args.mode,
                    'duration': duration, 'width': width, 'height': height,
                    'fps': [rate.numerator, rate.denominator], 'frames': len(frames),
                    'source': {'path': str(source), 'sha256': sha(source), 'probe': document},
                    'original_audio_present': bool(audios), 'subtitles': len(cues),
                    'subtitle_source': {'path': str(args.subtitles.absolute()), 'sha256': sha(args.subtitles)} if args.subtitles else None,
                    'timing': 'Container start normalized to zero; audio delay padded, video sampled without duration change.',
                    'commands': {'audio': audio_command, 'video': video_command}, 'files': files}
        (staging/'provenance.json').write_text(json.dumps(metadata, indent=2)+'\n')
        config = ['VALWOOD_NPC_INTRO_MEDIA 1', f'mode {args.mode}', f'duration {duration:.12f}',
                  f'audio {quoted(generation + "/audio.wav")}', f'audio_present {int(bool(audios))}',
                  f'index {quoted(generation + "/frames.idx") if args.mode == "video" else quoted("")}',
                  f'pack {quoted(generation + "/frames.pack") if args.mode == "video" else quoted("")}',
                  f'subtitles {quoted(generation + "/subtitles.vst") if cues else quoted("")}', 'end', '']
        for file in staging.iterdir():
            with file.open('rb') as handle:
                os.fsync(handle.fileno())
        final = install/generation
        os.replace(staging, final); renamed = True
        fd, temporary = tempfile.mkstemp(prefix='.media-cfg-', dir=install)
        try:
            with os.fdopen(fd, 'w', encoding='utf8') as out:
                out.write('\n'.join(config)); out.flush(); os.fsync(out.fileno())
            os.replace(temporary, install/'media.cfg')
            directory = os.open(install, os.O_RDONLY)
            try:
                os.fsync(directory)
            finally:
                os.close(directory)
        finally:
            if os.path.exists(temporary):
                os.unlink(temporary)
        return {'installed': True, 'mode': args.mode, 'duration': duration,
                'configuration': str(install/'media.cfg'), 'generation': str(final),
                'dimensions': [width, height], 'frames': len(frames), 'audio_present': bool(audios)}
    finally:
        if not renamed:
            shutil.rmtree(staging, ignore_errors=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--mode', required=True, choices=('video', 'voice'))
    parser.add_argument('--project', type=Path, required=True)
    parser.add_argument('--subtitles', type=Path)
    parser.add_argument('--max-width', type=int, default=3840)
    parser.add_argument('--max-height', type=int, default=2160)
    parser.add_argument('--fps', type=float, default=30)
    parser.add_argument('--jpeg-quality', type=int, default=2)
    args = parser.parse_args()
    try:
        print(json.dumps(import_media(args), indent=2))
    except (ValueError, OSError, subprocess.SubprocessError, json.JSONDecodeError, ZeroDivisionError) as error:
        parser.exit(1, 'Import failed; previous media configuration retained: ' + str(error) + '\n')


if __name__ == '__main__':
    main()
