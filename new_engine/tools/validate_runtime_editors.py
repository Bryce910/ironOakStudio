#!/usr/bin/env python3
"""Native isolated runtime-editor workflow and optional edited walking routes.

Build first and close the live engine. Only the new PID/executable-verified X11
window receives one real Graph Compile click. Production saves/settings remain
untouched. Apply pauses are correctness data, not walking FPS measurements.
"""
from pathlib import Path
import argparse,ctypes,ctypes.util,hashlib,json,os,re,subprocess,tempfile,time
import benchmark_graphics_settings as common
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else None

def click_owned(pid,x,y):
    assert os.readlink(f'/proc/{pid}/exe')==str(ROOT/'rpg')
    rows=[row.split(None,4) for row in subprocess.check_output(['wmctrl','-lp'],text=True).splitlines()]
    window=next(row[0] for row in rows if len(row)>2 and row[2]==str(pid))
    subprocess.run(['wmctrl','-ia',window],check=True)
    lib=ctypes.CDLL(ctypes.util.find_library('X11'));test=ctypes.CDLL(ctypes.util.find_library('Xtst'))
    D=ctypes.c_void_p;W=ctypes.c_ulong
    lib.XOpenDisplay.argtypes=[ctypes.c_char_p];lib.XOpenDisplay.restype=D
    lib.XDefaultRootWindow.argtypes=[D];lib.XDefaultRootWindow.restype=W
    lib.XTranslateCoordinates.argtypes=[D,W,W,ctypes.c_int,ctypes.c_int,ctypes.POINTER(ctypes.c_int),ctypes.POINTER(ctypes.c_int),ctypes.POINTER(W)]
    lib.XGetInputFocus.argtypes=[D,ctypes.POINTER(W),ctypes.POINTER(ctypes.c_int)]
    lib.XGetGeometry.argtypes=[D,W,ctypes.POINTER(W),ctypes.POINTER(ctypes.c_int),ctypes.POINTER(ctypes.c_int),ctypes.POINTER(ctypes.c_uint),ctypes.POINTER(ctypes.c_uint),ctypes.POINTER(ctypes.c_uint),ctypes.POINTER(ctypes.c_uint)]
    lib.XDefaultScreen.argtypes=[D];lib.XDefaultScreen.restype=ctypes.c_int
    lib.XDisplayWidth.argtypes=[D,ctypes.c_int];lib.XDisplayWidth.restype=ctypes.c_int
    lib.XDisplayHeight.argtypes=[D,ctypes.c_int];lib.XDisplayHeight.restype=ctypes.c_int
    lib.XQueryPointer.argtypes=[D,W,ctypes.POINTER(W),ctypes.POINTER(W),ctypes.POINTER(ctypes.c_int),ctypes.POINTER(ctypes.c_int),ctypes.POINTER(ctypes.c_int),ctypes.POINTER(ctypes.c_int),ctypes.POINTER(ctypes.c_uint)]
    lib.XQueryPointer.restype=ctypes.c_int
    lib.XSetInputFocus.argtypes=[D,W,ctypes.c_int,W];lib.XFlush.argtypes=[D];lib.XSync.argtypes=[D,ctypes.c_int];lib.XCloseDisplay.argtypes=[D]
    test.XTestFakeMotionEvent.argtypes=[D,ctypes.c_int,ctypes.c_int,ctypes.c_int,W];test.XTestFakeButtonEvent.argtypes=[D,ctypes.c_uint,ctypes.c_int,W]
    display=lib.XOpenDisplay(None);assert display
    try:
        wid=int(window,16);lib.XSetInputFocus(display,wid,2,0);lib.XSync(display,0)
        # Focus gain can run SDL's one-time relative-mode warmup, which warps
        # the pointer. Let that finish before positioning the real UI click.
        time.sleep(.3)
        state={}
        def pointer_at_target(phase):
            focus=W();revert=ctypes.c_int();lib.XGetInputFocus(display,ctypes.byref(focus),ctypes.byref(revert))
            root=W();child=W();rx=ctypes.c_int();ry=ctypes.c_int();wx=ctypes.c_int();wy=ctypes.c_int();mask=ctypes.c_uint()
            valid=lib.XQueryPointer(display,wid,ctypes.byref(root),ctypes.byref(child),ctypes.byref(rx),ctypes.byref(ry),ctypes.byref(wx),ctypes.byref(wy),ctypes.byref(mask))
            state.update(phase=phase,pid=pid,owned=hex(wid),focus=hex(focus.value),root=hex(root.value),child=hex(child.value),valid=bool(valid),pointer_root=[rx.value,ry.value],pointer_window=[wx.value,wy.value],target=[x,y],button_mask=hex(mask.value))
            print('[Runtime click] '+json.dumps(state,sort_keys=True),flush=True)
            assert not(mask.value&(1<<8)),'Primary mouse button is already held; no click injected: '+str(state)
            return valid and focus.value==wid and abs(wx.value-x)<=2 and abs(wy.value-y)<=2
        screen=lib.XDefaultScreen(display)
        print('[Runtime click] owned PID/window verified; display '+str([lib.XDisplayWidth(display,screen),lib.XDisplayHeight(display,screen)]),flush=True)
        pointer_at_target('after-focus-settle')
        positioned=False
        for _ in range(3):
            # Activation/WM layout may move the window: translate afresh on
            # every attempt rather than keeping a pre-focus origin.
            ox=ctypes.c_int();oy=ctypes.c_int();child=W()
            assert lib.XTranslateCoordinates(display,wid,lib.XDefaultRootWindow(display),0,0,ctypes.byref(ox),ctypes.byref(oy),ctypes.byref(child))
            root=W();gx=ctypes.c_int();gy=ctypes.c_int();width=ctypes.c_uint();height=ctypes.c_uint();border=ctypes.c_uint();depth=ctypes.c_uint()
            assert lib.XGetGeometry(display,wid,ctypes.byref(root),ctypes.byref(gx),ctypes.byref(gy),ctypes.byref(width),ctypes.byref(height),ctypes.byref(border),ctypes.byref(depth))
            print('[Runtime click] '+json.dumps(dict(attempt=_+1,window_origin=[ox.value,oy.value],window_size=[width.value,height.value],motion_root=[int(ox.value+x),int(oy.value+y)])),flush=True)
            assert test.XTestFakeMotionEvent(display,-1,int(ox.value+x),int(oy.value+y),0)
            lib.XSync(display,0);time.sleep(.08)
            if pointer_at_target('after-motion-'+str(_+1)):
                time.sleep(.06)
                if pointer_at_target('stable-'+str(_+1)):positioned=True;break
        assert positioned,'Owned window focus/pointer did not settle at the logged UI button: '+str(state)
        try:
            assert test.XTestFakeButtonEvent(display,1,1,0);lib.XFlush(display);time.sleep(.15)
        finally:
            test.XTestFakeButtonEvent(display,1,0,0);lib.XSync(display,0)
    finally:lib.XCloseDisplay(display)


def run(name,output,document,frames,workflow=False,scene='castle-finished-arcade',view='arrival',third=False):
    env,_=common.native_environment();env.update(VALWOOD_QA_FRAME_TIMES_ONLY='1',VALWOOD_QA_STUDIO_DOCUMENT=str(document))
    if not third:env['VALWOOD_CAPTURE_VIEW']='first'
    if workflow:env.update(VALWOOD_QA_RUNTIME_EDITORS='1',VALWOOD_QA_RUNTIME_UI='1')
    profile=output/(name+'.json');log=output/(name+'.log')
    for suffix in ('','.world.json','.csv'):Path(str(profile)+suffix).unlink(missing_ok=True)
    command=[str(common.LAUNCHER),'--editor'if workflow or name=='reload'else'--play','--scenic',scene,'--benchmark','--view',view,'--width','1600','--height','900','--frames',str(frames),'--warmup','120','--no-hud','--profile',str(profile),'--capture',str(output/(name+'.bmp'))]
    with log.open('w')as stream:
        process=subprocess.Popen(command,cwd=ROOT,env=env,stdout=stream,stderr=subprocess.STDOUT)
        clicked=False;expanded=False;deadline=time.monotonic()+600
        try:
            while process.poll() is None:
                if time.monotonic()>deadline:raise TimeoutError(name)
                if workflow and not clicked:
                    hits=re.findall(r'\[Runtime UI\] compile ([0-9.]+) ([0-9.]+)',log.read_text(errors='replace'))
                    if hits:click_owned(process.pid,*map(float,hits[-1]));clicked=True;print('Actual Graph Compile button clicked in owned window',flush=True)
                elif workflow and clicked and not expanded:
                    hits=re.findall(r'\[Runtime UI\] expand ([0-9.]+) ([0-9.]+)',log.read_text(errors='replace'))
                    if hits:click_owned(process.pid,*map(float,hits[-1]));expanded=True;print('Actual Graph workspace expanded in owned window',flush=True)
                time.sleep(.08)
        except BaseException:
            if process.poll()is None:process.terminate();process.wait(timeout=20)
            raise
    assert process.returncode==0 and profile.is_file(),(name,process.returncode,log.read_text()[-3000:])
    metrics=json.loads(profile.read_text());world=json.loads(Path(str(profile)+'.world.json').read_text())
    assert not any(s in metrics['renderer'].lower()for s in ('llvmpipe','software','softpipe'))
    assert not world['gl_error'] and not world['remaining_texture_references'],world
    runtime=world['runtime_editors'];assert runtime['actors']==3 and runtime['navigation_areas']==1 and runtime['particles']==2 and runtime['waters']==2 and runtime['graphs']==2,runtime
    if workflow:assert clicked and expanded and runtime['phase']==8 and runtime['graph_button_compiles']==1 and world['studio']['applied']==4 and world['studio']['rejected']==4,(runtime,world['studio'])
    if view=='river':assert world['benchmark_route']['complete']and not world['benchmark_route']['collision_blocked_steps']
    Image.open(output/(name+'.bmp')).save(output/(name+'.png'))
    summary=dict(name=name,mean_fps=metrics['mean_fps'],low_fps=metrics['one_percent_low_fps'],below45=metrics['frames_over_45fps_budget'],max_ms=metrics['max_ms'],runtime=runtime,studio=world['studio'])
    print(json.dumps(summary),flush=True)
    return summary


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--routes',action='store_true');parser.add_argument('--document',type=Path,help='Skip workflow and profile the full routes using this saved example');args=parser.parse_args()
    assert os.environ.get('VALWOOD_TEST_NATIVE_GL')=='1'
    output=args.output.resolve();output.mkdir(parents=True,exist_ok=True)
    settings=ROOT/'projects/valwood_world/graphics.settings';save=ROOT/'projects/valwood_world/world.studio';before=(digest(settings),digest(save));rows=[]
    try:
        with tempfile.TemporaryDirectory(prefix='valwood-runtime-',dir='/tmp')as folder:
            document=Path(folder)/'world.studio'
            if args.document:document.write_bytes(args.document.read_bytes())
            else:
                rows.append(run('workflow',output,document,1800,True));(output/'editable-example.world.studio').write_bytes(document.read_bytes())
                rows.append(run('reload',output,document,700))
            if args.routes or args.document:
                rows.append(run('edited-jungle-first',output,document,7700,scene='rainforest',view='river'))
                rows.append(run('edited-jungle-third',output,document,7700,scene='rainforest',view='river',third=True))
                rows.append(run('edited-castle-third',output,document,12000,scene='castle-west-street',view='river',third=True))
                rows.append(run('edited-arcade-first',output,document,4000,scene='castle-finished-arcade',view='river'))
            (output/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
    finally:assert (digest(settings),digest(save))==before,'Production settings or authoring document changed'
    print('Runtime workflow and saved preferences/source document: PASS',flush=True)

if __name__=='__main__':main()
