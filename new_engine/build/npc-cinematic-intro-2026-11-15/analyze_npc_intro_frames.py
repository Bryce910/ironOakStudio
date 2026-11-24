#!/usr/bin/env python3
"""Analyze exact rendered story telemetry; never synthesizes missing frames/events."""
from pathlib import Path
import hashlib,json,math
N=Path(__file__).resolve().parent;R=N/'review'
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def must(v,m):
 if not v:raise RuntimeError(m)
def main():
 inputs={str(Path(__file__).resolve()):sha(__file__)};results=[]
 for variant,report_name,folder in [('scene','NPC_INTRO_NATIVE_REVIEW.json','native-intro'),('voice','NPC_INTRO_VOICE_NATIVE_REVIEW.json','native-intro-voice')]:
  rp=R/report_name;report=json.loads(rp.read_text());must(report['passed'],'Native variant failed')
  fp=R/folder/'captures/NPC_INTRO_FRAMES.jsonl';rows=[json.loads(s) for s in fp.read_text().splitlines()]
  must(rows and len({r['frame'] for r in rows})==len(rows),'Repeated telemetry frame')
  must(all(a['frame']<b['frame'] for a,b in zip(rows,rows[1:])),'Telemetry reordered')
  active=[r for r in rows if r['active']];must(active,'No active intro')
  must(all(r['render_target']==r['display']==[1280,720] for r in active),'Intro did not render at native1280x720')
  must(all(all(math.isfinite(v) for v in r['root']+r['camera']) for r in active),'Nonfinite camera/root')
  must(all(r['spoken']==0 and not r['opening_controls'] for r in active),'Intro unlocked controls/questcredit')
  deltas=[]
  for c in range(3):
   a=[r for r in active if r['case']==c];must(a,'Missing playback scenario')
   root=a[0]['root'];yaw=a[0]['yaw'];delta=max(math.dist(root,r['root']) for r in a);angle=max(abs(math.remainder(r['yaw']-yaw,2*math.pi)) for r in a)
   must(delta<1e-4 and angle<1e-4,'Actual cinematic moved gameplay root/yaw')
   deltas.append({'case':c,'frames':len(a),'root_delta_m':delta,'yaw_delta_rad':angle})
  result={'variant':variant,'rendered_rows':len(rows),'active_rows':len(active),'native_target':[1280,720],'scenarios':deltas,'maximum_camera_fallback_counter':max(r['camera_fallbacks'] for r in rows)}
  if variant=='voice':
   live=[r for r in active if not r['paused_menu'] and r['focused']]
   must(all(r['media_mode']==1 and r['media_ready'] and r['media_decoded_frames']==0 for r in active),'Voice path became unavailable/movie mode')
   corrections=[]
   for a,b in zip(rows,rows[1:]):
    if a['active'] and b['active'] and a['case']==b['case'] and b['media_audio_frames']<a['media_audio_frames']:
     corrections.append({'frame':b['frame'],'from_sample':a['media_audio_frames'],'to_sample':b['media_audio_frames'],'phase':b['phase']})
   drift=[abs(r['media_audio_frames']/48000.-r['time']) for r in live]
   result.update(audio_rate=48000,observed_backward_cursor_corrections=len(corrections),observed_correction_rows=corrections,maximum_observed_cursor_vs_fixed_scene_clock_seconds=max(drift),exact_media_resync_counter_instrumented=False,real_time_audio_smoothness_certified=False,
    audio_timing_scope='Virtual60Hz story under slow software GL with wall-clock dummy audio can resynchronize. Observed cursor corrections are reported, not an exact Media resync counter. Separate real-time media CPU proof validates PCM clock/pause/gain.')
  inputs[str(rp)]=sha(rp);inputs[str(fp)]=sha(fp);results.append(result)
 out=R/'NPC_INTRO_FRAME_ANALYSIS.json';out.write_text(json.dumps({'schema':'valwood.npc-intro.frame-analysis.v1','passed':True,'variants':results,'inputs':inputs,'outputs':{},'limits':['Software GL/dummy audio and virtualcontrols do not certify hardwareFPS, audible playback or physical controllerfeel.','This checks actual rendered telemetry only; shot composition is reviewed separately.']},indent=2,sort_keys=True)+'\n');print('PASS',out)
if __name__=='__main__':main()
