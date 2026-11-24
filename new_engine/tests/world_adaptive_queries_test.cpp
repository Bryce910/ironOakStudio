// The runner extracts the real ring and viewer gate; no copied implementation.
#include "engine/world_quality_controller.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>

namespace {
bool track_allocations=false;
unsigned allocations=0;
}
void *operator new(std::size_t bytes){
    if(track_allocations)allocations++;
    if(void *memory=std::malloc(bytes?bytes:1))return memory;
    throw std::bad_alloc();
}
void operator delete(void *memory) noexcept{std::free(memory);}
void operator delete(void *memory,std::size_t) noexcept{std::free(memory);}

using GLuint=unsigned;
using GLenum=unsigned;
using GLint=int;
using GLuint64=std::uint64_t;
constexpr GLenum GL_TIMESTAMP=1,GL_QUERY_RESULT_AVAILABLE=2,GL_QUERY_RESULT=3;
namespace {
struct Query {bool live=false,ready=false;GLuint64 value=0;};
std::array<Query,64> mock={};
GLuint next_id=0;
GLuint64 gpu_time=0;
unsigned timestamps=0,availability_reads=0,result_reads=0,deleted=0;
}
void glGenQueries(int count,GLuint *ids){for(int i=0;i<count;i++){assert(next_id+1<mock.size());ids[i]=++next_id;mock[next_id].live=true;}}
void glDeleteQueries(int count,const GLuint *ids){for(int i=0;i<count;i++){assert(mock[ids[i]].live);mock[ids[i]].live=false;deleted++;}}
void glQueryCounter(GLuint id,GLenum target){assert(target==GL_TIMESTAMP&&mock[id].live);mock[id].value=gpu_time;mock[id].ready=false;timestamps++;}
void glGetQueryObjectiv(GLuint id,GLenum pname,GLint *value){assert(pname==GL_QUERY_RESULT_AVAILABLE&&mock[id].live);availability_reads++;*value=mock[id].ready;}
void glGetQueryObjectui64v(GLuint id,GLenum pname,GLuint64 *value){assert(pname==GL_QUERY_RESULT&&mock[id].live&&mock[id].ready);result_reads++;*value=mock[id].value;}
#include "adaptive_query_under_test.inc"

namespace {
void begin_frame(AdaptiveGPUQueries &adaptive_queries,bool automatic,std::uint64_t frame,std::uint64_t quality_epoch,bool quality_eligible=true){
    struct {bool auto_quality;} graphics{automatic};
#include "adaptive_query_begin_gate.inc"
}
void ready_all(){for(auto &query:mock)if(query.live)query.ready=true;}
void submit_frame(AdaptiveGPUQueries &queries,std::uint64_t frame,std::uint64_t epoch){
    gpu_time+=20000000;begin_frame(queries,true,frame,epoch);
    gpu_time+=9000000;queries.end(3.);queries.cadence(frame,.025);
}
unsigned pending(const AdaptiveGPUQueries &queries){unsigned count=0;for(const auto &slot:queries.slots)count+=slot.pending;return count;}
}

int main(){
    AdaptiveGPUQueries queries;queries.init();assert(next_id==16&&timestamps==0);
    QualityController controller;
    controller.reset({false,.85f,.70f,60.f},10);
    // Manual play performs neither timestamps nor query availability/result reads.
    track_allocations=true;
    for(unsigned frame=1;frame<=1000;frame++){
        begin_frame(queries,false,frame,10);queries.end(3.);
        queries.cadence(frame,.02);assert(!queries.poll(controller));
    }
    assert(timestamps==0&&availability_reads==0&&result_reads==0&&queries.begun_frames==0);
    assert(controller.status().excluded_samples==0&&!controller.status().ready);
    assert(controller.scale()==.85f&&pending(queries)==0);

    // Automatic mode retains the original asynchronous timing and actual cadence.
    controller.reset({true,.85f,.70f,60.f},11);
    submit_frame(queries,1001,11);assert(queries.begun_frames==1&&timestamps==2);
    assert(pending(queries)==1&&queries.slots[0].dt==.025);
    assert(!queries.poll(controller));assert(result_reads==0&&availability_reads==1);
    ready_all();assert(!queries.poll(controller));
    assert(result_reads==2&&controller.status().accepted_samples==1);
    assert(controller.status().last_serial==1001&&pending(queries)==0);

    // Later ready slots cannot overtake the oldest incomplete GPU sample.
    submit_frame(queries,1002,11);submit_frame(queries,1003,11);
    mock[queries.slots[1].start].ready=mock[queries.slots[1].end].ready=true;
    const auto reads_before=result_reads;
    assert(!queries.poll(controller)&&result_reads==reads_before);
    ready_all();assert(!queries.poll(controller));
    assert(controller.status().accepted_samples==3&&controller.status().last_serial==1003);
    assert(pending(queries)==0);

    // An outstanding auto sample drains after switching to manual, but its old
    // epoch cannot vote or revive readiness. New manual frames still issue none.
    submit_frame(queries,1004,11);
    controller.reset({false,.85f,.70f,60.f},12);
    const auto begun=queries.begun_frames;
    begin_frame(queries,false,1005,12);queries.end(3.);queries.cadence(1005,.03);
    ready_all();assert(!queries.poll(controller));
    assert(queries.begun_frames==begun&&pending(queries)==0);
    assert(controller.status().stale_samples==1&&controller.status().accepted_samples==0);
    assert(!controller.status().ready&&controller.scale()==.85f);

    // Re-enabling starts fresh at the new epoch, with original ring-full and
    // cadence behavior: all eight in-flight slots are bounded, ninth is skipped.
    controller.reset({true,.85f,.70f,60.f},13);
    for(unsigned frame=1010;frame<1018;frame++)submit_frame(queries,frame,13);
    const auto before_full=queries.begun_frames;
    submit_frame(queries,1018,13);
    assert(pending(queries)==8&&queries.begun_frames==before_full&&queries.active==-1);
    ready_all();assert(!queries.poll(controller));
    assert(controller.status().accepted_samples==8&&controller.status().last_serial==1017);
    assert(controller.status().stale_samples==0&&pending(queries)==0);
    assert(allocations==0);
    track_allocations=false;
    // Positive allocation control makes the no-hot-allocation assertion useful.
    void *(*volatile allocate_control)(std::size_t)=::operator new;
    void (*volatile free_control)(void *) noexcept=::operator delete;
    track_allocations=true;void *control=allocate_control(17);track_allocations=false;
    assert(allocations==1);free_control(control);
    queries.release();assert(deleted==16&&queries.active==-1&&pending(queries)==0);
    assert(queries.begun_frames==before_full); // Final telemetry survives cleanup.
    puts("PASS actual AdaptiveGPUQueries: manual zero-query path, auto/manual epoch drains, oldest-first readiness, cadence, bounded full ring, cleanup and zero hot allocations");
}
