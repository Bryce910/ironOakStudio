"""Stable entry point for the scene-only generation verifier."""
from pathlib import Path
import importlib.util
import sys
sys.dont_write_bytecode=True
spec=importlib.util.spec_from_file_location('forest_scene_generation_checker',Path(__file__).resolve().parent/'tools/check_unified.py')
implementation=importlib.util.module_from_spec(spec)
spec.loader.exec_module(implementation)
check=implementation.check
publish=implementation.publish
if __name__=='__main__':implementation.cli()
