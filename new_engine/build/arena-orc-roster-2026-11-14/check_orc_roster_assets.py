#!/usr/bin/env python3
"""Independent owned-character roster validation; no GPU or live-session writes."""
from pathlib import Path,PurePosixPath
import argparse,hashlib,importlib.util,json,re,sys
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent;ROOT=HERE/'arena_orcs_roster';OUT=HERE/'roster-review'
IDS={'orc01','orc02','orc03','orc04','orc05','orc06','azrak','costalmed','dardan','girra','gnur','kanit','magur','ursag'}
REPORT=OUT/'ROSTER_ASSET_REVIEW.json'
def require(ok,why):
 if not ok:raise RuntimeError(why)
def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def pin(p,inputs,expected=None):
 p=Path(p).resolve();require(p.is_file(),'Missing dependency: '+str(p));value=sha(p)
 require(expected is None or value==expected,'Changed dependency: '+str(p));inputs[str(p)]=value;return value
def safe_path(value,identity):
 require(isinstance(value,str)and 0<len(value)<=200 and re.fullmatch(r'[A-Za-z0-9_.\-/]+',value),'Invalid roster path')
 path=PurePosixPath(value);require(not path.is_absolute()and value==str(path)and all(p not in {'.','..',''}for p in value.split('/')),'Escaping/noncanonical roster path')
 require(path.parts[0]==identity,'A character cannot alias another character bank: '+value);return ROOT/path
def quoted(token):
 require(token.startswith('"')and token.endswith('"'),'Roster fields require explicit quotes')
 text=token[1:-1];result='';at=0
 while at<len(text):
  if text[at]=='\\':at+=1;require(at<len(text),'Invalid quoted roster field')
  result+=text[at];at+=1
 return result
def parse_cfg(text):
 tokens=re.findall(r'"(?:[^"\\]|\\.)*"|[^\s]+',text);require(tokens[:2]==['VALWOOD_ORC_ROSTER','1'],'Unknown roster config version');at=2;default=None;entries=[]
 while at<len(tokens):
  key=tokens[at];at+=1
  if key=='end':require(at==len(tokens),'Trailing roster data');break
  if key=='default':require(default is None and at<len(tokens),'Duplicate/missing default');default=tokens[at];at+=1
  elif key=='character':
   require(at+5<=len(tokens),'Truncated roster character');entry=dict(zip(('id','display_name','body_directory','anatomy_directory','materials_config'),[tokens[at],*[quoted(t)for t in tokens[at+1:at+5]]]))
   entries.append(entry);at+=5
  else:raise RuntimeError('Unknown roster token: '+key)
 else:raise RuntimeError('Roster end is missing')
 require(default and 1<=len(entries)<=32 and len({r['id']for r in entries})==len(entries)and default in {r['id']for r in entries},'Invalid roster default/entries')
 return default,entries
def inventory(inputs):
 path=HERE/'intake_roster/ORC_SOURCE_ROSTER.json';pin(path,inputs);doc=json.loads(path.read_text())
 require(doc.get('schema')=='valwood.owned-orc.source-roster.v1'and doc.get('passed')is True,'Owned source inventory did not pass')
 require(doc.get('character_count')==14 and doc.get('weapon_mesh_count')==16,'Owned source roster must contain14actual character bodies/16weapon meshes')
 rows={r['id']:r for r in doc['characters']};require(set(rows)==IDS and len(doc['characters'])==14,'Source inventory character identities differ')
 for p,h in doc['inputs'].items():pin(p,inputs,h)
 require(rows['dardan']['source_prefab_guid']=='91e1be665b1fec44a94bde22e25eef99','Dardan must use its actual prefab GUID, not its misleading filename')
 return rows
def no_symlink(p):
 relative=p.relative_to(ROOT)
 for i in range(1,len(relative.parts)+1):require(not (ROOT/Path(*relative.parts[:i])).is_symlink(),'Generated runtime bank cannot redirect through symlinks: '+str(p))
def relocated_gnur(inputs):
 origin=HERE/'ROSTER_STAGE_ORIGIN.json';pin(origin,inputs);reference=Path(json.loads(origin.read_text())['gnur_read_only_reference']).resolve();copy=ROOT/'gnur'
 before={str(p.relative_to(reference)):sha(p)for p in reference.rglob('*')if p.is_file()}
 current={str(p.relative_to(copy)):sha(p)for p in copy.rglob('*')if p.is_file()}
 require(current==before,'Relocated Gnur is not byte-identical to the immutable accepted bank')
 for relative,digest in current.items():
  p=copy/relative;no_symlink(p);pin(p,inputs,digest)
 return reference/'ORC_ASSETS.json',dict(inherited_geometry=True,reference=str(reference),physical_copy=str(copy),copied_files=len(current),byte_identical=True)
def child_contract(entry,source,inputs,helper):
 identity=entry['id'];guid=entry['source_prefab_guid'];require(guid==source['source_prefab_guid']and re.fullmatch('[0-9a-f]{32}',guid),'Character prefab identity mismatch: '+identity)
 for key in ['asset_manifest','body_directory','anatomy_directory','materials_config']:
  path=safe_path(entry[key],identity);no_symlink(path);require(path.exists(),'Missing runtime character path: '+str(path))
 actual=safe_path(entry['asset_manifest'],identity);pin(actual,inputs,entry['asset_manifest_sha256']);manifest=actual;relocation=None
 if identity=='gnur':manifest,relocation=relocated_gnur(inputs)
 result=helper.inspect(manifest,geometry_only=False);require(result['passed']and result['accepted'],'Character strict asset contract failed')
 inputs.update(result['inputs']);doc=json.loads(manifest.read_text())
 sources=doc['source_files'];hashes=set(sources.values())
 require(source['body']['sha256']in hashes and source['body_prefab']['sha256']in hashes,'Manifest does not pin the actual inventory body+prefab: '+identity)
 if identity!='gnur':require(doc.get('source_character_id')==identity and doc.get('source_prefab_guid')==guid,'Child manifest lost explicit source identity: '+identity)
 rigid=doc['rigid_weapon_primitives'];require(len(rigid)>=1,'No genuine held weapon primitive: '+identity)
 require(any(row['sha256']in hashes for row in source['weapons']),'Character held weapon source does not belong to its owned inventory: '+identity)
 mapping=json.loads((actual.parent/'ORC_TEXTURE_MAPPING.json').read_text());owned_weapon_hashes={row['sha256']for row in source['weapons']}
 if identity!='gnur':
  lineage={row['primitive_index']:row for row in mapping['primitive_mapping']}
  require({i for i,row in lineage.items()if row.get('is_weapon')}==set(rigid),'Rigid markers differ from actual held-weapon source primitive set')
  for i,row in lineage.items():
   if i in rigid:require(row['source_fbx_sha256']in owned_weapon_hashes,'Rigid primitive comes from another character weapon: '+identity)
   else:require(row['source_fbx_sha256']==source['body']['sha256'],'Body primitive comes from another character: '+identity)
 body=safe_path(entry['body_directory'],identity);anatomy=safe_path(entry['anatomy_directory'],identity);materials=safe_path(entry['materials_config'],identity)
 require(body==actual.parent/'body'and anatomy==actual.parent/'anatomy'and materials==actual.parent/'ORC_MATERIALS.cfg','Runtime config paths do not describe the inspected child bank')
 require(materials.read_text().split()==['VALWOOD_ORC_MATERIALS','1','original_textures','1','end'],'Original texture runtime status missing: '+identity)
 result.update(character_id=identity,source_prefab_guid=guid,runtime_manifest=str(actual),runtime_manifest_sha256=sha(actual),relocation=relocation,
               complete_roster_acceptance=False,actual_gpu_load_acceptance=False)
 return result
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--character',choices=sorted(IDS));parser.add_argument('--check-only',action='store_true');parser.add_argument('--reuse-children',action='store_true');args=parser.parse_args();require(not(args.character and args.reuse_children),'Reuse requires the final full14character roster');OUT.mkdir(exist_ok=True)
 if args.check_only:
  doc=json.loads(REPORT.read_text());require(doc.get('passed')is True and doc.get('strict_complete')is True and doc.get('character_count')==14,'Full actual roster has not passed')
  for p,h in doc['inputs'].items():pin(p,{},h)
  for p,h in doc['outputs_sha256'].items():pin(p,{},h)
  print('PASS current14character owned Orc file/material/anatomy roster contract');return
 inputs={};pin(Path(__file__),inputs);helper_path=HERE/'orc_child_contract.py';pin(helper_path,inputs);source=inventory(inputs)
 spec=importlib.util.spec_from_file_location('roster_child_contract',helper_path);helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
 if args.character:
  row=source[args.character];identity=row['id'];m=ROOT/identity/'ORC_ASSETS.json'
  entries=[dict(id=identity,display_name=row['display_name'],body_directory=identity+'/body',anatomy_directory=identity+'/anatomy',materials_config=identity+'/ORC_MATERIALS.cfg',asset_manifest=identity+'/ORC_ASSETS.json',asset_manifest_sha256=sha(m),source_prefab_guid=row['source_prefab_guid'])]
 else:
  path=ROOT/'ORC_ROSTER.json';cfg=ROOT/'ORC_ROSTER.cfg';pin(path,inputs);pin(cfg,inputs);doc=json.loads(path.read_text())
  require(doc.get('schema')=='valwood.owned-orc.roster.v1'and doc.get('default_id')=='gnur','Actual Orc roster schema/default mismatch');entries=doc['entries']
  require(len(entries)==14 and {r['id']for r in entries}==IDS,'Full roster does not advertise exactly the14owned body characters')
  default,parsed=parse_cfg(cfg.read_text());require(default==doc['default_id'],'Native and JSON default differ')
  require(parsed==[{k:r[k]for k in ('id','display_name','body_directory','anatomy_directory','materials_config')}for r in entries],'Native CFG and authoritative JSON entries/order differ')
 results=[];outputs={};folder=OUT/'child-contracts';folder.mkdir(exist_ok=True)
 for entry in entries:
  require(entry['display_name']==source[entry['id']]['display_name'],'Display identity differs from owned source inventory')
  path=folder/(entry['id']+'.json')
  if args.reuse_children:
   single_path=OUT/('SINGLE_CHARACTER_'+entry['id']+'.json');pin(single_path,inputs);single=json.loads(single_path.read_text())
   require(single.get('passed')is True and single.get('strict_complete')is False and single.get('character_count')==1 and single['characters'][0]['id']==entry['id'],'Missing fresh strict child proof: '+entry['id'])
   for p,h in single['inputs'].items():pin(p,inputs,h)
   for p,h in single['outputs_sha256'].items():pin(p,{},h)
   row=json.loads(path.read_text());actual=safe_path(entry['asset_manifest'],entry['id']);no_symlink(actual)
   require(row['passed']and row['accepted']and row['character_id']==entry['id']and row['source_prefab_guid']==entry['source_prefab_guid']==source[entry['id']]['source_prefab_guid']and row['runtime_manifest']==str(actual)and row['runtime_manifest_sha256']==sha(actual)==entry['asset_manifest_sha256'],'Runtime roster differs from independently checked child')
  else:
   row=child_contract(entry,source[entry['id']],inputs,helper);path.write_text(json.dumps(row,indent=2,sort_keys=True)+'\n')
  outputs[str(path)]=sha(path)
  results.append(dict(id=entry['id'],passed=True,body_poses=172,cap_poses=172,primitive_count=len(row['body_layouts']),profile=row['metadata']['measured_profile'],report=str(path),report_sha256=sha(path)))
  print('PASS strict owned character',entry['id'],flush=True)
 require(inputs=={p:sha(p)for p in inputs},'Source/geometry/materials changed during roster inspection')
 report=dict(schema='valwood.owned-orc.roster-asset-contract.v1',passed=True,strict_complete=not bool(args.character),character_count=len(results),expected_ids=sorted(IDS),reviewed_ids=sorted(r['id']for r in results),characters=results,
             inputs=inputs,outputs_sha256=outputs,actual_gpu_load_acceptance=False,actual_interactive_combat_acceptance=False,
             limits=['File/source/texture/172pose/anatomy contract only. Actual per-character physical contacts, sequential GPU resource release and native selection remain separate checks.'])
 target=OUT/('SINGLE_CHARACTER_'+args.character+'.json')if args.character else REPORT;target.write_text(json.dumps(report,indent=2,sort_keys=True)+'\n')
 print(json.dumps(dict(passed=True,strict_complete=report['strict_complete'],character_count=len(results),report=str(target))))
if __name__=='__main__':main()
