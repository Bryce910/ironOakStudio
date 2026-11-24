#include "engine/world_water_mirror_cache.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {
unsigned checks=0;
void check(bool value,const char *message) {
    ++checks;
    if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
using Pose=WorldWaterMirrorCache::Pose;
using Matrix=WorldWaterMirrorCache::Projection;
using Vec4=std::array<double,4>;
constexpr double pi=3.1415926535897932384626433832795;
Matrix multiply(const Matrix &a,const Matrix &b) {
    Matrix out{};
    for(int col=0;col<4;++col)for(int row=0;row<4;++row)
        for(int k=0;k<4;++k)out[col*4+row]+=a[k*4+row]*b[col*4+k];
    return out;
}
Matrix perspective(double fov,double aspect) {
    Matrix result{};const double f=1./std::tan(fov/2.);
    result[0]=float(f/aspect);result[5]=float(f);
    result[10]=float(-1000.1/999.9);result[11]=-1.f;
    result[14]=float(-200./999.9);return result;
}
Matrix rotate_x(double radians) {
    const float c=float(std::cos(radians)),s=float(std::sin(radians));
    return {1,0,0,0, 0,c,s,0, 0,-s,c,0, 0,0,0,1};
}
Vec4 transform(const Matrix &matrix,const Vec4 &point) {
    Vec4 result{};
    for(int row=0;row<4;++row)for(int col=0;col<4;++col)
        result[row]+=double(matrix[col*4+row])*point[col];
    return result;
}
double largest_difference(const Vec4 &a,const Vec4 &b) {
    double largest=0.;for(int i=0;i<4;++i)largest=std::max(largest,std::abs(a[i]-b[i]));
    return largest;
}
Pose normal_pose() { return {12000.,8.,-5000.,.4,-.2,pi/3.,16./9.,2.,800,450}; }
}

int main() {
    const Pose still=normal_pose();
    const Matrix lens=perspective(still.fov,still.aspect);
    // Reflecting a horizontal camera plane reverses its pitch. These are real
    // perspective/view matrices, rather than arbitrary differing sentinels.
    const Matrix mirror=multiply(lens,rotate_x(-still.pitch));
    const Matrix main=multiply(lens,rotate_x(still.pitch));
    const Vec4 test_vertex={1.,6.,-14.,1.};
    check(largest_difference(transform(mirror,test_vertex),transform(main,test_vertex))>1.,
        "Reflected and main cameras really produce visibly different projected water coordinates");
    WorldWaterMirrorCache cache;
    check(!cache.valid()&&!cache.can_reuse(1,still),"No image exists on the first frame");
    check(cache.commit(10,still,mirror),"Successful reflected capture commits");
    check(cache.valid()&&cache.can_reuse(11,still),"The next stationary frame reuses its image");
    check(!cache.can_reuse(10,still),"The capture frame is not reused as its next frame");
    check(!cache.can_reuse(12,still),"A two-frame-old image cannot be reused");
    check(!cache.can_reuse(9,still),"Frame rollback cannot reuse an unrelated image");
    check(cache.projection()==mirror&&cache.projection()!=main,
        "Reusing the image retains the captured reflected projection, never the main VP");
    check(largest_difference(transform(cache.rebased_projection(still),test_vertex),
        transform(mirror,test_vertex))==0.,"An unmoved camera preserves exact captured projection");
    for(std::uint64_t frame=12;frame<36;++frame) {
        if(frame%2==0) {
            check(!cache.can_reuse(frame,still),"Alternate stationary capture is required");
            check(cache.commit(frame,still,mirror),"Alternate successful capture updates image metadata");
        } else check(cache.can_reuse(frame,still),"Exactly one stationary frame between captures reuses");
    }

    const auto reject=[&](Pose changed,const char *message) {
        check(!cache.can_reuse(35,changed),message);
    };
    Pose changed=still;changed.width+=1;reject(changed,"Reflection width change requires capture");
    changed=still;changed.height+=1;reject(changed,"Reflection height change requires capture");
    changed=still;changed.fov=std::nextafter(still.fov,2.);reject(changed,"Any FOV change invalidates old projection");
    changed=still;changed.aspect=std::nextafter(still.aspect,2.);reject(changed,"Any aspect change invalidates old projection");
    changed=still;changed.level=std::nextafter(still.level,3.);reject(changed,"Any water-plane change requires a new image");
    changed=still;changed.x+=.00002;reject(changed,"Real lateral motion cannot reuse scissored reflection");
    changed=still;changed.y+=.00002;reject(changed,"Vertical motion cannot reuse scissored reflection");
    changed=still;changed.z+=.00002;reject(changed,"Forward motion cannot reuse scissored reflection");
    changed=still;changed.x+=8.e-6;changed.z+=8.e-6;
    reject(changed,"Combined movement uses physical Euclidean distance, not a per-axis box");
    changed=still;changed.yaw+=2.e-6;reject(changed,"Actual yaw motion requires capture");
    changed=still;changed.pitch+=2.e-6;reject(changed,"Actual pitch motion requires capture");
    check(!cache.can_reuse(35,still,true,false),"Force-every-frame mode prevents reuse");
    check(!cache.can_reuse(35,still,false,true),"Temporal cuts prevent reuse");
    check(cache.can_reuse(35,still),"Read-only force/cut queries do not pretend to capture another image");

    // Floating-origin jitter may be accepted, but the texture's old projection
    // must be rebased. Compare the SAME absolute scene vertex in old and current
    // reflected-camera origins, independently of origin_delta's implementation.
    changed=still;changed.x+=4.e-6;changed.y+=4.e-6;changed.z-=4.e-6;
    check(cache.can_reuse(35,changed),"Sub-tolerance stationary jitter may reuse");
    const std::array<double,3> world={still.x+1.,2.,still.z-14.};
    const Vec4 old_relative={world[0]-still.x,world[1]-(2*still.level-still.y),world[2]-still.z,1.};
    const Vec4 new_relative={world[0]-changed.x,world[1]-(2*changed.level-changed.y),world[2]-changed.z,1.};
    const Vec4 reference=transform(mirror,old_relative);
    const Vec4 corrected=transform(cache.rebased_projection(changed),new_relative);
    check(largest_difference(reference,transform(mirror,new_relative))>1.e-6,
        "The origin shift measurably changes projection if the cached matrix is left uncorrected");
    check(largest_difference(reference,corrected)<1.e-7,
        "Rebased cached mirror maps the same absolute vertex to the original captured pixel");
    check(cache.projection()==mirror,"Rebasing does not mutate the captured-image matrix");

    changed=still;changed.yaw+=2*pi;changed.pitch-=2*pi;
    check(cache.can_reuse(35,changed),"Equivalent full-turn yaw and pitch reuse the same image");
    Pose wrapped=still;wrapped.yaw=pi-2.e-7;wrapped.pitch=-pi+2.e-7;
    check(cache.commit(40,wrapped,mirror),"Boundary-angle capture is valid");
    wrapped.yaw=-pi+2.e-7;wrapped.pitch=pi-2.e-7;
    check(cache.can_reuse(41,wrapped),"Crossing the angle wrap by sub-tolerance amount remains stationary");
    wrapped.yaw=-pi+2.e-5;
    check(!cache.can_reuse(41,wrapped),"Wrap-angle motion beyond tolerance requires capture");

    const double nan=std::numeric_limits<double>::quiet_NaN();
    const double inf=std::numeric_limits<double>::infinity();
    for(int field=0;field<8;++field)for(double invalid:{nan,inf,-inf}) {
        Pose bad=still;
        double *fields[]={&bad.x,&bad.y,&bad.z,&bad.yaw,&bad.pitch,&bad.fov,&bad.aspect,&bad.level};
        *fields[field]=invalid;
        check(cache.commit(50,still,mirror),"A valid capture restores a cache for invalid-input checks");
        check(!cache.can_reuse(51,bad),"Nonfinite camera/plane metadata never reuses a reflection");
        check(!cache.commit(52,bad,mirror)&&!cache.valid(),"Nonfinite capture metadata cannot certify an image");
    }
    for(int dimension:{0,-1}) {
        Pose bad=still;bad.width=dimension;
        check(!cache.commit(60,bad,mirror),"Invalid capture width is rejected");
        bad=still;bad.height=dimension;
        check(!cache.commit(60,bad,mirror),"Invalid capture height is rejected");
    }
    for(double fov:{0.,-1.,pi,2*pi}) {
        Pose bad=still;bad.fov=fov;
        check(!cache.commit(60,bad,mirror),"Non-perspective FOV is rejected");
    }
    for(double aspect:{0.,-1.}) {
        Pose bad=still;bad.aspect=aspect;
        check(!cache.commit(60,bad,mirror),"Non-positive aspect is rejected");
    }
    for(unsigned index=0;index<16;++index) {
        Matrix broken=mirror;broken[index]=std::numeric_limits<float>::quiet_NaN();
        check(cache.commit(60,still,mirror),"Valid image restored before checking bad matrix");
        check(!cache.commit(61,still,broken)&&!cache.valid(),"Every projection component must be finite");
    }
    check(cache.commit(70,still,mirror),"Final valid image captured");
    check(!cache.can_reuse(72,still),"A failed/skipped capture without commit does not invent fresh image metadata");
    check(cache.projection()==mirror,"Failed/skipped work leaves the last actually captured matrix paired with its image");
    cache.invalidate();
    check(!cache.valid()&&!cache.can_reuse(71,still),"Resource reallocation can explicitly invalidate old image metadata");
    check(cache.commit(std::numeric_limits<std::uint64_t>::max(),still,mirror),"Final frame index can be recorded");
    check(!cache.can_reuse(0,still),"Frame index overflow cannot make an old image new");
    std::printf("{\"passed\":true,\"checks\":%u,\"gpu_used\":false,\"absolute_world_projection_checked\":true}\n",checks);
}
