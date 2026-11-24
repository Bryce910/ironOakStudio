#include "../engine/world_arena_progress.h"
#include <cstdlib>
#include <iostream>

namespace p=world_arena_progress;
unsigned checks=0;
void require(bool okay,const char *what){++checks;if(!okay){std::cerr<<"FAIL: "<<what<<'\n';std::exit(1);}}
bool same(const p::Progress &a,const p::Progress &b){return p::payload(a)==p::payload(b);}
int main(){
    std::string error,bytes;p::Progress empty,out;
    require(p::valid(empty),"empty purse valid");require(p::payout({8,1})==80&&p::payout({1,1})==10,"reward preview");
    for(unsigned total=0;total<=9;++total)for(unsigned concurrent=0;concurrent<=9;++concurrent){
        const p::Config config{total,concurrent};const bool okay=total>=1&&total<=8&&concurrent>=1&&concurrent<=total;
        require(p::valid(config)==okay,"config bounds");
        p::Progress allocated;std::uint64_t id=999;
        require(p::allocate(empty,config,allocated,id,error)==okay,"allocate every config");
        if(!okay){require(id==999&&same(allocated,empty),"invalid allocation unchanged");continue;}
        require(id==1&&allocated.active_run_id==1&&allocated.next_run_id==2&&allocated.balance==0,"allocation has no reward");
        require(allocated.selection==config&&allocated.active_config==config,"config frozen with run");
        require(p::encode(allocated,bytes,error)&&p::parse(bytes,out,error)&&same(out,allocated),"active record roundtrip");
        std::uint64_t award=7;p::Progress settled;
        require(p::settle(allocated,id,config,false,settled,award,error)==p::Settlement::Rejected,"loss grants nothing");
        require(same(settled,empty)&&award==0,"rejected grant unchanged");
        require(p::settle(allocated,id+1,config,true,settled,award,error)==p::Settlement::Rejected,"wrong run rejected");
        require(p::settle(allocated,id,config,true,settled,award,error)==p::Settlement::Ready,"genuine win stages");
        require(award==10*total&&settled.balance==award&&settled.wins==1&&settled.opponents==total&&!settled.active_run_id,"receipt exact payout");
        require(p::encode(settled,bytes,error)&&p::parse(bytes,out,error)&&same(out,settled),"settled record roundtrip");
        p::Progress duplicate;
        require(p::settle(out,id,config,true,duplicate,award,error)==p::Settlement::AlreadySaved&&award==0&&same(duplicate,out),"restore duplicate cannot pay");
        p::Progress next;std::uint64_t next_id=0;
        require(p::allocate(out,config,next,next_id,error)&&next_id==2,"repeat unique run");
        require(p::settle(next,id,config,true,duplicate,award,error)==p::Settlement::Rejected,"old receipt during new run rejected");
        p::Progress after_crash;std::uint64_t third_id=0;
        require(p::allocate(next,config,after_crash,third_id,error)&&third_id==3,"abandoned run consumed safely");
        require(p::settle(after_crash,third_id,config,true,duplicate,award,error)==p::Settlement::Ready&&duplicate.balance==20*total&&duplicate.wins==2,"later win after crash paid once");
    }
    require(p::encode(empty,bytes,error),"empty encode");const std::string canonical=bytes;
    for(std::size_t n=0;n<canonical.size();++n){
        p::Progress sentinel;sentinel.selection={3,2};const auto original=sentinel;
        require(!p::parse(canonical.substr(0,n),sentinel,error)&&same(sentinel,original),"every truncation rejected unchanged");
    }
    for(std::size_t n=0;n<canonical.size();++n){
        auto changed=canonical;changed[n]=changed[n]=='0'?'1':'0';p::Progress sentinel;sentinel.selection={2,2};const auto original=sentinel;
        require(!p::parse(changed,sentinel,error)&&same(sentinel,original),"every single byte tamper rejected unchanged");
    }
    for(const auto &bad:{canonical+"extra\n",canonical+canonical,std::string(1025,'x'),std::string(),std::string("VALWOOD_ARENA_PROGRESS 2\n")})
        require(!p::parse(bad,out,error),"extra duplicate oversize unknown rejected");
    for(unsigned field=0;field<9;++field){
        auto bad=empty;
        switch(field){case 0:bad.balance=10;break;case 1:bad.wins=1;break;case 2:bad.opponents=1;break;case 3:bad.next_run_id=0;break;case 4:bad.last_settled_run_id=1;break;case 5:bad.active_run_id=1;break;case 6:bad.selection={8,9};break;case 7:bad.active_config={0,0};break;case 8:bad.next_run_id=p::maximum_runs+2;break;}
        require(!p::valid(bad)&&!p::encode(bad,bytes,error),"inconsistent ledger rejected");
    }
    // Actual atomic file operations: a failed replacement must not publish a
    // candidate balance, and a corrupt load must not replace the caller state.
    char pattern[]="/tmp/valwood-arena-progress-XXXXXX";char *directory=mkdtemp(pattern);require(directory!=nullptr,"temporary folder");
    const std::filesystem::path root(directory),path=root/"saves/arena.progress";
    require(p::load(path,out,error)==p::Load::Missing&&same(out,empty),"missing starts empty");
    p::Progress allocated;std::uint64_t id=0;require(p::allocate(empty,{8,8},allocated,id,error),"stage file allocation");
    require(p::save(path,allocated,error),"commit allocation");
    require(p::load(path,out,error)==p::Load::Loaded&&same(out,allocated),"load allocation");
    p::Progress candidate;std::uint64_t award=0;
    require(p::settle(allocated,id,{8,8},true,candidate,award,error)==p::Settlement::Ready,"stage file win");
    const auto blocked=root/"blocked";std::filesystem::create_directory(blocked);
    require(!p::save(blocked,candidate,error),"replacement failure visible");
    require(p::load(path,out,error)==p::Load::Loaded&&out.balance==0,"failed commit preserves prior purse");
    unsigned temps=0;for(auto &entry:std::filesystem::directory_iterator(root))if(entry.path().filename().string().find(".tmp-")!=std::string::npos)++temps;
    require(temps==0,"failed replacement cleans staging file");
    require(p::save(path,candidate,error)&&p::load(path,out,error)==p::Load::Loaded&&out.balance==80,"commit win");
    auto sentinel=out;{std::ofstream bad(path);bad<<"corrupt untouched";}
    require(p::load(path,out,error)==p::Load::Invalid&&same(out,sentinel),"corrupt load preserves live purse");
    {std::ifstream file(path);std::string original;std::getline(file,original);require(original=="corrupt untouched","corrupt file preserved");}
    p::NeutralGate gate;require(!gate.observe(true,true),"held menu confirm disarmed");require(!gate.observe(false,false),"unfocused neutral cannot arm");
    require(gate.observe(true,false)&&gate.observe(true,true),"fresh neutral arms");gate.reset();require(!gate.observe(true,true),"phase transition requires release");
    std::filesystem::remove_all(root);
    std::cout<<"Arena progress: "<<checks<<" checks passed\n";
}
