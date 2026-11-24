#!/usr/bin/env python3
"""Compare matched native campsite/creek views, retaining every timing row.

This is a static-view comparison, never a sustained/gameplay FPS certificate.
Preparation, output report and raw profiling hashes are verified and pinned.
"""
from pathlib import Path
import argparse, csv, hashlib, json, math, sys
sys.dont_write_bytecode=True
N=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(N))
from freeze_review import preparation_inputs, sha
from review_frame_pacing import build_report

DEFAULTS={
 'baseline_camp':'review/forest-views/look-hardware-v1/baseline/camp-eye',
 'candidate_camp':'review/forest-views/look-hardware-v4/candidate/camp-eye',
 'baseline_water':'review/forest-views/look-hardware-v2/baseline/creek-surface',
 'candidate_water':'review/forest-views/look-hardware-v4/candidate/creek-surface',
}
GRAPHICS=('preset','automatic','target_fps','display_width','display_height',
 'render_width','render_height','effective_scale','minimum_observed_scale',
 'maximum_observed_scale','reflections','reflection_scale','contact_shadows',
 'volumetric_fog','volumetric_clouds')


def stage_statistics(rows):
    result={}
    for key in rows[0] if rows else ():
        if key=='frame' or not key.endswith('_ms'):continue
        values=[];invalid=0
        for row in rows:
            try:
                v=float(row[key])
                if not math.isfinite(v) or v<0:raise ValueError()
                values.append(v)
            except (ValueError,TypeError,KeyError):invalid+=1
        ordered=sorted(values)
        result[key]={
          'numeric_rows':len(values),'invalid_rows':invalid,
          'zero_rows_retained':sum(v==0 for v in values),
          'positive_rows':sum(v>0 for v in values),
          'mean_ms_including_zero_rows':math.fsum(values)/len(values) if values else None,
          'p95_ms_including_zero_rows':ordered[max(0,math.ceil(.95*len(ordered))-1)] if ordered else None,
          'max_ms':max(values) if values else None,
        }
    return result


def inspect(directory):
    directory=Path(directory).resolve();errors=[];pins={}
    execution=json.loads((directory/'EXECUTION.json').read_text())
    preparation=json.loads((directory/'PREPARATION.json').read_text())
    profile=json.loads((directory/'performance.json').read_text())
    if execution.get('passed') is not True:errors.append('Native execution did not pass')
    try:preparation_inputs(execution,directory/'EXECUTION.json',pins)
    except (RuntimeError,OSError,KeyError,ValueError) as error:errors.append(str(error))
    for name in ('EXECUTION.json','PREPARATION.json','FRAME_PACING_REVIEW.json',
                 'performance.json','performance.json.csv','performance.json.world.json'):
        p=directory/name
        if p.is_file():pins[str(p)]=sha(p)
        else:errors.append('Missing '+name)
    pacing=build_report(directory)
    for path,h in pacing['inputs'].items():pins[path]=h
    rows=[]
    with (directory/'performance.json.csv').open(newline='') as stream:
        rows=list(csv.DictReader(stream))
    warmup=profile.get('warmup_frames')
    post=[]
    for row in rows:
        try:
            if isinstance(warmup,int) and int(row['frame'])>warmup:post.append(row)
        except (ValueError,KeyError):pass
    graphics=execution.get('graphics',{})
    if execution.get('software_renderer') or 'llvmpipe' in profile.get('renderer','').lower():
        errors.append('Software renderer cannot be the native hardware comparison')
    return {
      'directory':str(directory),'passed':not errors,'errors':errors,
      'preparation':{k:preparation.get(k) for k in ('stage','view','generation','binary_sha256','camera',
                      'resolution','frames','warmup','quality_frozen','actual_settings_sha256','saved_settings_sha256')},
      'actual_renderer':profile.get('renderer'),
      'effective_graphics':{k:graphics.get(k) for k in GRAPHICS},
      'asset_streaming':execution.get('asset_streaming'),
      'authored_weather':execution.get('authored_weather'),
      'all_csv_rows_retained':len(rows),
      'all_timing_stage_statistics':stage_statistics(rows),
      'after_declared_warmup_stage_statistics':stage_statistics(post),
      'frame_pacing':pacing,
    },pins


def compare(before,after):
    errors=[]
    if before['preparation']['stage']!='baseline' or after['preparation']['stage']!='candidate':
        errors.append('Wrong baseline/candidate stages')
    for key in ('view','binary_sha256','camera','resolution','frames','warmup',
                'quality_frozen','actual_settings_sha256'):
        if before['preparation'][key]!=after['preparation'][key]:errors.append('Unmatched preparation '+key)
    if before['actual_renderer']!=after['actual_renderer']:errors.append('Different native renderers')
    for key in GRAPHICS:
        if before['effective_graphics'][key]!=after['effective_graphics'][key]:errors.append('Unmatched effective graphics '+key)
    if before['frame_pacing']['csv']['invalid_rows'] or after['frame_pacing']['csv']['invalid_rows']:
        errors.append('Invalid timing CSV rows')
    out={'passed':before['passed'] and after['passed'] and not errors,
         'matched_conditions':not errors,'comparison_errors':errors,
         'baseline':before,'candidate':after,
         'frame_time_deltas_ms':{}}
    for scope in ('all_positive_intervals','after_declared_warmup'):
        left=before['frame_pacing']['overall_csv_intervals'].get(scope) or {}
        right=after['frame_pacing']['overall_csv_intervals'].get(scope) or {}
        out['frame_time_deltas_ms'][scope]={key:(right[key]-left[key]) if
             isinstance(left.get(key),(int,float)) and isinstance(right.get(key),(int,float)) else None
             for key in ('mean_ms','median_ms','p95_ms','p99_ms','max_ms')}
    return out


def build(paths):
    result={'schema':'valwood.forest-matched-native-performance.v1',
       'passed':True,'normal_playable_frame_pacing_certified':False,
       'sustained_fps_certified':False,'AAA_quality_certified':False,
       'capture_or_state_based_exclusions_applied':False,'inputs_sha256':{},'comparisons':{},
       'limitations':[
         'Two bounded static views on this machine; not normal controls, a full route, or sustained gameplay FPS.',
         'All CSV timing rows remain in source evidence and statistics; zero/invalid intervals are counted explicitly.',
         'Declared warmup statistics are shown alongside all measured intervals; no slow-frame or capture exclusions are applied.',
         'GPU stage samples are asynchronous delayed estimates, may contain unavailable zero rows, and must not be added as a disjoint frame total.',
         'World art and weather deliberately differ; matched settings/camera/resolution only isolate this whole scenery/shader pass, not individual causes.',
       ]}
    for label in ('camp','water'):
        before,pins=inspect(paths['baseline_'+label]);result['inputs_sha256'].update(pins)
        after,pins=inspect(paths['candidate_'+label]);result['inputs_sha256'].update(pins)
        row=compare(before,after);result['comparisons'][label]=row
        result['passed']=result['passed'] and row['passed']
    for p in (Path(__file__).resolve(),N/'tools/freeze_review.py',N/'tools/review_frame_pacing.py'):
        result['inputs_sha256'][str(p)]=sha(p)
    return result


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name,default in DEFAULTS.items():ap.add_argument('--'+name.replace('_','-'),default=default)
    ap.add_argument('--output',default='review/MATCHED_NATIVE_PERFORMANCE.json')
    args=ap.parse_args();paths={k:(Path(getattr(args,k)) if Path(getattr(args,k)).is_absolute() else N/getattr(args,k)) for k in DEFAULTS}
    report=build(paths)
    output=Path(args.output);output=output if output.is_absolute() else N/output
    output.write_text(json.dumps(report,indent=2)+'\n')
    summary={'passed':report['passed'],'report':str(output),'comparisons':{}}
    for label,row in report['comparisons'].items():
        summary['comparisons'][label]={
          'matched_conditions':row['matched_conditions'],'errors':row['comparison_errors'],
          'baseline_all_mean_ms':row['baseline']['frame_pacing']['overall_csv_intervals']['all_positive_intervals']['mean_ms'],
          'candidate_all_mean_ms':row['candidate']['frame_pacing']['overall_csv_intervals']['all_positive_intervals']['mean_ms'],
          'baseline_post_warmup_mean_ms':row['baseline']['frame_pacing']['overall_csv_intervals']['after_declared_warmup']['mean_ms'],
          'candidate_post_warmup_mean_ms':row['candidate']['frame_pacing']['overall_csv_intervals']['after_declared_warmup']['mean_ms'],
        }
    print(json.dumps(summary,indent=2));raise SystemExit(0 if report['passed'] else 1)


if __name__=='__main__':main()
