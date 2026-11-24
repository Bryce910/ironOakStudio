#ifndef VALWOOD_WORLD_RIVER_REFLECTION_SCISSOR_H
#define VALWOOD_WORLD_RIVER_REFLECTION_SCISSOR_H
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <initializer_list>

// Exact float uniforms and uploaded RiverVertex positions. No allocation/GL.
// Only a certified sole-river consumer may use this support rectangle. Other
// consumers, unknown geometry, invalid values and near-plane uncertainty keep
// the full existing target. The reflected SKY always remains full resolution.
struct WorldRiverReflectionInput {
    float main_vp[16]={},reflection_vp[16]={};
    float origin[2]={},offset_y=0,shift_y=0,level=0;
    int width=0,height=0;
};
struct WorldRiverReflectionRegion {int x=0,y=0,width=0,height=0;bool restricted=false;};
class WorldRiverReflectionSupport {
    struct Point {std::array<double,4> main={},reflection={};double height=0;};
    WorldRiverReflectionInput in;
    bool valid=true,any=false;
    double xmin=1e100,ymin=1e100,xmax=-1e100,ymax=-1e100;
    static constexpr double eps=32.*std::numeric_limits<float>::epsilon();
    static bool finite(double x){return std::isfinite(x)&&std::abs(x)<1e9;}
    static Point blend(const Point&a,const Point&b,double t){Point p;for(unsigned k=0;k<4;k++){p.main[k]=a.main[k]+(b.main[k]-a.main[k])*t;p.reflection[k]=a.reflection[k]+(b.reflection[k]-a.reflection[k])*t;}p.height=a.height+(b.height-a.height)*t;return p;}
    template<class Distance> bool clip(std::array<Point,16>&poly,unsigned&count,Distance distance){
        if(!count)return true;
        std::array<Point,16> next;unsigned n=0;
        for(unsigned i=0;i<count;i++){
            const auto&a=poly[i],&b=poly[(i+1)%count];const double da=distance(a),db=distance(b);
            if(!finite(da)||!finite(db))return false;
            if(da>=0){if(n==next.size())return false;next[n++]=a;}
            if((da>=0)!=(db>=0)) {const double t=da/(da-db);if(!std::isfinite(t)||t<0||t>1||n==next.size())return false;next[n++]=blend(a,b,t);}
        }
        poly=next;count=n;return true;
    }
public:
    explicit WorldRiverReflectionSupport(const WorldRiverReflectionInput &input):in(input){
        valid=in.width>0&&in.height>0&&in.width<=8192&&in.height<=8192;
        for(float x:in.main_vp)valid=valid&&finite(x);
        for(float x:in.reflection_vp)valid=valid&&finite(x);
        for(float x:in.origin)valid=valid&&finite(x);
        valid=valid&&finite(in.offset_y)&&finite(in.shift_y)&&finite(in.level);
    }
    // Cheap source-range broad phase before visiting retained triangles. Every
    // rejection is a common main clip plane or the exact-zero height band.
    bool outside_range(double x0,double z0,double x1,double z1,float y0,float y1)const{
        if(!valid||!finite(x0)||!finite(z0)||!finite(x1)||!finite(z1)||!finite(y0)||!finite(y1)||x0>x1||z0>z1||y0>y1)return false;
        const double herror=eps*(std::max(std::abs(double(y0)),std::abs(double(y1)))+std::abs(double(in.level))+1);
        if(y0>double(in.level)+double(.40f)+herror||y1<double(in.level)-double(.40f)-herror)return true;
        double upper[6];for(double &v:upper)v=-std::numeric_limits<double>::infinity();
        for(unsigned i=0;i<8;i++){
            const float x=float(i&1?x1:x0),y=i&2?y1:y0,z=float(i&4?z1:z0);
            const float rx=x-in.origin[0],ry=y+in.offset_y,rz=z-in.origin[1];
            const double point[4]={rx,ry,rz,1},error[4]={eps*(std::abs(double(x))+std::abs(double(in.origin[0]))+std::abs(double(rx))+1),eps*(std::abs(double(y))+std::abs(double(in.offset_y))+std::abs(double(ry))+1),eps*(std::abs(double(z))+std::abs(double(in.origin[1]))+std::abs(double(rz))+1),0};
            double c[4]={},e[4]={};for(unsigned row=0;row<4;row++){double magnitude=0;for(unsigned k=0;k<4;k++){const double m=in.main_vp[k*4+row];c[row]+=m*point[k];magnitude+=std::abs(m*point[k]);e[row]+=std::abs(m)*error[k];}e[row]+=eps*(magnitude+1);}
            for(unsigned axis=0;axis<3;axis++)for(unsigned side=0;side<2;side++){const double q=c[3]+(side?-1.:1.)*c[axis]+e[3]+e[axis]+eps;if(!finite(q))return false;upper[axis*2+side]=std::max(upper[axis*2+side],q);}
        }
        for(double q:upper)if(q<0)return true;
        return false;
    }
    void fail_open(){valid=false;}
    template<class Vertex> void triangle(const Vertex&a,const Vertex&b,const Vertex&c){
        if(!valid)return;
        std::array<Point,16> poly;const Vertex*vertices[3]={&a,&b,&c};
        double me[4]={},re[4]={},height_error=0;
        for(unsigned j=0;j<3;j++){
            const auto&v=*vertices[j];if(!finite(v.x)||!finite(v.y)||!finite(v.z)){valid=false;return;}
            // The same single-precision subtract/add stages as water.vs.
            const float px=v.x-in.origin[0],py=v.y+in.offset_y,pz=v.z-in.origin[1];
            const float ry=py+in.shift_y;
            const double p[4]={px,py,pz,1},r[4]={px,ry,pz,1};
            const double pe[4]={eps*(std::abs(double(v.x))+std::abs(double(in.origin[0]))+std::abs(double(px))+1),eps*(std::abs(double(v.y))+std::abs(double(in.offset_y))+std::abs(double(py))+1),eps*(std::abs(double(v.z))+std::abs(double(in.origin[1]))+std::abs(double(pz))+1),0};
            double qe[4]={pe[0],pe[1]+eps*(std::abs(double(py))+std::abs(double(in.shift_y))+std::abs(double(ry))+1),pe[2],0};
            poly[j].height=v.y;height_error=std::max(height_error,eps*(std::abs(double(v.y))+std::abs(double(in.level))+1));
            for(unsigned row=0;row<4;row++){
                double mm=0,rm=0,merr=0,rerr=0;
                for(unsigned k=0;k<4;k++){const double m=in.main_vp[k*4+row],q=in.reflection_vp[k*4+row];poly[j].main[row]+=m*p[k];poly[j].reflection[row]+=q*r[k];mm+=std::abs(m*p[k]);rm+=std::abs(q*r[k]);merr+=std::abs(m)*pe[k];rerr+=std::abs(q)*qe[k];}
                me[row]=std::max(me[row],merr+eps*(mm+1));re[row]=std::max(re[row],rerr+eps*(rm+1));
            }
        }
        unsigned count=3;
        // Outside this band the production .10--.40m planar-coverage fade is
        // exactly zero. Ignore grade/shore/depth masks: that only enlarges support.
        const double band=double(.40f)+height_error;
        if(!clip(poly,count,[&](const Point&p){return p.height-in.level+band;})||!clip(poly,count,[&](const Point&p){return in.level+band-p.height;})){valid=false;return;}
        for(unsigned axis=0;axis<3&&count;axis++)for(unsigned side=0;side<2&&count;side++){
            const double sign=side?-1.:1.,guard=me[3]+me[axis]+eps;
            if(!clip(poly,count,[&](const Point&p){return p.main[3]+sign*p.main[axis]+guard;})){valid=false;return;}
        }
        for(unsigned j=0;j<count;j++){
            const auto&p=poly[j];const double lo=p.reflection[3]-re[3],hi=p.reflection[3]+re[3];
            // Perspective extrema over a convex clipped polygon occur at its
            // vertices when every denominator stays positive. Fail open otherwise.
            if(!finite(lo)||!finite(hi)||lo<=.001){valid=false;return;}
            for(unsigned axis=0;axis<2;axis++)for(int sign:{-1,1})for(double w:{lo,hi}){
                const double uv=(p.reflection[axis]+sign*re[axis])/w*.5+.5;
                if(!finite(uv)){valid=false;return;}
                if(axis==0){xmin=std::min(xmin,uv);xmax=std::max(xmax,uv);}else{ymin=std::min(ymin,uv);ymax=std::max(ymax,uv);}
            }
            any=true;
        }
    }
    WorldRiverReflectionRegion region()const{
        WorldRiverReflectionRegion full={0,0,std::max(in.width,0),std::max(in.height,0),false};
        if(!valid||!any)return full;
        // Exact river shader caps: .009UV distortion +8-texel blur. Three
        // further texels cover bilinear footprint/raster/rounding conservatively.
        const double px=std::ceil(.009*in.width)+11.,py=std::ceil(.009*in.height)+11.;
        const int x0=int(std::floor(std::clamp(xmin*in.width-px,0.,double(in.width)))),y0=int(std::floor(std::clamp(ymin*in.height-py,0.,double(in.height))));
        const int x1=int(std::ceil(std::clamp(xmax*in.width+px,0.,double(in.width)))),y1=int(std::ceil(std::clamp(ymax*in.height+py,0.,double(in.height))));
        if(x1<=x0||y1<=y0||(x0==0&&y0==0&&x1==in.width&&y1==in.height))return full;
        return {x0,y0,x1-x0,y1-y0,true};
    }
};
#endif
