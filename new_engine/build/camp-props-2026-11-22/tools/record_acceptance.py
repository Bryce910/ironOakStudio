"""Record exact asset/native evidence after human inspection of final captures."""
from pathlib import Path
import csv,hashlib,json,statistics
from PIL import Image
N=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    R=N/'review';base=R/'baseline-v3';final=R/'candidate-v4'
    audit=N/'audit-work/FINAL_AUDIT.json';a=json.loads(audit.read_text());assert a['passed']
    for p,h in a['inputs_sha256'].items():assert sha(Path(p))==h,p
    launches={name:json.loads((run/'launch.json').read_text()) for name,run in [('baseline',base),('final',final)]}
    models=launches['final']['model_sha256'];assert len(models)==4
    for p,h in models.items():assert sha(Path(p))==h,p
    results={};timing={}
    for name,run in [('baseline',base),('final',final)]:
        route=json.loads((run/'route.json').read_text());world=json.loads((run/'profile.world.json').read_text());stream=world['asset_streaming']
        assert route['complete'] and route['captures']==6 and route['failures']==0
        assert world['gl_error']==0 and stream['failed']==0 and stream['critical_pending']==0 and stream['loading']==0
        assert stream['resident']==stream['wanted'] and world['main_material_preparation']['failures']==0
        log=(run/'game.log').read_text();assert 'Startup resource validation failed' not in log
        results[name]={'route':route,'gl_error':world['gl_error'],'streaming':stream,'graphics':world['graphics'],'renderer':next(line for line in log.splitlines() if line.startswith('[World] Renderer '))}
        rows=list(csv.DictReader((run/'route.frames.csv').open()));timing[name]={}
        for label in ['camp','tent','hearth','bridge-wide','bridge-detail','camp-return']:
            vals=[float(row['gpu_ms']) for row in rows if row['label']==label and row['phase']=='hold' and row['capture_readback']=='0'][30:]
            assert len(vals)>=80
            timing[name][label]={'samples':len(vals),'median_gpu_ms':statistics.median(vals)}
        for p in run.glob('shot*.bmp'):Image.open(p).save(p.with_suffix('.png'))
    assert launches['baseline']['binary_sha256']==launches['final']['binary_sha256']
    for label,v in timing['final'].items():
        old=timing['baseline'][label]['median_gpu_ms'];v['delta_ms']=v['median_gpu_ms']-old
        assert v['delta_ms']<max(1.5,old*.07),'Material GPU regression at '+label
    assert (final/'project/world.studio').read_bytes()==(N/'audit-work/FINAL_AUDIT.json').parent.parent.joinpath('review/candidate-v3/project/world.studio').read_bytes()
    pins={str(p):sha(p) for p in [audit,N/'tools/publish_props.py',N/'GUIDE.txt',final/'launch.json',final/'route.json',final/'profile.world.json',final/'profile.csv',final/'route.frames.csv',final/'PREFLIGHT.json',final/'project/world.studio',final/'project/scenery/authored_instances.txt',base/'launch.json',base/'project/world.studio',base/'project/scenery/authored_instances.txt',N/'tools/record_acceptance.py']}
    pins.update(models)
    for p in final.glob('shot*.png'):pins[str(p)]=sha(p)
    result=dict(schema='valwood.camp-props.native-acceptance.v1',passed=True,scope='campsite close-up props',binary_sha256=launches['final']['binary_sha256'],tested_project=str(final/'project'),baseline_project=str(base/'project'),independent_checks=a['checks'],native=results,timing=timing,inputs_sha256=pins,visual_review={'camp':True,'tent':True,'hearth':True,'bridge-wide':True,'bridge-detail':True,'camp-return':True},limitations=['Short matched stationary-view samples, not sustained full-demo performance acceptance.','Source collision is unchanged and cosmetic bridge support is independently checked; this is not a new full interactive collision rehearsal.','Path, distant hill and creek bank art are outside this prop pass.','No claim of AAA/reference parity or facial/lip animation completion.'],windows_work_performed=False)
    (R/'NATIVE_ACCEPTANCE.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'passed':True,'checks':a['checks'],'final_texture_bytes':results['final']['streaming']['texture_bytes'],'timing':timing},indent=2))
if __name__=='__main__':main()
