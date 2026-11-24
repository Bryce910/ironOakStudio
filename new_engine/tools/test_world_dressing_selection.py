#!/usr/bin/env python3
"""Differential CPU test of actual viewer Dressing/GrassGPU implementations."""
from pathlib import Path
import argparse
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--benchmark', action='store_true', help='Time isolatedCPU controls after correctness checks; never usesGL.')
    args = parser.parse_args()
    source = (ROOT/'engine/world_viewer.cpp').read_text()

    def extract(name):
        start = source.index('    struct '+name+' {')
        depth = 0
        for at in range(source.index('{', start), len(source)):
            depth += (source[at] == '{') - (source[at] == '}')
            if not depth:
                return source[start:at+2]
        raise AssertionError(name)

    grass, dressing = extract('GrassGPU'), extract('Dressing')
    dressing = dressing.replace('        WorldCover *cover = nullptr;', '        std::array<std::vector<std::pair<double,size_t>>,24> observed;\n        WorldCover *cover = nullptr;')
    reserve = '            groups.push_back(std::move(g));'
    assert dressing.count(reserve) == 1
    dressing = dressing.replace(reserve, '            observed[groups.size()].reserve(kind==WORLD_COVER_GRASS?WorldGrassTransition::max_instances:kind==WORLD_COVER_TREES?WORLD_COVER_TREE_CAP:kind==WORLD_COVER_ROCKS?WORLD_COVER_ROCK_CAP:WORLD_COVER_FERN_CAP);\n'+reserve)
    cap = '                if(record_stats&&g.kind==WORLD_COVER_GRASS)peak_scanned_group_candidates='
    assert dressing.count(cap) == 1
    dressing = dressing.replace(cap, '                if(capture_candidates)observed[std::size_t(&g-groups.data())].assign(g.selected.begin(),g.selected.end());\n'+cap)

    def variant(prefix, revision, dispatch):
        g = grass.replace('struct GrassGPU {', 'struct '+prefix+'GrassGPU {', 1)
        d = dressing.replace('struct Dressing {', 'struct '+prefix+'Dressing {', 1).replace('        GrassGPU grass;', '        '+prefix+'GrassGPU grass;')
        if not revision:
            condition = 'if(!has_classification||classification_revision!=root_revision||procedural_keep.size()!=f.count)'
            assert g.count(condition) == 1
            g = g.replace(condition, 'if(root_revision||procedural_keep.size()!=f.count)')
            d = d.replace('cam,grass_transition.structure_revision());', 'cam,grass_changed);')
        if not dispatch:
            before = '            auto point_visible='
            assert d.count(before) == 1
            d = d.replace(before, '            tree_dispatch=false;grass_dispatch=false;\n'+before)
        return g+'\n'+d

    variants = '\n'.join(variant(prefix, revision, dispatch) for prefix, revision, dispatch in
        [('', True, True), ('Legacy', False, False), ('Revision', True, False), ('Dispatch', False, True)])
    with tempfile.TemporaryDirectory(prefix='valwood-dressing-selection-') as directory:
        tmp = Path(directory)
        (tmp/'dressing_selection_viewer.inc').write_text(variants)
        obj = tmp/'atlas.o'
        subprocess.run(['cc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-I'+str(ROOT), '-c', str(ROOT/'engine/world_atlas.c'), '-o', str(obj)], check=True)
        command = ['c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror', '-I'+str(ROOT), '-I'+str(tmp), str(ROOT/'tests/world_dressing_selection_test.cpp'), str(ROOT/'engine/world_cover.cpp'), str(obj), '-lm', '-o', str(tmp/'test')]
        subprocess.run(command, check=True)
        subprocess.run([str(tmp/'test')] + (['--benchmark'] if args.benchmark else []), check=True)


if __name__ == '__main__':
    main()
