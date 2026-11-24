#!/usr/bin/env python3
"""Export an explicitly edited preview using only actual opening readbacks.

Focus loss can freeze the game's story clock while display captures continue.
This separate exporter omits repeated frozen-clock samples and selects the
nearest distinct actual frame for each of 240 playback targets. It never
repairs or replaces the strict film cadence acceptance result.
"""
from pathlib import Path
import argparse
from bisect import bisect_left
import hashlib
import json
import math
import shutil
import subprocess
import tempfile

from PIL import Image, ImageSequence


FRAME_COUNT = 240
FPS = 15
SOURCE_TOLERANCE = 1 / 60 + 1e-5


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def choose(records):
    clocks = [record['story_seconds'] for record in records]
    selected = []
    for index in range(FRAME_COUNT):
        target = index / FPS
        right = bisect_left(clocks, target)
        candidates = [i for i in (right - 1, right) if 0 <= i < len(records)]
        nearest = min(candidates, key=lambda i: (abs(clocks[i] - target), i))
        error = abs(clocks[nearest] - target)
        if error > SOURCE_TOLERANCE:
            raise ValueError('No real source frame within 1/60 s of target %d' % index)
        if selected and nearest <= selected[-1]['source_index']:
            raise ValueError('Nearest source selections are not unique and increasing')
        selected.append(dict(playback_frame=index, playback_seconds=target,
                             source_index=nearest, source_file=records[nearest]['file'],
                             source_story_seconds=clocks[nearest],
                             source_wall_seconds=records[nearest]['wall_seconds'],
                             source_rendered_frame=records[nearest]['rendered_frame'],
                             source_shot_index=records[nearest]['shot_index'],
                             absolute_story_time_error_seconds=error))
    if {record['source_shot_index'] for record in selected} != {0, 1, 2, 3}:
        raise ValueError('Selected actual frames do not cover all four shots')
    return selected


def encode(film):
    film = Path(film)
    if film.is_symlink() or not film.is_dir():
        raise ValueError('Expected an ordinary existing film directory')
    film = film.resolve()
    raw = film / 'raw'
    manifest = raw / 'manifest.jsonl'
    if raw.is_symlink() or manifest.is_symlink() or not manifest.is_file():
        raise ValueError('Expected ordinary raw readbacks and manifest')
    manifest_before = sha(manifest)
    records = [json.loads(line) for line in manifest.read_text().splitlines()]
    if not FRAME_COUNT <= len(records) <= 4096:
        raise ValueError('Raw capture count is outside the bounded export range')
    for index, record in enumerate(records):
        expected = 'entry-%04d.bmp' % index
        if record.get('file') != expected or record.get('output_frame') != index:
            raise ValueError('Raw manifest filename/order mismatch')
        if record.get('width') != 1600 or record.get('height') != 900:
            raise ValueError('Raw metadata does not describe 1600x900 readbacks')
        for key in ('story_seconds', 'wall_seconds'):
            value = record.get(key)
            if not isinstance(value, (int, float)) or not math.isfinite(value) or value < 0:
                raise ValueError('Invalid source clock: ' + key)
            if index and value < records[index - 1][key] - 1e-9:
                raise ValueError('Source clock moved backwards: ' + key)
        if not isinstance(record.get('rendered_frame'), int) or record['rendered_frame'] != index * 4:
            raise ValueError('Unexpected rendered-frame readback order')
        if record.get('shot_index') not in (0, 1, 2, 3):
            raise ValueError('Invalid source shot')
    expected_names = {record['file'] for record in records}
    if {path.name for path in raw.glob('*.bmp')} != expected_names:
        raise ValueError('Raw files and manifest do not have exact closure')
    if records[0]['story_seconds'] > .04 or records[-1]['story_seconds'] < 15.93:
        raise ValueError('Source does not cover the complete 16-second entrance')
    selected = choose(records)
    output_names = ['apartment-entry.gif', 'apartment-entry.mp4',
                    'raw-frame-manifest.json', 'selected-frame-manifest.json',
                    'preview-results.json']
    output_names += ['preview-' + label + '.png' for label in ('entry', 'bills', 'television', 'handoff')]
    if any((film / name).exists() or (film / name).is_symlink() for name in output_names):
        raise ValueError('Preview outputs must be new')

    # Fingerprint ALL source files, including the paused samples omitted from
    # the edited preview. This ledger supports a separate verified cleanup.
    source_ledger = []
    for record in records:
        path = raw / record['file']
        if path.is_symlink() or not path.is_file():
            raise ValueError('Raw source must be an ordinary file')
        with Image.open(path) as source:
            if source.format != 'BMP' or source.size != (1600, 900):
                raise ValueError('Unexpected raw image header')
        source_ledger.append(dict(record, sha256=sha(path), bytes=path.stat().st_size))
    for record in selected:
        source = source_ledger[record['source_index']]
        record['source_sha256'] = source['sha256']
        record['source_bytes'] = source['bytes']

    stage = Path(tempfile.mkdtemp(prefix='.actual-preview-', dir=film))
    try:
        sequence = stage / 'sequence'
        sequence.mkdir()
        frames = []
        for record in selected:
            path = raw / record['source_file']
            (sequence / ('entry-%04d.bmp' % record['playback_frame'])).symlink_to(path)
            with Image.open(path) as source:
                source.load()
                frames.append(source.convert('RGB').resize((640, 360), Image.Resampling.LANCZOS))
        tiles = Image.new('RGB', (160 * 12, 90 * math.ceil(len(frames) / 12)))
        for index, frame in enumerate(frames):
            tiles.paste(frame.resize((160, 90), Image.Resampling.BOX), ((index % 12) * 160, (index // 12) * 90))
        palette = tiles.quantize(colors=128, method=Image.Quantize.MEDIANCUT)
        encoded = [frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames]
        gif = stage / 'apartment-entry.gif'
        encoded[0].save(gif, save_all=True, append_images=encoded[1:], loop=0,
                        duration=[70, 60, 70] * 80, disposal=2, optimize=False)
        durations = []
        with Image.open(gif) as check:
            for frame in ImageSequence.Iterator(check):
                durations.append(frame.info.get('duration', 0))
            if check.size != (640, 360) or len(durations) != FRAME_COUNT or sum(durations) != 16000:
                raise ValueError('GIF frame/duration verification failed')
        video = stage / 'apartment-entry.mp4'
        subprocess.run(['ffmpeg', '-y', '-v', 'error', '-threads', '3', '-framerate', str(FPS),
                        '-i', str(sequence / 'entry-%04d.bmp'), '-frames:v', str(FRAME_COUNT),
                        '-vf', 'scale=1280:720:flags=lanczos', '-c:v', 'libx264',
                        '-preset', 'medium', '-crf', '19', '-pix_fmt', 'yuv420p',
                        '-an', '-movflags', '+faststart', str(video)], check=True, timeout=180)
        probe = json.loads(subprocess.check_output(['ffprobe', '-v', 'error', '-show_streams',
                                                   '-show_format', '-of', 'json', str(video)], text=True))
        streams = probe['streams']
        if (len(streams) != 1 or streams[0]['codec_type'] != 'video' or streams[0]['width'] != 1280
                or streams[0]['height'] != 720 or int(streams[0]['nb_frames']) != FRAME_COUNT
                or abs(float(probe['format']['duration']) - 16) > .01
                or streams[0]['avg_frame_rate'] != '15/1'):
            raise ValueError('MP4 stream/frame/duration verification failed')
        for label, index in [('entry', 24), ('bills', 72), ('television', 138), ('handoff', 239)]:
            frames[index].save(stage / ('preview-' + label + '.png'))
        clocks = [record['story_seconds'] for record in records]
        duplicate_frames = sum(abs(b - a) < 1e-9 for a, b in zip(clocks, clocks[1:]))
        pause_runs = []
        first = 0
        for index in range(1, len(records) + 1):
            if index < len(records) and abs(clocks[index] - clocks[first]) < 1e-9:
                continue
            if index - first > 1:
                pause_runs.append(dict(first_source_index=first, last_source_index=index - 1,
                                       source_story_seconds=clocks[first], sample_count=index - first,
                                       repeated_clock_frames=index - first - 1,
                                       wall_seconds=records[index - 1]['wall_seconds'] - records[first]['wall_seconds']))
            first = index
        for name, data in [('raw-frame-manifest.json', source_ledger),
                           ('selected-frame-manifest.json', selected)]:
            (stage / name).write_text(json.dumps(data, indent=2) + '\n')
        if sha(manifest) != manifest_before or {p.name for p in raw.glob('*.bmp')} != expected_names:
            raise ValueError('Raw capture changed during encoding')
        result = dict(passed=True,
            scope='Edited silent preview using nearest actual display readbacks. Repeated frozen-story-clock samples are omitted; source and playback clocks are recorded separately. This is not strict capture-cadence acceptance or an FPS benchmark.',
            strict_film_cadence_acceptance=False,
            source_frame_count=len(records), exported_actual_frame_count=FRAME_COUNT,
            playback_fps=FPS, playback_seconds=16, generated_motion=False, interpolated_frames=False,
            selection='Nearest actual source frame per k/15 playback target; distinct increasing source indices',
            maximum_allowed_source_story_time_error_seconds=SOURCE_TOLERANCE,
            maximum_source_story_time_error_seconds=max(r['absolute_story_time_error_seconds'] for r in selected),
            source_story_first=clocks[0], source_story_last=clocks[-1],
            selected_source_story_first=selected[0]['source_story_seconds'],
            selected_source_story_last=selected[-1]['source_story_seconds'],
            omitted_source_frames=len(records) - FRAME_COUNT,
            repeated_frozen_story_clock_frames=duplicate_frames,
            other_sampling_omissions=len(records) - FRAME_COUNT - duplicate_frames,
            frozen_clock_runs=pause_runs,
            focus_pause_scope='Caller reports focus pauses. Encoder verifies repeated story clocks and elapsed wall time; it does not inspect operating-system focus events.',
            raw_manifest=dict(path=str(manifest), sha256=manifest_before, unchanged=True),
            all_raw_files_hashed=True,
            raw_frame_ledger=dict(path=str(film / 'raw-frame-manifest.json'),
                                  sha256=sha(stage / 'raw-frame-manifest.json'), files=len(source_ledger)),
            selected_frame_ledger=dict(path=str(film / 'selected-frame-manifest.json'),
                                       sha256=sha(stage / 'selected-frame-manifest.json'), files=FRAME_COUNT),
            transformations=['Omit unselected actual source samples', 'Whole-frame Lanczos resizing',
                             'Shared-palette GIF encoding', 'H.264 video encoding'],
            gif=dict(path=str(film / gif.name), sha256=sha(gif), bytes=gif.stat().st_size,
                     dimensions=[640, 360], duration_ms=16000, frames=FRAME_COUNT),
            video=dict(path=str(film / video.name), sha256=sha(video), bytes=video.stat().st_size,
                       dimensions=[1280, 720], frames=FRAME_COUNT, duration_seconds=16, audio=False),
            encoder_sha256=sha(Path(__file__)),
            raw_cleanup='All raw files and their original manifest retained; any later cleanup requires separate export/evidence verification.')
        (stage / 'preview-results.json').write_text(json.dumps(result, indent=2) + '\n')
        for name in output_names:
            (stage / name).rename(film / name)
        return result
    finally:
        shutil.rmtree(stage)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--film', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(encode(args.film), indent=2))
