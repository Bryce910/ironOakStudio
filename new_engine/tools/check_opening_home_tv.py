#!/usr/bin/env python3
"""Capture the actual native apartment TV animation and encode its display pixels.

This selects a private authored camera, not a player walking route. The existing
renderer captures 360 consecutive full-display frames over six seconds of its
deterministic frame clock. GIF creation only crops and encodes those pixels;
it never generates movement, interpolates frames or animates a still image.
Readbacks make this capture unsuitable as an FPS benchmark.
"""
from __future__ import annotations

import argparse
import fcntl
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import time

import numpy as np
from PIL import Image, ImageSequence

from benchmark_graphics_settings import native_environment
from check_opening_apartment_refresh import prepare_capture_project
from check_opening_prologue import ROOT, isolate_project, mutable_snapshot, sha

WIDTH, HEIGHT, FRAMES, SAMPLES = 1600, 900, 600, 360
TV_ROI = (520, 240, 1080, 660)
GIF_CROP = (440, 160, 1160, 740)
EYE = [-3502.799567, 601.267020, -10999.1883]
TARGET = [-3502.799567, 601.267020, -11000.7883]
FOV = .90
SCENIC = 'city-camera-02'
SCOPE = ('Actual native display capture from a private authored TV camera; '
         'not gameplay traversal, physical controller acceptance or an FPS benchmark')


def read_json(path):
    return json.loads(path.read_text()) if path.is_file() else {}


def change(a, b):
    difference = np.abs(a.astype(np.int16)-b.astype(np.int16))
    return dict(mean_absolute_rgb=float(difference.mean()),
                fraction_pixels_changed_12_levels=float((difference.max(axis=2)>=12).mean()),
                maximum_rgb_change=int(difference.max()))


def encode_evidence(raw, output):
    """Inspect all raw frames; encode every fourth actual frame at 15 FPS."""
    failures, manifest, samples, rois = [], [], [], []
    expected = [f'tv-{index:03d}.bmp' for index in range(SAMPLES)]
    actual = sorted(path.name for path in raw.glob('*.bmp'))
    if actual != expected:
        return dict(passed=False, failures=['Expected exactly 360 sequential full-display BMPs'],
                    expected_count=SAMPLES, actual_count=len(actual)), []
    for index, name in enumerate(expected):
        path = raw/name
        with Image.open(path) as frame:
            frame.load()
            if frame.size != (WIDTH, HEIGHT):
                raise ValueError(f'Raw frame {name} has unexpected dimensions {frame.size}')
            pixels = frame.convert('RGB')
            if index in (0, SAMPLES//2, SAMPLES-1):
                label = {0:'first', SAMPLES//2:'middle', SAMPLES-1:'last'}[index]
                pixels.save(output/f'tv-{label}.png')
            if index % 4 == 0:
                samples.append(pixels.crop(GIF_CROP))
                rois.append(np.array(pixels.crop(TV_ROI), dtype=np.uint8))
            if index == SAMPLES-1:
                final_roi = np.array(pixels.crop(TV_ROI), dtype=np.uint8)
        manifest.append(dict(name=name, bytes=path.stat().st_size, sha256=sha(path),
                             engine_frame=FRAMES-SAMPLES+index+1,
                             visual_time_seconds=17.+(FRAMES-SAMPLES+index+1)/60.))
    pairs = [change(a,b) for a,b in zip(rois,rois[1:])]
    fractions = [item['fraction_pixels_changed_12_levels'] for item in pairs]
    first_last = change(rois[0], final_roi)
    median_change = float(np.median(fractions))
    active_pairs = sum(fraction>=.003 for fraction in fractions)
    # Stable-camera temporal jitter can move a few edges. Require substantial
    # movement within the screen itself, throughout the captured sequence.
    if median_change < .008 or active_pairs < 72:
        failures.append('Insufficient sustained motion inside the TV screen ROI')
    if first_last['fraction_pixels_changed_12_levels'] < .03:
        failures.append('First/last TV pixels do not show a materially different animation state')
    roi_std = float(np.std(rois[0]))
    roi_mean = float(np.mean(rois[0]))
    if roi_std < 8. or not 10. < roi_mean < 245.:
        failures.append('TV ROI is blank, clipped or lacks visible game detail')

    # Shared palette is derived only from the actual captured pixels.
    tile_width, tile_height = 144, 116
    sheet = Image.new('RGB', (tile_width*10, tile_height*9))
    for index, sample in enumerate(samples):
        sheet.paste(sample.resize((tile_width,tile_height),Image.Resampling.BOX),
                    ((index%10)*tile_width,(index//10)*tile_height))
    palette = sheet.quantize(colors=128, method=Image.Quantize.MEDIANCUT)
    encoded = [sample.quantize(palette=palette, dither=Image.Dither.NONE) for sample in samples]
    gif = output/'tv-animation.gif'
    # GIF stores centiseconds. 70,60,70ms per three frames gives exactly 15FPS
    # on average and 6000ms for the 90 frames; constant 67ms would truncate.
    durations = [70,60,70]*(len(encoded)//3)
    encoded[0].save(gif, save_all=True, append_images=encoded[1:], duration=durations,
                    loop=0, disposal=2, optimize=False)
    quality, duration, count = [], 0, 0
    with Image.open(gif) as check:
        if check.size != (GIF_CROP[2]-GIF_CROP[0], GIF_CROP[3]-GIF_CROP[1]):
            failures.append('Encoded GIF dimensions differ from the exact display crop')
        if check.info.get('loop') != 0:
            failures.append('GIF does not loop continuously')
        for index, frame in enumerate(ImageSequence.Iterator(check)):
            duration += int(frame.info.get('duration', 0))
            if index < len(samples):
                loss = np.abs(np.asarray(frame.convert('RGB'),dtype=np.int16)-
                              np.asarray(samples[index],dtype=np.int16))
                quality.append(float(loss.mean()))
            count += 1
    if count != 90 or duration != 6000:
        failures.append(f'GIF is {count} frames/{duration}ms instead of 90 frames/6000ms')
    if not quality or max(quality) > 8.:
        failures.append('GIF palette encoding lost excessive detail compared with captured frames')
    (output/'raw-frame-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return dict(passed=not failures, failures=failures, raw_frame_count=len(manifest),
                full_display_dimensions=[WIDTH,HEIGHT], raw_manifest='raw-frame-manifest.json',
                raw_manifest_sha256=sha(output/'raw-frame-manifest.json'),
                animation_clock=dict(kind='Renderer frame clock, not elapsed wall time',
                    formula='17 + engine_frame/60', first_engine_frame=241,last_engine_frame=600,
                    first_visual_time_seconds=17.+241/60.,last_visual_time_seconds=27.,
                    first_to_last_delta_seconds=359/60.,nominal_playback_seconds=6.),
                motion=dict(roi=list(TV_ROI),threshold_rgb_levels=12, sample_step=4,
                    sampled_frames=len(rois),adjacent_pairs=pairs,
                    median_changed_fraction=median_change,active_pairs=active_pairs,
                    first_last=first_last,first_roi_mean=roi_mean,first_roi_std=roi_std),
                gif=dict(path=gif.name,sha256=sha(gif),bytes=gif.stat().st_size,
                    crop=list(GIF_CROP),frames=count,duration_ms=duration,
                    nominal_fps=15,frame_selection='Every fourth actual full-display frame',
                    maximum_frame_mean_absolute_palette_error=max(quality,default=None),
                    transformations=['Crop','Shared 128-colour palette encoding'],
                    generated_motion=False,interpolated_frames=False),
                retained_pngs={name:sha(output/name) for name in
                    ('tv-first.png','tv-middle.png','tv-last.png')}), manifest


def capture(binary, project, output):
    lighting = prepare_capture_project(project)
    dx,dy,dz = [b-a for a,b in zip(EYE,TARGET)]
    yaw,pitch = math.atan2(dx,-dz),math.atan2(dy,math.hypot(dx,dz))
    record = ' '.join([SCENIC,*(format(v,'.17g') for v in EYE),
                       format(yaw,'.17g'),format(pitch,'.17g'),format(FOV,'.17g')])+'\n'
    (project/'showcase.views').write_text(record)
    raw = output/'raw-frames';raw.mkdir()
    final = output/'tv-final.bmp'
    environment, removed = native_environment()
    for key in ('VALWOOD_VILLAGE_LIGHTING','VALWOOD_OPENING_PROLOGUE',
                'VALWOOD_PLAYTEST_SLICE','VALWOOD_SHOWCASE','VALWOOD_QA_FIXED_TIME',
                'VALWOOD_FOLIAGE_SEQUENCE_FULL'):
        if key in environment:
            removed.append(key);environment.pop(key)
    environment.update(VALWOOD_PRESENTATION='1', VALWOOD_NPC_SEQUENCE=str(raw/'tv'))
    # The engine deliberately disables display sequence readbacks during
    # --profile. --capture alone still emits its full .world.json sidecar.
    command = [str(binary),'--project',str(project),'--play','--no-hud','--scenic',SCENIC,
               '--width',str(WIDTH),'--height',str(HEIGHT),'--auto-quality','off',
               '--render-scale','1','--frames',str(FRAMES),'--warmup','120',
               '--capture',str(final)]
    (output/'command.json').write_text(json.dumps(dict(command=command,
        removed_environment=sorted(set(removed)),
        environment={key:value for key,value in environment.items() if key.startswith('VALWOOD_')},
        camera=dict(eye=EYE,target=TARGET,yaw=yaw,pitch=pitch,fov=FOV,scenic=SCENIC),
        scope=SCOPE,profile_omitted='Existing capture sequence is disabled during FPS profiles'),indent=2)+'\n')
    (output/'inspection-look.json').write_text(json.dumps(lighting,indent=2)+'\n')
    start = time.monotonic()
    log_path = output/'render.log'
    with (ROOT/'projects/seldonne/.valwood-instance.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        with log_path.open('w') as log:
            try:
                code=subprocess.run(command,cwd=ROOT,env=environment,stdout=log,
                                    stderr=subprocess.STDOUT,timeout=600).returncode
            except subprocess.TimeoutExpired:
                code=-1
    world = read_json(Path(str(final)+'.world.json'))
    text = log_path.read_text(errors='replace')
    matches = re.findall(r'^\[World\] Renderer (.+)$',text,re.MULTILINE)
    renderer = matches[-1] if matches else ''
    failures=[]
    if code:failures.append('Native process exit '+str(code))
    if not renderer or any(name in renderer.lower() for name in ('llvmpipe','softpipe','swrast','software rasterizer')):
        failures.append('Hardware renderer missing')
    if world.get('scenic') != SCENIC:
        failures.append('Requested authored camera not selected')
    expected=dict(zip(('x','y','z'),EYE));expected.update(yaw=yaw,pitch=pitch)
    camera=world.get('camera_final',{})
    if any(not isinstance(camera.get(key),(float,int)) or abs(camera[key]-value)>1.e-4
           for key,value in expected.items()):
        failures.append('Actual camera differs from the authored TV inspection pose')
    if (project/'showcase.views').read_text()!=record:
        failures.append('Private authored camera/FOV record changed during capture')
    viewport=world.get('designer_viewport',{})
    if viewport.get('scene_width')!=WIDTH or viewport.get('scene_height')!=HEIGHT:
        failures.append('Scene not rendered at full native 1600x900 resolution')
    if world.get('gl_error')!=0 or world.get('remaining_texture_references')!=0:
        failures.append('GL error or unreleased texture references')
    if world.get('asset_streaming',{}).get('failed')!=0:
        failures.append('Asset streaming failure')
    look=world.get('lighting',{})
    for key,value in (('sun_strength',.25),('sky_fill',.85),('exposure',1.12),('visual_time',27.)):
        if not isinstance(look.get(key),(float,int)) or abs(look[key]-value)>1.e-5:
            failures.append('Unexpected final lighting/time '+key)
    if '[Display QA] Captured360 consecutive 1600x900 display pixels' not in text:
        failures.append('Native renderer did not confirm all full-display sequence readbacks')
    if not final.is_file():
        failures.append('Final native display capture missing')
    else:
        with Image.open(final) as frame:
            frame.load()
            if frame.size!=(WIDTH,HEIGHT):failures.append('Final capture has wrong dimensions')
            frame.convert('RGB').save(output/'tv-final.png')
        final.unlink()
    frames,manifest=encode_evidence(raw,output)
    failures.extend(frames.get('failures',[]))
    return dict(passed=not failures,failures=failures,scope=SCOPE,renderer=renderer,
                process_exit=code,elapsed_wall_seconds=round(time.monotonic()-start,3),
                actual_camera=camera,authored_camera=dict(eye=EYE,target=TARGET,yaw=yaw,pitch=pitch,fov=FOV),
                fov_evidence='Exact immutable private showcase.views record; engine sidecar has no separate FOV field',
                width=WIDTH,height=HEIGHT,render_scale=1.,frames=FRAMES,warmup_frames=120,
                gl_error=world.get('gl_error'),asset_streaming_failed=world.get('asset_streaming',{}).get('failed'),
                final_png_sha256=sha(output/'tv-final.png') if (output/'tv-final.png').is_file() else None,
                lighting=lighting,sequence=frames),manifest


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary',type=Path,required=True)
    parser.add_argument('--project',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--keep-raw',action='store_true',help='Retain all 360 BMPs after a passing GIF verification')
    args=parser.parse_args()
    binary=args.binary.resolve(strict=True)
    if args.project.is_symlink():parser.error('Source project must be an ordinary directory')
    source=args.project.resolve(strict=True);output=args.output.resolve()
    if output.is_relative_to(source):parser.error('Evidence must be outside the playable project')
    output.mkdir(parents=True,exist_ok=False)
    before=mutable_snapshot(source);binary_before=sha(binary)
    (output/'source-before.json').write_text(json.dumps(before,indent=2)+'\n')
    result=dict(schema='valwood.opening-home-tv-animation',version=1,scope=SCOPE,
                source=str(source),binary=str(binary),binary_sha256=binary_before)
    manifest=[]
    try:
        project=output/'project'
        inputs=isolate_project(source,project)
        (output/'isolated-inputs.json').write_text(json.dumps(inputs,indent=2)+'\n')
        result['native_capture'],manifest=capture(binary,project,output)
    except Exception as error:
        result['native_capture']=dict(passed=False,failures=[f'{type(error).__name__}: {error}'])
    result['binary_unchanged']=sha(binary)==binary_before
    after=mutable_snapshot(source)
    result['source_unchanged']=after==before
    (output/'source-after.json').write_text(json.dumps(after,indent=2)+'\n')
    result['passed']=result['native_capture']['passed'] and result['binary_unchanged'] and result['source_unchanged']
    removed=[]
    if result['passed'] and not args.keep_raw:
        # Delete only the exact raw files generated and hashed by this runner.
        # Preserve diagnostic raw frames if any capture/encoding/check failed.
        raw=output/'raw-frames'
        for item in manifest:
            path=raw/item['name']
            if path.is_symlink() or not path.is_file() or sha(path)!=item['sha256']:
                raise ValueError('Owned raw frame changed before cleanup: '+str(path))
            path.unlink();removed.append(item['name'])
        raw.rmdir()
    result['raw_cleanup']=dict(deleted_count=len(removed),retained_manifest='raw-frame-manifest.json',
                               policy='Only owned BMPs after successful verification; hashes and representative PNGs retained')
    (output/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
    return 0 if result['passed'] else 1


if __name__=='__main__':
    raise SystemExit(main())
