from pathlib import Path
import json,math,subprocess
N=Path(__file__).resolve().parents[1];E=N.parents[1];R=N/'review/art/birch';P=N/'world-project';j=json.loads((R/'BIRCH_MODELS.json').read_text());rows=json.loads((R.parent/'COLLISION_BIRCH_PATCH.json').read_text())['append_only_catalog_rows'];fixture=R/'cpu-project';(fixture/'collision/structures').mkdir(parents=True,exist_ok=True);models=fixture/'models'
if not models.exists():models.symlink_to(P/'models')
(fixture/'collision/structures/catalog').write_text('VALWOOD_STRUCTURE_COLLISION 1\n'+'\n'.join(rows)+'\n');rr=[]
for i,rec in enumerate(j['records']):
 section=rec['soil_sections'][2];cx,cz=section['center_xz'];angle=.37;c=math.cos(angle);s=math.sin(angle);tx=100*i+c*cx+s*cz;tz=-s*cx+c*cz;rr.append('{"%s","%s",%.17g,%.17g,%.17g,%.17g},'%(rec['alias'],rec['stem'],100*i,10-rec['soil_plane_glb_y'],tx,tz))
(R/'birch_fixtures.inc').write_text('\n'.join(rr)+'\n')
cpp=R/'birch_collision_test.cpp';cpp.write_text(r'''#include "engine/world_structure_collision.h"
#include <cassert>
#include <cstdio>
struct Fixture{const char*alias,*stem;double x,y,tx,tz;};
const Fixture fixtures[]={
#include "birch_fixtures.inc"
};
static unsigned fallback=0;
namespace world_assets{Report inspect(const std::string&,bool){++fallback;Report r;r.error="Full canopy fallback forbidden for birch";return r;}}
std::size_t world_scenery_alias_count(){return 0;}WorldSceneryAlias world_scenery_alias(std::size_t){return {};}
WorldSceneryStats world_scenery_stats(const WorldScenery*){WorldSceneryStats s;s.source_instances=2;return s;}
bool world_scenery_placement(const WorldScenery*,std::size_t i,WorldSceneryPlacement*p){if(i>=2)return false;const auto&f=fixtures[i];*p={};p->alias=f.alias;p->x=f.x;p->y=float(f.y);p->scale=1;p->yaw=.37f;p->tree=true;return true;}
int main(int argc,char**argv){assert(argc==2);std::string error;world_studio::Document doc;for(const auto&f:fixtures){world_studio::Asset a;a.alias=f.alias;a.stem=f.stem;a.vegetation=2;a.lods=3;a.collision=0;doc.assets.push_back(a);}
world_structure_collision::Capture captured;assert(world_structure_collision::capture(argv[1],nullptr,doc,world_editor_scene_guard::Mode::City,captured,error));assert(captured.instances.size()==2);world_structure_collision::Cache cache;for(const auto&i:captured.instances){bool exact=false;assert(cache.original(argv[1],i.model,captured.catalog.at(i.alias),exact,error)&&exact);}
world_structure_collision::Geometry geometry;assert(world_structure_collision::build(captured,geometry,error,&cache));assert(geometry.instances==2&&geometry.documents.size()==1&&fallback==0);world_physics::World physics;assert(physics.configure(geometry.documents[0],error,world_physics::Profile::StaticQuery));unsigned rays=0,walks=0,cameras=0,clears=0;
for(unsigned i=0;i<2;i++){const auto&f=fixtures[i];auto body=geometry.documents[0].bodies[i].id;for(unsigned k=0;k<4;k++){double a=k*1.5707963267948966,dx=cos(a),dz=sin(a);world_physics::Hit hit;assert(physics.raycast({f.tx+dx*8,11.,f.tz+dz*8},{-dx*8,0,-dz*8},hit));assert(hit.hit&&hit.body==body);rays++;assert(physics.cast_capsule_movement({f.tx+dx*8,10.9,f.tz+dz*8},.37f,.50f,{-dx*8,0,-dz*8},hit));assert(hit.hit&&hit.body==body);walks++;assert(physics.cast_capsule({f.tx+dx*8,11.6,f.tz+dz*8},.22f,.02f,{-dx*8,0,-dz*8},hit));assert(hit.hit&&hit.body==body);cameras++;assert(physics.cast_capsule_movement({f.tx+dx*8,10.9,f.tz+dz*8},.37f,.50f,{dz,0,-dx},hit));assert(!hit.hit);clears++;}}
auto bad=captured;bad.catalog.at(fixtures[0].alias).model_hash^=1;world_structure_collision::Geometry rejected;assert(!world_structure_collision::build(bad,rejected,error));assert(fallback==1);printf("PASS exact source bark only: %u rays, %u walk capsule casts, %u camera casts, %u clear controls; stale hash negative control rejects full-canopy fallback.\n",rays,walks,cameras,clears);}
''')
binary=N/'tools/art_birch_collision_test';cmd=['g++','-std=c++17','-O2','-Wno-misleading-indentation','-I',str(N/'source-v1'),str(cpp),str(N/'runtime-native-v1/native-objects/engine/world_physics.cpp.o'),str(E/'build/physics/libJolt.a'),'-pthread','-o',str(binary)];c=subprocess.run(cmd,text=True,capture_output=True);(R/'collision-compile.log').write_text(c.stdout+c.stderr);assert c.returncode==0,c.stderr;r=subprocess.run([str(binary),str(fixture)],text=True,capture_output=True);(R/'collision-test.log').write_text(r.stdout+r.stderr);print(r.stdout+r.stderr);assert r.returncode==0
