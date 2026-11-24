#!/usr/bin/env python3
"""Verify house clipboard shortcuts, snapped rows and undo on the native GPU."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
from PIL import Image
from benchmark_graphics_settings import ROOT, PROJECT, LAUNCHER, native_environment
from demo_qa_state import protected_state


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--base', type=Path)
    parser.add_argument('--recovery-draft', type=Path)
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    before = protected_state()
    def recovery_state():
        return {p.name:hashlib.sha256(p.read_bytes()).hexdigest()
                for p in sorted(PROJECT.glob('world.studio.recovery*')) if p.is_file()}
    recovery_before=recovery_state()
    binary = hashlib.sha256((ROOT / 'rpg').read_bytes()).hexdigest()
    with tempfile.TemporaryDirectory(prefix='valwood-editor-clipboard-') as folder:
        document = Path(folder) / 'world.studio'
        shutil.copy2(args.base or PROJECT / 'world.studio', document)
        env, _ = native_environment()
        env.update(VALWOOD_QA_STUDIO_DOCUMENT=str(document),
                   VALWOOD_QA_EDITOR_COPY_PASTE='1')
        if args.recovery_draft:
            private_draft=Path(folder)/'rejected-draft.world.studio'
            shutil.copy2(args.recovery_draft,private_draft)
            env['VALWOOD_QA_EDITOR_COPY_PASTE_RECOVERY']=str(private_draft)
        profile = out / 'profile.json'
        capture = out / 'street.bmp'
        command = [str(LAUNCHER), '--editor', '--scenic', 'castle-finished-arcade',
                   '--frames', '1100', '--warmup', '0', '--width', '1600',
                   '--height', '900', '--auto-quality', 'off', '--profile',
                   str(profile), '--capture', str(capture)]
        with (out / 'native.log').open('w') as log:
            code = subprocess.run(command, cwd=ROOT, env=env, stdout=log,
                                  stderr=subprocess.STDOUT, timeout=240).returncode
        sidecar = Path(str(profile) + '.world.json')
        world = json.loads(sidecar.read_text()) if sidecar.exists() else {}
        metrics = json.loads(profile.read_text()) if profile.exists() else {}
        renderer = metrics.get('renderer', '')
        native = bool(renderer) and not any(x in renderer.lower() for x in ('llvmpipe', 'softpipe', 'swrast'))
        passed = (code == 0 and native and
                  '[Editor clipboard QA] PASS ' in (out / 'native.log').read_text() and
                  world.get('gl_error') == 0 and
                  world.get('remaining_texture_references') == 0 and
                  before == protected_state() and recovery_before == recovery_state())
        result = dict(passed=passed, exit=code, binary_sha256=binary,
                      renderer=renderer, native_gpu=native,
                      gl_error=world.get('gl_error'),
                      remaining_texture_references=world.get('remaining_texture_references'),
                      production_files_unchanged=before == protected_state(),
                      user_recovery_files_unchanged=recovery_before == recovery_state(),
                      note='Functional integration run; loading and Apply frames are included. No FPS claim.')
        if document.exists():
            shutil.copy2(document, out / 'street-example.world.studio')
        repaired=Path(folder)/'repaired-draft.world.studio'
        if repaired.exists():
            shutil.copy2(repaired,out/'repaired-user-draft.world.studio')
        if capture.exists():
            Image.open(capture).save(out / 'street.png')
        (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result, indent=2))
        return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
