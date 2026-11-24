#ifndef VALWOOD_WORLD_SELECTION_MEMBERSHIP_H
#define VALWOOD_WORLD_SELECTION_MEMBERSHIP_H
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

// Fixed-universe scratch and committed source-index sets. Distance ranking may
// permute a selected set without changing its drawable roots. Publication IDs
// distinguish reused numeric indices from a newly generated source field.
class WorldSelectionMembership {
public:
    struct Commit {
        bool initialize(std::size_t limit) noexcept {
            try {indices_.reserve(limit);limit_=limit;valid_=false;return true;}
            catch(...){return false;}
        }
        void invalidate() noexcept {valid_=false;}
        bool valid() const noexcept {return valid_;}
        std::size_t size() const noexcept {return indices_.size();}
        const std::vector<std::size_t> &source_indices() const noexcept {return indices_;}
        double gpu_anchor_x() const noexcept {return gpu_x_;}
        double gpu_anchor_z() const noexcept {return gpu_z_;}
        std::size_t gpu_count() const noexcept {return gpu_count_;}
        // Temporary loading/probe buffers use the new live field's origin;
        // successful restoration returns to the last committed payload origin.
        void note_probe_upload(double x,double z,std::size_t count,bool uploaded) noexcept {
            if(uploaded){gpu_x_=x;gpu_z_=z;gpu_count_=count;gpu_committed_=false;}
        }
        void note_restore_upload(bool uploaded) noexcept {
            if(uploaded){gpu_x_=committed_x_;gpu_z_=committed_z_;gpu_count_=indices_.size();gpu_committed_=true;}
        }
    private:
        friend class WorldSelectionMembership;
        std::vector<std::size_t> indices_;
        std::size_t limit_=0;
        std::uint64_t publication_=0;
        bool valid_=false,gpu_committed_=false;
        double committed_x_=0,committed_z_=0,gpu_x_=0,gpu_z_=0;
        std::size_t gpu_count_=0;
    };
    bool initialize(std::size_t universe) noexcept {
        try {tags_.assign(universe,0);stamp_=0;return true;}
        catch(...){return false;}
    }
    template<class Selection>
    bool equal(const Commit &old,const Selection &next,std::uint64_t publication) noexcept {
        if(!old.valid_||!old.gpu_committed_||old.publication_!=publication||old.indices_.size()!=next.size())return false;
        if(!mark(next))return false;
        // Consume each old index, also rejecting malformed duplicate metadata.
        for(auto index:old.indices_){
            if(index>=tags_.size()||tags_[index]!=stamp_)return false;
            tags_[index]=0;
        }
        return true;
    }
    template<class Selection>
    bool commit_if_uploaded(Commit &old,const Selection &next,std::uint64_t publication,bool uploaded,double anchor_x=0,double anchor_z=0) noexcept {
        if(!uploaded||next.size()>old.limit_||!mark(next))return false;
        // All validation precedes mutation. initialize() reserved this bound;
        // scalar resize/copy cannot allocate or throw here.
        old.indices_.resize(next.size());
        std::size_t i=0;for(const auto &item:next)old.indices_[i++]=std::size_t(item.second);
        old.publication_=publication;old.valid_=true;
        old.committed_x_=old.gpu_x_=anchor_x;old.committed_z_=old.gpu_z_=anchor_z;
        old.gpu_count_=next.size();old.gpu_committed_=true;return true;
    }
private:
    template<class Selection> bool mark(const Selection &items) noexcept {
        if(++stamp_==0){std::fill(tags_.begin(),tags_.end(),0);stamp_=1;}
        for(const auto &item:items){
            const auto index=std::size_t(item.second);
            if(index>=tags_.size()||tags_[index]==stamp_)return false;
            tags_[index]=stamp_;
        }
        return true;
    }
    std::vector<std::uint32_t> tags_;
    std::uint32_t stamp_=0;
};
#endif
