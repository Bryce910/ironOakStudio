#!/usr/bin/env python3
"""Refresh the read-only, exact battlefield publication delta after private art QA.

Retains the original baseline receipts rather than treating the current live
world as a new baseline. Never writes the live world or runtime manifest.
"""
import datetime
import importlib.util
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
N = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('publish_guards', N/'tools/publish_candidate.py')
g = importlib.util.module_from_spec(spec)
spec.loader.exec_module(g)


def info(path):
    g.plain_path(path, missing=True)
    if not path.exists():
        return {'path': str(path), 'exists': False, 'is_symlink': False,
                'resolved_path': str(path), 'bytes': None, 'sha256': None}
    data = g.read_regular(path)
    return {'path': str(path), 'exists': True, 'is_symlink': False,
            'resolved_path': str(path), 'bytes': len(data), 'sha256': g.sha(data)}


def records(lines):
    return [line for line in lines if line.strip() and not line.lstrip().startswith('#')]


plan = g.document(g.PLAN)
old = {entry['relative_path']: entry for entry in plan['files']}
entries = []
conflicts = []
for relative in sorted(g.SCENE):
    entry = old.get(relative, {'operation': 'create_regular_file',
                              'relative_path': relative, 'baseline': {'exists': False},
                              'scope': 'Static real-posed PBR battlefield corpse or terrain-conformed blood batch'})
    candidate = info(N/'world-project'/relative)
    if not candidate['exists']:
        raise g.Refusal('Missing candidate scene file: '+relative)
    entry['candidate'] = candidate
    entry['live_at_audit'] = info(g.LIVE/relative)
    baseline = entry['baseline']
    if baseline.get('exists'):
        # Baseline receipts are immutable. A live edit is reported, not absorbed.
        actual_baseline = info(Path(baseline['path']))
        if actual_baseline['sha256'] != baseline['sha256']:
            raise g.Refusal('Original baseline receipt changed: '+relative)
    current = entry['live_at_audit']
    same_original = current['exists'] == bool(baseline.get('exists')) and current.get('sha256') == baseline.get('sha256')
    same_candidate = current['exists'] and current['sha256'] == candidate['sha256']
    entry['status'] = ('already_applied' if same_candidate else
                       ('safe_compare_and_swap' if baseline.get('exists') else 'safe_create_if_absent')
                       if same_original else 'conflict')
    if entry['status'] == 'conflict':
        conflicts.append(relative)
    if relative.startswith('scenery/') and baseline.get('exists'):
        before = Path(baseline['path']).read_text().splitlines()
        after = (N/'world-project'/relative).read_text().splitlines()
        if len(before) != len(after) or len(records(before)) != len(records(after)):
            raise g.Refusal('Original source record count changed: '+relative)
        changes = []
        ordinal = -1
        for index, (a, b) in enumerate(zip(before, after)):
            if a.strip() and not a.lstrip().startswith('#'):
                ordinal += 1
            if a != b:
                changes.append({'relative_path': relative, 'line_1based': index+1,
                                'source_record_ordinal_0based': ordinal,
                                'alias': a.split()[0], 'baseline_line': a, 'candidate_line': b})
        entry.update(records_before=len(records(before)), records_after=len(records(after)), changed_lines=changes)
        entry['scope'] = f'{len(changes)} stable original source rows relocated; original ordinals retained (trees/shrubs and confirmed bridge-clearing props)'
    if relative == 'world.studio':
        before = Path(baseline['path']).read_text().splitlines()
        after = (N/'world-project'/relative).read_text().splitlines()
        changes = [{'line_1based': i+1, 'baseline_line': a, 'candidate_line': b}
                   for i, (a,b) in enumerate(zip(before, after)) if a != b]
        if changes != [{'line_1based': 27, 'baseline_line': 'structure_ids 5', 'candidate_line': 'structure_ids 6'}]:
            raise g.Refusal('Unexpected original authored world delta')
        entry.update(changed_existing_lines=changes, appended_lines=after[len(before):],
                     baseline_rows_retained_unchanged=len(before)-len(changes), entity_rows_unchanged=True)
        entry['scope'] = ('Append battlefield aliases, grounded wreckage/corpses/blood, local weather, mud/litter projections, '
                          'bounded smoke/fire lights and bridge; preserve original terrain/water/entities/source rows')
    if relative == 'scenery/battlefield_instances.txt':
        count = len(records((N/'world-project'/relative).read_text().splitlines()))
        entry['scope'] = f'{count} appended source prop records, including12 real posed static corpses'
        entry['records_after'] = count
    entries.append(entry)

relocated = sum(len(e.get('changed_lines', [])) for e in entries)
bridge_receipt = N/'review/battlefield_bridge_prop_relocations.json'
bridge_moves = g.document(bridge_receipt).get('relocations', []) if bridge_receipt.exists() else []
for move in bridge_moves:
    actual = (N/'world-project'/move['relative_path']).read_text().splitlines()[move['line_1based']-1]
    if actual != move['candidate_line']:
        raise g.Refusal('Bridge relocation receipt differs from candidate')
foliage_receipt = N/'review/bridge-foliage-audit/RELOCATED.json'
foliage_data = g.document(foliage_receipt) if foliage_receipt.exists() else {}
foliage_moves = [move for move in foliage_data.get('relocations', []) if move['relative_path'].startswith('world-project/scenery/')]
for move in foliage_moves:
    actual = (N/move['relative_path']).read_text().splitlines()[move['line_1based']-1]
    if actual != move['candidate_line']:
        raise g.Refusal('Bridge foliage relocation receipt differs from candidate')
expected_relocated = 45 + len(bridge_moves) + len(foliage_moves)
if relocated != expected_relocated:
    raise g.Refusal(f'Expected{expected_relocated} relocated source rows; found{relocated}')
props = next(e['records_after'] for e in entries if e['relative_path']=='scenery/battlefield_instances.txt')
plan['files'] = entries
plan['created_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
plan['source_status'] = 'Scene/audio headers frozen after CPU tests; private art adopted. Native visual/performance acceptance remains required.'
plan['snapshot_result'] = {'replace_files': sum(bool(e['baseline'].get('exists')) for e in entries),
                           'create_files': sum(not e['baseline'].get('exists') for e in entries),
                           'total_files': len(entries), 'relocated_source_records': relocated,
                           'new_prop_records': props, 'tree_shrub_relocations': 45, 'bridge_obstruction_relocations': len(bridge_moves), 'bridge_foliage_relocations': len(foliage_moves), 'runtime_generated_foliage_relocations': foliage_data.get('generated_relocations', 0), 'conflicts': conflicts, 'conflict_count': len(conflicts),
                           'safe_only_while_hashes_match': True,
                           'bytes_to_stage': sum(e['candidate']['bytes'] for e in entries)}
plan['source_identity_notes'][0] = (f'All nine original source files retain their counts/order; exactly{relocated} rows change only UV root and optional explicit Y:45 trees/shrubs,{len(bridge_moves)} bridge obstructions and{len(foliage_moves)} authored deck intrusions.')
plan['source_identity_notes'][1] = (f'The new battlefield source appends{props} records after the nine original files. '
                                   'Later optional sources may shift global indices; production persistent-source reconciliation is required.')
for item in plan.get('preserve_without_copy', []):
    relative = item['relative_path']
    live = g.LIVE/relative
    candidate = N/'world-project'/relative
    # These paths may intentionally resolve shared dependencies, but are never published.
    if live.is_file():
        item['live_sha256'] = g.sha(live.read_bytes())
        item['candidate_equals_live'] = candidate.is_file() and candidate.read_bytes() == live.read_bytes()
replaced=sum(bool(e['baseline'].get('exists'))for e in entries)
created=len(entries)-replaced
glbs=sum(e['relative_path'].endswith('.glb')for e in entries)
plan['transaction_steps'][3]['action'] = (f'Create timestamped backups of the {replaced} replaced scene files and active runtime manifest; '
                                         f'journal {created} new scene paths, preserving exact bytes and modes.')
plan['transaction_steps'][4]['action'] = (f'Stage exactly {len(entries)} regular scene files; verify hashes, parse all source definitions, '
                                         f'validate {glbs} GLBs and actual terrain/bridge in a disposable publication clone.')
plan['transaction_steps'][6]['action'] = (f'With no writer processes, atomically rename each staged file: {glbs} self-contained GLBs first, '
                                         f'then source replacements and {props}-prop source/settings, world.studio last. '
                                         'Maintain rollback journal; the runtime manifest is committed only after complete asset verification.')
plan['evidence'] = list(dict.fromkeys(plan['evidence'] + [str(N/'review/art-proposal-v2/CANDIDATE_ADOPTED.json'),
                                                        str(N/'review/art-proposal-v2/OFFLINE_CHECKS.json'), str(N/'review/battlefield_bridge_prop_relocations.json'),str(N/'review/bridge-foliage-audit/RELOCATED.json')]))
plan['runtime_overlay'] = ({'path': foliage_data['new_runtime_overlay'], 'sha256': foliage_data['overlay_sha256'], 'publish': False, 'reason': 'Private runtime dependency retained within candidate generation; original shared parent remains unchanged', 'ready_inputs_must_bind': True} if foliage_data else None)
g.receipt(g.PLAN, plan)
print(json.dumps(plan['snapshot_result']))
