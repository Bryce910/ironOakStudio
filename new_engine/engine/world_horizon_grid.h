#ifndef VALWOOD_WORLD_HORIZON_GRID_H
#define VALWOOD_WORLD_HORIZON_GRID_H
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

// A conservative broad phase for the existing horizon point predicate. Cells
// only skip proven misses; retained roots are still tested in source order by
// that exact predicate before distance partitioning. Storage is bounded and
// allocated once, and publication/classification perform no allocation.
class WorldHorizonGrid {
public:
    struct View {
        double anchor_x=0,anchor_z=0,x=0,y=0,z=0;
        double forward_x=0,forward_y=0,forward_z=-1,right_x=1,right_z=0;
        double aspect=16./9.,pool_y=0;
        bool pool_mirror=false;
    };
    struct Stats {std::size_t cells=0,retained_cells=0,candidate_roots=0;bool accelerated=false;};
    bool initialize(std::size_t root_capacity) noexcept {
        ready_=active_=false;
        try {root_cells_.resize(root_capacity);cells_.resize(max_cells);keep_.resize(max_cells);return true;}
        catch(...){return false;}
    }
    template<class Root> bool publish(const Root *roots,std::size_t count) noexcept {
        ready_=active_=false;count_=count;
        if(count>root_cells_.size()||(count&&!roots)||cells_.size()!=max_cells)return false;
        if(!count){cell_count_=0;ready_=true;return true;}
        double lo_x=std::numeric_limits<double>::infinity(),hi_x=-lo_x,lo_z=lo_x,hi_z=-lo_x;
        for(std::size_t i=0;i<count;i++) {
            const auto &a=roots[i];
            // Preserve the exact old float crown expression before widening.
            const float height=a.y+17*a.scale;
            if(!std::isfinite(a.x)||!std::isfinite(a.z)||!std::isfinite(height)||
               std::abs(double(a.x))>1e8||std::abs(double(a.z))>1e8||std::abs(double(height))>1e8)return false;
            lo_x=std::min(lo_x,double(a.x));hi_x=std::max(hi_x,double(a.x));
            lo_z=std::min(lo_z,double(a.z));hi_z=std::max(hi_z,double(a.z));
        }
        const double base_x=std::floor(lo_x/cell_size),base_z=std::floor(lo_z/cell_size);
        const double nx=std::floor(hi_x/cell_size)-base_x+1,nz=std::floor(hi_z/cell_size)-base_z+1;
        if(nx<1||nz<1||nx>max_side||nz>max_side)return false;
        width_=std::size_t(nx);cell_count_=width_*std::size_t(nz);
        for(std::size_t c=0;c<cell_count_;c++)cells_[c].count=0;
        for(std::size_t i=0;i<count;i++) {
            const auto &a=roots[i];const double height=float(a.y+17*a.scale);
            const auto ix=std::size_t(std::floor(double(a.x)/cell_size)-base_x);
            const auto iz=std::size_t(std::floor(double(a.z)/cell_size)-base_z);
            const auto index=iz*width_+ix;if(index>=cell_count_)return false;
            root_cells_[i]=std::uint16_t(index);auto &c=cells_[index];
            if(!c.count){c.x0=c.x1=a.x;c.z0=c.z1=a.z;c.h0=c.h1=height;}
            else {
                c.x0=std::min(c.x0,double(a.x));c.x1=std::max(c.x1,double(a.x));
                c.z0=std::min(c.z0,double(a.z));c.z1=std::max(c.z1,double(a.z));
                c.h0=std::min(c.h0,height);c.h1=std::max(c.h1,height);
            }
            c.count++;
        }
        ready_=true;return true;
    }
    Stats classify(const View &v) noexcept {
        Stats stats;active_=false;
        const double values[]={v.anchor_x,v.anchor_z,v.x,v.y,v.z,v.forward_x,v.forward_y,v.forward_z,v.right_x,v.right_z,v.aspect,v.pool_y};
        for(double a:values)if(!std::isfinite(a)||std::abs(a)>1e8)return stats;
        if(!ready_||v.aspect<=0||v.aspect>100||std::abs(v.forward_x)>2||std::abs(v.forward_y)>2||
           std::abs(v.forward_z)>2||std::abs(v.right_x)>2||std::abs(v.right_z)>2)return stats;
        const double tangent=v.aspect*std::tan(.54);
        for(std::size_t index=0;index<cell_count_;index++) {
            const auto &c=cells_[index];keep_[index]=0;if(!c.count)continue;
            stats.cells++;
            // Expand actual source extrema by1mm, well beyond world-origin
            // cancellation on this100km atlas. Final arithmetic supports also
            // receive outward error guards; near-boundary cells fail open.
            const double x0=v.anchor_x+c.x0-v.x-.001,x1=v.anchor_x+c.x1-v.x+.001;
            const double z0=v.anchor_z+c.z0-v.z-.001,z1=v.anchor_z+c.z1-v.z+.001;
            const double hx=std::max(std::abs(x0),std::abs(x1)),hz=std::max(std::abs(z0),std::abs(z1));
            const double lx=x0<=0&&x1>=0?0:std::min(std::abs(x0),std::abs(x1));
            const double lz=z0<=0&&z1>=0?0:std::min(std::abs(z0),std::abs(z1));
            const double min_d2=lx*lx+lz*lz,max_d2=hx*hx+hz*hz;
            const double radial_error=64*std::numeric_limits<double>::epsilon()*(1+max_d2);
            if(max_d2+radial_error<1450.*1450.||min_d2-radial_error>12000.*12000.)continue;
            const double horizontal=std::max(x0*v.forward_x,x1*v.forward_x)+std::max(z0*v.forward_z,z1*v.forward_z);
            const double h0=c.h0-.001,h1=c.h1+.001;
            const double positive=std::max(h0*v.forward_y,h1*v.forward_y);
            const double negative=std::max(-h0*v.forward_y,-h1*v.forward_y);
            double vertical=std::max(positive,negative);
            if(v.pool_mirror)vertical=std::max(vertical,negative+2*v.pool_y*v.forward_y);
            const double error=1e-6+64*std::numeric_limits<double>::epsilon()*
                (1+hx+hz+std::abs(h0)+std::abs(h1)+std::abs(v.y)+2*std::abs(v.pool_y));
            const double depth=horizontal-v.y*v.forward_y+vertical+error;
            if(depth<-45)continue;
            const double across0=std::min(x0*v.right_x,x1*v.right_x)+std::min(z0*v.right_z,z1*v.right_z);
            const double across1=std::max(x0*v.right_x,x1*v.right_x)+std::max(z0*v.right_z,z1*v.right_z);
            const double across=across0<=0&&across1>=0?0:std::min(std::abs(across0),std::abs(across1));
            if(across-error>std::max(0.,depth)*tangent+55+error)continue;
            keep_[index]=1;stats.retained_cells++;stats.candidate_roots+=c.count;
        }
        active_=true;stats.accelerated=true;return stats;
    }
    bool includes(std::size_t source_index) const noexcept {
        return !active_||source_index>=count_||keep_[root_cells_[source_index]]!=0;
    }
private:
    static constexpr std::size_t max_side=64,max_cells=max_side*max_side;
    static constexpr double cell_size=512.;
    struct Cell {double x0=0,x1=0,z0=0,z1=0,h0=0,h1=0;std::size_t count=0;};
    std::vector<Cell> cells_;
    std::vector<std::uint16_t> root_cells_;
    std::vector<std::uint8_t> keep_;
    std::size_t count_=0,width_=0,cell_count_=0;
    bool ready_=false,active_=false;
};
#endif
