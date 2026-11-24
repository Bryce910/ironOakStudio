#!/usr/bin/env python3
"""Native editor workflow plus separately measured steady frames."""
import argparse,csv,hashlib,json,math,shutil,subprocess,tempfile
from pathlib import Path
from PIL import Image
from benchmark_graphics_settings import ROOT,PROJECT,LAUNCHER,native_environment
from demo_qa_state import protected_state

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    before=protected_state();binary=hashlib.sha256((ROOT/'rpg').read_bytes()).hexdigest()
    with tempfile.TemporaryDirectory(prefix='valwood-editor-steady-') as temp:
        document=Path(temp)/'world.studio';shutil.copy2(PROJECT/'world.studio',document)
        environment,_=native_environment();environment.update(VALWOOD_QA_STUDIO_DOCUMENT=str(document),VALWOOD_QA_DESIGNER_WORKSPACE='1',VALWOOD_QA_EDITOR_OPTIMIZATION='1',VALWOOD_QA_FRAME_TIMES_ONLY='1')
        profile=out/'profile.json';capture=out/'workspace.bmp'
        command=[str(LAUNCHER),'--editor','--scenic','castle-finished-arcade','--frames','1800','--warmup','600','--width','1600','--height','900','--auto-quality','off','--no-hud','--profile',str(profile),'--capture',str(capture)]
        with (out/'native.log').open('w') as log:code=subprocess.run(command,cwd=ROOT,env=environment,stdout=log,stderr=subprocess.STDOUT,timeout=300).returncode
        world=json.loads(Path(str(profile)+'.world.json').read_text())if profile.exists()else{};metrics=json.loads(profile.read_text())if profile.exists()else{}
        rows=list(csv.DictReader(Path(str(profile)+'.csv').open()))if profile.exists()else[];steady=[r for r in rows if float(r['frame'])>=800];values=[float(r['frame_ms'])for r in steady];slow=sorted(values,reverse=True)[:max(1,math.ceil(len(values)*.01))]
        result=dict(exit=code,binary_sha256=binary,renderer=metrics.get('renderer'),samples=len(values),mean_fps=1000*len(values)/sum(values)if values else 0,one_percent_low_fps=1000*len(slow)/sum(slow)if slow else 0,frames_below45=sum(v>1000/45 for v in values),worst_ms=max(values)if values else None,mean_editor_cpu_ms=sum(float(r.get('raw_editor_ms',0))for r in steady)/len(steady)if steady else None,gl_error=world.get('gl_error'),texture_refs=world.get('remaining_texture_references'),protected_files_unchanged=before==protected_state(),notes='First 800 frames excluded from steady result; functional Apply and loading pauses are retained in the raw profile separately.')
        log=(out/'native.log').read_text();result['correctness_passed']=code==0 and '[Designer QA] PASS 67' in log and '[Editor optimization QA] PASS 137' in log and world.get('gl_error')==0 and world.get('remaining_texture_references')==0 and result['protected_files_unchanged'] and len(values)>=990
        result['statistical_target_passed']=result['correctness_passed']and result['mean_fps']>=45 and result['one_percent_low_fps']>=45;result['strict_floor_passed']=result['correctness_passed']and result['frames_below45']==0
        if capture.exists():Image.open(capture).save(out/'workspace.png')
        (out/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));return 0 if result['statistical_target_passed']else 1
if __name__=='__main__':raise SystemExit(main())
