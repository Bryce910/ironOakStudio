#!/usr/bin/env python3
"""Publish exact recovered fixture sockets to the bounded native source fire pool."""
from pathlib import Path
import argparse, collections, hashlib, json, math

ROOT = Path(__file__).resolve().parents[1]
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT/'docs/stronghold/2026-11-01/visual-finish/flame-candidate/flames.json')
    parser.add_argument('--project', type=Path, default=ROOT/'projects/valwood_showcase')
    args = parser.parse_args()
    source = args.source.resolve(); project = args.project.resolve()
    emitters = json.loads(source.read_text())['all_emitters']
    assert 0 < len(emitters) <= 240, 'Keep sixteen of256 renderer slots for Studio overlays'
    rows = ['VALWOOD_SOURCE_FLAMES 1', '# Exact publisher fuel-mouth positions; visual flames and sound; actor-only practicals preserve static baked lights.']
    for emitter in emitters:
        x, y, z = emitter['native_position']
        height, width, intensity = (emitter[key] for key in ('height', 'width', 'intensity'))
        assert all(math.isfinite(v) for v in (x, y, z, height, width, intensity))
        assert abs(x) <= 50400 and abs(z) <= 28400 and -2000 <= y <= 12000
        assert .05 <= height <= 4 and .02 <= width <= 2 and 0 <= intensity <= 8
        assert 0 <= emitter['seed'] <= 0xffffffff and emitter['dynamic_light_power'] == 0
        rows.append(' '.join(format(v, '.12g') for v in (x, y, z, height, width, intensity)) + f" {emitter['seed']}")
    output = project/'showcase.flames'
    output.write_text('\n'.join(rows)+'\n')
    report = dict(source=str(source), source_sha256=digest(source), output=str(output), output_sha256=digest(output),
                  emitters=len(emitters), maximum=240, renderer_capacity=256, studio_overlay_slots_reserved=16,
                  architecture_dynamic_lights=0, actor_practical_limit=8, actor_practical_radius_m=12,
                  actor_practical_power_per_visual_intensity=4,
                  actor_practical_gate='explicit uActorLighting hero/resident draws; restore ordinary/editor light set after actors',
                  by_family=dict(collections.Counter(e['family'] for e in emitters)),
                  maximum_light_validation_error_m=max(e['point_light_validation_error_m'] for e in emitters))
    output.with_suffix('.flames.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))

if __name__ == '__main__':
    main()
