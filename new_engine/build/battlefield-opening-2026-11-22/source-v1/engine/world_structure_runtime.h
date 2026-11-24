#ifndef VALWOOD_WORLD_STRUCTURE_RUNTIME_H
#define VALWOOD_WORLD_STRUCTURE_RUNTIME_H
#include "engine/world_structure_splines.h"
#include "engine/world_physics.h"
#include <array>
#include <cstring>

// Main-thread capture owns every terrain sample. The resulting geometry is
// immutable CPU data: workers and render upload consume exactly the same faces.
namespace world_structure_runtime {
using Point=world_structure::Point;
using Ground=double(*)(double,double,const void *);
struct Vertex {float x=0,y=0,z=0,nx=0,ny=1,nz=0,u=0,v=0,material=0;};
struct Mesh {
    world_structure::Id owner_id=0;world_structure::Kind kind=world_structure::Kind::Wall;
    Point origin,grain_forward={0,0,1};std::vector<Vertex> vertices;world_physics::Document collision;
    std::array<double,6> bounds={1e30,-1e30,1e30,-1e30,1e30,-1e30};
    unsigned segments=0,posts=0,supports=0;
};
struct Prepared {
    std::vector<Mesh> meshes;std::uint64_t fingerprint=14695981039346656037ull;
    std::uint64_t vertices=0,triangles=0;unsigned bridges=0,walls=0,segments=0;
    std::array<double,6> bounds={1e30,-1e30,1e30,-1e30,1e30,-1e30};
};
inline Point plus(Point p,double y){p.y+=y;return p;}
inline Point add(Point a,Point b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Point sub(Point a,Point b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Point mul(Point p,double v){return {p.x*v,p.y*v,p.z*v};}
inline double dot(Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Point cross(Point a,Point b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline std::uint64_t hash(const void *p,std::size_t n,std::uint64_t h){auto b=static_cast<const unsigned char*>(p);while(n--){h^=*b++;h*=1099511628211ull;}return h;}
inline void bound(std::array<double,6> &b,Point p){b[0]=std::min(b[0],p.x);b[1]=std::max(b[1],p.x);b[2]=std::min(b[2],p.y);b[3]=std::max(b[3],p.y);b[4]=std::min(b[4],p.z);b[5]=std::max(b[5],p.z);}
inline bool triangle(Mesh &m,Point a,Point b,Point c,float material){
    const auto n=cross(sub(b,a),sub(c,a));const double length=std::sqrt(dot(n,n));
    if(!std::isfinite(length)||length<1e-8)return false;
    const Point normal=mul(n,1/length);auto &body=m.collision.bodies.front();
    if(m.vertices.size()+3>world_physics::vertex_limit)return false;
    const auto start=std::uint32_t(body.vertices.size());
    for(Point p:{a,b,c}){
        const Point local=sub(p,m.origin);
        // Quantize once. Both GL and Jolt use these exact float local positions
        // with the same double world origin, including the sloping top deck.
        Vertex v;v.x=float(local.x);v.y=float(local.y);v.z=float(local.z);
        v.nx=float(normal.x);v.ny=float(normal.y);v.nz=float(normal.z);v.material=material;
        // Horizontal surfaces use XZ grain; near-vertical faces use their
        // dominant plane so stone courses retain metre-scaled detail.
        if(std::abs(normal.y)>.5&&material<.5f){v.u=float(local.x*m.grain_forward.x+local.z*m.grain_forward.z);v.v=float(-local.x*m.grain_forward.z+local.z*m.grain_forward.x);}
        else if(std::abs(normal.y)>.5){v.u=float(local.x);v.v=float(local.z);}
        else {v.u=float(std::abs(normal.x)>std::abs(normal.z)?local.z:local.x);v.v=float(local.y);}
        m.vertices.push_back(v);body.vertices.push_back({v.x,v.y,v.z});bound(m.bounds,add(m.origin,{v.x,v.y,v.z}));
    }
    body.triangles.push_back({start,start+1,start+2});return true;
}
inline bool quad(Mesh &m,Point a,Point b,Point c,Point d,float mat){return triangle(m,a,b,c,mat)&&triangle(m,a,c,d,mat);}
// Closed prism. a,b,c,d are ordered counter-clockwise from above; bottom
// vertices can have independent heights for terrain-contact foundations.
inline bool prism(Mesh &m,const std::array<Point,4> &top,const std::array<Point,4> &bottom,float mat){
    // Use the shared LB--RA diagonal, also used by deck_height().
    if(!triangle(m,top[0],top[1],top[3],mat)||!triangle(m,top[3],top[1],top[2],mat)||
       !triangle(m,bottom[0],bottom[3],bottom[1],mat)||!triangle(m,bottom[3],bottom[2],bottom[1],mat))return false;
    for(unsigned i=0;i<4;++i){unsigned j=(i+1)%4;if(!quad(m,top[i],bottom[i],bottom[j],top[j],mat))return false;}return true;
}
inline bool beam(Mesh &m,Point a,Point b,double width,double height,float mat){
    const double length=std::hypot(b.x-a.x,b.z-a.z);if(length<.001)return false;
    const Point side={-(b.z-a.z)/length*width*.5,0,(b.x-a.x)/length*width*.5};
    std::array<Point,4> top={plus(add(a,side),height*.5),plus(add(b,side),height*.5),plus(sub(b,side),height*.5),plus(sub(a,side),height*.5)},bottom=top;
    for(auto &p:bottom){p.y-=height;}
    return prism(m,top,bottom,mat);
}
inline bool post(Mesh &m,Point p,double bottom,double top,double width,float mat){
    if(top-bottom<.01)return true;
    const double h=width*.5;std::array<Point,4> hi={Point{p.x-h,top,p.z-h},Point{p.x-h,top,p.z+h},Point{p.x+h,top,p.z+h},Point{p.x+h,top,p.z-h}},lo=hi;
    for(auto &v:lo){v.y=bottom;}
    return prism(m,hi,lo,mat);
}
inline bool ground_at(Ground ground,const void *context,Point p,double &y,std::string &error){
    if(!ground){error="Structure supports need a terrain sampler";return false;}y=ground(p.x,p.z,context);
    if(!std::isfinite(y)||y< -2000||y>12000){error="Structure support reaches invalid terrain";return false;}return true;
}
inline bool prepare(const std::vector<world_structure::Definition> &definitions,Ground ground,const void *context,Prepared &out,std::string &error){
    Prepared next;if(definitions.size()>world_structure::definition_limit){error="Structure definition budget exceeded";return false;}
    std::vector<world_structure::Id> ids;
    for(const auto &d:definitions){
        if(std::find(ids.begin(),ids.end(),d.id)!=ids.end()){error="Duplicate structure identity";return false;}ids.push_back(d.id);
        world_structure::Generated generated;if(!world_structure::build(d,generated,error))return false;
        if(d.kind==world_structure::Kind::Wall&&d.follow_ground)for(auto &s:generated.segments){
            double a=0,b=0;if(!ground_at(ground,context,s.a,a,error)||!ground_at(ground,context,s.b,b,error))return false;
            if(std::abs(b-a)>s.horizontal_length*2){error="Ground-following wall crosses terrain too steep for a safe foundation";return false;}
            s.a.y=s.left_a.y=s.right_a.y=a;s.b.y=s.left_b.y=s.right_b.y=b;s.length=world_structure::distance(s.a,s.b);
        }
        Mesh m;m.owner_id=d.id;m.kind=d.kind;m.origin=d.points.front();m.segments=unsigned(generated.segments.size());
        world_physics::Body body;body.id=1;body.name=(d.kind==world_structure::Kind::Bridge?"Spline bridge ":"Spline wall ")+std::to_string(d.id);body.motion=world_physics::Motion::Static;body.shape=world_physics::Shape::Mesh;body.friction=.85f;body.restitution=0;body.pose.position={m.origin.x,m.origin.y,m.origin.z};m.collision.bodies.push_back(std::move(body));
        auto geometry_failure=[&](){error="Structure mesh exceeds its bounded geometry budget";return false;};
        double travelled=0,next_post=0,next_support=0;
        for(unsigned index=0;index<generated.segments.size();++index){
            const auto &s=generated.segments[index];
            m.grain_forward={(s.b.x-s.a.x)/s.horizontal_length,0,(s.b.z-s.a.z)/s.horizontal_length};
            std::array<Point,4> top={s.left_a,s.left_b,s.right_b,s.right_a},bottom=top;
            if(d.kind==world_structure::Kind::Wall){
                for(unsigned i=0;i<4;++i){top[i].y+=d.height;double y=bottom[i].y;if(d.supports){if(!ground_at(ground,context,bottom[i],y,error))return false;if(bottom[i].y-y>d.support_depth+.001){error="Wall foundation exceeds support depth; lower the wall or increase support depth";return false;}}bottom[i].y=std::min(bottom[i].y,y)-.12;}
                if(!prism(m,top,bottom,1))return geometry_failure();
                // A slightly proud coping gives the stone wall a finished top.
                Point a=plus(s.a,d.height+.06),b=plus(s.b,d.height+.06);
                if(!beam(m,a,b,d.thickness+.10,.16,1))return geometry_failure();
            }else{
                for(auto &p:bottom)p.y-=d.thickness;
                if(!prism(m,top,bottom,0))return geometry_failure();
                if(d.railings){
                    for(int side:{1,-1}){
                        auto a=side==1?s.left_a:s.right_a,b=side==1?s.left_b:s.right_b;
                        // Posts occupy the deck edge; top and middle rails are
                        // physical meshes too, not just decorative lines.
                        if(!beam(m,plus(a,d.rail_height),plus(b,d.rail_height),.16,.16,0)||!beam(m,plus(a,d.rail_height*.48),plus(b,d.rail_height*.48),.10,.12,0))return geometry_failure();
                    }
                }
                while(next_post<=travelled+s.length+.0001){
                    const double t=std::clamp((next_post-travelled)/s.length,0.,1.);
                    if(d.railings)for(int side:{1,-1}){auto p=world_structure::mix(side==1?s.left_a:s.right_a,side==1?s.left_b:s.right_b,t);if(!post(m,p,p.y-d.thickness,p.y+d.rail_height+.14,.22,0))return geometry_failure();++m.posts;}
                    next_post+=2.5;
                }
                if(d.supports){
                    while(next_support<=travelled+s.length+.0001){
                        const double t=std::clamp((next_support-travelled)/s.length,0.,1.);auto p=world_structure::mix(s.a,s.b,t);double y=0;
                        if(!ground_at(ground,context,p,y,error))return false;
                        const double underside=p.y-d.thickness;
                        if(underside-y>d.support_depth+.001){error="Bridge pier exceeds support depth; increase it or disable supports for this span";return false;}
                        if(underside>y+.15){
                            const Point side=mul(sub(world_structure::mix(s.left_a,s.left_b,t),p),.72);
                            for(int sign:{1,-1}){auto foot=add(p,mul(side,sign));double floor=0;if(!ground_at(ground,context,foot,floor,error))return false;if(underside-floor>d.support_depth+.001){error="Bridge pier exceeds support depth at its terrain contact";return false;}if(!post(m,foot,floor-.15,underside,.48,1))return geometry_failure();++m.supports;}
                            auto a=add(p,side),b=sub(p,side);if(!beam(m,plus(a,-d.thickness-.12),plus(b,-d.thickness-.12),.32,.28,0))return geometry_failure();
                        }
                        next_support+=8;
                    }
                }
            }
            travelled+=s.length;
        }
        // Always close the far railing end, regardless of span/spacing length.
        if(d.kind==world_structure::Kind::Bridge&&d.railings){const auto &s=generated.segments.back();for(Point p:{s.left_b,s.right_b}){if(!post(m,p,p.y-d.thickness,p.y+d.rail_height+.14,.22,0))return geometry_failure();++m.posts;}}
        if(m.vertices.empty())return geometry_failure();
        next.vertices+=m.vertices.size();next.triangles+=m.collision.bodies[0].triangles.size();next.segments+=m.segments;
        if(next.vertices>1000000||next.triangles>333333){error="Combined structures exceed their geometry budget";return false;}
        if(d.kind==world_structure::Kind::Bridge)++next.bridges;else ++next.walls;
        next.fingerprint=hash(&m.owner_id,sizeof m.owner_id,next.fingerprint);next.fingerprint=hash(&m.origin,sizeof m.origin,next.fingerprint);
        next.fingerprint=hash(m.vertices.data(),m.vertices.size()*sizeof(Vertex),next.fingerprint);
        bound(next.bounds,{m.bounds[0],m.bounds[2],m.bounds[4]});bound(next.bounds,{m.bounds[1],m.bounds[3],m.bounds[5]});next.meshes.push_back(std::move(m));
    }
    out=std::move(next);error.clear();return true;
}
// Exact render-triangle readback for CPU tests/picking. Storey filtering is
// explicit: a bridge overhead must never become an invisible floor below it.
inline bool floor_near(const Prepared &data,double x,double z,double feet,double &height,double down=3,double up=.45){
    if(!std::isfinite(feet)||!std::isfinite(x)||!std::isfinite(z))return false;
    bool found=false;double selected=-1e30;
    for(const auto &mesh:data.meshes){if(x<mesh.bounds[0]||x>mesh.bounds[1]||z<mesh.bounds[4]||z>mesh.bounds[5])continue;
        for(std::size_t i=0;i<mesh.vertices.size();i+=3){const auto &v=mesh.vertices[i];if(v.ny<.65)continue;
            auto p=[&](unsigned k){const auto &a=mesh.vertices[i+k];return Point{mesh.origin.x+a.x,mesh.origin.y+a.y,mesh.origin.z+a.z};};double y=0;
            if(world_structure::triangle_height(p(0),p(1),p(2),x,z,y)&&y>=feet-down&&y<=feet+up&&y>selected){found=true;selected=y;}
        }
    }if(found)height=selected;return found;
}
}
#endif
