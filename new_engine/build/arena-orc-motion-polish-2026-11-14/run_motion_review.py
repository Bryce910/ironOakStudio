#!/usr/bin/env python3
"""Continuous production-motion diagnostics. No game/session or source writes."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,time
import numpy as np
from PIL import Image,ImageDraw
HERE=Path(__file__).resolve().parent
BASE=HERE.parent/'arena-orcs-textured-2026-11-14'
OUT=HERE/'opponent-render-review'
def sha(p):
    with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def dump(p,v):p.write_text(json.dumps(v,indent=2,sort_keys=True)+'\n')
def closure(stage,build):
    result={str(Path(__file__)):sha(__file__)}
    receipt=OUT/build
    result[str(receipt)]=sha(receipt)
    data=json.loads(receipt.read_text());result.update(data['inputs']);result[data['binary']]=data['binary_sha256']
    for p in [OUT/'motion-probes.txt',OUT/'MOTION_PROBES.json',stage/'arena_orcs/ORC_ASSETS.json']:
        result[str(p)]=sha(p)
    for folder in ['arena_orcs','arena-project/shaders']:
        for p in (stage/folder).rglob('*'):
            if p.is_file():result[str(p.resolve())]=sha(p)
    for p,h in result.items():assert sha(p)==h,('Stale fixture input',p)
    reference=np.load(BASE/'arena_orcs/body-semantic.npz',allow_pickle=False)
    current=np.load(stage/'arena_orcs/body-semantic.npz',allow_pickle=False)
    assert np.array_equal(reference['bone_names'],current['bone_names'])
    for primitive in range(8):
        for field in ['rest','uv','indices','weights_raw']:
            key=f'{field}_{primitive}'
            assert np.array_equal(reference[key],current[key]),('Source-weighted probes cannot transfer to changed source topology',key)
    return result
def metrics(rows):
    assert len(rows)==2520 and [r['frame'] for r in rows]==list(range(2520))
    assert np.isfinite(np.array([r['yaw'] for r in rows])).all()
    result={}
    for chapter in dict.fromkeys(r['chapter'] for r in rows):
        q=[r for r in rows if r['chapter']==chapter];phase={}
        for name in q[0]['probes']:
            p=np.array([r['probes'][name]['p'] for r in q]);v=np.diff(p,axis=0)*60
            assert np.isfinite(p).all()
            speed=np.linalg.norm(v,axis=1)
            values=dict(max_speed_m_s=float(speed.max()),p99_speed_m_s=float(np.quantile(speed,.99)),
                        max_frame_step_m=float(speed.max()/60),min_height_m=float(p[:,1].min()),max_height_m=float(p[:,1].max()),
                        peak_acceleration_m_s2=float(np.linalg.norm(np.diff(v,axis=0)*60,axis=1).max()))
            if name.startswith('sole'):
                floor=np.array([r['probes'][name]['min_y'] for r in q]);stance=(floor<.035)&(np.abs(np.gradient(p[:,1])*60)<.10)
                runs=[];start=None
                for i,flag in enumerate([*stance,False]):
                    if flag and start is None:start=i
                    if not flag and start is not None:
                        if i-start>=6:runs.append(float(np.linalg.norm((p[start:i]-p[start])[:,[0,2]],axis=1).max()))
                        start=None
                values.update(min_vertex_height_m=float(floor.min()),stance_intervals=len(runs),
                              max_detected_stance_drift_m=max(runs,default=0.),
                              stance_detector='>=6 frames, lowest sole vertex <35mm and centroid vertical speed <0.10m/s; geometric diagnostic, not authored foot-contact truth')
            phase[name]=values
        base=np.array([r['blade_base'] for r in q]);tip=np.array([r['blade_tip'] for r in q]);axis=tip-base
        axis/=np.linalg.norm(axis,axis=1)[:,None]
        angles=np.arccos(np.clip((axis[1:]*axis[:-1]).sum(axis=1),-1,1))*60
        phase['blade']=dict(max_angular_speed_rad_s=float(angles.max()),p99_angular_speed_rad_s=float(np.quantile(angles,.99)),
                            max_tip_step_m=float(np.linalg.norm(np.diff(tip,axis=0),axis=1).max()))
        phase['frame_count']=len(q);result[chapter]=phase
    # Actual committed gait wraps, not filename endpoint equality alone.
    seams=[]
    for i in range(2,len(rows)-2):
        if rows[i]['walk']==rows[i-1]['walk']==2 and rows[i-1]['phase']>.8 and rows[i]['phase']<.2:
            entry={"t":rows[i]['t']}
            for name in ['sole_l','sole_r','hand_r']:
                p=np.array([r['probes'][name]['p'] for r in rows[i-2:i+2]])
                v=np.diff(p,axis=0)*60;entry[name]=dict(frame_step_m=float(np.linalg.norm(p[2]-p[1])),velocity_change_m_s=float(np.linalg.norm(v[1]-v[0])))
            seams.append(entry)
    return dict(phases=result,walk_loop_boundaries=seams)
def srt(directory,rows):
    chapters=[]
    for r in rows:
        if not chapters or chapters[-1][2]!=r['chapter']:chapters.append([r['t'],42.,r['chapter']])
    for i in range(len(chapters)-1):chapters[i][1]=chapters[i+1][0]
    def stamp(t):
        ms=round(t*1000);return f'{ms//3600000:02}:{ms//60000%60:02}:{ms//1000%60:02},{ms%1000:03}'
    (directory/'motion-chapters.srt').write_text('\n\n'.join(f'{i+1}\n{stamp(a)} --> {stamp(b)}\n{label.replace("_"," ")}' for i,(a,b,label) in enumerate(chapters))+'\n')
def filmstrips(directory):
    # Every encoded frame is represented, in order; this complements the full
    # continuous MP4 rather than replacing motion review with sparse keyframes.
    out=directory/'continuous-filmstrips';out.mkdir(exist_ok=True)
    command=['ffmpeg','-v','error','-i',str(directory/'motion-continuous.mp4'),'-vf','scale=128:128','-f','rawvideo','-pix_fmt','rgb24','pipe:1']
    process=subprocess.Popen(command,stdout=subprocess.PIPE);seen=0
    for page in range(13):
        sheet=Image.new('RGB',(1280,1440),'#111111');draw=ImageDraw.Draw(sheet)
        for k in range(100):
            raw=process.stdout.read(128*128*3)
            if not raw:break
            assert len(raw)==128*128*3
            x=k%10*128;y=k//10*144;sheet.paste(Image.frombytes('RGB',(128,128),raw),(x,y))
            draw.text((x+2,y+128),f'{seen/30:.2f}s',fill='white');seen+=1
        sheet.save(out/f'{page:02}.jpg',quality=90)
    process.stdout.close();assert process.wait()==0 and seen==1260
def validate(stage,label,build,before=None):
    directory=OUT/('motion-'+label);log=OUT/('motion-'+label+'.log')
    inputs=closure(stage,build)
    if before is not None:assert inputs==before,'Inputs changed during continuous capture'
    result=[json.loads(s) for s in log.read_text().splitlines() if s.startswith('{')][-1]
    assert result['passed'] and result['gl_error']==0 and result['continuous_frames']==1260 and result['updates']==2520
    video=directory/'motion-continuous.mp4'
    info=json.loads(subprocess.check_output(['ffprobe','-v','error','-count_frames','-select_streams','v:0','-show_entries','stream=width,height,r_frame_rate,nb_read_frames,duration','-of','json',str(video)],text=True))['streams'][0]
    assert info['width']==info['height']==640 and info['r_frame_rate']=='30/1' and int(info['nb_read_frames'])==1260 and abs(float(info['duration'])-42)<.001
    rows=[json.loads(s) for s in (directory/'motion-trace.jsonl').read_text().splitlines()]
    prediction=[json.loads(s) for s in (directory/'guard-prediction.jsonl').read_text().splitlines()]
    assert len(prediction)==150 and max(r['error_m'] for r in prediction)<1.e-4,('Future guard/render mismatch',max(r['error_m'] for r in prediction))
    reaction=[json.loads(s) for s in (directory/'reaction-prediction.jsonl').read_text().splitlines()]
    assert len(reaction)==750 and max(r['error_m'] for r in reaction)<1.e-4,('Future reaction/render mismatch',max(r['error_m'] for r in reaction))
    srt(directory,rows)
    filmstrips(directory)
    report=dict(schema='valwood.orc.continuous-motion-diagnostic.v1',passed=True,label=label,
                video=info,result=result,guard_prediction=dict(cases=150,max_error_m=max(r['error_m'] for r in prediction),preroll_cases=sum(r['preroll'] for r in prediction)),
                shared_probe_correspondence='Exact original rest geometry, UVs, triangle indices, source bone names and skin-weight matrices match accepted baseline for all8primitives; identical probe vertex indices in both films.',
                reaction_prediction=dict(cases=750,max_error_m=max(r['error_m'] for r in reaction),first_contact_step_max_m=max(r['first_contact_step_m'] for r in reaction)),
                metrics=metrics(rows),inputs_sha256=inputs,outputs_sha256={str(p):sha(p) for p in directory.rglob('*') if p.is_file() and p.name!='MOTION_DIAGNOSTIC.json'},
                limits=['Scripted input timeline using actual production pose selection and PNT, not interactive AI/controller rehearsal.',
                        'Probe centroids derive source skin weights; sole stance detector is diagnostic, not authored contact truth.',
                        'Fast striking peaks are reported, not treated automatically as animation errors. No hardware frame rate claim.'])
    dump(directory/'MOTION_DIAGNOSTIC.json',report);return report
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--baseline',action='store_true');parser.add_argument('--validate-only',action='store_true');args=parser.parse_args()
    label='baseline' if args.baseline else 'candidate';stage=BASE if args.baseline else HERE
    build='BASELINE_PREVIEW_BUILD.json' if args.baseline else 'PREVIEW_BUILD.json';before=closure(stage,build)
    if not args.validate_only:
        record=json.loads((OUT/build).read_text());command=[record['binary'],str(stage/'arena_orcs/body'),str(stage/'arena_orcs/anatomy'),str(stage/'arena-project/shaders'),str(OUT/('motion-'+label)),'motion',str(OUT/'motion-probes.txt')]
        with (OUT/('motion-'+label+'.log')).open('w') as log:
            subprocess.run(command,env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LP_NUM_THREADS='4'),stdout=log,stderr=subprocess.STDOUT,timeout=480,check=True)
    report=validate(stage,label,build,before)
    print(json.dumps(dict(passed=report['passed'],label=label,guard_prediction=report['guard_prediction'])),flush=True)
if __name__=='__main__':main()
