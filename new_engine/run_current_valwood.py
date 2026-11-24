#!/usr/bin/env python3
"""Stable entry point. One manifest selects the active Valwood generation."""
from pathlib import Path
import json,os,subprocess,sys
E=Path(__file__).resolve().parent;ROOT=E.parent
try:
 manifest=json.loads((ROOT/'CURRENT_VALWOOD.json').read_text())
 stage=Path(manifest['runtime_generation']).resolve(strict=True)
 if not stage.is_relative_to(E/'build'):raise ValueError('Current runtime must be a workspace build generation')
 args=sys.argv[1:]
 imports={'import-cutscene-voice':'import_cutscene_voice.py','import-npc-voice':'import_npc_intro_media.py'}
 if args and args[0] in imports:
  tool=imports[args.pop(0)]
  subprocess.run([sys.executable,str(stage/'check_unified.py'),'--check-only'],check=True)
  if not any(a=='--project' or a.startswith('--project=') for a in args):args += ['--project',manifest['working_world']]
  os.execv(sys.executable,[sys.executable,str(stage/'source-v1/tools'/tool),*args])
 else:os.execv(sys.executable,[sys.executable,str(stage/'launch.py'),*args])
except Exception as error:print('Valwood current runtime:',error,file=sys.stderr);sys.exit(1)
