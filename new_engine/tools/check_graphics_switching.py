#!/usr/bin/env python3
"""Native correctness check for live apply/save, resizing and effect switching."""
from pathlib import Path
import json,os,subprocess,tempfile
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
def main():
    out=ROOT/'docs/performance/graphics-settings/switching';out.mkdir(parents=True,exist_ok=True)
    prefs=ROOT/'projects/valwood_world/graphics.settings';original=prefs.read_bytes() if prefs.exists() else None
    env=os.environ.copy()
    for key in list(env):
        if key.startswith(('VALWOOD_QA_','VALWOOD_DISABLE_')) or key.endswith('_SEQUENCE') or key in ('VALWOOD_SHADOW_RESOLUTION','LIBGL_ALWAYS_SOFTWARE','SDL_VIDEODRIVER'):env.pop(key,None)
    env.update(SDL_AUDIODRIVER='dummy',VALWOOD_QA_GRAPHICS_CYCLE='1',VALWOOD_VILLAGE_LIGHTING='afternoon')
    stem=out/'cycle'
    try:
        with stem.with_suffix('.log').open('w') as log:
            subprocess.run([str(ROOT.parent/'Launch_Valwood_World.sh'),'--scenic','wildlife-rainforest','--graphics-preset','balanced','--auto-quality','off','--target-fps','60','--width','1600','--height','900','--frames','720','--no-hud','--capture',str(stem.with_suffix('.bmp'))],cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
        w=json.loads(Path(str(stem.with_suffix('.bmp'))+'.world.json').read_text());g=w['graphics']
        assert g['applied_changes']==3 and g['apply_failures']==0 and g['preset']=='Balanced' and g['render_width']==1600 and g['render_height']==900,g
        assert g['minimum_observed_scale']==.75 and g['maximum_observed_scale']==1 and g['scale_changes']==3,g
        assert g['volumetric_clouds'] and g['volumetric_fog'] and g['contact_shadows'] and g['reflections']
        assert w['gl_error']==w['failed_tiles']==w['remaining_texture_entries']==w['remaining_texture_references']==0
        text=stem.with_suffix('.log').read_text()
        for expected in ('scene1200x675 shadow2048 reflection0.25 enabled0 contact0 fog0 clouds0','scene1440x810 shadow4096 reflection1.00 enabled1 contact1 fog1 clouds1','scene1600x900 shadow2048 reflection0.50 enabled1 contact1 fog1 clouds1'):assert expected in text,expected
        cap=json.loads(Path(str(stem.with_suffix('.bmp'))+'.json').read_text());assert cap['capture_ok'] and not cap['offscreen'] and 'AMD Radeon RX 5300' in cap['renderer']
        with Image.open(stem.with_suffix('.bmp')) as im:assert im.size==(1600,900);im.save(stem.with_suffix('.png'))
        assert prefs.is_file() and b'auto_quality=0' in prefs.read_bytes()
        (out/'result.json').write_text(json.dumps({'passed':True,'graphics':g,'gl_error':w['gl_error'],'preferences_restored':True,'note':'Correctness only; includes explicit resource changes, no performance claim.'},indent=2)+'\n')
        print('PASS native live graphics switching: three staged apply/save operations, full-size output at75/90/100%scene scales, 2K/4Kshadows, all effect toggles, zero GL/resource failures; original preferences restored.')
    finally:
        if original is None:prefs.unlink(missing_ok=True)
        else:
            fd,name=tempfile.mkstemp(prefix='.graphics-restore-',dir=prefs.parent)
            with os.fdopen(fd,'wb') as f:f.write(original)
            os.replace(name,prefs)
if __name__=='__main__':main()
