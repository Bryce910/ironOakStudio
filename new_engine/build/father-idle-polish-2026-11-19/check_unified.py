"""Stable entry point for this generation's publication verifier."""
from pathlib import Path
import importlib.util
spec = importlib.util.spec_from_file_location('father_idle_generation_checker', Path(__file__).resolve().parent / 'tools/check_unified.py')
implementation = importlib.util.module_from_spec(spec)
spec.loader.exec_module(implementation)
check = implementation.check
publish = implementation.publish
if __name__ == '__main__':
    implementation.cli()
