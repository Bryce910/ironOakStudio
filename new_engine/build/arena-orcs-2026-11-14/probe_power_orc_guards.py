"""Source-arm feasibility probe only; never writes the arena bank."""
from pathlib import Path
import json,sys
stage=Path(__file__).resolve().parent
sys.path.insert(0,str(stage))
import power_orc_guard_fit as guard
guard.TARGETS={
 'left':((.35,1.075,.45),(0.,1.,0.)),
 'right':((-.30,1.825,.60),(0.,1.,0.)),
 'high':((0.,1.86,.58),(1.,0.,0.)),
 'low':((.095,.975,.70),(0.,1.,0.)),
 'thrust':((.065,1.225,.25),(1.,0.,0.)),
}
source=stage/'cook_power_orc_body.py'
scope={'__file__':str(source),'__name__':'power_orc_source_guard_probe'}
exec(compile(source.read_text().split('GUARD_POSES,GUARD_MEASUREMENTS=')[0],str(source),'exec'),scope)
poses,measurements=guard.fit(scope['NEUTRAL'],scope['ARM'].data.bones,scope['BONE_INDEX'],scope['AXE_HAND_LOCAL'],scope['AXE_EDGE_LOCAL'],scope['SCALE'],scope['ORIGIN'],scope['BASIS'],scope['ik_pose'])
(stage/'orc-intake/GUARD_CANDIDATE_FEASIBILITY.json').write_text(json.dumps(measurements,indent=2)+'\n')
print('CANDIDATE_SOURCE_IK_REACHABLE',flush=True)
