// CPU regression for committed terrain height changes through streamed grass.
// Build: c++ -std=c++17 -O2 -I. tests/world_grass_terrain_transition_test.cpp -o /tmp/world-grass-terrain-test
#include "engine/world_grass_transition.h"
#include <cstdio>
#include <cstdlib>

namespace {
size_t checks=0;
void require(bool ok,const char *message) {
    ++checks;
    if(!ok){std::fprintf(stderr,"FAIL grass terrain transition: %s (check%zu)\n",message,checks);std::exit(1);}
}
bool exact(float a,float b){return std::memcmp(&a,&b,sizeof(a))==0;}
WorldModelInstance root(unsigned id,float height) {
    WorldModelInstance result={float(id)*1.125f-40.25f,height,float(id)*-.875f+12.5f,
                               float(id+1)*.03125f,1.f+float(id%5)*.0625f};
    result.pitch=.125f;result.roll=-.0625f;result.axis_x=.25f;
    result.wind_sway=.75f;result.wind_flutter=.5f;
    return result;
}
WorldCoverField field(const std::vector<WorldModelInstance> &roots,uint64_t generation,
                      double x=1000.125,double z=-700.375) {
    return {WORLD_COVER_GRASS,generation,x,z,roots.size(),roots.data()};
}
size_t find(const WorldGrassTransition &transition,unsigned id) {
    const auto expected=root(id,0);const auto &actual=transition.field();
    for(size_t i=0;i<actual.count;++i)
        if(exact(actual.instances[i].yaw,expected.yaw)&&exact(actual.instances[i].scale,expected.scale))return i;
    require(false,"expected root identity missing");return 0;
}
void verify_root(const WorldGrassTransition &transition,unsigned id,float height,float fade,
                 double anchor_x=1000.125,double anchor_z=-700.375) {
    const auto at=find(transition,id);const auto &f=transition.field();const auto &actual=f.instances[at];
    const auto original=root(id,height);
    require(exact(actual.y,height),"root follows exact newly published terrain height");
    require(exact(transition.fades()[at],fade),"root keeps current fade through height replacement");
    require(std::abs(f.anchor_x+actual.x-(anchor_x+original.x))<.00001,"horizontal world X remains fixed");
    require(std::abs(f.anchor_z+actual.z-(anchor_z+original.z))<.00001,"horizontal world Z remains fixed");
    require(exact(actual.pitch,original.pitch)&&exact(actual.roll,original.roll)&&
            exact(actual.axis_x,original.axis_x)&&exact(actual.wind_sway,original.wind_sway)&&
            exact(actual.wind_flutter,original.wind_flutter),"retained root pose and wind are unchanged");
}
void small_transitions() {
    WorldGrassTransition transition;
    std::vector<WorldModelInstance> roots={root(0,10),root(1,12),root(2,14)};
    require(transition.replace(field(roots,1)),"initial generation");
    for(unsigned i=0;i<3;++i)verify_root(transition,i,10.f+2*i,1);
    const auto revision=transition.structure_revision();
    // Same seeded roots, raised ground. The old implementation left Y at10/12/14.
    for(auto &r:roots)r.y+=4;
    require(transition.replace(field(roots,2)),"raise committed terrain");
    require(transition.structure_revision()>revision,"height publication invalidates renderer root cache");
    for(unsigned i=0;i<3;++i)verify_root(transition,i,14.f+2*i,1);
    // Lower then Undo then Redo use identical root identities and exact heights.
    for(float displacement:{-3.5f,0.f,-3.5f,4.f,0.f}) {
        for(unsigned i=0;i<3;++i)roots[i].y=10.f+2*i+displacement;
        require(transition.replace(field(roots,3)),"lower/Undo/Redo terrain publication");
        for(unsigned i=0;i<3;++i)verify_root(transition,i,10.f+2*i+displacement,1);
    }
    roots.push_back(root(3,16));
    require(transition.replace(field(roots,4)),"new root begins fading in");
    verify_root(transition,3,16,0);
    require(transition.advance(.2),"quarter transition advances");
    verify_root(transition,3,16,.25f);
    // Rebase the same roots, with harmless input X/Z drift. Retention must use
    // stable double-precision world roots, while adopting every desired Y.
    for(auto &r:roots){r.x-=24.f;r.z+=24.f;r.x+=.0078125f;r.z-=.0078125f;r.y+=3;}
    require(transition.replace(field(roots,5,1024.125,-724.375)),"rebase and sculpt while fading");
    for(unsigned i=0;i<4;++i)verify_root(transition,i,13.f+2*i,i==3?.25f:1.f);
    // A root absent from the new publication remains at its last known height
    // while fading out. Reintroducing it must retain opacity and update height.
    roots.erase(roots.begin()+1);
    require(transition.replace(field(roots,6,1024.125,-724.375)),"remove one root");
    require(transition.advance(.2),"outgoing and incoming fades advance");
    verify_root(transition,1,15,.75f);verify_root(transition,3,19,.5f);
    auto returning=root(1,-9);returning.x-=24;returning.z+=24;roots.push_back(returning);
    require(transition.replace(field(roots,7,1024.125,-724.375)),"Undo removal while still visible");
    verify_root(transition,1,-9,.75f);verify_root(transition,3,19,.5f);
    require(transition.advance(.2),"restored root resumes fade in");
    verify_root(transition,1,-9,1);verify_root(transition,3,19,.75f);
    // Invalid input preserves the committed root bytes, fades and revision.
    const auto saved=std::vector<WorldModelInstance>(transition.field().instances,transition.field().instances+transition.field().count);
    const auto saved_fades=transition.fades();const auto saved_revision=transition.structure_revision();
    auto invalid=roots;invalid[0].y=std::numeric_limits<float>::quiet_NaN();
    require(!transition.replace(field(invalid,8)),"nonfinite desired height rejected");
    invalid=roots;invalid.push_back(roots.front());
    require(!transition.replace(field(invalid,8)),"duplicate identity rejected");
    require(transition.structure_revision()==saved_revision&&transition.fades()==saved_fades&&
            transition.field().count==saved.size()&&std::memcmp(transition.field().instances,saved.data(),saved.size()*sizeof(saved[0]))==0,
            "invalid replacement preserves visible generation");
    // Long teleports intentionally use new positions immediately and no fades.
    require(transition.replace(field(roots,9,3000,-4000)),"teleport replacement");
    for(size_t i=0;i<roots.size();++i){
        require(exact(transition.field().instances[i].y,roots[i].y)&&
                exact(transition.field().instances[i].x,roots[i].x)&&
                exact(transition.field().instances[i].z,roots[i].z),"teleport uses complete desired position");
        require(exact(transition.fades()[i],1),"teleport is opaque immediately");
    }
    std::vector<WorldModelInstance> empty;
    require(transition.replace(field(empty,10,3000,-4000)),"empty generation retains outgoing roots");
    require(transition.field().count==roots.size(),"removed roots remain until fade completes");
    require(transition.advance(1),"complete removal fade");
    require(transition.field().count==0&&transition.fades().empty(),"outgoing roots compact to zero");
}
void repeated_sculpt_rebase() {
    WorldGrassTransition transition;
    constexpr unsigned count=96; // Exercises the production radix lookup path.
    std::vector<WorldModelInstance> roots;roots.reserve(count);
    for(unsigned i=0;i<count;++i)roots.push_back(root(i,10.f+i*.125f));
    require(transition.replace(field(roots,1)),"bulk initial generation");
    for(unsigned step=0;step<1024;++step) {
        const double dx=step%2?24.:0.,dz=step%3?0.:-24.;
        const float height_delta=step%4==0?4.f:step%4==1?-3.f:0.f;
        // Alternate source ordering to test identity-based retention rather
        // than accidentally matching roots by index. Input horizontal jitter
        // must never compound into world-root drift during repeated Undo/Redo.
        for(unsigned i=0;i<count;++i) {
            const unsigned id=step%2?count-1-i:i;auto value=root(id,10.f+id*.125f+height_delta);
            value.x-=float(dx);value.z-=float(dz);
            value.x+=step%2?.0078125f:-.0078125f;
            roots[i]=value;
        }
        require(transition.replace(field(roots,step+2,1000.125+dx,-700.375+dz)),"repeated sculpt/rebase/Undo/Redo");
        require(transition.field().count==count,"matched generations do not accumulate outgoing roots");
        for(unsigned id=0;id<count;++id)verify_root(transition,id,10.f+id*.125f+height_delta,1);
        require(!transition.advance(.2),"opaque matched roots do not restart transition");
    }
}
}
int main() {
    small_transitions();repeated_sculpt_rebase();
    std::printf("PASS grass terrain transition: %zu checks; raised/lowered/Undo/Redo heights, fading survival, removal/reentry, rejection, teleport and1024 rebase cycles\n",checks);
}
