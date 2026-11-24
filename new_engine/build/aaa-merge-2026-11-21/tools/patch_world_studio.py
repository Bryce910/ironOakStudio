#!/usr/bin/env python3
"""AAA merge: apply the camp smoke and ecology record changes to a world.studio.
Usage: patch_world_studio.py IN OUT   (records are matched by position/seed; all else untouched)"""
import sys
SMOKE='particle -1 0 1 -11988 12.495000190700001 5172 '
NEW_SMOKE=SMOKE+'26 2.6 9.5 .34 0.20000000298023224 1.4 3.4 1.2 1.5 0 379 0.42 0.41 0.40 0'
# ecology seed -> (grass, trees, rocks, ferns)
ECO={'11073675':('0.550','0','0.100','0.180')}                        # camp clearing
for s in range(11072802,11073482,97):ECO[str(s)]=('0.920','0','0.220','0.850')  # woodland groves (preset 1)
LAYER='surfaces "layer \\"Compacted camp earth\\" '
NEW_LAYER=LAYER+'12 13 -1 1.45 1.22 0.92 2.6 0.95 0 1.2 0"'
ROAD='surfaces "ribbon \\"Seldonne arrival road\\" 0 5 4.5 0.8 '
LITTER={'surfaces "layer \\"Arrival detailed woodland litter\\" ':'2 3 4 1.2 1.1 1.0 2.0 0.98 0 1.10 0"',
        'surfaces "layer \\"Woodland leaf litter\\" ':'2 3 4 1.15 1.1 0.95 2.4 0.97 0 1 0"'}
EMBERS='particle -1 2 1 -11988 12.55 5172 26 0.45 3.0 0.28 0.95 0.5 1.6 0.4 0.7 0 911 1.0 0.50 0.14 0.7'   # rising hearth embers
RAIN='weather_area "Seldonne woodland rain" '
def main(src,dst):
    out=[];hits=0
    src_text=open(src).read()
    for line in open(src).read().split('\n'):
        f=line.split()
        if line.startswith(SMOKE):
            line=NEW_SMOKE;hits+=1
        elif line.startswith(LAYER):line=NEW_LAYER;hits+=1
        elif any(line.startswith(k) for k in LITTER):
            k=next(k for k in LITTER if line.startswith(k));line=k+LITTER[k];hits+=1   # natural leaf colour, larger leaves
        elif line.startswith(ROAD):line=line.replace(' 0 5 4.5 0.8 ',' 0 5 3.2 1.4 ',1);hits+=1   # narrower track, softer verge
        elif line.startswith(RAIN):
            # rain off (clear sky, no drizzle); widen the cloud/fog footprint so
            # its 1.25 km rim no longer shows as a hard arc in the sky
            f[12]='0';f[8]='7000';f[9]='10000';line=' '.join(f);hits+=1
        elif f and f[0]=='ecology' and len(f)>=11 and f[8] in ECO:
            f[4:8]=ECO[f[8]];line=' '.join(f);hits+=1
        out.append(line)
    # every particle needs a scene-graph entity (kind 5, binding index = particle order)
    if EMBERS in out and not any('"Hearth embers"' in l for l in out):
        hi=next(i for i,l in enumerate(out) if l.startswith('entities "VALWOOD_ENTITIES '))
        ver,next_id,count=out[hi].split('"')[1].split()[1:4]
        particle_index=sum(1 for l in out if l.startswith('particle '))-1
        out[hi]='entities "VALWOOD_ENTITIES %s %d %d"'%(ver,int(next_id)+1,int(count)+1)
        last=max(i for i,l in enumerate(out) if l.startswith('entities "'))
        out.insert(last+1,'entities "%s 0 \\"Hearth embers\\" 5 %d -11988 12.55 5172 0 1 \\"\\" 1 0 0 0 1 1 1"'%(next_id,particle_index))
        hits+=1
    open(dst,'w').write('\n'.join(out));print('patched records:',hits)
if __name__=='__main__':main(*sys.argv[1:3])
