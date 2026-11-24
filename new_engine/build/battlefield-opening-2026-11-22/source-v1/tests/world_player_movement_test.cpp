#include "../engine/world_player_movement.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

namespace m=world_player_movement;
struct Run {m::State state;double x=0,z=0;};
static void near(double a,double b,double tolerance=1.e-10){assert(std::abs(a-b)<=tolerance);}
static void drive(Run &r,double x,double z,double duration,int hz){
    double elapsed=0;
    while(elapsed<duration-1.e-12){
        const double dt=std::min(1./hz,duration-elapsed);
        const auto d=r.state.step(x,z,dt);
        r.x+=d.dx;r.z+=d.dz;r.state.commit(d.dx,d.dz,d.dx,d.dz);
        elapsed+=dt;
    }
}
static void jitter(Run &r,double x,double z,double duration){
    constexpr double steps[]={.003,.041,.008,.019,.002,.029,.049};
    double elapsed=0;unsigned i=0;
    while(elapsed<duration-1.e-12){
        const double dt=std::min(steps[i++%7],duration-elapsed);
        const auto d=r.state.step(x,z,dt);
        r.x+=d.dx;r.z+=d.dz;r.state.commit(d.dx,d.dz,d.dx,d.dz);
        elapsed+=dt;
    }
}
int main(){
    // Controller radial travel, normalized combined input and arbitrary yaw.
    for(double yaw:{0.,.37,1.5707963267948966,3.141592653589793}){
        auto a=m::target(.3,.4,yaw,5);near(std::hypot(a.dx,a.dz),2.5);
        auto b=m::target(1,1,yaw,5);near(std::hypot(b.dx,b.dz),5);
        auto c=m::target(2,2,yaw,5);near(b.dx,c.dx);near(b.dz,c.dz);
    }
    // Real displacement invariance across plausible frame rates, including
    // release-to-snap and a reversal crossing inside a frame.
    Run reference;
    for(int hz:{30,60,120,144}){
        Run r;drive(r,0,-5,.7,hz);drive(r,3,0,.19,hz);
        drive(r,-3,0,.23,hz);drive(r,0,0,.4,hz);
        assert(r.state.vx==0&&r.state.vz==0);
        if(hz==30)reference=r;
        else{near(r.x,reference.x);near(r.z,reference.z);}
    }
    Run irregular;jitter(irregular,0,-5,.7);jitter(irregular,3,0,.19);
    jitter(irregular,-3,0,.23);jitter(irregular,0,0,.4);
    near(irregular.x,reference.x);near(irregular.z,reference.z);
    assert(irregular.state.vx==0&&irregular.state.vz==0);
    // Responsive onset/reversal and short, monotonic release; exact integrated
    // distance is checked against the ODE, not merely frame-count snapshots.
    Run start;drive(start,5,0,m::acceleration_tau*std::log(20.),120);
    near(start.state.vx,4.75);near(start.x,5*(m::acceleration_tau*std::log(20.)-m::acceleration_tau*.95));
    m::State stopping;stopping.vx=5;
    double stop_distance=0;for(int i=0;i<60;i++)stop_distance+=stopping.step(0,0,1./120).dx;
    near(stop_distance,(5-m::stop_snap_speed)*m::stopping_tau);assert(stopping.vx==0);
    for(int hz:{30,60,144}){
        Run release;release.state.vx=5;drive(release,0,0,.5,hz);
        near(release.x,stop_distance);assert(release.state.vx==0);
    }
    Run released_jitter;released_jitter.state.vx=5;jitter(released_jitter,0,0,.5);
    near(released_jitter.x,stop_distance);assert(released_jitter.state.vx==0);
    Run reverse;reverse.state.vx=5;drive(reverse,-5,0,.125,120);
    assert(reverse.state.vx<-4.75);
    // Axis sweep wall slide: latent normal velocity is discarded, accepted
    // tangent speed retained, and immediate reversal moves away from the wall.
    Run wall;
    for(int i=0;i<120;i++){
        auto d=wall.state.step(3,4,1./120);
        const double dx=std::max(0.,std::min(d.dx,.1-wall.x));
        wall.x+=dx;wall.z+=d.dz;wall.state.commit(d.dx,d.dz,dx,d.dz);
    }
    near(wall.x,.1);near(wall.state.vx,0);assert(wall.state.vz>3.99);
    auto away=wall.state.step(-3,4,1./120);assert(away.dx<0);
    m::State correction;correction.vx=4;correction.vz=3;
    correction.commit(.1,.1,.3,.1);near(correction.vx,4);near(correction.vz,3);
    correction.commit(.1,.1,0,.05);near(correction.vx,0);near(correction.vz,1.5);
    // Pause must not advance state. Blocked controls/mode discontinuity reset
    // is explicit, and huge/invalid deltas cannot cause a teleport or NaNs.
    m::State control;control.vx=5;auto d=control.step(5,0,0);near(d.dx,0);near(control.vx,5);
    control.step(5,0,1./60,false);near(control.vx,0);
    control.vx=5;d=control.step(5,0,1000);near(d.dx,5*m::maximum_dt);
    control.reset();near(control.vx,0);near(control.vz,0);
    for(double invalid:{-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
        control.vx=5;d=control.step(5,0,invalid);near(d.dx,0);near(control.vx,0);
    }
    control.vx=5;control.commit(.1,.1,std::numeric_limits<double>::quiet_NaN(),0);near(control.vx,0);
    std::puts("PASS radial/diagonal magnitude, analytic 30/60/120/144 Hz+jitter displacement and stop distance, 105ms onset, 125ms reversal, short exact stop, wall slide/rebase, pause/reset/finite/dt bounds");
}
