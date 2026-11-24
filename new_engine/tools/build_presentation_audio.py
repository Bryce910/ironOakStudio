#!/usr/bin/env python3
"""Original deterministic preview score and cues; no third-party recordings."""
import argparse,hashlib,json,wave
from pathlib import Path
import numpy as np

RATE=22050
def instrument(note,length,kind='string'):
    t=np.arange(round(length*RATE))/RATE;f=440*2**((note-69)/12)
    if kind=='pluck':
        s=sum(np.sin(2*np.pi*f*k*t)*np.exp(-t*(2+k*.6))/k for k in range(1,7))
        env=np.minimum(1,t/.008)*np.minimum(1,(length-t)/.12)
    elif kind=='bell':
        s=sum(np.sin(2*np.pi*f*k*t)*np.exp(-t*(.6+k*.4))/(k*k) for k in (1,2,3,5))
        env=np.minimum(1,t/.012)*np.minimum(1,(length-t)/.35)
    else:
        s=sum(np.sin(2*np.pi*f*k*t+.008*k*np.sin(t*4.3)) /(k*k) for k in (1,2,3,4))
        env=np.minimum(1,t/.9)*np.minimum(1,(length-t)/1.4)
    return s*env

def compose(length,combat=False):
    data=np.zeros((round(length*RATE),2),np.float64)
    def add(note,at,duration,gain,kind='string',pan=.5):
        signal=instrument(note,min(duration,length-at),kind)*gain
        i=round(at*RATE);n=min(len(signal),len(data)-i)
        if n>0:data[i:i+n,0]+=signal[:n]*np.sqrt(1-pan);data[i:i+n,1]+=signal[:n]*np.sqrt(pan)
    roots=[50,46,53,48,50,46,48,45]
    bar=4 if combat else 8
    for n,root in enumerate(roots[:int(length/bar)]):
        start=n*bar
        for j,note in enumerate([root-12,root,root+7,root+12]):add(note,start,bar+.5,.085 if combat else .065,pan=.2+j*.2)
        pattern=[0,7,12,15,12,7,3,7] if not combat else [0,0,7,3,0,7,12,7]
        for j,step in enumerate(pattern):add(root+12+step,start+j*bar/8,1.7,.075,'pluck',.3+.4*(j%2))
        if combat:
            for j in range(8):
                at=start+j*.5;i=round(at*RATE);t=np.arange(int(RATE*.35))/RATE
                thump=np.sin(2*np.pi*(65*t+18*(1-np.exp(-t*22))))*np.exp(-t*13)*.18
                end=min(len(thump),len(data)-i);data[i:i+end]+=thump[:end,None]
    # Short stereo room echoes, then edge fades. Deliberate quiet loop boundary.
    for delay,g in [(.137,.14),(.293,.09),(.491,.045)]:
        k=round(delay*RATE);data[k:]+=data[:-k,::-1]*g
    edge=min(RATE*2,len(data)//2);data[:edge]*=np.linspace(0,1,edge)[:,None];data[-edge:]*=np.linspace(1,0,edge)[:,None]
    return data

def cue(notes,length):
    out=np.zeros((int(length*RATE),2))
    for j,note in enumerate(notes):
        i=int(j*.22*RATE);s=instrument(note,length-j*.22,'bell')*.20;n=min(len(s),len(out)-i)
        out[i:i+n,0]+=s[:n]*.75;out[i:i+n,1]+=s[:n]*.68
    return out

def foley(kind):
    duration={'slash':.52,'impact':.48,'parry':.65,'dash':.36}[kind]
    t=np.arange(round(duration*RATE))/RATE;rng=np.random.default_rng(7241+len(kind));noise=rng.normal(0,1,len(t))
    smooth=np.convolve(noise,np.ones(9)/9,mode='same')
    if kind=='slash':s=(noise-smooth)*np.sin(np.pi*np.minimum(1,t/.45))**2*np.exp(-t*6)*.22
    elif kind=='dash':s=smooth*np.sin(np.pi*t/duration)**2*.60
    else:
        frequencies=[720,1217,2131,3397]if kind=='parry'else[178,393,891,1723]
        s=sum(np.sin(2*np.pi*f*t)*np.exp(-t*(11+i*4))*.12/(1+i*.4)for i,f in enumerate(frequencies))
        s+=(noise-smooth)*np.exp(-t*80)*.22
        s*=np.minimum(1,t/.003)
    return np.column_stack([s,s*.94])

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    tracks={'journey':compose(64),'oathbreaker':compose(32,True),'chapter':cue([62,69,74],3.5),'reward':cue([62,66,69,74,78,81],5),'remember':cue([69,74],2.5)}
    tracks.update({name:foley(name)for name in ('slash','impact','parry','dash')})
    records=[]
    for name,s in tracks.items():
        peak=float(np.max(np.abs(s)));s*=min(1,.78/max(peak,1e-8));pcm=np.round(np.clip(s,-.98,.98)*32767).astype('<i2')
        f=a.output/(name+'.wav')
        with wave.open(str(f),'wb')as w:w.setparams((2,2,RATE,len(pcm),'NONE','not compressed'));w.writeframes(pcm.tobytes())
        records.append(dict(file=f.name,seconds=len(pcm)/RATE,peak=float(np.max(np.abs(pcm.astype(float))))/32767,sha256=hashlib.sha256(f.read_bytes()).hexdigest()))
    (a.output/'manifest.json').write_text(json.dumps(dict(source='Original synthesized composition for Valwood; deterministic script, no sampled recordings or generated speech',sample_rate=RATE,tracks=records),indent=2)+'\n')
    print(json.dumps(records,indent=2))
if __name__=='__main__':main()
