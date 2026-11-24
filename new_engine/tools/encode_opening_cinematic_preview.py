#!/usr/bin/env python3
"""Encode a silent preview from actual, timed opening display readbacks.

Only resizing and video/palette encoding are applied. No generated frames,
interpolation, camera synthesis or replacement artwork is used.
"""
from pathlib import Path
import argparse
import hashlib
import json
import math
import subprocess

from PIL import Image, ImageSequence


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def encode(film):
    film=Path(film).resolve();raw=film/'raw'
    records=[json.loads(line) for line in (raw/'manifest.jsonl').read_text().splitlines()]
    if len(records)!=240 or {r['shot_index'] for r in records}!={0,1,2,3}:
        raise ValueError('Expected all four shots and 240 actual display frames')
    times=[r['story_seconds'] for r in records]
    if any(abs((b-a)-1/15)>1e-5 for a,b in zip(times,times[1:])):
        raise ValueError('Readback story clock is not a continuous 15 FPS sequence')
    if times[0]>.04 or abs((times[-1]-times[0])-239/15)>1e-5:
        raise ValueError('Readback clock does not cover the complete entrance')
    manifest=[];frames=[]
    for index,record in enumerate(records):
        name=f'entry-{index:04d}.bmp'
        if record['file']!=name:raise ValueError('Unexpected readback filename/order')
        path=raw/name
        with Image.open(path) as source:
            source.load()
            if source.size!=(1600,900):raise ValueError('Unexpected native display dimensions')
            rgb=source.convert('RGB')
            frame=rgb.resize((640,360),Image.Resampling.LANCZOS)
        frames.append(frame)
        manifest.append({**record,'sha256':sha(path),'bytes':path.stat().st_size})

    tiles=Image.new('RGB',(160*12,90*math.ceil(len(frames)/12)))
    for index,frame in enumerate(frames):
        tiles.paste(frame.resize((160,90),Image.Resampling.BOX),((index%12)*160,(index//12)*90))
    palette=tiles.quantize(colors=128,method=Image.Quantize.MEDIANCUT)
    encoded=[frame.quantize(palette=palette,dither=Image.Dither.NONE) for frame in frames]
    gif=film/'apartment-entry.gif'
    encoded[0].save(gif,save_all=True,append_images=encoded[1:],loop=0,
                    duration=[70,60,70]*80,disposal=2,optimize=False)
    durations=[]
    with Image.open(gif) as check:
        for frame in ImageSequence.Iterator(check):durations.append(frame.info.get('duration',0))
        if check.size!=(640,360) or sum(durations)!=16000:
            raise ValueError('GIF verification failed')

    video=film/'apartment-entry.mp4'
    command=['ffmpeg','-y','-v','error','-threads','3','-framerate','15',
             '-i',str(raw/'entry-%04d.bmp'),'-frames:v','240','-vf','scale=1280:720:flags=lanczos',
             '-c:v','libx264','-preset','medium','-crf','19','-pix_fmt','yuv420p',
             '-an','-movflags','+faststart',str(video)]
    subprocess.run(command,check=True,timeout=180)
    probe=json.loads(subprocess.check_output(['ffprobe','-v','error','-show_streams','-show_format',
                                             '-of','json',str(video)],text=True))
    streams=probe['streams'];videos=[s for s in streams if s['codec_type']=='video']
    if len(streams)!=1 or len(videos)!=1 or videos[0]['width']!=1280 or videos[0]['height']!=720 or int(videos[0]['nb_frames'])!=240 or abs(float(probe['format']['duration'])-16)>0.01:
        raise ValueError('Silent MP4 verification failed')
    for label,index in [('entry',24),('bills',72),('television',138),('handoff',239)]:
        frames[index].save(film/f'preview-{label}.png')
    (film/'raw-frame-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    result=dict(passed=True,scope='Actual native display readbacks, silent preview; capture is not an FPS benchmark',
        actual_frames=len(records),story_clock=dict(first=times[0],last=times[-1],sample_fps=15,playback_seconds=16),
        generated_motion=False,interpolated_frames=False,
        transformations=['Whole-frame Lanczos resizing','Shared palette GIF encoding','H.264 video encoding'],
        gif=dict(path=str(gif),sha256=sha(gif),bytes=gif.stat().st_size,dimensions=[640,360],duration_ms=16000,frames=len(durations)),
        video=dict(path=str(video),sha256=sha(video),bytes=video.stat().st_size,dimensions=[1280,720],frames=240,duration_seconds=16,audio=False),
        raw_cleanup='Raw files retained until acceptance and preview verification complete')
    (film/'preview-results.json').write_text(json.dumps(result,indent=2)+'\n')
    return result


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--film',required=True,type=Path)
    args=parser.parse_args();print(json.dumps(encode(args.film),indent=2))
