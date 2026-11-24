"""Freeze reviewed art/shader inputs and final native evidence; never publish."""
from pathlib import Path
import argparse,json,sys
sys.dont_write_bytecode=True
N=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(N))
from check_unified import B,sha,read,need,verify_reuse,scene_scope,tree,mutable,catalog_transition,resource_overlays,shader_delivery

GROUPS=('scene_input_sha256','shader_input_sha256','inputs_sha256','input_sha256','private_input_sha256',
        'source_inputs_sha256')
VIEWS={'camp':'camp-eye','west_creek':'camp-west-creek','water':'creek-surface','road':'village-road'}


def matched_native_matrix(paths):
    """Require the actual eight final receipts and their matched performance report."""
    selected={};performance=[]
    for path in paths:
        doc=read(path)
        if path.name=='EXECUTION.json' and doc.get('schema')=='valwood.forest-matched-views.execution.v1':
            key=(doc.get('stage'),doc.get('view'))
            need(key not in selected,'Duplicate native stage/view selection')
            selected[key]=path
        if doc.get('schema')=='valwood.forest-matched-native-performance.v1': performance.append((path,doc))
    expected={(stage,view) for stage in ('baseline','candidate') for view in VIEWS.values()}
    need(set(selected)==expected,'Four final baseline/candidate native view pairs are required')
    need(len(performance)==1,'One final matched native performance report is required')
    performance_path,report=performance[0]
    need(report.get('passed') is True and set(report.get('comparisons',{}))==set(VIEWS) and
         report.get('sustained_fps_certified') is False and report.get('AAA_quality_certified') is False and
         report.get('capture_or_state_based_exclusions_applied') is False,
         'Matched native performance report scope differs')
    need(report.get('shader_delivery')==shader_delivery(),
         'Matched comparison shader delivery differs from the reviewed override')
    inputs=report.get('inputs_sha256',{})
    for tool in ('compare_forest_performance.py','freeze_review.py','review_frame_pacing.py'):
        path=N/'tools'/tool
        need(inputs.get(str(path))==sha(path),'Performance report used another helper: '+tool)
    keys=('view','camera','resolution','frames','warmup','quality_frozen','actual_settings_sha256')
    for label,view in VIEWS.items():
        row=report['comparisons'][label]
        need(row.get('passed') is True and row.get('matched_conditions') is True and not row.get('comparison_errors'),
             'Native performance pair is not matched: '+view)
        preps=[];execs=[]
        for stage in ('baseline','candidate'):
            path=selected[(stage,view)];prep_path=path.with_name('PREPARATION.json')
            prep=read(prep_path);execution=read(path);record=row[stage]
            need(Path(record.get('directory','')).resolve()==path.parent.resolve() and
                 record.get('passed') is True and record.get('csv_frame_ids_match_declared_range') is True and
                 record.get('all_csv_rows_retained')==prep['frames'],
                 'Performance report does not describe the selected native directory: '+view)
            for raw in ('EXECUTION.json','PREPARATION.json','FRAME_PACING_REVIEW.json','performance.json',
                        'performance.json.csv','performance.json.world.json'):
                raw_path=path.parent/raw
                need(inputs.get(str(raw_path))==sha(raw_path),'Performance input missing/stale: '+str(raw_path))
            need(all(record.get('preparation',{}).get(key)==prep.get(key) for key in (*keys,'stage','generation','binary_sha256')),
                 'Performance preparation summary differs from selected input')
            need(record.get('water_quality')==execution.get('water_quality'),
                 'Performance water quality summary differs from selected input')
            need(isinstance(record.get('water_quality'),dict) and record['water_quality'].get('level') in
                 ('Low','Medium','High','Ultra') and record.get('effective_graphics'),
                 'Effective water/graphics summary missing from matched report')
            need(all(record.get('effective_graphics',{}).get(key)==execution.get('graphics',{}).get(key)
                     for key in record.get('effective_graphics',{})),
                 'Performance graphics summary differs from selected input')
            preps.append(prep);execs.append(execution)
        need(all(preps[0].get(key)==preps[1].get(key) for key in (*keys,'binary_sha256')) and
             execs[0].get('water_quality')==execs[1].get('water_quality') and
             row['baseline'].get('effective_graphics')==row['candidate'].get('effective_graphics'),
             'Selected native pair has unmatched conditions: '+view)
    return dict(performance_path=str(performance_path),performance_sha256=sha(performance_path),
                executions={stage+'/'+view:str(path) for (stage,view),path in selected.items()})


def pin_inputs(doc,pins):
    for group in GROUPS:
        values=doc.get(group,{})
        need(isinstance(values,dict),'Invalid input pin map: '+group)
        for name,h in values.items():
            p=Path(name);need(p.is_file() and sha(p)==h,'Receipt input changed: '+name)
            if not p.resolve().is_relative_to('/tmp'): pins[str(p.resolve())]=h


def pin_receipt(path,pins):
    doc=read(path);need(doc.get('passed') is True,'Review did not pass: '+str(path))
    pin_inputs(doc,pins);candidate=False
    if path.name=='EXECUTION.json':
        need(doc.get('schema')=='valwood.forest-matched-views.execution.v1','Unrecognized native execution schema')
        prep_path=path.with_name('PREPARATION.json');prep=read(prep_path)
        need(doc.get('preparation_sha256')==sha(prep_path),'Native preparation pin differs')
        need(prep.get('schema')=='valwood.forest-matched-views.preparation.v1','Wrong native preparation schema')
        stage=prep.get('stage');need(stage in ('baseline','candidate'),'Wrong native stage')
        generation=N if stage=='candidate' else B
        need(prep.get('generation')==str(generation),'Wrong native generation')
        for key in ('stage','view','binary_sha256','quality_frozen'):
            need(doc.get(key)==prep.get(key),'Native preparation differs: '+key)
        need(doc['binary_sha256']==sha(N/'runtime/rpg'),'Native runtime changed')
        need(Path(prep['command'][0]).resolve()==(N/'runtime/rpg').resolve(),'Native command used another binary')
        scene=generation/'world-project/world.studio';catalog=generation/'world-project/collision/structures/catalog'
        inputs=prep.get('input_sha256',{})
        need(inputs.get(str(scene))==sha(scene) and inputs.get(str(catalog))==sha(catalog),
             'Native preparation lacks final scene/catalog pins')
        private=Path(prep['private_project'])/'world.studio'
        need(prep.get('private_input_sha256',{}).get(str(private))==sha(private)==sha(scene),
             'Captured private world differs from frozen candidate')
        pin_inputs(prep,pins)
        bank=(generation/'shared/shaders').resolve()
        actual={str(p.relative_to(bank)):sha(p) for p in bank.rglob('*') if p.is_file()}
        loaded={str(Path(p).resolve().relative_to(bank)):h for p,h in inputs.items()
                if Path(p).resolve().is_relative_to(bank)}
        need(loaded==actual and len(loaded)==204,'Native preparation did not pin complete reused shader bank for its stage')
        pacing_path=path.with_name('FRAME_PACING_REVIEW.json')
        need(doc.get('frame_pacing_report_sha256')==sha(pacing_path),'Native frame pacing report differs')
        pacing=read(pacing_path);pin_inputs({'inputs_sha256':pacing.get('inputs',{})},pins)
        pins[str(pacing_path)]=sha(pacing_path);pins[str(prep_path)]=sha(prep_path)
        for item in path.parent.rglob('*'):
            if item.is_file(): pins[str(item)]=sha(item)
        candidate=stage=='candidate'
    pins[str(path)]=sha(path)
    return candidate


def freeze(evidence):
    need(not (N/'UNIFIED_READY.json').exists() and not (N/'PUBLICATION.json').exists(),
         'Published generations are immutable')
    binary,bank=verify_reuse();delta,additions=scene_scope()
    need(delta,'No candidate authoring delta')
    pins,aliases={},{}
    for name in ('runtime','shared','assets','motion','source-v1','world-project','arena-project','joust-project','area-projects','tools'):
        root=N/name
        if root.is_symlink():
            aliases[str(root)]=str(root.resolve())
            # Published baseline readiness anchors immutable directory contents.
            if root.is_file(): pins[str(root)]=sha(root)
            continue
        for p in sorted(root.rglob('*')):
            if mutable(p.relative_to(N)): continue
            if p.is_symlink(): aliases[str(p)]=str(p.resolve())
            if p.is_file(): pins[str(p)]=sha(p)
    for name in ('BASELINE.json','launch.py','check_unified.py','GUIDE.txt','review/LIVE_BEFORE.json','review/ACTIVE_SETTINGS.json'):
        pins[str(N/name)]=sha(N/name)
    for name in ('UNIFIED_READY.json','PUBLICATION.json','check_unified.py'):
        pins[str(B/name)]=sha(B/name)
    for relative,h in bank.items(): pins[str(N/'shared/shaders'/relative)]=h
    delivery=shader_delivery()
    if delivery['shader_override']:
        path=Path(delivery['shader_override']['path']);pins[str(path)]=sha(path)
    receipts={};candidate=False;selected_paths=[]
    for relative in evidence:
        rel=Path(relative);path=N/rel
        need(not rel.is_absolute() and '..'not in rel.parts and path.is_file() and not path.is_symlink() and
             path.resolve().is_relative_to(N/'review'),'Evidence must be an owned review receipt')
        candidate=pin_receipt(path,pins) or candidate;receipts[str(path)]=sha(path);selected_paths.append(path)
    need(candidate,'Final candidate native world evidence is required')
    matrix=matched_native_matrix(selected_paths)
    final=N/'review/FINAL_REVIEW.json';doc=read(final)
    need(doc.get('passed') is True and doc.get('visual_review_performed') is True and
         doc.get('AAA_quality_certified') is False,'Scoped final visual review is required')
    pins[str(final)]=sha(final)
    optional=N/'review/MODEL_ADDITIONS.json'
    if optional.exists(): pins[str(optional)]=sha(optional)
    transition=catalog_transition()
    if transition:
        for key in ('preimage_path','review_path'):
            path=Path(transition[key]);pins[str(path)]=sha(path)
        pin_inputs(read(transition['review_path']),pins)
    manifest=dict(schema='valwood.scene-art.changeset.v1',reviewed=True,runtime_reused=True,
        source_reused=True,Windows_work_performed=False,binary_sha256=binary,
        source_changes=[],**delivery,
        baseline=str(B),shader_sha256=bank,scene_delta=delta,model_additions=additions,
        catalog_transition=transition,resource_overlays=resource_overlays(),native_matrix=matrix,pins=pins,aliases=aliases,evidence=receipts)
    (N/'review/CHANGESET.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return manifest


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--evidence',action='append',required=True)
    args=ap.parse_args();manifest=freeze(args.evidence)
    print('Frozen scene-only delta',len(manifest['scene_delta']),'inputs',len(manifest['pins']))


if __name__=='__main__': main()
