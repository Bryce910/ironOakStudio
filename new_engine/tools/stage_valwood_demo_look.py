#!/usr/bin/env python3
"""Stage an authored demo look without modifying the player's saved world."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT/'projects/valwood_world'
SOURCE = ROOT/'assets/environments/polyhaven_overcast_soil/overcast_soil_puresky_4k.hdr'
RUNTIME = 'environments/overcast_soil_puresky_4k.hdr'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--copy-hdr', action='store_true', help='Copy an unused immutable HDR revision into the project.')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    original = (PROJECT/'world.studio').read_text()
    source_hash = hashlib.sha256(original.encode()).hexdigest()
    irradiance = json.loads((ROOT/'docs/studio/surface-finish-2026-10-29/hdr-irradiance.json').read_text())
    provenance = json.loads((SOURCE.parent/'PROVENANCE.json').read_text())
    assert hashlib.sha256(SOURCE.read_bytes()).hexdigest() == provenance['sha256']
    if args.copy_hdr:
        target = PROJECT/RUNTIME
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.exists():
            assert target.read_bytes() == SOURCE.read_bytes()
        else:
            shutil.copy2(SOURCE, target)
        shutil.copy2(SOURCE.parent/'LICENSE.txt', target.parent/'overcast_soil_puresky_LICENSE.txt')
        shutil.copy2(SOURCE.parent/'PROVENANCE.json', target.parent/'overcast_soil_puresky_PROVENANCE.json')
    variants = {}
    for name, sky in [('cloudy-soft', 1.35), ('cloudy-readable', 1.50)]:
        lines = original.splitlines()
        old = next(line for line in lines if line.startswith('look '))
        values = old.split()[1:]
        # Preserve sharpness, rain, wind and fog; change only lighting design.
        for index, value in {1:.85, 2:sky, 3:1.08, 4:16.1, 5:-20, 6:.55}.items():
            values[index] = str(value)
        new = 'look '+' '.join(values)
        lines = [new if line == old else line for line in lines]
        no_hdr = '\n'.join(line for line in lines if not line.startswith('environment '))+'\n'
        (args.output/(name+'-analytic.world.studio')).write_text(no_hdr)
        # Match the panorama's upward irradiance to the authored analytic sky.
        base_rgb = [.010+.19+.065*.55, .014+.245+.030*.55, .023+.325-.015*.55]
        luminance = sum(value*weight for value, weight in zip(base_rgb, [.2126,.7152,.0722]))*sky
        intensity = luminance/irradiance['up_luminance']
        assert 0 < intensity <= 8
        hdr = no_hdr+f'environment {RUNTIME} -20 {intensity:.9f} 0.72 1\n'
        target = args.output/(name+'-hdr.world.studio')
        target.write_text(hdr)
        # No geometry, placed props, surfaces or material rows may change.
        stable = lambda text: [line for line in text.splitlines() if not line.startswith(('look ', 'environment '))]
        assert stable(hdr) == stable(original)
        variants[name] = dict(sky_fill=sky, sun=.85, hour=16.1, bearing=-20, cloud=.55,
                              exposure=1.08, hdr_intensity=intensity, hdr_blend=.72,
                              target_up_irradiance_over_pi=luminance, staged_document=target.name)
    report = dict(runtime_world_unchanged=hashlib.sha256((PROJECT/'world.studio').read_bytes()).hexdigest() == source_hash,
                  runtime_applied=False, original_world_sha256=source_hash,
                  asset=provenance, irradiance=irradiance, variants=variants,
                  notes=['Private comparison documents; no published look change.',
                         'HDR normalization preserves an explicit energy reference rather than compensating with a post-process shadow floor.',
                         'Jungle needs the compiled biome environment_gain before final acceptance.',
                         'The existing procedural clouds still composite above the HDR background; review for double cloud appearance.'])
    (args.output/'look-stage.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(dict(staged=True, runtime_applied=False, variants=variants), indent=2))


if __name__ == '__main__':
    main()
