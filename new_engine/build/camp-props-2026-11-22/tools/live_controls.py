"""Private normal-play UI review using the active published scene."""
from pathlib import Path
import ctypes as C, hashlib, importlib.util, json, os, shutil, subprocess, sys, time
N=Path(__file__).resolve().parents[1];E=N.parents[1];R=N/'review/live-controls-v2';Q=R/'project'

def launch():
    R.mkdir(exist_ok=False);Q.mkdir()
    manifest=json.loads((E.parent/'CURRENT_VALWOOD.json').read_text());B=Path(manifest['runtime_generation']);P=Path(manifest['working_world'])
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    protected={str(p):sha(p) for p in [P/'graphics.settings',P/'world.studio',*(P/'saves').glob('*')] if p.is_file()}
    for f in P.iterdir():
        if f.name in ('saves','shaders'):continue
        if f.is_dir():(Q/f.name).symlink_to(f.resolve(),target_is_directory=True)
        else:shutil.copy2(f,Q/f.name,follow_symlinks=True)
    shutil.copytree(P/'saves',Q/'saves')
    (Q/'shaders').symlink_to(B/'shared/shaders',target_is_directory=True)
    shutil.copy2(B/'review/high-tier-acceptance/project/graphics.settings',Q/'graphics.settings')
    spec=importlib.util.spec_from_file_location('live_launcher',B/'launch.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
    cmd,env,_=m.private_configuration('demo');cmd[cmd.index('--project')+1]=str(Q)
    env.update(VALWOOD_QA_GRAPHICS_FILE=str(Q/'graphics.settings'),SDL_GAMECONTROLLER_IGNORE_DEVICES='0x046d/0xc21d',LD_PRELOAD=str(B/'tools/probe_gl_debug.so'))
    (R/'launch.json').write_text(json.dumps(dict(command=cmd,manifest=manifest,protected=protected,environment={k:v for k,v in env.items() if k.startswith('VALWOOD_')}),indent=2)+'\n')
    with (R/'game.log').open('w') as out:
        p=subprocess.Popen(cmd,cwd=E,env=env,stdout=out,stderr=subprocess.STDOUT)
        (R/'pid').write_text(str(p.pid));print('Private normal-play PID',p.pid,flush=True);rc=p.wait();print('exit',rc,flush=True);sys.exit(rc)

def control():
    global X,T,D
    X=C.CDLL('libX11.so.6');T=C.CDLL('libXtst.so.6')
    X.XOpenDisplay.restype=C.c_void_p;D=X.XOpenDisplay(None);assert D
    X.XDefaultRootWindow.argtypes=[C.c_void_p];X.XDefaultRootWindow.restype=C.c_ulong
    X.XStringToKeysym.argtypes=[C.c_char_p];X.XStringToKeysym.restype=C.c_ulong
    X.XKeysymToKeycode.argtypes=[C.c_void_p,C.c_ulong];X.XKeysymToKeycode.restype=C.c_uint
    X.XFlush.argtypes=[C.c_void_p]
    X.XGetGeometry.argtypes=[C.c_void_p,C.c_ulong,C.POINTER(C.c_ulong),C.POINTER(C.c_int),C.POINTER(C.c_int),C.POINTER(C.c_uint),C.POINTER(C.c_uint),C.POINTER(C.c_uint),C.POINTER(C.c_uint)]
    X.XTranslateCoordinates.argtypes=[C.c_void_p,C.c_ulong,C.c_ulong,C.c_int,C.c_int,C.POINTER(C.c_int),C.POINTER(C.c_int),C.POINTER(C.c_ulong)]
    T.XTestFakeKeyEvent.argtypes=[C.c_void_p,C.c_uint,C.c_int,C.c_ulong]
    T.XTestFakeMotionEvent.argtypes=[C.c_void_p,C.c_int,C.c_int,C.c_int,C.c_ulong]
    T.XTestFakeRelativeMotionEvent.argtypes=[C.c_void_p,C.c_int,C.c_int,C.c_ulong]
    T.XTestFakeButtonEvent.argtypes=[C.c_void_p,C.c_uint,C.c_int,C.c_ulong]
    row=next(l for l in subprocess.check_output(['wmctrl','-lp'],text=True).splitlines() if 'Legends of Valwood | A Place in the Watch' in l)
    w=int(row.split()[0],16);subprocess.run(['wmctrl','-ia',hex(w)],check=True);time.sleep(.2)
    root=C.c_ulong();x=C.c_int();y=C.c_int();wid=C.c_uint();hei=C.c_uint();b=C.c_uint();dep=C.c_uint();child=C.c_ulong()
    X.XGetGeometry(D,w,C.byref(root),C.byref(x),C.byref(y),C.byref(wid),C.byref(hei),C.byref(b),C.byref(dep))
    X.XTranslateCoordinates(D,w,X.XDefaultRootWindow(D),0,0,C.byref(x),C.byref(y),C.byref(child))
    return x.value,y.value,wid.value,hei.value

def main():
    if sys.argv[1]=='launch':return launch()
    rect=control();action=sys.argv[1]
    if action=='shot':
        from PIL import ImageGrab
        x,y,w,h=rect;p=R/(sys.argv[2]+'.png');ImageGrab.grab().crop((x,y,x+w,y+h)).save(p);print(p)
    elif action=='key':
        keys=sys.argv[2].split('+');duration=float(sys.argv[3]) if len(sys.argv)>3 else .10;assert 0<duration<=20
        codes=[X.XKeysymToKeycode(D,X.XStringToKeysym(k.encode())) for k in keys];assert all(codes)
        count=int(sys.argv[4]) if len(sys.argv)>4 else 1;assert 1<=count<=8
        for _ in range(count):
            for k in codes:T.XTestFakeKeyEvent(D,k,1,0)
            X.XFlush(D);time.sleep(duration)
            for k in codes[::-1]:T.XTestFakeKeyEvent(D,k,0,0)
            X.XFlush(D)
            if count>1:time.sleep(.3)
    elif action=='click':
        x,y=map(int,sys.argv[2:4]);assert 0<=x<rect[2] and 0<=y<rect[3]
        T.XTestFakeMotionEvent(D,-1,rect[0]+x,rect[1]+y,0);X.XFlush(D);time.sleep(.08)
        T.XTestFakeButtonEvent(D,1,1,0);T.XTestFakeButtonEvent(D,1,0,0);X.XFlush(D)
    elif action=='look':
        dx,dy=map(int,sys.argv[2:4]);assert abs(dx)<=1000 and abs(dy)<=1000
        T.XTestFakeRelativeMotionEvent(D,dx,dy,0);X.XFlush(D)
    elif action=='aim':
        dx,dy=map(int,sys.argv[2:4]);assert abs(dx)<=1000 and abs(dy)<=1000
        T.XTestFakeButtonEvent(D,1,1,0);T.XTestFakeButtonEvent(D,1,0,0);X.XFlush(D);time.sleep(.25)
        T.XTestFakeRelativeMotionEvent(D,dx,dy,0);X.XFlush(D);time.sleep(.3)
    elif action=='wheel':
        button=int(sys.argv[2]);assert button in [4,5]
        count=int(sys.argv[3]) if len(sys.argv)>3 else 1;assert 1<=count<=12
        for _ in range(count):
            T.XTestFakeButtonEvent(D,button,1,0);T.XTestFakeButtonEvent(D,button,0,0);X.XFlush(D);time.sleep(.1)
    elif action=='release':
        for k in ('Alt_L','Alt_R','Shift_L','Shift_R','Control_L','Control_R','w','a','s','d','Tab'):
            code=X.XKeysymToKeycode(D,X.XStringToKeysym(k.encode()))
            T.XTestFakeKeyEvent(D,code,0,0)
        X.XFlush(D)
    else:raise ValueError(action)
    with (R/'actions.jsonl').open('a') as f:f.write(json.dumps(dict(time=time.time(),action=sys.argv[1:],bounds=rect))+'\n')

if __name__=='__main__':main()
