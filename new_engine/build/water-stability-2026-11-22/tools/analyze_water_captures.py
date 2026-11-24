#!/usr/bin/env python3
"""CPU-only comparison of synchronized native presentation captures."""
import argparse,csv,json,pathlib
import numpy as np
from PIL import Image

def read(d):
 rows=list(csv.DictReader((d/'frame-frames.csv').open()))
 matrices=list(csv.DictReader((d/'frame-matrices.csv').open()))
 assert len(rows)==32 and all(r['status']=='ok' for r in rows)
 frames={int(r['swap']):np.asarray(Image.open(d/f"frame-{int(r['capture_index']):03}.ppm")) for r in rows}
 swaps=sorted(frames)
 assert swaps==list(range(swaps[0],swaps[0]+32))
 m=np.array([[float(r[f'm{i}']) for i in range(16)] for r in matrices])
 return frames,{'frame_count':len(frames),'first_swap':min(frames),'last_swap':max(frames),'matrix_count':len(m),
  'matrix_m7_values':sorted(set(m[:,7].tolist())), 'matrix_m7_sign_changes':int(np.count_nonzero(np.sign(m[1:,7])!=np.sign(m[:-1,7]))),
  'matrix_max_adjacent_delta':float(np.max(np.abs(np.diff(m,axis=0))))}

def compare(a,b,rect):
 common=sorted(set(a)&set(b)); assert common
 x0,y0,x1,y1=rect
 stats=[]
 for frame in common:
  diff=np.abs(a[frame][y0:y1,x0:x1].astype(np.int16)-b[frame][y0:y1,x0:x1].astype(np.int16))
  stats.append((float(diff.mean()),float(np.mean(diff.max(axis=-1)>8)),float(np.mean(diff.max(axis=-1)>32))))
 means=np.array(stats)
 return {'matching_actual_swaps':len(common),'first_swap':common[0],'last_swap':common[-1],
  'rectangle_xyxy':rect,'mean_abs_channel_difference_255':float(means[:,0].mean()),'worst_frame_mean_abs_channel_difference_255':float(means[:,0].max()),
  'mean_fraction_pixels_any_channel_delta_gt8':float(means[:,1].mean()),'mean_fraction_pixels_any_channel_delta_gt32':float(means[:,2].mean())}

def main():
 p=argparse.ArgumentParser();p.add_argument('review',type=pathlib.Path);p.add_argument('--output',type=pathlib.Path);p.add_argument('--runs',nargs='+',default=['baseline-medium','baseline-full-refresh','candidate-medium']);args=p.parse_args()
 runs={name:read(args.review/name) for name in args.runs}
 out={'scope':'Native captured images, CPU-only analysis. Synchronous readback timings are not performance evidence. Creek view is dominated by steep-angle SSR and cannot independently demonstrate reduced planar-reflection flashing.','runs':{name:info for name,(_,info) in runs.items()},'comparisons':{}}
 pairs=[(args.runs[i],args.runs[j]) for i in range(len(args.runs)) for j in range(i+1,len(args.runs))]
 for an,bn in pairs:
  a,b=runs[an][0],runs[bn][0];h,w=next(iter(a.values())).shape[:2]
  out['comparisons'][f'{an}__{bn}']={'scene_excluding_top_ui':compare(a,b,[0,60,w,h]),'creek_central_water_sample':compare(a,b,[600,430,900,475])}
 result=json.dumps(out,indent=2)
 if args.output: args.output.write_text(result+'\n')
 print(result)
if __name__=='__main__':main()
