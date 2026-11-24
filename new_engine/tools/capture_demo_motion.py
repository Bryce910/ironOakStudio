#!/usr/bin/env python3
"""Silent native camp motion proof; no FPS claim or saved-world writes."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from PIL import Image
from benchmark_graphics_settings import ROOT, PROJECT, LAUNCHER, native_environment
from demo_qa_state import protected_state


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    protected=protected_state()
    binary=sha(ROOT/'rpg')
    with tempfile.TemporaryDirectory(prefix='valwood-motion-native-') as tmp:
        tmp=Path(tmp);document=tmp/'world.studio';shutil.copy2(PROJECT/'world.studio',document)
        prefix=tmp/'frame'
        env,_=native_environment();env.update(VALWOOD_CAPTURE_VIEW='first',
            VALWOOD_QA_STUDIO_DOCUMENT=str(document),VALWOOD_NPC_SEQUENCE=str(prefix))
        command=[str(LAUNCHER),'--scenic','rainforest-camp','--frames','600','--no-hud',
            '--width','1600','--height','900','--auto-quality','off',
            '--capture',str(out/'camp.bmp')]
        with (out/'motion.log').open('w') as log:
            try:
                run=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=300)
                code=run.returncode
            except subprocess.TimeoutExpired:
                code=-1
        # The viewer intentionally disables sequence readbacks during profiles.
        # A normal capture still writes the complete world correctness report.
        worldfile=out/'camp.bmp.world.json';world=json.loads(worldfile.read_text()) if worldfile.exists() else {}
        frames=sorted(tmp.glob('frame-*.bmp'))
        contact=re.findall(r'\[Live contacts\] NPC yields (\d+), player blocks (\d+), minimum player separation ([\d.]+)m',
                           (out/'motion.log').read_text())
        separation=float(contact[-1][2]) if contact else None
        report=dict(purpose='Untimed native visual/motion proof; readbacks included, not FPS evidence',
            exit=code,binary_sha256=binary,frames=len(frames),gl_error=world.get('gl_error'),
            references=world.get('remaining_texture_references'),contact_summary=contact,
            minimum_player_separation_m=separation,
            source_world_unchanged=protected_state()==protected,
            binary_unchanged=sha(ROOT/'rpg')==binary,video_has_audio=False)
        if code==0 and len(frames)==360:
            # Encoding starts only after the GL process has closed.
            subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-y','-framerate','60',
                '-start_number','0','-i',str(prefix)+'-%03d.bmp','-frames:v','360','-an','-c:v','libx264',
                '-threads','2','-preset','fast','-crf','18','-pix_fmt','yuv420p','-movflags','+faststart',
                str(out/'camp-motion.mp4')],check=True)
            Image.open(frames[180]).convert('RGB').save(out/'camp-motion.png')
            Image.open(out/'camp.bmp').convert('RGB').save(out/'camp.png')
        report['passed']=code==0 and len(frames)==360 and report['gl_error']==0 and report['references']==0 and report['source_world_unchanged'] and report['binary_unchanged'] and separation is not None and separation>=.78
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(report,indent=2))
    raise SystemExit(0 if report['passed'] else 1)


if __name__=='__main__':
    main()
