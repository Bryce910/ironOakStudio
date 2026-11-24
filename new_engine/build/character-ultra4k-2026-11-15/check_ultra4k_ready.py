#!/usr/bin/env python3
"""Verify the optional character-4K candidate without changing prior releases."""
from pathlib import Path
import argparse,hashlib,importlib.util,json,os,struct,subprocess,sys,time
sys.dont_write_bytecode=True
K=Path(__file__).resolve().parent;E=K.parents[1];ROOT=E.parent
B=K.parent/'arena-closeup-finish-2026-11-15';J=K.parent/'arena-joust-2026-11-15'
RUNTIME=K/'runtime-native-v1';BINARY=RUNTIME/'native-bin/rpg'
READY=K/'ULTRA4K_READY.json';AUDIT=K/'BASELINE_AUDIT.json'
WRAPPERS=('Launch_Valwood_Arena.sh','Launch_Valwood_Arena_Visual.sh','Launch_Valwood_Joust.sh')
_cache={}
def need(ok,message):
    if not ok:raise RuntimeError(message)
def read(p):return json.loads(Path(p).read_text())
def stat(p):
    s=Path(p).stat();return [s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns]
def sha(p):
    key=tuple(stat(p))
    if key not in _cache:
        with Path(p).open('rb') as f:_cache[key]=hashlib.file_digest(f,'sha256').hexdigest()
    return _cache[key]
def inputs(items):
    need(isinstance(items,dict) and items,'Reviewed input hashes missing')
    for p,h in items.items():need(Path(p).is_file() and sha(p)==h,'Reviewed input changed: '+p)
def report(name):
    d=read(K/'review'/name);need(d.get('passed') is True,'Review incomplete: '+name)
    if 'inputs' in d:inputs(d['inputs'])
    return d
def image(path):
    with path.open('rb') as f:h=f.read(30)
    if path.suffix=='.png':need(h[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',h[16:24])==(1600,900),'Wrong native PNG dimensions: '+str(path))
    else:need(h[:2]==b'BM' and struct.unpack_from('<ii',h,18)==(1600,900),'Wrong native BMP dimensions: '+str(path))
def audit_baselines():
    need(not AUDIT.exists(),'Baseline audit already exists; it is immutable')
    for name in WRAPPERS:need(sha(ROOT/name)==sha(K/'previous-launchers'/name),'Root launcher changed before baseline audit: '+name)
    commands=[]
    for directory,checker,receipt in [(B,'check_closeup_ready.py','CLOSEUP_FINISH_READY.json'),(J,'check_joust_ready.py','JOUST_READY.json')]:
        command=[sys.executable,str(directory/checker),'--check-only'];result=subprocess.run(command,cwd=E,text=True,capture_output=True)
        need(result.returncode==0,'Published baseline failed: '+result.stdout[-1200:]+result.stderr[-1200:])
        commands.append({'command':command,'exit_code':result.returncode,'output':result.stdout.strip(),'receipt':str(directory/receipt),'sha256':sha(directory/receipt)})
    d={'schema':'valwood.ultra4k.baseline-audit.v1','passed':True,'before_root_promotion':True,'commands':commands,'previous_wrappers':{str(K/'previous-launchers'/name):sha(ROOT/name) for name in WRAPPERS},'baseline_receipts':{str(B/'CLOSEUP_FINISH_READY.json'):sha(B/'CLOSEUP_FINISH_READY.json'),str(J/'JOUST_READY.json'):sha(J/'JOUST_READY.json')}}
    AUDIT.write_text(json.dumps(d,indent=2,sort_keys=True)+'\n');print('PASS: both published baselines audited before coordinated launcher promotion.')
def audit_identity():
    a=read(AUDIT);need(a.get('passed') is True and a.get('before_root_promotion') is True,'Missing successful pre-promotion baseline audit')
    inputs(a['previous_wrappers']);inputs(a['baseline_receipts'])
    need(len(a['commands'])==2 and all(r['exit_code']==0 for r in a['commands']),'Baseline audits did not both pass')
def verify_build():
    # Reuse the immutable joust release's already-reviewed 104-unit source /
    # compiler-dependency verifier, pointed at this isolated candidate only.
    spec=importlib.util.spec_from_file_location('ultra_native_closure',J/'check_joust_ready.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
    m.N=K;m.SOURCE=K/'source-v1';m.RUNTIME=RUNTIME;m.BINARY=BINARY
    return m.verify_build()
def verify_cpu():
    inherited=report('INHERITED_CHARACTER_INPUTS_REVIEW.json')
    need(inherited.get('schema')=='valwood.ultra4k.inherited-character-inputs.v1' and inherited.get('cpu_only') is True and inherited.get('current_bytes_equal_published_baselines') is True and inherited.get('executed_native_receipts_or_scripts_modified') is False and len(inherited.get('references',[]))==7,'Inherited shader/equipment provenance incomplete')
    for row in inherited['references']:need(read(row['published_receipt'])[row['published_map']].get(row['path'])==row['sha256'] and sha(row['path'])==row['sha256'],'Inherited character reference differs from its published original')
    bank=report('ULTRA_TEXTURE_CPU_REVIEW.json')
    for field in ('original_three_tiers_byte_identical','original_three_tiers_independent_inodes','all_14_orcs_covered','all_18_knights_covered','genuine_native_resolution_only'):need(bank.get(field) is True,'Four-tier bank invariant failed: '+field)
    need(bank.get('original_source_and_pose_meshes_modified') is False and not bank.get('source_mapping_conflicts'),'Original source/pose geometry changed or mapping conflicts remain')
    need(bank.get('schema')=='valwood.characters.ultra4k.source-bank.v2' and bank.get('manifest_version')==2 and bank.get('source_identities')==673 and bank.get('manifest_variants')==2692,'Four-tier bank coverage incomplete')
    need(bank.get('ultra_data_map_bytes_equal_high') is True and bank.get('ultra_data_map_entries_equal_high')==443 and bank.get('native4k_color_upgrades')==30,'Ultra must retain exact High surface maps and native color-only upgrades')
    outputs=bank.get('output_files',{});need(len(outputs)==bank.get('distinct_variant_files')==2049,'Referenced texture file coverage differs')
    for name,meta in outputs.items():
        p=K/'character-textures'/name;need(p.resolve().is_relative_to(K/'character-textures') and p.is_file() and p.stat().st_size==meta['bytes'] and sha(p)==meta['sha256'],'Bank output bytes differ from audited source mapping: '+name)
    manifest=K/'character-textures/textures.manifest';need(sha(manifest)==bank.get('output_manifest_sha256'),'Final manifest differs from CPU bank review')
    need(manifest.read_text().splitlines()[0]=='VALWOOD_CHARACTER_TEXTURES 2','New bank is not the four-tier manifest version')
    color=report('ULTRA_COLOR_COOK.json');pixel=report('ULTRA_COLOR_PIXEL_REVIEW.json')
    need(color.get('registered_source_identities')==673 and color.get('genuine_4k_color_images',0)>0 and color.get('source_art_and_pose_meshes_modified') is False and color.get('source_data_calibration_modified') is False,'Color-only source classification incomplete')
    need(pixel.get('source_color_images',0)>0 and pixel.get('rgba_texels_checked',0)>0 and pixel.get('source_upscaling') is False and pixel.get('normal_and_ARM_calibration_change') is False and pixel.get('original_orc_RGB_and_alpha_exact') is True and pixel.get('knight_skin_source_resolution_retained_with_established_JPEG94_policy') is True,'Original color pixel/protected surface audit failed')
    # Earlier native-surface experiments are diagnostic history, not release
    # receipts: the final compact release is the color-only source-bank v2.
    code=report('ULTRA_CODE_CPU_REVIEW.json')
    need(code.get('schema')=='valwood.character-ultra4k.code-cpu-review.v1' and code.get('cpu_only') is True and code.get('native_GPU_required_separately') is True,'CPU review must distinguish native rendering proof')
    contracts=code['contracts']
    expected_contracts={'tiers':{'Low':512,'Medium':1024,'High':2048,'Ultra':4096},'default':'Medium','settings_version':5,'manifest_version':2,'manifest_columns':4,'inherit_sentinel':4,'compact_Ultra_color_profile':3,'compact_Ultra_surface_profile':2,'skeletal_High_and_Ultra_reference_ceiling_MiB':768,'skeletal_Low_and_Medium_reference_ceiling_MiB':256}
    for name,value in expected_contracts.items():need(contracts.get(name)==value,'CPU settings/texture policy changed: '+name)
    for name in ('presets_preserve_texture_preference','old_settings_migrate_to_Medium','v1_manifest_Ultra_falls_back_to_exact_High','all_old_tier_selection_policies_unchanged','Ultra_protects_head_hair_hand_color','Ultra_prefers_data_only_surface_fallback_before_other_colors','fallback_registered_bytes_only','hand_old_tiers_preserve_exact_prior_High_derivative_contract','hand_Ultra_classifies_curated_original_before_replacement','no_runtime_upscaling','no_draw_time_file_IO_added'):
        need(contracts.get(name) is True,'CPU compatibility invariant failed: '+name)
    expected_runs={base+suffix for base in ('world_graphics_settings_ultra_test','world_character_textures_test') for suffix in ('-compile','','-sanitized-compile','-sanitized')}|{'renderer-syntax'}
    runs=code.get('runs',[]);need(len(runs)==9 and {r['name'] for r in runs}==expected_runs,'Actual settings/manifest/sanitizer test executions missing')
    for run in runs:
        log=Path(run['log']);need(log.is_relative_to(K/'review') and run.get('exit_code')==0 and run.get('command') and log.is_file() and sha(log)==run.get('log_sha256'),'CPU command failed or its recorded log changed')
        if not run['name'].endswith('compile') and run['name']!='renderer-syntax':need(run.get('output','').startswith('PASS '),'CPU test lacks actual successful assertions')
    availability=report('ULTRA_SOURCE_AVAILABILITY.json')
    need(availability.get('source_headers_verified') is True and availability.get('original_assets_modified') is False and availability.get('all_14_orcs_have_genuine_4k_body_or_armor') is True,'Original-resolution source availability not verified')
    budget=report('ULTRA_CHARACTER_TEXTURE_BUDGET_CPU.json')
    need(budget.get('schema')=='valwood.character-ultra4k.actual-roster-budget.v1' and budget.get('cpu_only') is True and budget.get('native_GPU_required_separately') is True and budget.get('test_exit_code')==0 and budget.get('command'),'Actual roster budget test was not successfully executed')
    for name in ('raw_output','error_log'):need(sha(budget[name])==budget[name+'_sha256'],'Roster budget execution output changed: '+name)
    original=read(budget['raw_output'])
    need(all(budget.get(key)==value for key,value in original.items()),'Roster receipt differs from actual test stdout')
    baseline=budget['accepted_baseline'];need(sha(baseline['path'])==baseline['sha256'] and baseline.get('old_profile_cases_compared')==54 and baseline.get('exact_old_Low_Medium_High_selection_and_accounting_preserved') is True,'Legacy tier loader behavior was not compared against all 54 accepted cases')
    need(budget.get('bank_identities')==673 and budget.get('actual_roster_profiles')==72 and budget.get('protected_image_checks',0)>0,'18-knight/four-tier budget coverage incomplete')
    rows=budget.get('cases',[]);expected={(f'knight{i:02}',level) for i in range(1,19) for level in range(4)}
    need(len(rows)==72 and {(r['knight'],r['profile']) for r in rows}==expected,'Missing/repeated knight texture-budget case')
    old_rows=read(baseline['path'])['cases'];need(len(old_rows)==54 and {(r['knight'],r['profile']):r for r in rows if r['profile']<3}=={(r['knight'],r['profile']):r for r in old_rows},'Current loader changed actual Low/Medium/High accounting or selected image levels')
    for row in rows:
        ceiling=(768 if row['profile']>=2 else 256)*1024*1024
        need(row['budget']==ceiling and 0<row['bytes']<=ceiling and len(row['levels'])==row['images'] and all(0<=p<=row['profile'] for p in row['levels']),'Texture adaptation exceeds the existing bounded loader policy')
    for field in ('high_and_medium_knight12_loadable','medium_to_low_under_tighter_explicit_budget','unchanged_source_bytes','exact_registered_variant_bytes','deterministic_selection','unknown_and_corrupt_fail_closed','derived_arm_and_color_space_accounting','neutral_unchanged','loading_scope_restored'):
        need(budget.get(field) is True,'Budget invariant failed: '+field)
    return bank,budget
def launchers(promoted=False):
    for name,project,flag in [('launch_arena.sh','arena-project','VALWOOD_COMBAT_ARENA=1'),('launch_joust.sh','joust-project','VALWOOD_JOUST_ARENA=1')]:
        p=K/name;text=p.read_text();need(os.access(p,os.X_OK),'Stage launcher is not executable: '+name)
        for token in ('check_ultra4k_ready.py','--check-only','runtime-native-v1/native-bin/rpg',project,flag,'flock -n','unset LD_PRELOAD'):need(token in text,'Launcher lacks routing/isolation: '+token)
        need((K/project/'saves').is_dir() and not (K/project/'saves').is_symlink(),'Practice saves must be separate')
        link=K/project/'character-textures';need(link.is_symlink() and link.resolve()==K/'character-textures','Practice project selects another texture bank')
    if promoted:
        for name in WRAPPERS:
            text=(ROOT/name).read_text();expected='launch_joust.sh' if name.endswith('Joust.sh') else 'launch_arena.sh'
            need(os.access(ROOT/name,os.X_OK) and (str(K) in text or str(K.relative_to(ROOT)) in text) and expected in text,'Root wrapper does not select this coordinated candidate: '+name)
def native_execution(directory,build,kind):
    need(directory.is_absolute() and directory.is_relative_to(K/'review'),'Native evidence escapes candidate review directory')
    ex=read(directory/'EXECUTION.json');need(ex.get('exit_code')==0 and ex.get('native_passed') is True and ex.get('private_saves') is True and ex.get('native_GPU') is True,'Native run failed or is not isolated: '+str(directory))
    inputs(ex['inputs']);need(ex['inputs'].get(str(BINARY))==build['sha256'],'Native run used an earlier/different binary')
    pins=ex['inputs'];need(pins.get(str(RUNTIME/'READY.json'))==sha(RUNTIME/'READY.json'),'Native run omitted final build receipt')
    bank=read(K/'review/ULTRA_TEXTURE_CPU_REVIEW.json')
    need(pins.get(str(K/'review/ULTRA_TEXTURE_CPU_REVIEW.json'))==sha(K/'review/ULTRA_TEXTURE_CPU_REVIEW.json'),'Native run omitted final compact bank audit')
    for name,meta in bank['output_files'].items():need(pins.get(str(K/'character-textures'/name))==meta['sha256'],'Native run omitted an actual bank file: '+name)
    command=ex.get('command',[]);need(command and command[0]==str(BINARY) and '--play' in command and (kind=='settings' or '--hidden' in command) and '--benchmark' not in command and '--editor' not in command,'Native replay command uses another binary/mode')
    private=directory/'private-project';project=K/('joust-project' if kind=='joust' else 'arena-project')
    for flag,value in (('--project',str(private)),('--width','1600'),('--height','900'),('--scenic','city-walk')):need(flag in command and command[command.index(flag)+1]==value,'Native replay argument differs: '+flag)
    need((private/'saves').is_dir() and not (private/'saves').is_symlink(),'Native run used public saves')
    need(sha(private/'world.studio')==sha(project/'world.studio'),'Native scene differs from launch project')
    for shader in (project/'shaders').glob('*.glsl'):need(sha(private/'shaders'/shader.name)==sha(shader),'Native shader differs from launch project: '+shader.name)
    if kind=='joust':
        for folder in ('ground','scenery'):
            actual={str(p.relative_to(private/folder)):sha(p) for p in (private/folder).rglob('*') if p.is_file()}
            expected={str(p.relative_to(project/folder)):sha(p) for p in (project/folder).rglob('*') if p.is_file()}
            need(actual==expected,'Native course '+folder+' differs from launched course')
    return ex
def verify_native(build):
    summary=report('ULTRA_NATIVE_REVIEW.json');runs=summary.get('runs',[]);need(runs,'Native representative coverage missing')
    seen=set();capture_count=0;proofs=[];saved_settings=[];restarted_settings=[];orc_ids=set()
    for entry in runs:
        directory=Path(entry['directory']);kind=entry['kind'];quality=entry['quality'];ex=native_execution(directory,build,kind);seen.add((kind,quality));proofs.append(directory/'EXECUTION.json')
        if kind in ('knights','orcs'):
            need(ex.get('quality')==quality,'Native quality differs from reviewed tier')
            need('character_textures='+quality in (directory/'private-project/graphics.settings').read_text().splitlines(),'Native character startup setting differs from reviewed tier')
            raw=read(directory/('KNIGHT_PRESENTATION_RAW.json' if kind=='knights' else 'ORC_PRESENTATION_RAW.json'))
            need(raw.get('passed') is True and raw.get('error')=='' and raw.get('ordinary_start_and_loading') is True and raw.get('private_profile') is True and raw.get('events_injected') is False and raw.get('requested_texture_quality')==({'Low':0,'Medium':1,'High':2,'Ultra':3}[quality]),'Character survey bypassed ordinary loading/tier or failed')
            need(raw.get('characters')==raw.get('expected_characters') and raw.get('captures')==raw.get('expected_captures') and raw.get('captures',0)>0,'Character survey coverage incomplete')
            if kind=='knights':need(raw['characters']>=3,'Native knight close-up selection incomplete')
            elif quality=='Ultra':orc_ids.update(raw['selected_ids'])
            for row in raw['rows']:need(row.get('GL_errors')==0 and row.get('compact_failures')==0,'Character survey has GL/compact-render failure')
            gpu=read(directory/'TEXTURE_GPU.json')['orc_body']
            proofs.extend([directory/('KNIGHT_PRESENTATION_RAW.json' if kind=='knights' else 'ORC_PRESENTATION_RAW.json'),directory/'TEXTURE_GPU.json'])
            need(gpu.get('native_forwarded_gl_only') is True and gpu.get('passed') is True and gpu.get('draws_with_organic',0)>0 and gpu.get('uniform_reset_observed') is True,'Actual face/body GL texture witness missing')
            witnesses=gpu.get('texture_witnesses',[]);need({r['role'] for r in witnesses}=={'base','normal','arm'},'Character map roles missing at actual draws')
            base_limit=4096 if quality=='Ultra' else 2048
            for row in witnesses:
                limit=base_limit if row['role']=='base' else 2048
                need(row.get('enabled') is True and row.get('storage_observed') is True and row.get('encounters',0)>0 and 0<row['width']<=limit and 0<row['height']<=limit,'Uploaded character map exceeds role/tier / is unobserved')
            need(any(r['role']=='base' and max(r['width'],r['height'])==base_limit for r in witnesses),'Native color map did not reach actual selected resolution')
            paths=[]
            for view in raw['views']:
                path=directory/view['file'];need(path.parent==directory,'Native screenshot path escapes run');paths.append(path)
                need(view.get('width')==1600 and view.get('height')==900 and view.get('GL_errors')==0,'Native view has wrong dimensions or GL error')
            need(len(paths)==raw['captures'] and len(set(paths))==len(paths),'Native captures missing/duplicated')
            if kind=='knights' and quality=='Ultra':restarted_settings.append(ex)
        elif kind=='joust':
            need(quality==ex.get('quality')=='High' and 'character_textures=High' in (directory/'private-project/graphics.settings').read_text().splitlines(),'Joust regression must preserve the existing High profile')
            raw=read(directory/'JOUST_NATIVE_RAW.json');proofs.append(directory/'JOUST_NATIVE_RAW.json')
            need(raw.get('passed') is True and raw.get('error')=='' and raw.get('actual_SDL_event_route') is True and raw.get('contacts_injected') is False and raw.get('live_state_injected') is False and raw.get('difficulty')==0 and raw.get('passes')==1 and raw.get('match_starts')==raw.get('match_completions')==1 and raw.get('GL_errors')==raw.get('renderer_failures')==0,'High joust regression failed')
            need(raw.get('audio_dispatches',0)>0 and raw.get('renderer_shadow_draws',0)>0,'High joust feedback/shadows missing')
            need(raw.get('physical_contacts',0)>0 and raw.get('verified_contact_proxy_sweeps')==raw['physical_contacts'] and raw.get('input_events',0)>0 and raw.get('renderer_color_draws',0)>100,'Joust scoring/input/color witnesses missing')
            paths=[Path(p) for p in raw['capture_paths']];need(len(paths)==raw['captures'] and len(paths)>=7 and len(set(paths))==len(paths),'Joust screenshots missing/duplicated')
            need({'setup','charge-third-person','paused','charge-first-person','match-complete','pass1-result'}<={p.stem for p in paths} and any(p.stem.endswith('-impact') for p in paths),'Joust full-match visual route incomplete')
        elif kind=='settings':
            raw=read(directory/'ULTRA_UI_RAW.json');proofs.append(directory/'ULTRA_UI_RAW.json')
            need(quality=='Ultra' and raw.get('schema')=='valwood.ultra4k.actual-settings-ui.v1' and raw.get('passed') is True and raw.get('error')=='','Actual settings UI did not complete')
            for name in ('actual_SDL_input','private_project','draft_ultra_observed','saved_ultra','resident_unchanged_until_restart'):need(raw.get(name) is True,'Settings UI witness missing: '+name)
            need(raw.get('setting_injected') is False and raw.get('resident_quality')==2 and raw.get('active_preference')==3 and raw.get('input_events',0)>0 and raw.get('GL_errors')==0,'Settings changed resident maps in place or bypassed actual UI')
            saved=directory/'private-project/graphics.settings';initial=directory/'INITIAL_GRAPHICS.settings'
            need(sha(saved)==ex.get('saved_graphics_sha256') and 'character_textures=Ultra' in saved.read_text().splitlines() and 'character_textures=High' in initial.read_text().splitlines(),'Actual saved/initial texture preference differs')
            saved_settings.append(saved);paths=[Path(p) for p in raw['capture_paths']];need(len(paths)>=3 and len(paths)==len(set(paths)),'Settings UI captures missing/duplicated')
        else:raise RuntimeError('Unknown native proof kind: '+kind)
        need(set(directory.glob('*.bmp'))==set(paths),'Native screenshot inventory differs from final raw report')
        log=(directory/'native.log').read_text(errors='replace');need('GL_INVALID_' not in log and 'Mesa: User error' not in log,'Driver reported native GL failures')
        if kind in ('knights','orcs'):
            inherited=read(K/'review/INHERITED_CHARACTER_INPUTS_REVIEW.json')
            need('[Skeletal shader] '+inherited['external_vertex_shader'] in log and '[Skeletal hero candidate] CPU asset loaded: '+inherited['hero_path'] in log,'Native run loaded another inherited shader/hero')
        for path in paths:
            need(path.parent==directory and path.suffix=='.bmp','Native capture escapes its reviewed run');image(path);image(path.with_suffix('.png'));proofs.extend((path,path.with_suffix('.png')))
        capture_count+=len(paths)
    need({('knights','High'),('knights','Ultra'),('orcs','Ultra'),('joust','High'),('settings','Ultra')}<=seen,'Native High/Ultra character, settings or joust coverage incomplete')
    coverage=read(K/'review/ULTRA_TEXTURE_CPU_REVIEW.json')['coverage'];orcs=[r for r in coverage if not r['character'].startswith('knight')];largest=max(orcs,key=lambda r:r['upper_bound_unique_image_rgba8_mips']['Ultra'])['character']
    need(largest in orc_ids,'Actual Ultra GPU survey omitted the largest Orc body budget')
    need(any(ex['inputs'].get(str(saved))==sha(saved) and sha(Path(ex['command'][ex['command'].index('--project')+1])/'graphics.settings')==sha(saved) for saved in saved_settings for ex in restarted_settings),'Ultra startup did not reload the exact settings saved by the real GUI')
    visual=report('VISUAL_REVIEW.json')
    for path in proofs:need(visual['inputs'].get(str(path))==sha(path),'Visual review omitted final native evidence: '+str(path))
    return capture_count
def verify(promoted=False):
    build=verify_build();verify_cpu();launchers(promoted);captures=verify_native(build);return build,captures
def links():return {str(p):str(p.resolve()) for directory in ('arena-project','joust-project') for p in(K/directory).iterdir() if p.is_symlink()}
def collect():
    pins={}
    def pin(p):
        p=Path(p);need(p.is_file(),'Missing immutable release input: '+str(p));pins[str(p.absolute())]=sha(p)
    for folder in ('source-v1','character-textures'):
        for p in(K/folder).rglob('*'):
            if p.is_file():pin(p)
    for project in ('arena-project','joust-project'):
        for p in(K/project).rglob('*'):
            rel=p.relative_to(K/project)
            if p.is_file() and not any(x in rel.parts for x in ('saves',)) and p.name not in ('graphics.settings','arena_player_character.cfg','studio-layout.ini','world.studio.workspace'):pin(p)
    for p in K.iterdir():
        if p.is_file() and p.suffix in ('.py','.sh','.md','.json') and p.name not in (READY.name,'RELEASE_SWITCH.json'):pin(p)
    for p in(K/'review').rglob('*'):
        if p.is_file() and '__pycache__' not in p.parts and 'private-project' not in p.parts and p.suffix in ('.json','.py','.cpp','.c','.so','.png','.bmp','.log','.md'):pin(p)
    for name in ('READY.json','BUILD_PLAN.json','native-bin/rpg'):pin(RUNTIME/name)
    # Preserve the exact files used by accepted native runs, including their
    # private scene/settings snapshots. This is a finite execution input list;
    # it does not recurse through unrelated sibling release trees.
    for entry in read(K/'review/ULTRA_NATIVE_REVIEW.json')['runs']:
        for name in read(Path(entry['directory'])/'EXECUTION.json')['inputs']:pin(name)
    for name in read(K/'review/INHERITED_CHARACTER_INPUTS_REVIEW.json')['inputs']:pin(name)
    for p in (B/'CLOSEUP_FINISH_READY.json',J/'JOUST_READY.json',J/'check_joust_ready.py'):pin(p)
    for name in WRAPPERS:pin(K/'previous-launchers'/name);pin(ROOT/name)
    return pins
def publish():
    need(not READY.exists(),'Ultra release receipt is immutable');audit_identity();build,captures=verify(True)
    pins=collect();statistics={p:stat(p) for p in pins}
    d={'schema':'valwood.character-ultra4k.release.v1','ready':True,'native_only':True,'windows_work_performed':False,'binary_sha256':build['sha256'],'inputs':pins,'immutable_stats':statistics,'project_links':links(),'native_captures':captures,'scope':'Optional real-original character texture tier for the practice Arena and Joust. Low/Medium/High retained. Original-source limits and bounded material loading apply; older Demo/Editor not migrated.'}
    temp=READY.with_suffix('.tmp');temp.write_text(json.dumps(d,indent=2,sort_keys=True)+'\n');temp.replace(READY)
def check():
    d=read(READY);need(d.get('ready') is True and d.get('schema')=='valwood.character-ultra4k.release.v1','Ultra release is incomplete')
    # Immutable unchanged files use their recorded identity/size/timestamps;
    # changed metadata falls back to exact SHA, avoiding a full multi-GB bank
    # reread on every launch while still detecting changed texture bytes.
    for p,h in d['inputs'].items():need(Path(p).is_file() and (stat(p)==d['immutable_stats'][p] or sha(p)==h),'Published input changed: '+p)
    need(links()==d['project_links'],'Published project asset routing changed');audit_identity();launchers(True)
def main():
    ap=argparse.ArgumentParser();group=ap.add_mutually_exclusive_group()
    for option in ('audit-baselines','preflight','publish','check-only'):group.add_argument('--'+option,action='store_true')
    args=ap.parse_args()
    if args.audit_baselines:audit_baselines();return
    if args.preflight:_,n=verify(False);print(f'PASS: optional 4K candidate preflight, {n} native captures.');return
    if args.publish:publish()
    check();print('PASS: reviewed optional 4K character textures and Arena/Joust routing.')
if __name__=='__main__':
    try:main()
    except (OSError,ValueError,KeyError,TypeError,IndexError,RuntimeError) as error:sys.exit(str(error))
