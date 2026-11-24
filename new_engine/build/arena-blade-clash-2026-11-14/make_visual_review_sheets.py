#!/usr/bin/env python3
"""Arrange final native captures for manual inspection; does not grant approval."""
import json
from pathlib import Path
from PIL import Image, ImageDraw

stage = Path(__file__).resolve().parent
review = stage / 'clash-review'
proof = json.loads((review / 'BLADE_CLASH_NATIVE.json').read_text())
assert proof['passed'] is True and len(proof['raw']['cases']) == 30
out = review / 'visual-review'
out.mkdir(exist_ok=True)
rows = proof['raw']['cases']
for group in range(5):
    sheet = Image.new('RGB', (1280, 6 * 205 + 30), (16, 21, 27))
    draw = ImageDraw.Draw(sheet)
    draw.text((5, 7), 'Actual native combat: before / contact / recoil / core recovery', fill='white')
    for row_index, row in enumerate(rows[group * 6:(group + 1) * 6]):
        y = 30 + row_index * 205
        for index, phase in enumerate(('before', 'contact', 'recoil', 'recovery')):
            path = review / 'native-cases' / (row['name'] + '-' + phase + '.png')
            with Image.open(path) as image:
                image = image.convert('RGB')
                image.thumbnail((320, 180))
                sheet.paste(image, (index * 320, y + 25))
            draw.text((index * 320 + 5, y + 5), row['name'] + ' / ' + phase, fill='white')
    path = out / ('native-combat-' + str(group + 1) + '.png')
    sheet.save(path)
    print(path)
