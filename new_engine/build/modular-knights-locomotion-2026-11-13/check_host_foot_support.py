#!/usr/bin/env python3
"""Exercise the actual host support callback against production Jolt geometry."""
import hashlib
import json
import subprocess
from pathlib import Path

BASE = Path(__file__).resolve().parent
ROOT = BASE.parents[1]
SOURCE = BASE / 'source-v1'
HOST = SOURCE / 'engine/world_viewer.cpp'

def sha(p):
    with p.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()

text = HOST.read_text()
start = text.index('    bool skeletal_foot_support(')
end = text.index('\n    bool camera_swimming(', start)
callback = text[start:end]
code = r'''
#include "engine/world_skeletal_hero.h"
#include "engine/world_physics.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
world_physics::World *active_playtest_walk=nullptr,*active_city_physics=nullptr;
bool active_structure_world=false;
bool sloped_atlas=false;
float world_atlas_walk_height(double x,double){return sloped_atlas?float(x*.2-.05):0.f;}
'''+callback+r'''
unsigned checks=0;
void require(bool c,const char *m){++checks;if(!c){std::fprintf(stderr,"%s\n",m);std::exit(1);}}
int main(){
    using namespace world_physics;
    World world,empty,unconfigured;Document d,blank;std::string error;
    Body floor;floor.id=1;floor.size={1,.5,1};floor.pose.position={0,-.5,0};d.bodies.push_back(floor);
    Body stair=floor;stair.id=2;stair.pose.position={3,-.25,0};d.bodies.push_back(stair);
    Body slope;slope.id=3;slope.shape=Shape::Mesh;slope.pose.position={6,0,0};
    slope.vertices={{-1,-.2,-1},{1,-.2,-1},{1,.2,1},{-1,.2,1}};
    slope.triangles={{{0,2,1}},{{0,3,2}}};d.bodies.push_back(slope);
    require(world.configure(d,error,Profile::StaticQuery),error.c_str());
    require(empty.configure(blank,error,Profile::StaticQuery),error.c_str());
    WorldSkeletalHero::Support out;active_playtest_walk=&world;
    for(double probe:{-.05,0.,.05,.29}){
        require(skeletal_foot_support(0,probe,0,out,nullptr),"Real flat support rejected");
        require(std::abs(out.height)<1.e-6&&out.ny>.999,"Flat support differs from actual collision floor");
    }
    require(skeletal_foot_support(3,.25,0,out,nullptr),"Step support rejected");
    require(std::abs(out.height-.25)<1.e-6,"Step support uses wrong floor");
    require(skeletal_foot_support(6,.1,.5,out,nullptr),"Slope support rejected");
    require(std::abs(out.height-.1)<1.e-5&&out.ny>.97&&out.nz<-.19,"Slope height/normal differs from actual mesh");
    require(!skeletal_foot_support(0,.5,0,out,nullptr),"Out-of-reach floor accepted");
    require(!skeletal_foot_support(20,0,0,out,nullptr),"Constrained imported hole fell back to atlas");
    active_playtest_walk=&unconfigured;
    require(!skeletal_foot_support(0,0,0,out,nullptr),"Failed physics query fell back to atlas");
    active_playtest_walk=&empty;active_structure_world=true;
    require(skeletal_foot_support(20,0,0,out,nullptr),"Explicit structure-over-terrain miss did not sample real atlas");
    active_playtest_walk=nullptr;active_city_physics=&world;
    require(skeletal_foot_support(3,.25,0,out,nullptr)&&std::abs(out.height-.25)<1.e-6,"City physics support ignored");
    active_city_physics=nullptr;sloped_atlas=true;
    require(skeletal_foot_support(0,0,0,out,nullptr)&&out.height==0&&out.ny>.999&&std::abs(out.nx)<1.e-7,"Clamped shoreline floor has inconsistent normal");
    require(skeletal_foot_support(1,.15,0,out,nullptr)&&std::abs(out.height-.15)<1.e-6&&out.nx<-.19,"Atlas slope support wrong");
    require(!skeletal_foot_support(NAN,0,0,out,nullptr),"Nonfinite support query accepted");
    std::printf("{\"accepted\":true,\"checks\":%u,\"real_Jolt_flat_step_slope\":true,\"constrained_holes_rejected\":true,\"query_failures_rejected\":true,\"clamped_shore_normal\":true}\n",checks);
}
'''
test = BASE / 'host_foot_support_test.cpp'
test.write_text(code)
binary = BASE / 'host_foot_support_test'
physics = BASE / 'runtime-native-v1/native-objects/engine/world_physics.cpp.o'
subprocess.run(['/usr/bin/x86_64-linux-gnu-g++-13', '-std=c++17', '-O2', '-I',str(SOURCE),str(test),str(physics),str(ROOT/'build/physics/libJolt.a'),'-pthread','-o',str(binary)],check=True)
result = json.loads(subprocess.check_output([str(binary)],text=True))
result['host_callback_sha256'] = hashlib.sha256(callback.encode()).hexdigest()
result['dependencies_sha256'] = {str(p):sha(p) for p in (HOST,test,physics,ROOT/'build/physics/libJolt.a',binary)}
(BASE/'host-foot-support-check.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if k!='dependencies_sha256'}))
