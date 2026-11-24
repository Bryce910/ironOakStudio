#!/usr/bin/env python3
"""CPU-only fixed-camera temporal metrics, using explicit image-space ROIs."""
import argparse,csv,json,pathlib
import numpy as np
from PIL import Image

def stats(directory,rois):
 rows=list(csv.DictReader((directory/'frame-frames.csv').open()))
 assert len(rows)==32 and all(r['status']=='ok' for r in rows)
 swaps=np.array([int(r['swap']) for r in rows]); assert np.all(np.diff(swaps)==1)
 images=np.stack([np.asarray(Image.open(directory/f"frame-{int(r['capture_index']):03}.ppm")) for r in rows])
 mr=list(csv.DictReader((directory/'frame-matrices.csv').open()))
 m=np.array([[float(r[f'm{i}']) for i in range(16)] for r in mr]); assert len(m)==32
 out={'frame_count':len(rows),'first_swap':int(swaps.min()),'last_swap':int(swaps.max()),'m7_values':sorted(set(m[:,7].tolist())),
 'm7_sign_changes':int(np.count_nonzero(np.sign(m[1:,7])!=np.sign(m[:-1,7]))),'max_adjacent_matrix_element_jump':float(np.max(np.abs(np.diff(m,axis=0)))),'regions':{}}
 for name,rect in rois.items():
  x0,y0,x1,y1=rect;a=images[:,y0:y1,x0:x1].astype(np.float32)
  even=a[swaps%2==0].mean(axis=0);odd=a[swaps%2==1].mean(axis=0);pd=np.abs(even-odd)
  out['regions'][name]={'rectangle_xyxy':rect,'adjacent_frame_mean_absolute_rgb_delta_255':float(np.abs(a[1:]-a[:-1]).mean()),
   'two_frame_mean_absolute_rgb_delta_255':float(np.abs(a[2:]-a[:-2]).mean()),'odd_even_mean_image_absolute_rgb_delta_255':float(pd.mean()),
   'odd_even_fraction_pixels_any_channel_delta_gt32':float(np.mean(pd.max(axis=-1)>32)),
   'even_mean_channel_brightness_255':float(even.mean()),'odd_mean_channel_brightness_255':float(odd.mean())}
 return out

def main():
 p=argparse.ArgumentParser();p.add_argument('review',type=pathlib.Path);p.add_argument('--runs',nargs='+',required=True);p.add_argument('--output',type=pathlib.Path);args=p.parse_args()
 rois={'right_water':[1020,420,1560,800],'left_comparison':[10,340,450,640],'whole_scene_below_ui':[0,60,1600,900]}
 out={'scope':'Fixed bridge camera. Hand-selected image-space rectangles retain geometry/vegetation inside them; these are reproducible temporal samples, not semantic water masks. Synchronous captures cannot establish FPS/performance.','rois':rois,'runs':{run:stats(args.review/run,rois) for run in args.runs}}
 text=json.dumps(out,indent=2)
 if args.output:args.output.write_text(text+'\n')
 print(text)
if __name__=='__main__':main()
