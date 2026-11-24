#!/usr/bin/env python3
"""Adapt old arena capture checks to explicit attack/defense presentation cues.

This is a capture-parser adapter only. It neither launches the old full-game
runner nor changes captured production evidence or its original source tool.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
N = Path(__file__).resolve().parent
TOOLS = N.parents[1] / 'tools'
sys.path.insert(0, str(TOOLS))
spec = importlib.util.spec_from_file_location('original_arena_capture_checks', TOOLS / 'check_combat_arena.py')
original = importlib.util.module_from_spec(spec)
spec.loader.exec_module(original)

def capture_failures(name, state):
    adapted = dict(state)
    errors = []
    # Validate the real split-channel field first. The alias below merely
    # feeds the untouched unrelated legacy blood/collision/purse assertions.
    if name == 'body-hit':
        if state.get('attack_cue_kind') != 2:
            errors.append('new outgoing body cue missing')
        adapted['contact_kind'] = 1 if state.get('attack_cue_kind') == 2 else 0
    elif name == 'metal-block':
        if state.get('attack_cue_kind') != 5:
            errors.append('new outgoing enemy-block cue missing')
        adapted['contact_kind'] = 3 if state.get('attack_cue_kind') == 5 else 0
    elif name == 'metal-parry':
        if state.get('defense_cue_kind') != 6:
            errors.append('new incoming parry cue missing')
        adapted['contact_kind'] = 4 if state.get('defense_cue_kind') == 6 else 0
        # A separate simultaneous outgoing body notice can retain its damage;
        # the incoming parry is nondamaging regardless of that other channel.
        adapted['damage_cue'] = 0
    return errors + original.capture_failures(name, adapted)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('captures', type=Path, nargs='?')
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        # Check exact migration decisions while retaining all other old
        # validation failures; this does not certify a native capture.
        for name, field, kind in [('body-hit', 'attack_cue_kind', 2),
                                  ('metal-block', 'attack_cue_kind', 5),
                                  ('metal-parry', 'defense_cue_kind', 6)]:
            missing = capture_failures(name, {})
            present = capture_failures(name, {field: kind, 'damage_cue': 17 if name == 'body-hit' else 0})
            assert any('new ' in item for item in missing)
            assert not any('new ' in item for item in present)
            assert len(present) > 0  # unrelated actual renderer checks survive
            assert not any('contact cue incorrect' in item for item in present)
        print(json.dumps({'passed': True, 'adapter_cases': 3, 'actual_native_acceptance': False}))
        return
    if args.captures is None:
        parser.error('Supply a legacy arena capture directory or --self-test')
    failures = {}
    for path in sorted(args.captures.glob('*.json')):
        state = json.loads(path.read_text())
        name = state.get('name', path.stem)
        failures[str(path)] = capture_failures(name, state)
    print(json.dumps({'passed': bool(failures) and not any(failures.values()),
                      'captures': failures}, indent=2))
    sys.exit(0 if failures and not any(failures.values()) else 1)

if __name__ == '__main__':
    main()
