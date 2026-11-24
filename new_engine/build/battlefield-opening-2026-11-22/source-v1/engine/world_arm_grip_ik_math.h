#ifndef VALWOOD_WORLD_ARM_GRIP_IK_MATH_H
#define VALWOOD_WORLD_ARM_GRIP_IK_MATH_H
#include <algorithm>
#include <array>
#include <cmath>
// CPU-only candidate, not installed in the production source. Coordinates and
// xyzw quaternion rotation agree with world_rigid_weapon_pose; matrices below
// are row-major and preserve the actual imported joint-basis scale.
namespace world_arm_grip_ik {
using Point=std::array<double,3>;
using Basis=std::array<Point,3>;
using Quaternion=std::array<double,4>;
struct Transform {Basis basis={};Point position={};};
using Pose=std::array<Transform,8>; // L Shoulder/Arm/ForeArm/Hand, then R.
struct Interval {Pose a={},b={};Quaternion weapon_delta={0,0,0,1};Point pivot_a={},pivot_b={};};
struct Limits {
    double clavicle_radians=.31415926535897932385,shoulder_move=.045;
    double elbow_move=.18,elbow_pole=.001,reserve=.004,reserve_fade=.12;
    double length_tolerance=1.e-7,reach_epsilon=1.e-6;
};
struct ArmMetrics {double upper_length=0,fore_length=0,clavicle_length=0,upper_error=0,fore_error=0,clavicle_error=0;
    double reach_margin=0,reserve=0,clavicle_radians=0,shoulder_move=0,elbow_move=0,wrist_error=0;};
struct Result {Pose pose={};Basis weapon_rotation={};std::array<ArmMetrics,2> arms={};};
enum class Failure {None,Input,Quaternion,Basis,Length,ClavicleReach,ClavicleLimit,ArmReach,ElbowPole,ElbowLimit,Rotation};
inline Point add(const Point&a,const Point&b){return {a[0]+b[0],a[1]+b[1],a[2]+b[2]};}
inline Point sub(const Point&a,const Point&b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
inline Point mul(const Point&a,double s){return {a[0]*s,a[1]*s,a[2]*s};}
inline double dot(const Point&a,const Point&b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline Point cross(const Point&a,const Point&b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
inline double length(const Point&a){return std::sqrt(dot(a,a));}
inline bool finite(const Point&a){return std::isfinite(a[0])&&std::isfinite(a[1])&&std::isfinite(a[2]);}
inline bool unit(Point&a,double epsilon=1.e-8){const double n=length(a);if(!std::isfinite(n)||n<=epsilon)return false;a=mul(a,1/n);return true;}
inline Point lerp(const Point&a,const Point&b,double t){return add(mul(a,1-t),mul(b,t));}
inline Basis identity(){return {{{1,0,0},{0,1,0},{0,0,1}}};}
inline Basis transpose(const Basis&a){Basis out;for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)out[r][c]=a[c][r];return out;}
inline Point apply(const Basis&a,const Point&b){return {dot(a[0],b),dot(a[1],b),dot(a[2],b)};}
inline Basis multiply(const Basis&a,const Basis&b){const Basis bt=transpose(b);Basis out;for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)out[r][c]=dot(a[r],bt[c]);return out;}
inline double determinant(const Basis&a){return dot(a[0],cross(a[1],a[2]));}
inline bool finite(const Basis&a){return finite(a[0])&&finite(a[1])&&finite(a[2]);}
inline bool inverse(const Basis&a,Basis&out){const double det=determinant(a);if(!std::isfinite(det)||std::abs(det)<1.e-20)return false;const Basis cof={cross(a[1],a[2]),cross(a[2],a[0]),cross(a[0],a[1])};out=transpose(cof);for(auto&r:out)r=mul(r,1/det);return finite(out);}
// Polar extraction matches scipy Rotation.from_matrix for the imported nearly
// rigid positive-determinant bases. It never replaces the actual output basis.
inline bool orthogonal(const Basis&input,Basis&out){
    if(!finite(input)||determinant(input)<=0)return false;
    double n=0;for(const auto&r:input)n+=dot(r,r);n=std::sqrt(n/3);
    if(!(n>1.e-8&&n<1.e8))return false;
    out=input;for(auto&r:out)r=mul(r,1/n);
    for(unsigned step=0;step<10;++step){Basis inv;if(!inverse(out,inv))return false;const Basis invt=transpose(inv);double change=0;for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c){const double v=.5*(out[r][c]+invt[r][c]);change=std::max(change,std::abs(v-out[r][c]));out[r][c]=v;}if(change<1.e-14)break;}
    const Basis gram=multiply(out,transpose(out));double error=0;for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)error=std::max(error,std::abs(gram[r][c]-(r==c?1.:0.)));
    return error<1.e-10&&determinant(out)>.999999;
}
inline bool positive(Quaternion&q){double n=0;for(double v:q){if(!std::isfinite(v))return false;n+=v*v;}if(!(n>1.e-20))return false;n=std::sqrt(n);for(double&v:q)v/=n;bool flip=q[3]<0;
    if(q[3]==0){unsigned i=0;for(unsigned j=1;j<3;++j)if(std::abs(q[j])>std::abs(q[i]))i=j;flip=q[i]<0;}
    if(flip)for(double&v:q)v=-v;
    return true;
}
// Keep the native float quaternion's rounding BEFORE the identity mix, just
// as the shader does. Normalizing Q early is a subtly different motion path.
inline bool signed_delta(Quaternion&q){double n=0;for(double v:q){if(!std::isfinite(v))return false;n+=v*v;}if(std::abs(n-1)>1.e-4)return false;bool flip=q[3]<0;if(q[3]==0){unsigned i=0;for(unsigned j=1;j<3;++j)if(std::abs(q[j])>std::abs(q[i]))i=j;flip=q[i]<0;}if(flip)for(double&v:q)v=-v;return true;}
inline Basis matrix(const Quaternion&q){const double x=q[0],y=q[1],z=q[2],w=q[3];return {{{1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)},{2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)},{2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)}}};}
inline bool quaternion(const Basis&input,Quaternion&q){Basis a;if(!orthogonal(input,a))return false;const double trace=a[0][0]+a[1][1]+a[2][2];
    if(trace>0){const double s=std::sqrt(trace+1)*2;q={ (a[2][1]-a[1][2])/s,(a[0][2]-a[2][0])/s,(a[1][0]-a[0][1])/s,.25*s};}
    else{unsigned i=a[1][1]>a[0][0]?1:0;if(a[2][2]>a[i][i])i=2;const unsigned j=(i+1)%3,k=(i+2)%3;const double s=std::sqrt(std::max(0.,1+a[i][i]-a[j][j]-a[k][k]))*2;if(!(s>1.e-12))return false;q[i]=.25*s;q[j]=(a[j][i]+a[i][j])/s;q[k]=(a[k][i]+a[i][k])/s;q[3]=(a[k][j]-a[j][k])/s;}
    return positive(q);
}
inline bool nlerp(Quaternion delta,double t,Quaternion&out){if(!signed_delta(delta)||!std::isfinite(t)||t<0||t>1)return false;out={delta[0]*t,delta[1]*t,delta[2]*t,1+(delta[3]-1)*t};return positive(out);}
inline bool mix_rotation(const Basis&a,const Basis&b,double t,Basis&out){Quaternion q,p;if(!quaternion(multiply(b,transpose(a)),q)||!nlerp(q,t,p))return false;out=multiply(matrix(p),a);return finite(out);}
inline bool swing(Point from,Point to,Basis&out){if(!unit(from)||!unit(to))return false;const double cosine=std::clamp(dot(from,to),-1.,1.);if(cosine<-1+1.e-10)return false;Quaternion q;const Point axis=cross(from,to);q={axis[0],axis[1],axis[2],1+cosine};if(!positive(q))return false;out=matrix(q);return true;}
inline double smooth(double x){x=std::clamp(x,0.,1.);return x*x*(3-2*x);}
inline bool solve(const Interval&in,double t,Result&out,Failure*failure=nullptr,const Limits&limit={}){
    auto fail=[&](Failure reason){if(failure)*failure=reason;return false;};if(failure)*failure=Failure::None;
    if(!std::isfinite(t)||t<0||t>1||!std::isfinite(limit.clavicle_radians)||limit.clavicle_radians<=0||limit.clavicle_radians>.31415926535898||!std::isfinite(limit.shoulder_move)||limit.shoulder_move<=0||limit.shoulder_move>.04500000001||!std::isfinite(limit.elbow_move)||limit.elbow_move<=0||limit.elbow_move>.18||!std::isfinite(limit.elbow_pole)||limit.elbow_pole<=0||!std::isfinite(limit.reserve)||limit.reserve<0||limit.reserve>.004||!std::isfinite(limit.reserve_fade)||limit.reserve_fade<=0||!std::isfinite(limit.length_tolerance)||limit.length_tolerance<0||!std::isfinite(limit.reach_epsilon)||limit.reach_epsilon<0)return fail(Failure::Input);
    for(unsigned i=0;i<8;++i){Basis ignored;if(!finite(in.a[i].position)||!finite(in.b[i].position)||!orthogonal(in.a[i].basis,ignored)||!orthogonal(in.b[i].basis,ignored))return fail(Failure::Basis);}
    if(!finite(in.pivot_a)||!finite(in.pivot_b))return fail(Failure::Input);
    Quaternion q=in.weapon_delta,qt;if(!signed_delta(q)||!nlerp(q,t,qt))return fail(Failure::Quaternion);
    Quaternion full=q;if(!positive(full))return fail(Failure::Quaternion);
    Result result;result.weapon_rotation=matrix(qt);const Basis inverse_q=transpose(matrix(full));
    for(unsigned i=0;i<8;++i){result.pose[i].position=lerp(in.a[i].position,in.b[i].position,t);for(unsigned r=0;r<3;++r)result.pose[i].basis[r]=lerp(in.a[i].basis[r],in.b[i].basis[r],t);}
    // Exact endpoint packets are part of the contract, including imported scale
    // and residual rounding. Never run a corrective solve on the endpoints.
    if(t==0||t==1){result.pose=t==0?in.a:in.b;out=result;return true;}
    const Point primary=lerp(in.pivot_a,in.pivot_b,t);
    for(unsigned side=0;side<2;++side){const unsigned clav=side*4,a=clav+1,b=clav+2,c=clav+3;auto&proof=result.arms[side];
        const Point original_s=result.pose[a].position,C=result.pose[clav].position;
        Point S=original_s,clavicle_direction=sub(S,C);if(!unit(clavicle_direction))return fail(Failure::Length);
        const Point offset_a=sub(in.a[c].position,in.pivot_a),offset_b=world_arm_grip_ik::apply(inverse_q,sub(in.b[c].position,in.pivot_b));
        const Point target=add(primary,world_arm_grip_ik::apply(result.weapon_rotation,lerp(offset_a,offset_b,t)));
        const double u=length(sub(in.a[b].position,in.a[a].position)),v=length(sub(in.a[c].position,in.a[b].position)),clen=length(sub(in.a[a].position,in.a[clav].position));
        if(!(u>1.e-6&&v>1.e-6&&clen>1.e-6)||std::abs(u-length(sub(in.b[b].position,in.b[a].position)))>limit.length_tolerance||std::abs(v-length(sub(in.b[c].position,in.b[b].position)))>limit.length_tolerance||std::abs(clen-length(sub(in.b[a].position,in.b[clav].position)))>limit.length_tolerance)return fail(Failure::Length);
        S=add(C,mul(clavicle_direction,clen));const double fixed_reach=length(sub(target,S));
        const double reserve=limit.reserve*smooth(t/limit.reserve_fade)*smooth((1-t)/limit.reserve_fade),width=2*reserve,excess=std::max(0.,fixed_reach-(u+v-width));
        const double correction=width>1.e-10?(excess<width?excess*excess/(2*width):excess-reserve):std::max(0.,fixed_reach-u-v);
        const double reach_limit=fixed_reach-correction;double clavicle_angle=0;
        if(correction>1.e-10){Point axis_direction=sub(target,C);const double d=length(axis_direction);if(!unit(axis_direction)||std::abs(d-clen)>=reach_limit)return fail(Failure::ClavicleReach);
            const double cone=std::acos(std::clamp((clen*clen+d*d-reach_limit*reach_limit)/(2*clen*d),-1.,1.)),beta=std::acos(std::clamp(dot(clavicle_direction,axis_direction),-1.,1.));clavicle_angle=std::max(0.,beta-cone);
            if(clavicle_angle>0){Point axis=cross(clavicle_direction,axis_direction);if(!unit(axis))return fail(Failure::ClavicleReach);const double half=clavicle_angle*.5;Quaternion rotation={axis[0]*std::sin(half),axis[1]*std::sin(half),axis[2]*std::sin(half),std::cos(half)};S=add(C,mul(world_arm_grip_ik::apply(matrix(rotation),clavicle_direction),clen));}}
        const double shoulder_move=length(sub(S,original_s));if(clavicle_angle>limit.clavicle_radians+1.e-7||shoulder_move>limit.shoulder_move+1.e-7)return fail(Failure::ClavicleLimit);
        Basis clavicle_rotation,align;if(!mix_rotation(in.a[clav].basis,in.b[clav].basis,t,clavicle_rotation))return fail(Failure::Rotation);
        const Point clavicle_axis=world_arm_grip_ik::apply(multiply(clavicle_rotation,transpose(in.a[clav].basis)),sub(in.a[a].position,in.a[clav].position));if(!swing(clavicle_axis,sub(S,C),align))return fail(Failure::Rotation);result.pose[clav].basis=multiply(align,clavicle_rotation);result.pose[a].position=S;
        Point direction=sub(target,S);const double distance=length(direction);if(!(std::abs(u-v)+limit.reach_epsilon<distance&&distance<u+v-limit.reach_epsilon)||!unit(direction))return fail(Failure::ArmReach);
        const Point guide=lerp(in.a[b].position,in.b[b].position,t);Point pole=sub(sub(guide,S),mul(direction,dot(sub(guide,S),direction)));if(length(pole)<=limit.elbow_pole||!unit(pole))return fail(Failure::ElbowPole);
        const double along=(u*u-v*v+distance*distance)/(2*distance);const Point E=add(add(S,mul(direction,along)),mul(pole,std::sqrt(std::max(0.,u*u-along*along))));
        Basis arm_rotation,fore_rotation;if(!mix_rotation(in.a[a].basis,in.b[a].basis,t,arm_rotation)||!mix_rotation(in.a[b].basis,in.b[b].basis,t,fore_rotation))return fail(Failure::Rotation);
        Point arm_axis=world_arm_grip_ik::apply(multiply(arm_rotation,transpose(in.a[a].basis)),sub(in.a[b].position,in.a[a].position)),fore_axis=world_arm_grip_ik::apply(multiply(fore_rotation,transpose(in.a[b].basis)),sub(in.a[c].position,in.a[b].position));
        if(!swing(arm_axis,sub(E,S),align))return fail(Failure::Rotation);
        result.pose[a].basis=multiply(align,arm_rotation);
        if(!swing(fore_axis,sub(target,E),align))return fail(Failure::Rotation);
        result.pose[b].basis=multiply(align,fore_rotation);result.pose[b].position=E;
        Basis residual;if(!mix_rotation(in.a[c].basis,multiply(inverse_q,in.b[c].basis),t,residual))return fail(Failure::Rotation);result.pose[c].basis=multiply(result.weapon_rotation,residual);result.pose[c].position=target;
        const double elbow_move=length(sub(E,guide));if(elbow_move>=limit.elbow_move)return fail(Failure::ElbowLimit);
        proof={u,v,clen,std::abs(length(sub(E,S))-u),std::abs(length(sub(target,E))-v),std::abs(length(sub(S,C))-clen),u+v-distance,reserve,clavicle_angle,shoulder_move,elbow_move,length(sub(result.pose[c].position,target))};
    }
    out=result;return true;
}
}
#endif
