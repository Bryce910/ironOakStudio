#include "engine/world_local_probes.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

int main() {
    constexpr double pi=3.14159265358979323846;
    assert(world_local_probe_cube_bytes(8,128)==6291360);
    assert(world_local_probe_cube_bytes(8,128)+128*128*4<8*1024*1024);
    assert(world_local_probe_cube_bytes(0,128)==0&&world_local_probe_cube_bytes(9,128)==0);
    for(int side:{0,1,3,12,129,256})assert(world_local_probe_cube_bytes(1,side)==0);
    float f[3],u[3],r[3];assert(!world_local_probe_face_basis(6,f,u,r));
    assert(!world_local_probe_face_basis(0,nullptr,u,r));
    const float directions[6][3]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    for(unsigned face=0;face<6;face++) {
        assert(world_local_probe_face_basis(face,f,u,r));
        for(int k=0;k<3;k++)assert(f[k]==directions[face][k]);
        assert(f[0]*u[0]+f[1]*u[1]+f[2]*u[2]==0);
        assert(f[0]*r[0]+f[1]*r[1]+f[2]*r[2]==0);
        assert(u[0]*r[0]+u[1]*r[1]+u[2]*r[2]==0);
        const float cross[3]={r[1]*u[2]-r[2]*u[1],r[2]*u[0]-r[0]*u[2],r[0]*u[1]-r[1]*u[0]};
        for(int k=0;k<3;k++)assert(cross[k]==-f[k]);
    }
    double max_constant_error=0,max_linear_error=0;
    for(int side:{4,16,64,128}) {
        std::vector<float> data(std::size_t(side)*side*3);
        double constant[9][3]={},directional[9][3]={};
        for(unsigned face=0;face<6;face++) {
            world_local_probe_face_basis(face,f,u,r);
            for(int y=0;y<side;y++)for(int x=0;x<side;x++) {
                auto *pixel=data.data()+(std::size_t(y)*side+x)*3;
                pixel[0]=.3f;pixel[1]=.2f;pixel[2]=.4f;
            }
            assert(world_local_probe_add_face(face,side,data.data(),data.size(),constant));
            for(int y=0;y<side;y++)for(int x=0;x<side;x++) {
                const double sx=2.*(x+.5)/side-1.,sy=2.*(y+.5)/side-1.;
                double n[3];for(int k=0;k<3;k++)n[k]=f[k]+sx*r[k]+sy*u[k];
                const double inverse=1./std::hypot(std::hypot(n[0],n[1]),n[2]);for(double &v:n)v*=inverse;
                auto *pixel=data.data()+(std::size_t(y)*side+x)*3;
                pixel[0]=float(.3+.12*n[0]);pixel[1]=float(.2+.08*n[1]);pixel[2]=float(.4+.10*n[2]);
            }
            assert(world_local_probe_add_face(face,side,data.data(),data.size(),directional));
        }
        const double expected_constant[3]={.3f,.2f,.4f};
        for(int c=0;c<3;c++)assert(std::abs(constant[0][c]-expected_constant[c]*std::sqrt(4*pi))<1.e-11);
        for(int i=1;i<9;i++)for(int c=0;c<3;c++)assert(std::abs(constant[i][c])<1.e-12);
        for(int step=0;step<200;step++) {
            double n[3]={std::sin(step*.139),std::cos(step*.073),std::sin(step*.193+.2)};
            const double inverse=1./std::hypot(std::hypot(n[0],n[1]),n[2]);for(double &v:n)v*=inverse;
            double actual[3];world_local_probe_diffuse(constant,n,actual);
            for(int c=0;c<3;c++)max_constant_error=std::max(max_constant_error,std::abs(actual[c]-expected_constant[c]));
            world_local_probe_diffuse(directional,n,actual);
            const double expected[3]={.3+(2./3.)*.12*n[0],.2+(2./3.)*.08*n[1],.4+(2./3.)*.10*n[2]};
            for(int c=0;c<3;c++)max_linear_error=std::max(max_linear_error,std::abs(actual[c]-expected[c]));
        }
        assert(max_constant_error<1.e-11&&max_linear_error<1.e-7);
        // A malformed source is rejected transactionally, before accumulation.
        const auto old=std::array<double,27>{constant[0][0],constant[0][1],constant[0][2]};
        double before[9][3];std::memcpy(before,constant,sizeof(before));data[3]=NAN;
        assert(!world_local_probe_add_face(0,side,data.data(),data.size(),constant));
        assert(std::memcmp(before,constant,sizeof(before))==0&&old[0]==constant[0][0]);
        data[3]=-1;assert(!world_local_probe_add_face(0,side,data.data(),data.size(),constant));
        data[3]=.2f;assert(!world_local_probe_add_face(6,side,data.data(),data.size(),constant));
        assert(!world_local_probe_add_face(0,side,data.data(),data.size()-1,constant));
    }
    WorldLocalProbeDef invalid;assert(world_local_probes_create(nullptr,1)==nullptr);
    assert(world_local_probes_create(&invalid,0)==nullptr&&world_local_probes_create(&invalid,9)==nullptr);
    assert(world_local_probes_create(&invalid,1)==nullptr); // Empty domain, before GL calls.
    double result[3],zero[3]={},coefficients[9][3]={};world_local_probe_diffuse(coefficients,zero,result);
    assert(result[0]==0&&result[1]==0&&result[2]==0);
    std::printf("PASS local probes CPU: OpenGL six-face orientation, exact solid-angle constant convolution maxerror%.3g, analytic directional radiance diffuse maxerror%.3g across4/16/64/128 grids; transactional malformed input, finite normalization; max8 logicalcube+depth budget6356896B\n",max_constant_error,max_linear_error);
}
