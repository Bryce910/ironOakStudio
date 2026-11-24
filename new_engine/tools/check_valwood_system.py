#!/usr/bin/env python3
"""Read-only package preflight. A pass does not replace a real GPU launch test."""
import argparse
import ctypes
import json
from pathlib import Path
import platform
import subprocess


def assess(package, system=None, machine=None, libc=None, cache=None, loader=ctypes.CDLL):
    package = Path(package)
    native = json.loads((package / 'native-dependencies.json').read_text())
    system = platform.system() if system is None else system
    machine = platform.machine() if machine is None else machine
    libc = platform.libc_ver() if libc is None else libc
    problems = []
    if system != 'Linux':
        problems.append('This export contains a Linux executable; a native Windows/macOS export is not available.')
    architecture = native.get('architecture', '')
    expected = 'x86_64' if 'X86-64' in architecture else 'aarch64' if 'AArch64' in architecture else None
    aliases = {'AMD64': 'x86_64', 'arm64': 'aarch64'}
    if expected and aliases.get(machine, machine) != expected:
        problems.append(f'CPU architecture mismatch: package requires {expected}, host reports {machine}.')
    minimum = native.get('minimum_glibc')
    def version(value):
        return tuple(int(part) for part in value.split('.'))
    if minimum and (libc[0] != 'glibc' or not libc[1] or version(libc[1]) < version(minimum)):
        problems.append(f'Package requires glibc {minimum} or newer; host reports {libc[0]} {libc[1]}.')
    if cache is None:
        result = subprocess.run(['ldconfig', '-p'], text=True, capture_output=True, check=False)
        cache = result.stdout if result.returncode == 0 else ''
    available = {line.split()[0] for line in cache.splitlines() if line.split()}
    for library in native.get('libraries', []):
        name = library['soname']
        bundled = package / 'lib' / name
        if library.get('bundled') and not bundled.is_file():
            problems.append('Missing bundled library: ' + name)
        elif not library.get('bundled') and name not in available:
            # Private dependencies (e.g. PulseAudio's libpulsecommon) are often
            # resolved through a parent's RPATH rather than the ldconfig cache.
            try:
                for parent in library.get('needed_by', []):
                    if parent != 'rpg':
                        candidate = package / 'lib' / parent
                        loader(str(candidate) if candidate.is_file() else parent)
                loader(name)
            except OSError:
                problems.append('Required host library could not be resolved by the loader: ' + name)
    return {
        'passed': not problems, 'system': system, 'machine': machine,
        'host_libc': list(libc), 'minimum_glibc': minimum,
        'problems': problems,
        'requires_real_launch': True,
        'notes': ['A functioning OpenGL 3.3 driver and a display session are required.',
                  'Driver plugins, audio services and actual GPU memory are not certified by this metadata check.'],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()
    try:
        result = assess(args.package)
    except (OSError, ValueError) as error:
        result = {'passed': False, 'problems': [str(error)], 'requires_real_launch': True}
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print('System metadata check: ' + ('PASS' if result['passed'] else 'NEEDS ATTENTION'))
        for item in result.get('problems', []) + result.get('notes', []):
            print(' - ' + item)
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
