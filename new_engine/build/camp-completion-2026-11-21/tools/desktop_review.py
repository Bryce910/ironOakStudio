import ctypes as C,subprocess,time,os
from pathlib import Path
from PIL import ImageGrab
X=C.CDLL('libX11.so.6');T=C.CDLL('libXtst.so.6')
X.XOpenDisplay.restype=C.c_void_p;X.XDefaultRootWindow.argtypes=[C.c_void_p];X.XDefaultRootWindow.restype=C.c_ulong
X.XStringToKeysym.argtypes=[C.c_char_p];X.XStringToKeysym.restype=C.c_ulong
X.XKeysymToKeycode.argtypes=[C.c_void_p,C.c_ulong];X.XKeysymToKeycode.restype=C.c_uint
X.XFlush.argtypes=[C.c_void_p]
X.XGetGeometry.argtypes=[C.c_void_p,C.c_ulong,C.POINTER(C.c_ulong),C.POINTER(C.c_int),C.POINTER(C.c_int),C.POINTER(C.c_uint),C.POINTER(C.c_uint),C.POINTER(C.c_uint),C.POINTER(C.c_uint)]
X.XTranslateCoordinates.argtypes=[C.c_void_p,C.c_ulong,C.c_ulong,C.c_int,C.c_int,C.POINTER(C.c_int),C.POINTER(C.c_int),C.POINTER(C.c_ulong)]
T.XTestFakeKeyEvent.argtypes=[C.c_void_p,C.c_uint,C.c_int,C.c_ulong]
T.XTestFakeMotionEvent.argtypes=[C.c_void_p,C.c_int,C.c_int,C.c_int,C.c_ulong]
T.XTestFakeButtonEvent.argtypes=[C.c_void_p,C.c_uint,C.c_int,C.c_ulong]
D=X.XOpenDisplay(None)
def window():
 row=next(l for l in subprocess.check_output(['wmctrl','-lp'],text=True).splitlines() if 'Legends of Valwood | A Place in the Watch' in l)
 return int(row.split()[0],16)
def bounds():
 w=window();root=C.c_ulong();x=C.c_int();y=C.c_int();wid=C.c_uint();hei=C.c_uint();b=C.c_uint();dep=C.c_uint()
 X.XGetGeometry(D,w,C.byref(root),C.byref(x),C.byref(y),C.byref(wid),C.byref(hei),C.byref(b),C.byref(dep));child=C.c_ulong()
 X.XTranslateCoordinates(D,w,X.XDefaultRootWindow(D),0,0,C.byref(x),C.byref(y),C.byref(child));return x.value,y.value,wid.value,hei.value

def focus():subprocess.run(['wmctrl','-ia',hex(window())]);time.sleep(.3)
def key(name,hold=.08):
 code=X.XKeysymToKeycode(D,X.XStringToKeysym(name.encode()));T.XTestFakeKeyEvent(D,code,1,0);X.XFlush(D);time.sleep(hold);T.XTestFakeKeyEvent(D,code,0,0);X.XFlush(D)
def click(x,y):
 a,b,_,_=bounds();T.XTestFakeMotionEvent(D,-1,a+x,b+y,0);X.XFlush(D);time.sleep(.08);T.XTestFakeButtonEvent(D,1,1,0);T.XTestFakeButtonEvent(D,1,0,0);X.XFlush(D)
def shot(name):
 p=Path(__file__).resolve().parents[1]/'review'/os.environ.get('VALWOOD_REVIEW_RUN','baseline-native')/name;a,b,w,h=bounds();ImageGrab.grab().crop((a,b,a+w,b+h)).save(p);print(str(p),bounds())
if __name__=='__main__':
 import sys
 focus()
 if sys.argv[1]=='shot':shot(sys.argv[2])
 elif sys.argv[1]=='key':key(sys.argv[2],float(sys.argv[3]) if len(sys.argv)>3 else .08)
 elif sys.argv[1]=='click':click(int(sys.argv[2]),int(sys.argv[3]))
