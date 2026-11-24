#include "engine/world_surface_authoring.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <numeric>
using namespace world_surface;
namespace {
void close(double a,double b,double tolerance=1e-5){assert(std::abs(a-b)<tolerance);}
Data material(){Data d;d.images.push_back({"Stone color","surfaces/stone.png",ImageRole::Color});Layer a;a.name="Stone";a.color=0;d.layers.push_back(a);a.name="Moss";d.layers.push_back(a);return d;}
void codec(){
 auto d=material();d.bindings.push_back({"castle",0,1,Blend::Slope,.7f,.3f,.6f,.1f});
 d.paint.push_back({128,0,12,.7f,1,false});Projection p;p.layer=1;p.rotation={90,0,20};d.projections.push_back(p);
 Wind w;w.seed=4294967295u;d.winds.push_back(w);Ribbon r;r.layer=0;r.points={{0,2,0},{20,3,10},{40,2,0}};d.ribbons.push_back(r);
 std::string encoded,error,again;assert(encode(d,encoded,error));Data copy;assert(decode(encoded,copy,error));assert(encode(copy,again,error)&&again==encoded);
 const auto before=again;assert(!decode(encoded+"image \"wrong\" \"../escape.png\" 0\n",copy,error));assert(encode(copy,again,error)&&again==before);
 assert(!decode("VALWOOD_SURFACES 1 junk\n",copy,error));assert(!decode("VALWOOD_SURFACES 1\npaint 0 0 4 1 0 2\n",copy,error));
 auto bad=d;bad.layers[0].normal=0;assert(!validate(bad,error));bad=d;bad.images[0].path="surfaces/a/../../secret";assert(!validate(bad,error));
 bad=d;bad.projections[0].size[0]=std::numeric_limits<float>::quiet_NaN();assert(!validate(bad,error));
}
void paint(){
 auto d=material();d.paint.push_back({128,0,12,.75f,0,false});d.paint.push_back({128,0,12,.5f,1,false});
 auto w=paint_weights(d,128,0);close(w[0],.375);close(w[1],.5);assert(std::accumulate(w.begin(),w.end(),0.f)<=1);
 d.paint.push_back({128,0,12,.5f,0,true});w=paint_weights(d,128,0);close(w[0],.1875);close(w[1],.25);
 PaintField field;std::string error;assert(bake_paint(d,field,error));assert(field.tiles.size()==4);
 const PaintTile *left=nullptr,*right=nullptr;for(const auto &t:field.tiles){if(t.x==0&&t.z==0)left=&t;if(t.x==1&&t.z==0)right=&t;}
 assert(left&&right);for(unsigned z=0;z<paint_side;z++)for(unsigned k=0;k<layer_limit;k++)assert(left->weights[(z*paint_side+64)*layer_limit+k]==right->weights[z*paint_side*layer_limit+k]);
 for(unsigned i=0;i<field.tiles.size();i++){const auto &t=field.tiles[i];unsigned at=paint_hash(t.x,t.z);while(field.lookup[at][2]>=0&&(field.lookup[at][0]!=t.x||field.lookup[at][1]!=t.z))at=(at+1)&127;assert(field.lookup[at][2]==int(i));}
 const auto before=field.tiles.size();d.paint.clear();for(unsigned i=0;i<65;i++)d.paint.push_back({double(i)*512,4,2,1,0,false});assert(!bake_paint(d,field,error)&&field.tiles.size()==before);
}
void riverbed_material(){
 auto d=material();d.paint.push_back({128,0,64,1,0,false});
 Ribbon river;river.name="River bed";river.kind=RibbonKind::River;river.layer=1;river.width=8;river.shoulder=4;river.points={{96,8,0},{160,8,0}};d.ribbons.push_back(river);
 Ribbon road;road.name="Bridge approach";road.layer=0;road.width=4;road.shoulder=2;road.points={{128,10,-16},{128,10,16}};d.ribbons.push_back(road);
 std::string error,before,after;assert(encode(d,before,error));PaintField field;assert(bake_paint(d,field,error));assert(encode(d,after,error)&&before==after);
 auto weight=[&](int x,int z,unsigned layer){for(const auto &tile:field.tiles){int ix=(x-tile.x*128)/2,iz=(z-tile.z*128)/2;if(ix>=0&&ix<=64&&iz>=0&&iz<=64)return tile.weights[(iz*paint_side+ix)*layer_limit+layer]/255.;}return 0.;};
 close(weight(128,0,1),1);close(weight(128,0,0),0); // Waterbed wins over road ground at a crossing.
 close(weight(128,4,1),1);close(weight(128,6,1),127./255.);close(weight(128,8,1),0); // Feathered dry bank.
 const PaintTile *left=nullptr,*right=nullptr;for(const auto &tile:field.tiles){if(tile.x==0&&tile.z==0)left=&tile;if(tile.x==1&&tile.z==0)right=&tile;}assert(left&&right);
 for(unsigned z=0;z<paint_side;++z)for(unsigned k=0;k<layer_limit;++k)assert(left->weights[(z*paint_side+64)*layer_limit+k]==right->weights[z*paint_side*layer_limit+k]);
 d.ribbons[0].layer=-1;assert(bake_paint(d,field,error));close(weight(128,0,1),0); // Explicit None preserves legacy behavior.
}
void projection(){
 Projection p;auto c=project(p,{0,0,0},{0,1,0});close(c.u,.5);close(c.v,.5);close(c.coverage,1);
 assert(project(p,{0,0,0},{0,-1,0}).coverage==0);assert(project(p,{2,0,0},{0,1,0}).coverage==0);
 assert(project(p,{.95,0,0},{0,1,0}).coverage>0&&project(p,{.95,0,0},{0,1,0}).coverage<.5);
 p.rotation={90,0,0};c=project(p,{0,0,0},{0,0,1});close(c.coverage,1);assert(project(p,{0,0,0},{0,1,0}).coverage<1e-5);
 p.size[0]=0;assert(project(p,{0,0,0},{0,0,1}).coverage==0);
}
void wind_field(){
 Data d;Wind w;w.speed=2;w.direction={0,1};d.winds.push_back(w);
 auto a=wind(d,{0,0,0},3,{0,0}),b=wind(d,{0,0,0},11,{0,0});close(a[0],0);close(a[1],b[1]);assert(a[1]>1&&a[1]<3);
 a=wind(d,{12,0,0},3,{.2f,.3f});close(a[0],.2);close(a[1],.3);
 for(unsigned i=0;i<16;i++)d.winds.push_back(w);
 a=wind(d,{0,0,0},3,{0,0});close(std::hypot(a[0],a[1]),5);
}
void spline(){
 Ribbon r;r.smooth=false;r.points={{0,2,0},{20,4,0},{40,4,10}};BakedRibbon out;std::string error;assert(bake_ribbon(r,out,error));assert(out.samples.size()>20);
 auto hit=nearest(out,10,.5);assert(hit.inside);close(hit.along,10);close(hit.height,3);assert(!nearest(out,10,10).inside);
 r.smooth=true;assert(bake_ribbon(r,out,error));const auto before=out.samples.size();r.layer=std::numeric_limits<int>::max();assert(!bake_ribbon(r,out,error)&&out.samples.size()==before);
 r.layer=-1;r.points[1]=r.points[0];assert(!bake_ribbon(r,out,error));
 assert(!nearest(out,std::numeric_limits<double>::quiet_NaN(),0).inside);
}
void background_image_import(){
 auto d=material();const auto original=d.images[0];Image revision={"source file","surfaces/revision.png",ImageRole::Color};
 Data staged;std::string error;int selected=7;
 assert(stage_image_import(d,revision,0,&original,staged,selected,error));
 assert(selected==0&&staged.images[0].name==original.name&&staged.images[0].path==revision.path&&d.images[0].path==original.path);
 // Undo can replace/remove an image while its source is decoding. Never
 // replace the new occupant of that index, or append a stale replacement.
 const auto before=staged.images[0].path;auto changed=d;changed.images[0].path="surfaces/other.png";selected=7;
 assert(!stage_image_import(changed,revision,0,&original,staged,selected,error)&&selected==7&&staged.images[0].path==before);
 changed=d;changed.images.clear();changed.layers[0].color=changed.layers[1].color=-1;
 assert(!stage_image_import(changed,revision,0,&original,staged,selected,error)&&staged.images[0].path==before);
 auto wrong=revision;wrong.role=ImageRole::Normal;
 assert(!stage_image_import(d,wrong,0,&original,staged,selected,error));
 revision.name=original.name;assert(stage_image_import(d,revision,-1,nullptr,staged,selected,error));
 assert(selected==1&&staged.images[1].name==original.name+" 2");
 auto full=d;for(unsigned i=1;i<image_limit;i++)full.images.push_back({"Image "+std::to_string(i),"surfaces/image.png",ImageRole::Color});
 assert(!stage_image_import(full,revision,-1,nullptr,staged,selected,error)&&staged.images.size()==2);
 wrong=revision;wrong.path="../escape.png";
 assert(!stage_image_import(d,wrong,-1,nullptr,staged,selected,error)&&staged.images.size()==2);
}
}
int main(){codec();paint();riverbed_material();projection();wind_field();spline();background_image_import();std::puts("PASS surface authoring: atomic codec/validation, role safety, tile seams/erase/budget, riverbed coverage/bank feather/crossing priority, oriented projection, bounded periodic wind/splines and stale background image reimport rejection");}
