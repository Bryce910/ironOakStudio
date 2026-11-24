#!/usr/bin/env python3
"""Verify native Studio mesh selection, a real X-axis drag/undo, imports and HDR.

Build first and close the live engine. Requires X11, wmctrl and Xtst. Only the
newly launched window receives input after exact executable/PID verification.
Uses disposable managed fixtures and a private document; does not change saves.
Workflow timings include explicit rebuilds and are not walking FPS benchmarks.
"""
from pathlib import Path
import argparse,ctypes,ctypes.util,sys,json,time,subprocess,re,hashlib,os
import importlib.util,math,struct,tempfile,shutil,uuid,atexit
import benchmark_graphics_settings as common

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
assert os.environ.get('VALWOOD_TEST_NATIVE_GL')=='1','Native GPU execution required'
r=Path(__file__).resolve().parents[1];out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
project=r/'projects/valwood_world';alias='qa_authoring_'+uuid.uuid4().hex
private=None
def cleanup_fixtures():
 shutil.rmtree(project/'models/imports'/alias,ignore_errors=True)
 (project/'environments'/(alias+'.hdr')).unlink(missing_ok=True)
 if private is not None:shutil.rmtree(private,ignore_errors=True)
atexit.register(cleanup_fixtures)
spec=importlib.util.spec_from_file_location('cooker',r/'tools/import_world_asset.py')
cooker=importlib.util.module_from_spec(spec);spec.loader.exec_module(cooker)
stem=cooker.cook(project,alias,(project/'models/bep_prop_barrel_near.glb',project/'models/bep_prop_barrel_mid.glb',None),.85)
(project/'environments').mkdir(exist_ok=True);hdr=project/'environments'/(alias+'.hdr')
with hdr.open('xb')as stream:
 stream.write(b'#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 32 +X 64\n')
 for y in range(32):
  for x in range(64):
   rgb=(.07+.035*math.sin(x/64*math.tau)**2,.09+.03*(1-y/32),.10+.05*math.cos(x/64*math.tau)**2)
   _,exponent=math.frexp(max(rgb));scale=math.ldexp(256.,-exponent)
   stream.write(bytes([min(255,int(v*scale))for v in rgb]+[exponent+128]))
private=Path(tempfile.mkdtemp(prefix='valwood-authoring-native-',dir='/tmp'))
production=project/'world.studio';production_before=production.read_bytes()if production.exists()else None
state=dict(root=str(r),out=str(out),stem=stem,hdr=str(hdr.relative_to(project)),document=str(private/'world.studio'),alias=alias,preferences=hashlib.sha256((project/'graphics.settings').read_bytes()).hexdigest(),production_doc_exists=production.exists())
# All X11 input is confined to the newly launched, PID-verified Valwood window.
x=ctypes.CDLL(ctypes.util.find_library('X11'));t=ctypes.CDLL(ctypes.util.find_library('Xtst'));D=ctypes.c_void_p;W=ctypes.c_ulong
x.XOpenDisplay.argtypes=[ctypes.c_char_p];x.XOpenDisplay.restype=D;x.XDefaultRootWindow.argtypes=[D];x.XDefaultRootWindow.restype=W
x.XTranslateCoordinates.argtypes=[D,W,W,ctypes.c_int,ctypes.c_int,ctypes.POINTER(ctypes.c_int),ctypes.POINTER(ctypes.c_int),ctypes.POINTER(W)];x.XTranslateCoordinates.restype=ctypes.c_int
x.XQueryPointer.argtypes=[D,W,ctypes.POINTER(W),ctypes.POINTER(W),ctypes.POINTER(ctypes.c_int),ctypes.POINTER(ctypes.c_int),ctypes.POINTER(ctypes.c_int),ctypes.POINTER(ctypes.c_int),ctypes.POINTER(ctypes.c_uint)];x.XQueryPointer.restype=ctypes.c_int
x.XDisplayWidth.argtypes=[D,ctypes.c_int];x.XDisplayWidth.restype=ctypes.c_int;x.XDisplayHeight.argtypes=[D,ctypes.c_int];x.XDisplayHeight.restype=ctypes.c_int
x.XSetInputFocus.argtypes=[D,W,ctypes.c_int,W];x.XGetInputFocus.argtypes=[D,ctypes.POINTER(W),ctypes.POINTER(ctypes.c_int)];x.XFlush.argtypes=[D];x.XCloseDisplay.argtypes=[D];t.XTestFakeMotionEvent.argtypes=[D,ctypes.c_int,ctypes.c_int,ctypes.c_int,W];t.XTestFakeButtonEvent.argtypes=[D,ctypes.c_uint,ctypes.c_int,W]
display=x.XOpenDisplay(None);assert display,'Cannot open native display'
root=x.XDefaultRootWindow(display)
rows=[]
def execute(name,frames,extra=None,interaction=False,document=True,scene='rainforest',view='arrival',first=True):
 env,_=common.native_environment();env['VALWOOD_QA_FRAME_TIMES_ONLY']='1'
 if first:env['VALWOOD_CAPTURE_VIEW']='first'
 else:env.pop('VALWOOD_CAPTURE_VIEW',None)
 if document:env['VALWOOD_QA_STUDIO_DOCUMENT']=state['document']
 if extra:env.update(extra)
 profile=out/(name+'.json');log=out/(name+'.log')
 if interaction:Path(state['document']).unlink(missing_ok=True)
 for suffix in ('','.world.json','.csv'):Path(str(profile)+suffix).unlink(missing_ok=True)
 command=[str(common.LAUNCHER),'--editor'if document else'--play','--scenic',scene,'--benchmark','--view',view,'--frames',str(frames),'--warmup','120','--width','1600','--height','900','--no-hud','--profile',str(profile),'--capture',str(out/(name+'.bmp'))]
 with log.open('w')as f:
  p=subprocess.Popen(command,cwd=r,env=env,stdout=f,stderr=subprocess.STDOUT)
  deadline=time.monotonic()+180;inputs=False
  try:
   while p.poll()is None:
    assert time.monotonic()<deadline,'Native run timed out'
    if interaction and not inputs:
     data=log.read_text(errors='replace');ready=re.findall(r'\[Studio interaction\] ready (\d+)((?: [0-9.\-]+){10})',data)
     if ready:
      windows=[line.split(None,4)for line in subprocess.check_output(['wmctrl','-lp'],text=True).splitlines()];window=next((v[0]for v in windows if len(v)>=5 and v[2]==str(p.pid)),None);assert window,'Owned SDL window missing'
      executable=os.readlink(f'/proc/{p.pid}/exe');assert executable==str(r/'rpg'),executable
      subprocess.run(['wmctrl','-ia',window],check=True);wid=int(window,16);x.XSetInputFocus(display,wid,2,0);x.XFlush(display);time.sleep(.2);focus=W();revert=ctypes.c_int();x.XGetInputFocus(display,ctypes.byref(focus),ctypes.byref(revert));print('Input focus',hex(focus.value),'owned',window,flush=True);ox=ctypes.c_int();oy=ctypes.c_int();child=W();assert x.XTranslateCoordinates(display,wid,root,0,0,ctypes.byref(ox),ctypes.byref(oy),ctypes.byref(child))
      def move(px,py):
       assert t.XTestFakeMotionEvent(display,-1,int(ox.value+px),int(oy.value+py),0),'Motion injection failed';x.XFlush(display)
      def button(down):
       assert t.XTestFakeButtonEvent(display,1,int(down),0),'Button injection failed';x.XFlush(display)
      def click(px,py):move(px,py);time.sleep(.12);button(True);time.sleep(.12);button(False);time.sleep(.2)
      values=list(map(float,ready[-1][1].split()));startx,starty,cx,cy,ux,uy,sx,sy,mx,my=values
      print('Owned window origin',ox.value,oy.value,'screen',x.XDisplayWidth(display,0),x.XDisplayHeight(display,0),'body click',cx,cy,flush=True)
      click(sx,sy);click(cx,cy)
      rr=W();ch=W();rx=ctypes.c_int();ry=ctypes.c_int();wx=ctypes.c_int();wy=ctypes.c_int();mask=ctypes.c_uint();x.XQueryPointer(display,wid,ctypes.byref(rr),ctypes.byref(ch),ctypes.byref(rx),ctypes.byref(ry),ctypes.byref(wx),ctypes.byref(wy),ctypes.byref(mask));print('Native pointer after click: root',rx.value,ry.value,'window',wx.value,wy.value,'mask',mask.value,flush=True)
      # Wait for the actual background triangle picker, then use its fresh handle location.
      end=time.monotonic()+8
      while time.monotonic()<end:
       data=log.read_text(errors='replace');fresh=re.findall(r'\[Studio interaction\] ready (\d+)((?: [0-9.\-]+){10})',data)
       if fresh and int(fresh[-1][0])>=1:values=list(map(float,fresh[-1][1].split()));break
       time.sleep(.05)
      else:raise AssertionError('Native mesh pick did not resolve after viewport click')
      startx,starty,cx,cy,ux,uy,sx,sy,mx,my=values;click(mx,my);move(startx,starty);time.sleep(.15);button(True);time.sleep(.15)
      for i in range(1,9):move(startx+40*i/8,starty);time.sleep(.04)
      button(False);time.sleep(.25);click(ux,uy);inputs=True;print('Native mouse: actual mesh click, X handle drag/release and Undo button sent',flush=True)
    time.sleep(.05)
  except BaseException:
   subprocess.run(['wmctrl','-ic',next((v.split()[0]for v in subprocess.check_output(['wmctrl','-lp'],text=True).splitlines()if v.split()[2]==str(p.pid)),'0')],check=False)
   try:p.wait(timeout=10)
   except subprocess.TimeoutExpired:p.terminate();p.wait(timeout=10)
   raise
  assert p.returncode==0,(name,p.returncode,log.read_text()[-3000:])
  assert not interaction or inputs,'Interaction readiness marker never arrived'
 assert profile.is_file(),log.read_text()[-3000:]
 world=json.loads(Path(str(profile)+'.world.json').read_text());metrics=json.loads(profile.read_text());assert not any(v in metrics['renderer'].lower()for v in ('llvmpipe','softpipe','software')),metrics['renderer'];assert not world['gl_error']and not world['remaining_texture_references'],world
 if interaction:
  assert world['studio']['applied']==2 and world['studio']['rejected']==2 and world['studio']['qa_phase']==5,world['studio']
  assert world['studio_tools']['asset_definitions']==1 and world['studio_tools']['groups']==1 and world['studio_tools']['hdr_enabled'] and world['studio_tools']['hdr_gpu_bytes']<8*1024*1024,world['studio_tools']
  assert world['studio_tools']['gizmo_drags']==1 and world['studio_tools']['gizmo_finishes']==1,world['studio_tools']
  assert world['studio_input']['mesh_hits']>=1 and world['studio_input']['ui_undos']==1 and abs(world['studio_input']['draft_first_delta_x'])<.0001,world['studio_input']
 from PIL import Image
 if(out/(name+'.bmp')).exists():Image.open(out/(name+'.bmp')).save(out/(name+'.png'))
 summary=dict(name=name,mean_fps=metrics['mean_fps'],one_percent_low=metrics['one_percent_low_fps'],frames_below45=metrics['frames_over_45fps_budget'],max_ms=metrics['max_ms'],studio=world['studio'],tools=world['studio_tools'],input=world['studio_input']);rows.append(summary);(out/'authoring-results.json').write_text(json.dumps(rows,indent=2)+'\n');print(json.dumps(summary),flush=True)
 return world
try:
 extra={'VALWOOD_QA_STUDIO_WORKFLOW':'1','VALWOOD_QA_STUDIO_TOOLS':'1','VALWOOD_QA_STUDIO_INTERACTION':'1','VALWOOD_QA_STUDIO_IMPORT_STEM':state['stem'],'VALWOOD_QA_STUDIO_ENVIRONMENT':state['hdr']}
 execute('authoring-interaction',1800,extra,True)
 execute('authoring-reload',600)
 assert hashlib.sha256((r/'projects/valwood_world/graphics.settings').read_bytes()).hexdigest()==state['preferences'],'Saved graphics preferences changed'
 assert (r/'projects/valwood_world/world.studio').exists()==state['production_doc_exists'],'Production authoring document changed'
 print('Native authoring workflow and reload: PASS; saved preferences/source document unchanged',flush=True)
finally:
 x.XCloseDisplay(display)
 shutil.rmtree(project/'models/imports'/alias)
 hdr.unlink(missing_ok=True)
 shutil.rmtree(private)
 assert (production.read_bytes()if production.exists()else None)==production_before,'Production document contents changed'
