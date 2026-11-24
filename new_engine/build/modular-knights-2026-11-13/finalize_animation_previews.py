#!/usr/bin/env python3
"""Label actual controller captures; record final source and asset provenance."""
import csv
import hashlib
import json
import subprocess
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASE = Path(__file__).resolve().parent
REVIEW = BASE / 'animation-review-v3'
ASSET = ROOT / 'assets/purchased/modular_knights/derived/polish-v3/modular_knight_hero_v3.glb'
SCENARIOS = ('locomotion', 'combat', 'combat-moving', 'combat-running', 'guard', 'guard-switch', 'run-entries', 'reaction', 'steering')

def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def stamp(seconds):
    milliseconds = round(seconds * 1000)
    return f'{milliseconds//3600000:02}:{milliseconds//60000%60:02}:{milliseconds//1000%60:02},{milliseconds%1000:03}'

def main():
    from PIL import Image, ImageDraw, ImageFont
    title = REVIEW / 'preview-title.txt'
    title.write_text('Knight animation controller preview | Neutral lighting | Scripted inputs\n')
    asset_bytes = ASSET.read_bytes()
    json_length, json_tag = struct.unpack_from('<II', asset_bytes, 12)
    assert json_tag == 0x4e4f534a
    asset_document = json.loads(asset_bytes[20:20+json_length])
    primitives = sum(len(mesh.get('primitives', [])) for mesh in asset_document['meshes'])
    results = {}
    for scenario in SCENARIOS:
        folder = REVIEW / scenario
        rows = list(csv.DictReader((folder / 'controller-trace.csv').open()))
        fps = 30
        labels = []
        start = 0
        for index in range(1, len(rows) + 1):
            if index == len(rows) or rows[index]['label'] != rows[start]['label']:
                labels.append(f'{len(labels)+1}\n{stamp(start/fps)} --> {stamp(index/fps)}\n{rows[start]["label"]}\n')
                start = index
        subtitles = folder / 'labels.srt'
        subtitles.write_text('\n'.join(labels))
        output = REVIEW / f'knight-{scenario}-preview.mp4'
        filters = f'drawtext=fontfile=/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf:textfile={title}:fontsize=17:fontcolor=white:box=1:boxcolor=black@0.65:x=18:y=18,subtitles={subtitles}:force_style=\'FontSize=19,MarginV=25,Outline=1\''
        subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y', '-framerate', str(fps), '-i', str(folder / 'frame-%06d.png'), '-vf', filters, '-c:v', 'libx264', '-crf', '19', '-pix_fmt', 'yuv420p', '-movflags', '+faststart', str(output)], check=True)
        indexes = [round(i*(len(rows)-1)/11) for i in range(12)]
        if scenario == 'guard-switch':
            indexes = [round(t*fps) for t in (.9, 1.1, 1.23, 2.2, 2.33, 3.3, 3.43, 4.4, 4.53, 6.6, 6.73, 7.83)]
        sheet = Image.new('RGB', (4*384, 3*320), '#1b1b1b')
        draw = ImageDraw.Draw(sheet)
        font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 13)
        for tile, index in enumerate(indexes):
            frame = Image.open(folder / f'frame-{index:06}.png').convert('RGB')
            frame.thumbnail((384, 288))
            x, y = tile%4*384, tile//4*320
            sheet.paste(frame, (x, y))
            draw.text((x+4,y+289), f'{index/fps:.2f}s {rows[index]["label"]}'[:50], font=font, fill='white')
        contact_sheet = REVIEW / f'{scenario}-review.jpg'
        sheet.save(contact_sheet, quality=94)
        log = (REVIEW / f'{scenario}.log').read_text()
        expected = f'frames={len(rows)} uploads={len(rows)} draws={len(rows)*primitives} failures=0'
        if expected not in log:
            raise RuntimeError(f'{scenario}: GPU accounting mismatch')
        results[scenario] = {'frames':len(rows), 'fps':fps, 'gpu_failures':0, 'video':str(output), 'video_sha256':digest(output), 'sheet':str(contact_sheet), 'log':str(REVIEW/f'{scenario}.log')}
    dependencies = [ASSET, ROOT/'tools/preview_knight_controller.cpp', ROOT/'tools/preview_hero_skeletal.cpp', BASE/'source-v1/engine/world_skeletal_hero.cpp', BASE/'source-v1/engine/world_skeletal_hero.h', BASE/'source-v1/engine/world_animation.cpp', BASE/'source-v1/engine/world_animation_render.h', BASE/'source-v1/engine/world_animation_render.cpp', ROOT/'shaders/world_skeletal.vs.glsl', ROOT/'shaders/hero_preview.fs.glsl', ROOT/'build/knight-controller-preview/preview_knight_controller']
    receipt = {'schema':'valwood.knight-animation-controller-preview.v1','asset':str(ASSET),'sha256':{str(p):digest(p) for p in dependencies},'scenarios':results,'limits':['Scripted resolved-travel and combat inputs, not an interactive gameplay rehearsal.','Neutral preview lighting and offscreen software OpenGL; no hardware FPS claim.','Visual review is recorded separately.']}
    (REVIEW/'PREVIEW_RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps({'videos':len(results),'frames':sum(v['frames'] for v in results.values()),'receipt':str(REVIEW/'PREVIEW_RECEIPT.json')}))

if __name__ == '__main__':
    main()
