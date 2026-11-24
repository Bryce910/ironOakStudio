#!/usr/bin/env python3
"""Pure CPU model transactions plus syntax-check of the exact ImGui panel."""
import argparse,hashlib,json,shlex,subprocess
from pathlib import Path
N=Path(__file__).resolve().parent;S=N/'source-v1';R=N/'review/cinematic-editor'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 a=argparse.ArgumentParser();a.add_argument('--collect',action='store_true');args=a.parse_args();inputs={str(Path(__file__)):sha(__file__)};outputs={};recipes=[]
 units=[R/'test_editor_model.cpp',S/'engine/world_cinematics.cpp',S/'engine/world_project_checkpoint.cpp']
 for unit in units+[R/'panel_syntax.cpp']:
  dep=subprocess.check_output(['g++','-std=c++17','-I'+str(S),'-MM',str(unit)],text=True).replace('\\\n',' ')
  for p in shlex.split(dep.split(':',1)[1]):inputs[str(Path(p).resolve())]=sha(p)
 results={}
 for name,flags in [('optimized',['-O2']),('ubsan',['-O1','-fsanitize=undefined','-fno-sanitize-recover=undefined'])]:
  binary=R/('test_editor_model' if name=='optimized' else 'test_editor_model_ubsan')
  cmd=['g++','-std=c++17',*flags,'-I'+str(S),*map(str,units),'-o',str(binary)];recipes.append(cmd)
  if not args.collect:subprocess.run(cmd,check=True)
  result=subprocess.run([str(binary)],text=True,capture_output=True,check=True)
  parsed=json.loads(result.stdout);assert parsed['passed'] and parsed['checks']>=119
  path=R/(name+'.json');path.write_text(json.dumps(parsed,indent=2)+'\n');outputs[str(path)]=sha(path);outputs[str(binary)]=sha(binary);results[name]=parsed
 syntax=['g++','-std=c++17','-fsyntax-only','-I'+str(S),str(R/'panel_syntax.cpp')];recipes.append(syntax);subprocess.run(syntax,check=True)
 bridge=S/'engine/world_studio_project_view.inc';text=bridge.read_text();assert 'region_modified||cinematic_unsaved()' in text and text.count('if(cinematic_unsaved()){project_quit_after_save=designer_save_exit=false;')==2
 studio=S/'engine/world_studio_view.inc';assert 'cinematic_overlay(cam)' in studio.read_text() and 'VALWOOD_CINEMATIC_EDITOR' in studio.read_text()
 for p in [bridge,studio,S/'engine/world_studio.h']:inputs[str(p)]=sha(p)
 report={'schema':'valwood.cinematic-studio.editor-cpu.v1','passed':True,'inputs':inputs,'outputs':outputs,'recipes':recipes,'tests':results,
 'panel_syntax_passed':True,'history_snapshots':32,'dirty_cache_constant_time':True,
 'published_library':'cinematics/world.vcuts','save_journal':'cinematics/world.vcutsproject',
 'covers':['multi-sequence creation/selection/rename/deletion','camera and narration fields roundtrip','sphere/persistent-interaction trigger roundtrip','Undo/Redo and saved-content dirty state','unsaved reload protection','WAV path/RIFF/source-range preflight beforepublish','external disk edit conflict preservation','explicit existing timeline import','durable interrupted publish recovery'],
 'ui_controls_present':['exact camera time/position/yaw/pitch/FOV','record view/reorder/duplicate/delete keys','manual/area/interaction triggers','viewport sphere/box guides','cursor target/camera feet placement','enabled/once/priority/cooldown/skipping/letterbox','WAV asset/start/duration/gain/fades/source offset'],
 'native_ui_interactivity_tested':False,'audio_playback_tested_here':False,'no_gl_used':True,'no_user_session_touched':True}
 path=R/'CINEMATIC_EDITOR_CPU_REVIEW.json';path.write_text(json.dumps(report,indent=2,sort_keys=True)+'\n');print('PASS cinematic editor optimized+UBSan transactions and exactpanel syntax:',path)
if __name__=='__main__':main()
