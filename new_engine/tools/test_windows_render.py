#!/usr/bin/env python3
"""Isolated Wine render smoke using a project-local Xvfb; no Windows FPS claim."""
import json
import os
from pathlib import Path
import subprocess
import time

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'docs/windows-build/2026-11-01'
def win(path): return 'Z:'+str(path.resolve()).replace('/','\\')

def main():
    env=os.environ.copy();env.update(DISPLAY=':94',WINEPREFIX=str(ROOT/'build/windows/render-prefix'),
        WINEDEBUG='-all',WINEDLLOVERRIDES='mscoree,mshtml=',SDL_AUDIODRIVER='dummy',
        LIBGL_ALWAYS_SOFTWARE='1',GALLIUM_DRIVER='llvmpipe')
    with (OUT/'virtual-display.log').open('w') as display_log:
        display=subprocess.Popen([str(ROOT/'build/windows/xvfb/usr/bin/Xvfb'),':94','-screen','0','1280x720x24','-nolisten','tcp','-ac'],env=env,stdout=display_log,stderr=subprocess.STDOUT)
        try:
            for _ in range(30):
                if display.poll() is not None: raise RuntimeError('Virtual display could not start')
                if Path('/tmp/.X11-unix/X94').exists(): break
                time.sleep(.1)
            with (OUT/'render-prefix.log').open('w') as log:
                subprocess.run(['/usr/lib/wine/wine64','wineboot','-u'],env=env,stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,timeout=45)
            command=['/usr/lib/wine/wine64',str(ROOT/'build/windows/editor/bin/rpg.exe'),'--project',win(ROOT/'build/windows/smoke/project'),
                '--editor','--scenic','castle-finished-arcade','--frames','3','--warmup','0','--width','960','--height','540',
                '--graphics-preset','performance','--capture',win(OUT/'windows-smoke.bmp'),'--profile',win(OUT/'windows-smoke.json')]
            code=-1
            with (OUT/'windows-render.log').open('w') as log:
                try: code=subprocess.run(command,env=env,stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,timeout=180).returncode
                except subprocess.TimeoutExpired: pass
            report=dict(exit=code,scope='Windows executable under Wine and virtual X display; not Windows hardware/FPS acceptance')
            capture=OUT/'windows-smoke.bmp'
            report['capture_exists']=capture.is_file()
            if (OUT/'windows-smoke.json').is_file(): report['profile']=json.loads((OUT/'windows-smoke.json').read_text())
            (OUT/'render-result.json').write_text(json.dumps(report,indent=2)+'\n')
            print(json.dumps(report));return 0 if code==0 and capture.is_file() else 1
        finally:
            subprocess.run(['wineserver','-k'],env=env,capture_output=True,timeout=15)
            display.terminate()
            try:display.wait(timeout=5)
            except subprocess.TimeoutExpired:display.kill()
if __name__=='__main__':raise SystemExit(main())
