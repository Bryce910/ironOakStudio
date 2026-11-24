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
from freeze_review import pin_receipt, sha
from review_frame_pacing import build_report
from check_unified import B,N as GENERATION,shader_delivery,runtime_delivery

VIEWS={'camp':'camp-eye','west_creek':'camp-west-creek','water':'creek-surface','road':'village-road'}
DEFAULTS={stage+'_'+label:'review/forest-views/final-v1/'+stage+'/'+view
          for stage in ('baseline','candidate') for label,view in VIEWS.items()}
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
    try:pin_receipt(directory/'EXECUTION.json',pins)
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
    frames=preparation.get('frames'); declared_warmup=preparation.get('warmup')
    valid_frames=isinstance(frames,int) and not isinstance(frames,bool) and frames>0
    valid_warmup=(isinstance(declared_warmup,int) and not isinstance(declared_warmup,bool)
                  and valid_frames and 0<=declared_warmup<frames)
    if not valid_frames or not valid_warmup:errors.append('Invalid declared frames/warmup')
    if profile.get('frames')!=frames or profile.get('warmup_frames')!=declared_warmup:
        errors.append('Profile frames/warmup differ from native preparation')
    ids=[]
    for row in rows:
        try:ids.append(int(row['frame']))
        except (ValueError,TypeError,KeyError):ids.append(None)
    coverage=bool(valid_frames and ids==list(range(1,frames+1)))
    if not coverage:errors.append('Timing CSV frame coverage is not exactly1..declared frames')
    graphics=execution.get('graphics',{})
    if not isinstance(graphics,dict):graphics={}
    for key in GRAPHICS:
        value=graphics.get(key)
        if value is None or isinstance(value,(float,int)) and not math.isfinite(value):
            errors.append('Missing/non-finite effective graphics '+key)
    if execution.get('software_renderer') or 'llvmpipe' in profile.get('renderer','').lower():
        errors.append('Software renderer cannot be the native hardware comparison')
    return {
      'directory':str(directory),'passed':not errors,'errors':errors,
      'preparation':{k:preparation.get(k) for k in ('stage','view','generation','binary_sha256','camera',
                      'resolution','frames','warmup','quality_frozen','actual_settings_sha256','saved_settings_sha256')},
      'actual_renderer':profile.get('renderer'),
      'effective_graphics':{k:graphics.get(k) for k in GRAPHICS},
      'water_quality':execution.get('water_quality'),
      'asset_streaming':execution.get('asset_streaming'),
      'authored_weather':execution.get('authored_weather'),
      'all_csv_rows_retained':len(rows),
      'csv_frame_ids_match_declared_range':coverage,
      'all_timing_stage_statistics':stage_statistics(rows),
      'after_declared_warmup_stage_statistics':stage_statistics(post),
      'frame_pacing':pacing,
    },pins


def compare(before,after):
    errors=[]
    if before['preparation']['stage']!='baseline' or after['preparation']['stage']!='candidate':
        errors.append('Wrong baseline/candidate stages')
    for key in ('view','camera','resolution','frames','warmup',
                'quality_frozen','actual_settings_sha256'):
        if before['preparation'][key]!=after['preparation'][key]:errors.append('Unmatched preparation '+key)
    if before['actual_renderer']!=after['actual_renderer']:errors.append('Different native renderers')
    expected=(sha(B/'runtime/rpg'),sha(GENERATION/'runtime/rpg'))
    if (before['preparation']['binary_sha256'],after['preparation']['binary_sha256'])!=expected:
        errors.append('Comparison executables differ from the explicit published baseline/native candidate')
    if before['water_quality']!=after['water_quality']:errors.append('Different effective water quality')
    for key in GRAPHICS:
        if before['effective_graphics'][key]!=after['effective_graphics'][key]:errors.append('Unmatched effective graphics '+key)
    for label,record in (('baseline',before),('candidate',after)):
        csv_review=record['frame_pacing']['csv']
        if csv_review.get('error'):errors.append(label+' timing CSV parser error')
        if csv_review.get('invalid_rows'):errors.append(label+' invalid timing CSV rows')
        if csv_review.get('duplicate_frame_ids'):errors.append(label+' duplicate timing frame IDs')
        if (csv_review.get('rows')!=record['preparation']['frames'] or
                record.get('csv_frame_ids_match_declared_range') is not True):
            errors.append(label+' incomplete/noncontiguous timing frame coverage')
        for scope in ('all_positive_intervals','after_declared_warmup'):
            values=record['frame_pacing']['overall_csv_intervals'].get(scope) or {}
            if not isinstance(values.get('samples'),int) or values['samples']<=0:
                errors.append(label+' missing positive timing distribution '+scope)
        for key in GRAPHICS:
            value=record['effective_graphics'].get(key)
            if value is None or isinstance(value,(float,int)) and not math.isfinite(value):
                errors.append(label+' missing/non-finite effective graphics '+key)
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
       'runtime_reused':False,'CXX_source_reused':False,'shader_delivery':shader_delivery(),
       'runtime_delivery':runtime_delivery(),
       'limitations':[
         'Four bounded static views on this machine; not normal controls, a full route, or sustained gameplay FPS.',
         'All CSV timing rows remain in source evidence and statistics; zero/invalid intervals are counted explicitly.',
         'Declared warmup statistics are shown alongside all measured intervals; no slow-frame or capture exclusions are applied.',
         'GPU stage samples are asynchronous delayed estimates, may contain unavailable zero rows, and must not be added as a disjoint frame total.',
         'The explicit published baseline executable is compared against the reviewed native candidate rebuild. Two C++ source changes and the full art/terrain pass are combined; timing differences do not isolate a single cause. Both stages use the exact same 204 shader contents.',
       ]}
    for label in VIEWS:
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
