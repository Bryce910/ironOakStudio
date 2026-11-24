#include "engine/world_selection_membership.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <numeric>
#include <utility>
#include <vector>
namespace {bool track=false;unsigned allocations=0;}
[[gnu::noinline]] void *operator new(std::size_t size){if(track)allocations++;if(auto*p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void operator delete(void*p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void*p,std::size_t)noexcept{std::free(p);}
using Selection=std::vector<std::pair<double,std::size_t>>;
int main(){
    WorldSelectionMembership scratch;assert(scratch.initialize(100000));
    WorldSelectionMembership::Commit committed,other;assert(committed.initialize(3500)&&other.initialize(3500));
    Selection empty,a={{1,9},{2,4},{3,17}},permuted={{300,17},{100,9},{200,4}},changed={{1,9},{2,4},{3,18}};
    Selection duplicate={{1,9},{2,9}},outside={{1,100000}},oversize(3501),full;
    full.reserve(3500);for(std::size_t i=0;i<3500;i++)full.emplace_back(double(i),i*17);
    for(std::size_t i=0;i<oversize.size();i++)oversize[i]={double(i),i};
    // Cold empty is not mistaken for an uploaded empty batch. Failed physical
    // upload notification cannot publish membership, even with an empty set.
    assert(!scratch.equal(committed,empty,1));assert(!scratch.commit_if_uploaded(committed,empty,1,false));assert(!committed.valid());
    assert(scratch.commit_if_uploaded(committed,empty,1,true));assert(scratch.equal(committed,empty,1));assert(!scratch.equal(committed,empty,2));
    assert(scratch.commit_if_uploaded(committed,a,4,true));const auto exact_order=committed.source_indices();
    assert(scratch.equal(committed,permuted,4));assert(committed.source_indices()==exact_order);
    assert(!scratch.equal(committed,changed,4));assert(!scratch.commit_if_uploaded(committed,changed,4,false));
    assert(scratch.equal(committed,a,4)&&!scratch.equal(committed,changed,4));
    assert(scratch.commit_if_uploaded(committed,changed,4,true));assert(scratch.equal(committed,changed,4));assert(!scratch.equal(committed,a,4));
    // New live fields may reuse numeric indices. A different publication must
    // miss, and failed retry must retain the old successful publication.
    assert(!scratch.equal(committed,changed,5));assert(!scratch.commit_if_uploaded(committed,changed,5,false));assert(scratch.equal(committed,changed,4));
    assert(scratch.commit_if_uploaded(committed,changed,5,true));assert(scratch.equal(committed,changed,5));assert(!scratch.equal(committed,changed,4));
    const auto before=committed.source_indices();
    for(const auto*bad:{&duplicate,&outside,&oversize}){
        assert(!scratch.equal(committed,*bad,5));assert(!scratch.commit_if_uploaded(committed,*bad,5,true));assert(committed.source_indices()==before&&scratch.equal(committed,changed,5));
    }
    committed.invalidate();assert(!scratch.equal(committed,changed,5));assert(committed.source_indices()==before);
    assert(scratch.commit_if_uploaded(committed,full,8,true));assert(scratch.commit_if_uploaded(other,a,8,true));
    // Failed source-field publication must leave GPU-local coordinates tied
    // to their actual old anchor, rather than translating them by1024m.
    WorldSelectionMembership::Commit anchored;assert(anchored.initialize(3500));
    constexpr double old_x=-27648,old_z=9216,new_x=old_x+1024,new_z=old_z+1024;
    assert(scratch.commit_if_uploaded(anchored,a,1,true,old_x,old_z));
    constexpr double local_x=3,local_z=5,world_x=old_x+local_x,world_z=old_z+local_z;
    anchored.invalidate();assert(!scratch.commit_if_uploaded(anchored,a,2,false,new_x,new_z));
    assert(anchored.gpu_anchor_x()+local_x==world_x&&anchored.gpu_anchor_z()+local_z==world_z&&anchored.gpu_count()==a.size());
    assert(new_x+local_x!=world_x&&new_z+local_z!=world_z); // old-global-anchor negative control
    // Probe buffers temporarily use the new live anchor, independent of the
    // committed rollback payload. Failed preparation/restoration must preserve
    // whichever payload actually remains on the GPU.
    anchored.note_probe_upload(new_x,new_z,1,false);assert(anchored.gpu_anchor_x()==old_x&&anchored.gpu_count()==a.size());
    anchored.note_probe_upload(new_x,new_z,1,true);assert(anchored.gpu_anchor_x()==new_x&&anchored.gpu_anchor_z()==new_z&&anchored.gpu_count()==1);
    assert(!scratch.equal(anchored,a,1));anchored.note_restore_upload(false);
    assert(anchored.gpu_anchor_x()==new_x&&anchored.gpu_count()==1);
    assert(!scratch.commit_if_uploaded(anchored,a,2,false,new_x,new_z));assert(anchored.gpu_anchor_x()==new_x&&anchored.gpu_count()==1);
    anchored.note_restore_upload(true);assert(anchored.gpu_anchor_x()+local_x==world_x&&anchored.gpu_anchor_z()+local_z==world_z&&anchored.gpu_count()==a.size());
    assert(scratch.commit_if_uploaded(anchored,a,2,true,new_x,new_z));
    const double rebased_local_x=world_x-new_x,rebased_local_z=world_z-new_z;
    assert(anchored.gpu_anchor_x()+rebased_local_x==world_x&&anchored.gpu_anchor_z()+rebased_local_z==world_z&&scratch.equal(anchored,permuted,2));
    anchored.note_probe_upload(old_x,old_z,2,true);assert(!scratch.equal(anchored,a,2));anchored.note_restore_upload(true);
    assert(scratch.equal(anchored,a,2)&&anchored.gpu_anchor_x()==new_x&&anchored.gpu_anchor_z()==new_z&&anchored.gpu_count()==a.size());
    std::puts("PASS committed/GPU anchor transactions: failed1024m publication retains old world positions; temporary probe origins/counts, failed prepare/restore, successful rollback and retry preserve actual GPU coordinates; temporary buffers never qualify for membership reuse");
    // All scratch, candidates and commit buffers already own their maximum
    // capacity. Repeated rank permutations must neither allocate nor mutate
    // committed source order, while independent buckets share the tag scratch.
    const auto full_order=committed.source_indices();std::size_t reuses=0;
    track=true;
    for(unsigned iteration=0;iteration<2000;iteration++){
        std::rotate(full.begin(),full.begin()+37,full.end());
        for(auto &item:full)item.first+=.125;
        assert(scratch.equal(committed,full,8));reuses++;assert(committed.source_indices()==full_order);
        assert(scratch.equal(other,permuted,8));reuses++;
        full[0].second=99999;assert(!scratch.equal(committed,full,8));assert(!scratch.commit_if_uploaded(committed,full,9,false));assert(scratch.equal(other,a,8));
        full[0].second=full_order[(iteration*37+37)%3500];
        // Restore the changed source ID from the prior committed set, keeping
        // the rotated order; no data or extra heap workspace is needed.
        for(auto &item:full)if(item.second==99999)std::abort();
        assert(scratch.equal(committed,full,8));
    }
    // Simulate temporary probe buffers: CPU commit stays unchanged throughout
    // prepare/restore; a failed restore invalidates it and forces a retry.
    assert(scratch.equal(committed,full,8));committed.invalidate();assert(!scratch.equal(committed,full,8));
    assert(scratch.commit_if_uploaded(committed,full,8,true));assert(scratch.equal(committed,full,8));
    track=false;assert(!allocations);
    std::printf("PASS bounded selection membership: 100k source universe/3500 roots, %zu unordered-set reuses, exact retained order; changed sets, same-count publication, cold empty, invalid/duplicate/budget input, failed upload/retry and temporary-probe invalidation; independent bucket commits; zero hot allocations\n",reuses);
}
