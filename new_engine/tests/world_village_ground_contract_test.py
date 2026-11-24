#!/usr/bin/env python3
"""Purchased ground source channel contract, independent of shader formulas.

The recovered UE TextureSource stores BGRA bytes in its PNG payload. Runtime
images must be normalized to ordinary RGB before either sRGB decoding or normal
map upload. Checking the actual fixed source tile catches a plausible-looking
but wrong blue albedo / sideways normal / zero-AO channel swap.
"""
import argparse
import hashlib
import json
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
EXPECTED_REFLECTANCE = (.16384777, .12907667, .09379962)


def channels(path):
    with Image.open(path) as image:
        assert image.size == (2048, 2048), (path, image.size)
        image = image.convert("RGB")
        count = image.width * image.height
        histograms = image.histogram()
    result = []
    for channel in range(3):
        bins = histograms[channel * 256:(channel + 1) * 256]
        mean = sum(value * amount for value, amount in enumerate(bins)) / (255 * count)
        result.append((bins, mean, count))
    return result


def linear(value):
    return value / 12.92 if value <= .04045 else ((value + .055) / 1.055) ** 2.4


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ground", type=Path, default=ROOT / "projects/valwood_world/ground")
    args = parser.parse_args()
    for kind in ("BC", "N", "ARM"):
        # Compare runtime aliases against preserved, unmodified TextureSource
        # storage bytes, rather than inferring a convention from channel means.
        raw_path = ROOT / "assets/purchased/medieval_village/source_storage_png" / f"T_SoilStoneMix_{kind}.png"
        with Image.open(raw_path) as raw, Image.open(args.ground / f"village_stone_{kind}.png") as runtime:
            red, green, blue, alpha = raw.convert("RGBA").split()
            expected = Image.merge("RGBA", (blue, green, red, alpha))
            assert runtime.convert("RGBA").tobytes() == expected.tobytes(), ("Exact BGRA-to-RGBA source decode", kind)
    albedo = channels(args.ground / "village_stone_BC.png")
    means = tuple(sum(linear(value / 255) * amount for value, amount in enumerate(bins)) / count
                  for bins, _, count in albedo)
    for actual, expected in zip(means, EXPECTED_REFLECTANCE):
        assert abs(actual - expected) < 1e-7, ("Source albedo must be normalized RGB before linear mean", means)

    normal = channels(args.ground / "village_stone_N.png")
    assert normal[2][1] > .85 and normal[2][1] > normal[0][1] + .25, ("Source normal Z must be blue", [c[1] for c in normal])
    assert abs(normal[0][1] - .5) < .15 and abs(normal[1][1] - .5) < .15
    arm = channels(args.ground / "village_stone_ARM.png")
    assert arm[0][1] > .85, ("Source AO must be red, not zero metallic", [c[1] for c in arm])
    assert .8 < arm[1][1] < 1, "Source roughness must remain green"
    assert sum(arm[2][0][1:]) == 0, "Nonmetallic source ground must have zero blue metallic channel"

    provenance = json.loads((args.ground / "village_material_provenance.json").read_text())
    for entry in provenance["maps"]:
        path = args.ground / Path(entry["runtime"]).name
        assert hashlib.sha256(path.read_bytes()).hexdigest() == entry["sha256"], path
    print("PASS purchased ground source: normalized RGB albedo, linear reflectance "
          f"{means}, blue normal Z, red AO/green roughness/zero blue metal, provenance hashes")


if __name__ == "__main__":
    main()
