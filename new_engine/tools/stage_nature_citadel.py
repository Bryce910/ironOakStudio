#!/usr/bin/env python3
"""Stage purchased prefab dependencies into the private Unity conversion project."""
from pathlib import Path
import json,re,shutil

ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'assets/purchased/nature_castle'
BRIDGE=ROOT.parent/'.local-tools/unity-source/asset-bridge'
SELECTION={
 'main_gate':'Prefab_Main_Gate',
 'outer_gate':'Prefab_Main_Gate_3',
 'barbican':'Prefab_Main_Gate_Barbican',
 'tower_base':'Prefab_Square_Tower_Foundation',
 'tower_level':'Prefab_Square_Tower_Level',
 'tower_top':'Prefab_Square_Tower_Level_Top',
 'tower_roof':'Prefab_Square_Tower_Level_Roof',
 'tower_ornament':'Prefab_Square_Tower_Ornaments',
 'wall_ornament':'Prefab_Temple_Wall_Ornament',
 'buttress':'Prefab_Buttress_arch_01_c',
 'hall_front':'Prefab_Temple_Front',
 'hall_doors':'Prefab_Temple_Front_Doors',
 'hall_stairs':'Prefab_Temple_Front_Stairs',
 'hall_wall':'Prefab_Temple_Wall_1',
 'hall_wall_alt':'Prefab_Temple_Wall_2',
 'hall_column':'Prefab_Temple_Altar_Column',
 'hall_column_statue':'Prefab_Temple_Altar_Column_Statue',
 'hall_gallery':'Prefab_Temple_Wall_Interior_Level_2side',
 'hall_gallery_stairs':'Prefab_Temple_Wall_Interior_Stairs',
 'hall_vault':'Prefab_Temple_Top_Roof_Interior_2side',
 'hall_end':'Prefab_Temple_Altar_Wall',
 'altar_floor':'Prefab_Temple_Altar_Floor',
 'altar_roof':'Prefab_Temple_Altar_Roof',
 'house':'Prefab_House_Quater_2Side',
 'house_front':'Prefab_House_Front',
 'market':'Prefab_Market_Building',
 'fireplace':'Prefab_Fireplace',
 'bridge':'Prefab_Bridge_Short_Castle',
 'bridge_pier':'Prefab_Bridge_support_complete_01',
 'parapet':'Prefab_Bariere_01_Long',
 'stair':'Prefab_Stone_stairs_08_cathedral',
 'arch':'Prefab_indoor_arch_01',
 'floor':'Prefab_floor_plate_02',
 'wall':'Prefab_Wall_01_Door',
 'roof':'Prefab_Roof_01_Big_Long',
 'brazier':'Prefab_Brazier_01_off',
 'torch':'Prefab_Forged_torch_complete_01_off',
 'guardian':'Prefab_stone_guard_01_full',
 'sculpture':'Prefab_sculpture_01',
}

def main():
 inventory=json.loads((DATA/'INVENTORY.json').read_text())
 guids={x['guid']:x for x in inventory['entries']}
 by_name={Path(x['path']).stem:x for x in inventory['entries']if x['path'].endswith('.prefab')}
 chosen=[{'name':name,'path':by_name[prefab]['path']}for name,prefab in SELECTION.items()]
 pending=[x['path']for x in chosen];seen=set();missing=set()
 while pending:
  path=pending.pop()
  if path in seen:continue
  seen.add(path)
  source=DATA/'source'/path
  if not source.is_file():raise FileNotFoundError(source)
  if source.suffix.lower()in{'.prefab','.mat'}:
   for guid in re.findall(r'guid: ([a-fA-F0-9]{32})',source.read_text()):
    dep=guids.get(guid)
    if dep and Path(dep['path']).suffix.lower()in{'.fbx','.prefab','.mat','.png','.tga','.jpg','.jpeg','.tif','.tiff'}:pending.append(dep['path'])
    elif dep is None and guid.strip('0'):missing.add(guid)
 for path in sorted(seen):
  if Path(path).suffix.lower() not in {'.prefab','.fbx','.mat'}:continue
  source=DATA/'source'/path;dest=BRIDGE/path;dest.parent.mkdir(parents=True,exist_ok=True)
  for suffix in ['', '.meta']:
   src=Path(str(source)+suffix);dst=Path(str(dest)+suffix)
   if src.is_file()and not dst.exists():shutil.copy2(src,dst)
 report={'assets':chosen,'staged_dependencies':len(seen),'unresolved_non_data_guids':sorted(missing)}
 target=BRIDGE.parent/'citadel-selection.json';target.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({'selected_prefabs':len(chosen),'staged_dependencies':len(seen),'selection':str(target)}))

if __name__=='__main__':main()
