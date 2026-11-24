// CPU integration against the actual Coordinator implementation. Build with
// -DVALWOOD_QA_REGIONS_SHUTDOWN and engine/world_regions.cpp; no renderer/GL.
// The optional --expect-lost-wake mode is for an isolated old-stop negative
// control. Only this test's forked child can be killed by its bounded watchdog.
#include "engine/world_regions.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cerrno>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>
#include <csignal>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
using Clock=std::chrono::steady_clock;
unsigned checks=0;
void require(bool good,const char *message){++checks;if(!good)throw std::runtime_error(message);}
void write_all(int descriptor,const void *data,std::size_t size){
    const auto *bytes=static_cast<const unsigned char *>(data);
    while(size){const auto count=::write(descriptor,bytes,size);if(count<0&&errno==EINTR)continue;if(count<=0)throw std::runtime_error("CPU test receipt pipe failed");bytes+=count;size-=std::size_t(count);}
}
template<class Predicate> void until(Predicate predicate,const char *message){
    const auto deadline=Clock::now()+std::chrono::seconds(2);
    while(!predicate()){if(Clock::now()>=deadline)throw std::runtime_error(message);std::this_thread::yield();}
}
struct Gate {
    std::atomic<bool> armed{false};std::mutex mutex;std::condition_variable wake;
    bool reached=false,stop_entered=false,released=false;
} gate;
struct Context {
    enum class Mode {Quick,Cooperative,ReturnAfterCancellation};
    Mode mode=Mode::Quick;std::atomic<bool> entered{false},cancelled{false};
    std::atomic<unsigned> loads{0},destroyed{0};
    unsigned uploads=0,releases=0;bool partial=false,release_on_owner=true;
    std::uint64_t released_token=0;
    std::thread::id owner=std::this_thread::get_id();
};
struct Payload {
    Context *context;
    explicit Payload(Context *value):context(value){}
    ~Payload(){context->destroyed.fetch_add(1,std::memory_order_release);}
};
world_regions::Document document(){
    world_regions::Document out;world_regions::Region region;region.id=1;
    region.bounds={{-10,-10,-10},{10,10,10}};world_regions::Layer layer;layer.id=1;region.layers.push_back(layer);out.regions.push_back(region);return out;
}
std::shared_ptr<const world_regions::CpuBlob> load(const world_regions::LoadRequest &request,void *data){
    auto &context=*static_cast<Context *>(data);++context.loads;context.entered.store(true,std::memory_order_release);
    if(context.mode!=Context::Mode::Quick){
        while(!request.cancelled())std::this_thread::yield();
        context.cancelled.store(true,std::memory_order_release);
        if(context.mode==Context::Mode::Cooperative)return {};
    }
    auto blob=std::make_shared<world_regions::CpuBlob>();blob->data.resize(64,1);blob->gpu_bytes=16;blob->payload=std::make_shared<Payload>(&context);return blob;
}
world_regions::UploadResult upload(const world_regions::UploadRequest &request,void *data){
    auto &context=*static_cast<Context *>(data);++context.uploads;
    world_regions::UploadResult out;out.token=77;out.consumed_bytes=context.partial?std::min(std::uint64_t(8),request.byte_budget):request.blob->data.size();out.complete=!context.partial;out.gpu_bytes=out.complete?16:0;return out;
}
void release(world_regions::Id id,std::uint64_t token,void *data){
    auto &context=*static_cast<Context *>(data);++context.releases;context.released_token=token;context.release_on_owner&=std::this_thread::get_id()==context.owner&&id==1;
}
void configure(world_regions::Coordinator &coordinator,Context &context){
    world_regions::Config config;config.cpu_bytes=1024*1024;config.gpu_bytes=1024;config.upload_bytes_per_frame=64;config.upload_jobs_per_frame=1;
    std::string error;require(coordinator.configure(document(),config,{load,upload,release,&context},error),"configure actual CPU coordinator");
}
void request(world_regions::Coordinator &coordinator,std::uint64_t frame=1){
    require(coordinator.request({0,0,0},frame)&&coordinator.tick(),"submit actual region work");
}
void deterministic_idle_race(int signal_fd){
    Context context;world_regions::Coordinator coordinator;gate.armed.store(true,std::memory_order_release);configure(coordinator,context);
    {
        std::unique_lock<std::mutex> lock(gate.mutex);
        require(gate.wake.wait_for(lock,std::chrono::seconds(2),[]{return gate.reached;}),"worker reaches false predicate while holding coordinator mutex");
    }
    std::thread releaser([]{
        std::unique_lock<std::mutex> lock(gate.mutex);
        if(!gate.wake.wait_for(lock,std::chrono::seconds(2),[]{return gate.stop_entered;}))return;
        lock.unlock();
        // In the negative control this guarantees notify precedes the actual
        // condition wait. The fixed owner is blocked on the coordinator mutex.
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        lock.lock();gate.released=true;lock.unlock();gate.wake.notify_all();
    });
    const char armed='S';if(signal_fd>=0)write_all(signal_fd,&armed,1);
    require(coordinator.stop(),"owner stops worker from exact check/sleep interleaving");releaser.join();gate.armed.store(false,std::memory_order_release);
    require(context.loads==0&&context.uploads==0&&context.releases==0,"idle shutdown invokes no asset callbacks");
    require(coordinator.stop(),"a second stop is inert");
}
void idle_stop(){Context context;world_regions::Coordinator coordinator;configure(coordinator,context);require(coordinator.stop(),"idle worker shutdown joins");require(context.loads==0,"idle worker did not load");}
void cooperative_stop(){
    Context context;context.mode=Context::Mode::Cooperative;world_regions::Coordinator coordinator;configure(coordinator,context);request(coordinator);
    std::uint64_t frame=1;until([&]{require(coordinator.request({0,0,0},++frame)&&coordinator.tick(),"pump cooperative CPU load");return context.entered.load(std::memory_order_acquire);},"active loader starts");require(coordinator.stop(),"active worker joins after cooperative cancellation");
    require(context.cancelled.load(std::memory_order_acquire)&&context.loads==1,"active loader observes actual cancellation flags");require(context.uploads==0&&context.releases==0,"cancelled CPU load never publishes a token");
}
void completed_stop(){
    Context context;world_regions::Coordinator coordinator;configure(coordinator,context);request(coordinator);
    std::uint64_t frame=1;until([&]{require(coordinator.request({0,0,0},++frame)&&coordinator.tick(),"pump committed region");return coordinator.state(1).resident;},"region reaches actual committed resident state");
    require(context.uploads==1&&coordinator.state(1).token==77,"actual uploader committed one token");require(coordinator.stop(),"resident shutdown completes");
    require(context.releases==1&&context.released_token==77&&context.release_on_owner,"resident token released exactly once on owner thread");require(context.destroyed==1,"prepared CPU payload destroyed");require(coordinator.stop()&&context.releases==1,"repeated stop does not release token twice");
}
void partial_stop(){
    Context context;context.partial=true;world_regions::Coordinator coordinator;configure(coordinator,context);request(coordinator);
    std::uint64_t frame=1;until([&]{require(coordinator.request({0,0,0},++frame)&&coordinator.tick(),"pump partial region");return context.uploads>0;},"real partial uploader starts");
    require(!coordinator.state(1).resident&&coordinator.state(1).loading,"partial upload remains unpublished");require(coordinator.stop(),"partial upload shutdown completes");
    require(context.releases==1&&context.released_token==77&&context.release_on_owner,"partial token released once on owner");require(context.destroyed==1,"partial CPU payload destroyed after join");
}
void result_after_cancel(){
    Context context;context.mode=Context::Mode::ReturnAfterCancellation;world_regions::Coordinator coordinator;configure(coordinator,context);request(coordinator);
    std::uint64_t frame=1;until([&]{require(coordinator.request({0,0,0},++frame)&&coordinator.tick(),"pump late-result CPU load");return context.entered.load(std::memory_order_acquire);},"late-result loader starts");require(coordinator.stop(),"late CPU result joins without publication");
    require(context.cancelled&&context.uploads==0&&context.releases==0,"late cancelled result cannot publish GPU work");require(context.destroyed==1,"late cancelled payload is destroyed");
}
void repeated_configure(){
    Context context;world_regions::Coordinator coordinator;
    for(unsigned i=0;i<256;++i){configure(coordinator,context);if(i%2==0)request(coordinator);require(coordinator.stop(),"repeated idle/queued stop joins");require(coordinator.stats().resident==0&&coordinator.stats().loading==0,"stop clears live residency and pending state");}
    require(context.uploads==0&&context.releases==0,"queued-only shutdowns publish no callbacks");
}
void wrong_thread_stop(){
    Context context;world_regions::Coordinator coordinator;configure(coordinator,context);bool accepted=true;
    std::thread other([&]{accepted=coordinator.stop();});other.join();require(!accepted,"non-owner stop remains rejected");require(coordinator.stop(),"owner can stop after rejected foreign call");
}
void unconfigured_stop(){
    Context context;world_regions::Coordinator coordinator;require(coordinator.stop()&&coordinator.stop(),"unconfigured stop is repeatable");configure(coordinator,context);require(coordinator.stop()&&coordinator.stop(),"configure after stop remains supported");
}
struct Outcome {bool passed=false,timed_out=false,gate_armed=false;unsigned checks=0;};
Outcome child_case(const std::function<void(int)> &test){
    int pipe[2];if(::pipe(pipe))throw std::runtime_error("test pipe failed");
    const pid_t child=::fork();if(child<0)throw std::runtime_error("test fork failed");
    if(!child){::close(pipe[0]);checks=0;try{test(pipe[1]);const char done='C';write_all(pipe[1],&done,1);write_all(pipe[1],&checks,sizeof(checks));::_exit(0);}catch(const std::exception &error){std::fprintf(stderr,"[Regions shutdown CPU] %s\n",error.what());std::fflush(stderr);::_exit(2);}}
    ::close(pipe[1]);const int flags=::fcntl(pipe[0],F_GETFL);(void)::fcntl(pipe[0],F_SETFL,flags|O_NONBLOCK);
    const auto deadline=Clock::now()+std::chrono::seconds(4);int status=0;Outcome out;std::vector<unsigned char> bytes;
    for(;;){unsigned char buffer[32];const auto size=::read(pipe[0],buffer,sizeof(buffer));if(size>0)bytes.insert(bytes.end(),buffer,buffer+size);
        const auto result=::waitpid(child,&status,WNOHANG);if(result==child){out.passed=WIFEXITED(status)&&WEXITSTATUS(status)==0;break;}if(result<0)throw std::runtime_error("test child wait failed");
        if(Clock::now()>=deadline){out.timed_out=true;(void)::kill(child,SIGKILL);(void)::waitpid(child,&status,0);break;}std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    for(;;){unsigned char buffer[32];const auto size=::read(pipe[0],buffer,sizeof(buffer));if(size<=0)break;bytes.insert(bytes.end(),buffer,buffer+size);}::close(pipe[0]);
    for(std::size_t i=0;i<bytes.size();++i){if(bytes[i]=='S')out.gate_armed=true;if(bytes[i]=='C'&&i+1+sizeof(unsigned)<=bytes.size()){std::memcpy(&out.checks,bytes.data()+i+1,sizeof(unsigned));break;}}
    return out;
}
}
namespace world_regions {
void shutdown_test_wait_predicate(bool ready){
    if(ready||!gate.armed.load(std::memory_order_acquire))return;
    std::unique_lock<std::mutex> lock(gate.mutex);if(gate.reached)return;gate.reached=true;gate.wake.notify_all();gate.wake.wait(lock,[]{return gate.released;});
}
void shutdown_test_stop_enter(){
    if(!gate.armed.load(std::memory_order_acquire))return;std::lock_guard<std::mutex> lock(gate.mutex);gate.stop_entered=true;gate.wake.notify_all();
}
}
int main(int argc,char **argv){
    try{
        if(argc==2&&std::strcmp(argv[1],"--expect-lost-wake")==0){const auto result=child_case(deterministic_idle_race);const bool demonstrated=result.timed_out&&result.gate_armed;
            std::printf("{\"passed\":%s,\"gpu_used\":false,\"negative_control\":true,\"old_stop_lost_notification_demonstrated\":%s,\"owned_child_watchdog_seconds\":4}\n",demonstrated?"true":"false",demonstrated?"true":"false");return demonstrated?0:1;}
        if(argc!=1)throw std::runtime_error("unexpected argument");
        const std::pair<const char *,std::function<void(int)>> cases[]={
            {"deterministic_false_predicate_stop",deterministic_idle_race},
            {"idle_stop",[](int){idle_stop();}},
            {"active_cooperative_cancellation",[](int){cooperative_stop();}},
            {"resident_owner_release",[](int){completed_stop();}},
            {"partial_upload_release",[](int){partial_stop();}},
            {"late_cancelled_result_cleanup",[](int){result_after_cancel();}},
            {"256_repeated_configure_stop",[](int){repeated_configure();}},
            {"foreign_stop_rejected",[](int){wrong_thread_stop();}},
            {"unconfigured_repeat_stop",[](int){unconfigured_stop();}}
        };
        unsigned total=0;for(const auto &test:cases){const auto result=child_case(test.second);if(!result.passed||result.timed_out)throw std::runtime_error(test.first);total+=result.checks;}
        std::printf("{\"passed\":true,\"gpu_used\":false,\"negative_control\":false,\"cases\":9,\"checks\":%u,\"repeated_configure_stop\":256,\"deterministic_actual_coordinator_barrier\":true,\"owner_thread_token_release_verified\":true}\n",total);return 0;
    }catch(const std::exception &error){std::fprintf(stderr,"[Regions shutdown CPU] FAIL %s\n",error.what());return 1;}
}
