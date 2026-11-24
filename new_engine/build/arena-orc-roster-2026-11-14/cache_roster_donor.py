from pathlib import Path
import bpy,json,numpy as np,hashlib,math
P=Path(__file__).resolve().parent;SOURCE=P.parent/'arena-orcs-textured-2026-11-14/intake/animations/Animations';OUT=P/'intake_roster/donor-cache';OUT.mkdir(parents=True,exist_ok=True)
paths=[SOURCE/'1With_Weapon'/('Anim_Warrior_'+n+'.FBX')for n in ['Idle','Walk','Run']+[f'Attack_{i}'for i in range(1,9)]+['Get_Hit_1','Get_Hit_2','Dead_1']]+[SOURCE/'3Root_With_Weapon/Anim_Warrior_Run_Root.FBX']
for path in paths:
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.context.preferences.filepaths.use_scripts_auto_execute=False;bpy.ops.import_scene.fbx(filepath=str(path),use_anim=True,use_image_search=False)
 arm=next(o for o in bpy.data.objects if o.type=='ARMATURE');action=max(bpy.data.actions,key=lambda a:sum('pose.bones['in f.data_path for f in a.fcurves));arm.animation_data.action=action
 for track in arm.animation_data.nla_tracks:track.mute=True
 names=list(arm.data.bones.keys());parents=[names.index(b.parent.name)if b.parent else-1 for b in arm.data.bones];rest=np.asarray([list(map(list,arm.matrix_world@b.matrix_local))for b in arm.data.bones]);lo,hi=map(float,action.frame_range);frames=[]
 for t in np.linspace(0.,1.,121):
  f=lo+(hi-lo)*t;bpy.context.scene.frame_set(math.floor(f),subframe=f%1);bpy.context.view_layer.update();frames.append([list(map(list,arm.matrix_world@arm.pose.bones[n].matrix))for n in names])
 array=OUT/(path.stem+'.npz');np.savez_compressed(array,names=np.asarray(names,dtype='U64'),parents=np.asarray(parents),rest=rest.astype(np.float32),frames=np.asarray(frames,dtype=np.float32));meta={'source':str(path),'source_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'array_sha256':hashlib.sha256(array.read_bytes()).hexdigest(),'frames':[lo,hi],'fps':bpy.context.scene.render.fps,'samples':121,'action':action.name};(OUT/(path.stem+'.json')).write_text(json.dumps(meta,indent=2)+'\n');print('DONOR_READY',path.stem,flush=True)
