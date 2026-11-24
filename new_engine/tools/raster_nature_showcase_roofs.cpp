#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

// Input is a stream of source-world triangles [x,absoluteY,1000-sourceZ].
// Raster samples exact projected triangles at grid nodes, never AABB caps.
int main(int argc,char **argv){
    if(argc!=4)return 2;
    const int n=std::atoi(argv[3]);if(n<3||n>4097)return 3;
    FILE *input=std::fopen(argv[1],"rb");if(!input)return 4;
    std::vector<float> heights(std::size_t(n)*n,0);
    const double scale=(n-1)/1000.;float t[9];std::size_t count=0,writes=0;
    while(std::fread(t,sizeof(float),9,input)==9){
        const double x0=t[0]*scale,z0=t[2]*scale,x1=t[3]*scale,z1=t[5]*scale,x2=t[6]*scale,z2=t[8]*scale;
        const double den=(z1-z2)*(x0-x2)+(x2-x1)*(z0-z2);if(std::abs(den)<1e-12)continue;
        const int xmin=std::max(0,int(std::ceil(std::min({x0,x1,x2})-1e-7))),xmax=std::min(n-1,int(std::floor(std::max({x0,x1,x2})+1e-7)));
        const int zmin=std::max(0,int(std::ceil(std::min({z0,z1,z2})-1e-7))),zmax=std::min(n-1,int(std::floor(std::max({z0,z1,z2})+1e-7)));
        for(int z=zmin;z<=zmax;z++)for(int x=xmin;x<=xmax;x++){
            const double a=((z1-z2)*(x-x2)+(x2-x1)*(z-z2))/den,b=((z2-z0)*(x-x2)+(x0-x2)*(z-z2))/den,c=1-a-b;
            if(a< -1e-6||b< -1e-6||c< -1e-6)continue;
            const float y=float(a*t[1]+b*t[4]+c*t[7]);auto &height=heights[std::size_t(z)*n+x];if(y>height){height=y;writes++;}
        }
        count++;
    }
    std::fclose(input);FILE *output=std::fopen(argv[2],"wb");if(!output)return 5;
    const auto written=std::fwrite(heights.data(),sizeof(float),heights.size(),output);std::fclose(output);
    std::printf("Rasterized %zu triangles, %zu height updates, %zu samples\n",count,writes,heights.size());return written==heights.size()?0:6;
}
