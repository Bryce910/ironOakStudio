#!/usr/bin/env python3
"""Deterministic original demo sound design; no downloaded or third-party audio.

Creates seamless subdued environmental beds and twelve material footfalls.
This is environmental sound, not recorded crowds or intelligible dialogue.
All synthesis happens offline; runtime only loads finished PCM once.
"""
from pathlib import Path
import hashlib
import json
import wave
import numpy as np

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'projects/valwood_world/audio/demo'
RATE=22050
SEED=0x56414C57
LOOP_SECONDS=16

def noise(rng,n,low,high):
    bins=np.fft.rfftfreq(n,1/RATE)
    spectrum=np.fft.rfft(rng.standard_normal(n))
    envelope=np.exp(-np.power(bins/max(high,1),4))
    if low: envelope*=1-np.exp(-np.power(bins/low,4))
    result=np.fft.irfft(spectrum*envelope,n)
    return result/max(np.std(result),1e-9)

def add_event(out,start,event,pan):
    # Wrap into the circular loop, including the tail at the end of the file.
    index=(np.arange(len(event))+int(start*RATE))%len(out)
    angle=(pan*.7+1)*np.pi/4
    out[index,0]+=event*np.cos(angle)
    out[index,1]+=event*np.sin(angle)

def transient(rng,duration,kind):
    t=np.arange(round(duration*RATE))/RATE
    if kind=='bird':
        frequency=2150+600*np.sin(t*22)+1300*t/max(duration,.01)
        sound=np.sin(2*np.pi*np.cumsum(frequency)/RATE)*np.sin(np.pi*t/duration)**3
        sound*=.18+.82*np.sin(t*21)**2
    elif kind=='clink':
        sound=sum(np.sin(2*np.pi*f*t)*np.exp(-t*d) for f,d in [(1700,15),(2798,20),(4213,27)])/3
        sound*=np.minimum(t/.002,1)
    elif kind=='creak':
        sound=(np.sin(2*np.pi*(160*t+18*np.sin(t*5)))+.28*np.sin(2*np.pi*370*t))
        sound*=np.sin(np.pi*t/duration)**2
    elif kind=='twig':
        sound=noise(rng,len(t),700,7000)*np.exp(-t*35)*np.minimum(t/.001,1)
    else:
        sound=noise(rng,len(t),100,4500)*np.sin(np.pi*t/duration)**2
    return sound

def loop(kind,index):
    rng=np.random.default_rng(SEED+index)
    n=RATE*LOOP_SECONDS;t=np.arange(n)/RATE
    out=np.zeros((n,2),dtype=np.float64)
    if kind=='wind':
        for c in range(2):out[:,c]=noise(rng,n,35,1100)*(.035+.015*np.sin(2*np.pi*t/16+c)**2)
    elif kind in ('forest','night'):
        for c in range(2):out[:,c]=noise(rng,n,300,3800)*.013
        if kind=='forest':
            for _ in range(12):add_event(out,rng.uniform(0,16),transient(rng,rng.uniform(.13,.33),'bird')*rng.uniform(.025,.065),rng.uniform(-1,1))
        else:
            for c in range(2):
                carrier=np.sin(2*np.pi*(3800+c*187)*t)+.3*np.sin(2*np.pi*5240*t)
                gate=(.5+.5*np.sin(2*np.pi*7*t))**12
                gate*=.5+.5*np.sin(2*np.pi*t/4+c)
                out[:,c]+=carrier*gate*.035
    elif kind=='creek':
        for c in range(2):
            out[:,c]=noise(rng,n,140,5400)*.045+noise(rng,n,40,400)*.020
            out[:,c]*=.84+.16*np.sin(2*np.pi*t/8+c)
        for _ in range(42):
            d=rng.uniform(.05,.17);et=np.arange(round(d*RATE))/RATE
            frequency=rng.uniform(350,950)*np.exp(-et*4)
            event=np.sin(2*np.pi*np.cumsum(frequency)/RATE)*np.sin(np.pi*et/d)**2
            add_event(out,rng.uniform(0,16),event*.023,rng.uniform(-1,1))
    elif kind=='fire':
        for c in range(2):out[:,c]=noise(rng,n,90,1500)*.034
        for _ in range(66):add_event(out,rng.uniform(0,16),transient(rng,rng.uniform(.035,.1),'twig')*rng.uniform(.035,.12),rng.uniform(-.5,.5))
    elif kind=='rain':
        for c in range(2):out[:,c]=noise(rng,n,300,8500)*.055+noise(rng,n,100,900)*.014
        for _ in range(95):add_event(out,rng.uniform(0,16),transient(rng,.035,'twig')*.037,rng.uniform(-1,1))
    elif kind in ('market','tavern'):
        # Quiet cloth, timber and crockery texture. Avoid synthesized pseudo
        # speech: a professionally recorded crowd layer can replace this bed.
        for c in range(2):out[:,c]=noise(rng,n,170,1600)*.008
        for _ in range(17 if kind=='market' else 10):
            event_kind=rng.choice(['creak','clink','cloth'])
            duration=rng.uniform(.45,1.4) if event_kind=='creak' else rng.uniform(.10,.32)
            add_event(out,rng.uniform(0,16),transient(rng,duration,event_kind)*rng.uniform(.009,.030),rng.uniform(-.75,.75))
    # Blend the last25ms onto the first25ms and remove the consumed head.
    # The final sample then leads naturally into the following head sample;
    # copying the same crossfade to both ends would introduce a second jump.
    seam=round(.025*RATE)
    tail=out[-seam:].copy();head=out[:seam].copy();alpha=np.linspace(0,1,seam)[:,None]
    out[-seam:]=tail*(1-alpha)+head*alpha
    out=out[seam:]
    out-=out.mean(axis=0)
    peak=np.max(np.abs(out))
    if peak>.36:out*=.36/peak
    return out

def footfall(surface,index):
    rng=np.random.default_rng(SEED+100+surface*3+index)
    duration=.32;n=round(RATE*duration);t=np.arange(n)/RATE
    thud=np.sin(2*np.pi*(76+index*9)*t)*np.exp(-t*(32 if surface==2 else 44))
    contact=noise(rng,n,650,6800)*np.exp(-t*29)
    follow=noise(rng,n,250,3600)*np.exp(-np.maximum(0,t-.065)*20)*np.minimum(t/.065,1)
    if surface==0:sample=.18*thud+.070*contact+.025*follow
    elif surface==1:sample=.20*thud+.13*contact+.012*follow
    elif surface==2:sample=.23*thud+.045*contact+np.sin(2*np.pi*143*t)*np.exp(-t*23)*.05
    else:sample=.12*thud+.045*contact+.07*follow
    sample*=np.minimum(t/.0018,1)*np.minimum((duration-t)/.025,1)
    sample*=.46/max(np.max(np.abs(sample)),1e-9)
    return np.column_stack([sample,sample])

def write(name,samples):
    path=OUT/f'{name}.wav'
    pcm=np.rint(np.clip(samples,-1,1)*32767).astype('<i2')
    with wave.open(str(path),'wb') as stream:
        stream.setparams((2,2,RATE,len(pcm),'NONE','not compressed'));stream.writeframes(pcm.tobytes())
    return {'file':path.name,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
            'sample_rate':RATE,'channels':2,'frames':len(pcm),'duration_s':len(pcm)/RATE,
            'peak':round(float(np.max(np.abs(samples))),8),'rms':round(float(np.sqrt(np.mean(samples*samples))),8)}

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    assets=[write(name,loop(name,i)) for i,name in enumerate(['wind','forest','creek','fire','rain','market','tavern','night'])]
    assets += [write(f'step_{surface}_{variant}',footfall(i,variant)) for i,surface in enumerate(['soil','stone','wood','wet']) for variant in range(3)]
    metadata={'version':1,'seed':SEED,'generator':'tools/build_demo_audio.py',
              'provenance':'Original deterministic procedural audio synthesized locally for Valwood; no third-party recordings or voices.',
              'intent':'Subtle environmental texture and material footsteps; market and tavern contain timber, cloth and crockery, not recorded people.',
              'runtime':'Eight continuous bed voices, four bounded transient voices; all PCM decoded at startup; no runtime synthesis.',
              'assets':assets}
    (OUT/'manifest.json').write_text(json.dumps(metadata,indent=2)+'\n')
    print(json.dumps({'assets':len(assets),'bytes':sum((OUT/a['file']).stat().st_size for a in assets),'directory':str(OUT)},indent=2))

if __name__=='__main__':main()
