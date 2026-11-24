#include "engine/world_studio_height_changes.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
unsigned checks=0;
void expect(bool passed,const char *message) {
    ++checks;
    if(!passed){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}
}
bool equal(const world_studio_height_changes::Rect &a,
           const world_studio_height_changes::Rect &b) {
    return a.xmin==b.xmin&&a.zmin==b.zmin&&a.xmax==b.xmax&&a.zmax==b.zmax;
}
bool has(const std::vector<world_studio_height_changes::Rect> &rects,
         world_studio_height_changes::Rect wanted) {
    return std::any_of(rects.begin(),rects.end(),[&](const auto &r){return equal(r,wanted);});
}
template<class Tile> Tile tile(int x,int z,unsigned ix,unsigned iz,float value) {
    Tile t;t.x=x;t.z=z;t.delta[iz*65+ix]=value;return t;
}
using Document=world_studio::Document;
using Terrain=world_studio::TerrainTile;
using FieldTile=world_surface::TerrainTile;
using Field=world_surface::TerrainField;
using world_studio_height_changes::changed;
}

int main() {
    Document empty,a,b;
    expect(changed(empty,empty).empty(),"empty documents have no height changes");
    a.terrain.push_back(tile<Terrain>(0,0,10,20,3));
    auto bounds=changed(empty,a);
    expect(bounds.size()==1&&has(bounds,{18,38,22,42}),"added node includes neighboring2m cells");
    expect(changed(a,a).empty(),"identical document has no changes");
    bounds=changed(a,empty);
    expect(bounds.size()==1&&has(bounds,{18,38,22,42}),"removed tile restores its nonzero node support");

    a.terrain.push_back(tile<Terrain>(-3,2,64,0,-2));
    b=a;std::reverse(b.terrain.begin(),b.terrain.end());
    expect(changed(a,b).empty(),"tile vector order is irrelevant");
    b.terrain[0].delta[0*65+64]=-3;
    bounds=changed(a,b);
    expect(bounds.size()==1&&has(bounds,{-258,254,-254,258}),"negative tile coordinates and corner interpolation");
    b=a;b.terrain[0].delta[22*65+13]=.0000001f;
    bounds=changed(a,b);
    expect(bounds.size()==1&&has(bounds,{24,42,28,46}),"even subepsilon node edits invalidate their support");
    b=a;b.terrain[0].delta[20*65+10]=0;b.terrain[0].delta[22*65+13]=4;
    bounds=changed(a,b);
    expect(bounds.size()==1&&has(bounds,{18,38,28,46}),"tight per-tile bounds include both old and new support");

    Document zeros;zeros.terrain.push_back(tile<Terrain>(3,-9,0,0,0));
    expect(changed(empty,zeros).empty()&&changed(zeros,empty).empty(),"adding and removing all-zero tiles changes no height");
    a={};a.terrain.push_back(tile<Terrain>(0,0,64,32,1));a.terrain.push_back(tile<Terrain>(1,0,0,32,1));
    b=a;b.terrain[0].delta[32*65+64]=2;b.terrain[1].delta[32*65+0]=2;
    bounds=changed(a,b);
    expect(bounds.size()==2&&has(bounds,{126,62,130,66}),"shared edge invalidates cells on both sides");
    expect(equal(bounds[0],bounds[1]),"both copies of one shared node agree on support");

    auto old_field=std::make_shared<Field>();old_field->tiles.push_back(tile<FieldTile>(-1,-1,63,2,-3));
    a={};a.surface_field=old_field;
    auto next_field=std::make_shared<Field>(*old_field);
    next_field->tiles[0].delta[2*65+63]=-4;
    b=a;b.surface_field=next_field;
    bounds=changed(a,b);
    expect(bounds.size()==1&&has(bounds,{-4,-126,0,-122}),"prepared river or road rebake includes bilinear support");
    bounds=changed(a,empty);
    expect(bounds.size()==1&&has(bounds,{-4,-126,0,-122}),"removing a derived field restores its former grade");
    bounds=changed(empty,a);
    expect(bounds.size()==1&&has(bounds,{-4,-126,0,-122}),"adding a derived field includes its new grade");
    next_field->tiles.push_back(tile<FieldTile>(2,1,4,8,5));
    a.surface_field=next_field;
    auto reversed=std::make_shared<Field>(*next_field);std::reverse(reversed->tiles.begin(),reversed->tiles.end());
    b.surface_field=reversed;
    expect(changed(a,b).empty(),"derived tile vector order is irrelevant");
    reversed->tiles[0].road.fill(1);reversed->ribbons.push_back({});
    expect(changed(a,b).empty(),"road masks and ribbon metadata alone do not change height");
    auto zero_field=std::make_shared<Field>();zero_field->tiles.push_back(tile<FieldTile>(0,0,32,32,0));
    b={};b.surface_field=zero_field;
    expect(changed(empty,b).empty()&&changed(b,empty).empty(),"empty and all-zero derived fields are equivalent");

    a={};a.terrain.push_back(tile<Terrain>(1,1,4,5,2));a.surface_field=old_field;
    bounds=changed(empty,a);
    expect(bounds.size()==2&&has(bounds,{134,136,138,140})&&has(bounds,{-4,-126,0,-122}),"sculpt and derived grade changes are both included");
    b=a;b.waters.push_back({});b.stamps.push_back({});b.look.hour=12;
    expect(changed(a,b).empty(),"water, foliage and look changes alone have no height bounds");
    b=a;b.waters.push_back({});b.waters[0].tint={.4f,.8f,1.1f};
    expect(changed(a,b).empty(),"water shading alone has no height bounds");

    // Every local grid corner gets precisely one-cell interpolation support,
    // including all shared edges and negative world-space coordinates.
    for(unsigned z=0;z<65;++z)for(unsigned x=0;x<65;++x) {
        a={};a.terrain.push_back(tile<Terrain>(-2,3,x,z,1));
        bounds=changed(empty,a);
        expect(bounds.size()==1&&has(bounds,{-258.+x*2,382.+z*2,-254.+x*2,386.+z*2}),"grid-node support coverage");
    }
    std::cout<<"Height change bounds: "<<checks<<" checks passed\n";
}
