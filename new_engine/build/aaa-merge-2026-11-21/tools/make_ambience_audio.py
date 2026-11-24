#!/usr/bin/env python3
"""AAA merge: build camp ambience WAVs into ../audio.
camp_fire_loop.wav : 24 s seamless synthetic campfire (roar + crackle + pops)
step_single.wav    : first footstep cut from sound_effects/snd_step.wav"""
import wave, numpy as np
from pathlib import Path
N=Path(__file__).resolve().parents[1]; E=N.parents[1]; OUT=N/'audio'; OUT.mkdir(exist_ok=True)
SR=44100; rng=np.random.default_rng(1103)

def onepole(x,a):   # simple low-pass
    y=np.empty_like(x); acc=0.
    for i,v in enumerate(x): acc+=a*(v-acc); y[i]=acc
    return y
def lowpass(x,cut):   # FFT brickwall-ish smooth low-pass (fast)
    X=np.fft.rfft(x); f=np.fft.rfftfreq(len(x),1/SR); X*=1/(1+(f/cut)**4); return np.fft.irfft(X,len(x))
def bandpass(x,lo,hi):
    X=np.fft.rfft(x); f=np.fft.rfftfreq(len(x),1/SR); X*=(1/(1+(lo/np.maximum(f,1))**4))*(1/(1+(f/hi)**4)); return np.fft.irfft(X,len(x))

def fire(seconds=24.):
    n=int(SR*seconds); t=np.arange(n)/SR
    roar=lowpass(rng.standard_normal(n),260.)
    flicker=1+.35*lowpass(rng.standard_normal(n),1.6)/np.std(lowpass(rng.standard_normal(n),1.6))
    roar*=np.clip(flicker,.4,1.8); roar/=np.max(np.abs(roar))
    hiss=bandpass(rng.standard_normal(n),2500.,9000.); hiss/=np.max(np.abs(hiss))
    out=np.zeros((n,2))
    out+=np.stack([roar,np.roll(roar,37)],1)*.30+np.stack([hiss,np.roll(hiss,91)],1)*.035
    # crackles: Poisson events, short filtered bursts, random stereo
    def burst(length,lo,hi,decay):
        b=bandpass(rng.standard_normal(length),lo,hi); env=np.exp(-np.arange(length)/(decay*SR)); return b*env/np.max(np.abs(b)+1e-9)
    rate=9.0; tcur=0.
    while True:
        tcur+=rng.exponential(1/rate)
        if tcur>seconds-0.2: break
        i=int(tcur*SR); big=rng.random()<.06
        L=int(SR*(.05 if big else rng.uniform(.004,.02)))
        b=burst(L,700. if big else 1500.,7000. if big else 11000.,.012 if big else rng.uniform(.0015,.005))*(rng.uniform(.5,1.)*(.9 if big else rng.uniform(.15,.45)))
        pan=rng.uniform(.25,.75); out[i:i+L,0]+=b*(1-pan)*1.4; out[i:i+L,1]+=b*pan*1.4
        if rng.random()<.25:   # crackle clusters
            for k in range(rng.integers(2,6)):
                j=i+int(SR*rng.uniform(.01,.12)); L2=int(SR*rng.uniform(.002,.008))
                if j+L2<n: b2=burst(L2,2000.,12000.,.002)*rng.uniform(.08,.3); out[j:j+L2,0]+=b2*(1-pan)*1.4; out[j:j+L2,1]+=b2*pan*1.4
    # seamless loop: crossfade last 1.5 s into the start
    xf=int(SR*1.5); w=np.linspace(0,1,xf)[:,None]
    out[:xf]=out[:xf]*w+out[-xf:]*(1-w); out=out[:-xf]
    return out/np.max(np.abs(out))*.85

def write(path,data,sr=SR):
    data=np.clip(data,-1,1); pcm=(data*32767).astype('<i2')
    with wave.open(str(path),'wb') as w: w.setnchannels(pcm.shape[1] if pcm.ndim>1 else 1); w.setsampwidth(2); w.setframerate(sr); w.writeframes(pcm.tobytes())

write(OUT/'camp_fire_loop.wav',fire())
w=wave.open(str(E/'sound_effects/snd_step.wav')); sr=w.getframerate(); ch=w.getnchannels()
d=np.frombuffer(w.readframes(w.getnframes()),np.int16).astype(float).reshape(-1,ch)/32768.
step=d[int(.10*sr):int(.62*sr)].copy(); fade=np.linspace(1,0,int(.12*sr))[:,None]; step[-len(fade):]*=fade
step[:int(.004*sr)]*=np.linspace(0,1,int(.004*sr))[:,None]
write(OUT/'step_single.wav',step,sr)
print('wrote',sorted(p.name for p in OUT.iterdir()))
