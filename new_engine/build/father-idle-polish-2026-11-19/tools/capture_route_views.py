"""Observe actual road gameplay; no input, camera or story state is changed."""
from pathlib import Path
import argparse,json,re,subprocess,time

ap=argparse.ArgumentParser();ap.add_argument('review');a=ap.parse_args()
D=Path(a.review).resolve();targets=[5342.,5432.,5522.];records=[];end=time.monotonic()+720
while targets and time.monotonic()<end:
    rows=(D/'native.log').read_text(errors='replace').splitlines()
    traces=[re.search(r'TRACE frame(\d+) phase24 stagetown player(-?[\d.]+) ([\d.]+)',row) for row in rows]
    traces=[m for m in traces if m]
    if not traces or float(traces[-1][3])<targets[0]:time.sleep(.3);continue
    latest=traces[-1]
    if (D/'EXECUTION.json').exists():break
    for row in subprocess.check_output(['wmctrl','-lpG'],text=True).splitlines():
        if 'Legends of Valwood' not in row:continue
        fields=row.split();wid=fields[0];pid=fields[2]
        try:cmd=Path('/proc/'+pid+'/cmdline').read_bytes().split(b'\0')
        except OSError:continue
        if not any(D.name.encode() in arg and b'/tmp/valwood-father-idle-polish-' in arg for arg in cmd):continue
        info=subprocess.check_output(['xwininfo','-id',wid],text=True)
        x=int(re.search(r'Absolute upper-left X:\s*(-?\d+)',info)[1]);y=int(re.search(r'Absolute upper-left Y:\s*(-?\d+)',info)[1]);w=int(re.search(r'Width:\s*(\d+)',info)[1]);h=int(re.search(r'Height:\s*(\d+)',info)[1])
        target=targets.pop(0);path=D/f'actual-road-after-{int(target)}.png'
        subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-y','-f','x11grab','-video_size',f'{w}x{h}','-i',f':0.0+{x},{y}','-frames:v','1','-threads','1',str(path)],check=True)
        records.append(dict(path=str(path),target_z=target,last_trace_frame=int(latest[1]),last_trace_x=float(latest[2]),last_trace_z=float(latest[3]),actual_camera_not_exactly_synchronized_with_trace=True,external_compositor_capture=True,inputs_injected=False))
        break
    time.sleep(.3)
(D/'EXTERNAL_ROAD_VIEWS.json').write_text(json.dumps(dict(captures=records,missing_targets=targets,scope='Observed native compositor frames; approximate source trace location. Capture overhead remains in measured timings.'),indent=2)+'\n')
print('Actual road views:',len(records))
