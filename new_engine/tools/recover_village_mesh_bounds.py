#!/usr/bin/env python3
"""Read exact tagged ExtendedBounds from the owned UE4.26 static meshes."""
from pathlib import Path
import argparse
import hashlib
import json
import math
import struct
from village_source_registry import SourcePackage, SOURCE


def bounds(path):
    package = SourcePackage(path)
    entry = next(e for e in package.exports if e['name'] == path.stem)
    package.pos = entry['offset']
    while (tag := package.header_tag()) is not None:
        if tag['name'] != 'ExtendedBounds':
            package.pos = tag['start'] + tag['size']
            continue
        assert tag['meta'] == 'BoxSphereBounds'
        end = tag['start'] + tag['size']
        values = {}
        while (child := package.header_tag()) is not None:
            name = child['name']
            if name in ('Origin', 'BoxExtent'):
                assert child['meta'] == 'Vector' and child['size'] == 12
                values[name] = list(struct.unpack_from('<3f', package.data, child['start']))
            elif name == 'SphereRadius':
                assert child['kind'] == 'FloatProperty' and child['size'] == 4
                values[name] = struct.unpack_from('<f', package.data, child['start'])[0]
            else:
                raise ValueError('Unexpected source bounds field: ' + name)
            package.pos = child['start'] + child['size']
        defaulted = []
        if 'Origin' not in values:
            # A centred mesh omits the zero-valued reflected FVector. Keep
            # this native default explicit in the provenance record.
            values['Origin'] = [0., 0., 0.]
            defaulted.append('Origin')
        assert package.pos == end and set(values) == {'Origin', 'BoxExtent', 'SphereRadius'}
        assert all(math.isfinite(v) for row in ('Origin', 'BoxExtent') for v in values[row])
        assert min(values['BoxExtent']) >= 0 and math.isfinite(values['SphereRadius'])
        lo = [c - e for c, e in zip(values['Origin'], values['BoxExtent'])]
        hi = [c + e for c, e in zip(values['Origin'], values['BoxExtent'])]
        return dict(source=str(path.resolve()), source_sha256=hashlib.sha256(package.data).hexdigest(),
                    bounds_unreal_cm=[lo, hi], defaulted_zero_fields=defaulted, **values)
    raise ValueError('No authored ExtendedBounds: ' + str(path))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=SOURCE / 'Meshes')
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[2] / '.local-tools/village-source/mesh-bounds.json')
    args = parser.parse_args()
    meshes, errors = {}, []
    for path in sorted(args.source.glob('SM_*.uasset')):
        try:
            meshes[path.stem] = bounds(path)
        except (AssertionError, ValueError, StopIteration) as error:
            errors.append(dict(path=str(path), error=str(error)))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(dict(coordinates='Unreal local X/Y/Z centimetres',
        source_executed=False, source_unmodified=True, meshes=meshes, errors=errors), indent=2) + '\n')
    print(json.dumps(dict(meshes=len(meshes), errors=errors, output=str(args.output)), indent=2))
    if errors:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
