// Lossless pose paging on the GL owner thread. VAOs keep stable buffer names;
// discarding an inactive buffer's storage cannot change any vertex bindings.
// The private temporary backing file is closed when the last page is freed.
// Only current endpoints are read back into the bounded GPU working set.
namespace {
struct PosePage {
    GLuint buffer=0;
    std::size_t offset=0,bytes=0;
    unsigned pins=0;
    bool resident=false;
    PosePage *previous=nullptr,*next=nullptr;
};
struct PosePager {
    FILE *backing=nullptr;
    std::size_t end=0,pages=0,resident=0,peak=0;
    std::uint64_t uploads=0,hits=0,evictions=0,failures=0;
    PosePage *first=nullptr,*last=nullptr;
    std::vector<unsigned char> scratch;
};
thread_local PosePager pose_pager;
static std::size_t pose_cache_budget(){
    const char *qa=std::getenv("VALWOOD_QA_POSE_CACHE_MIB");
    if(qa&&*qa){char *end=nullptr;const long n=std::strtol(qa,&end,10);
        if(end&&!*end&&n>=1&&n<=256)return std::size_t(n)*1024*1024;}
    return 96u*1024u*1024u;
}
static void pose_page_unlink(PosePage &p){
    auto &g=pose_pager;
    if(p.previous)p.previous->next=p.next;else g.first=p.next;
    if(p.next)p.next->previous=p.previous;else g.last=p.previous;
    p.previous=p.next=nullptr;
}
static void pose_page_recent(PosePage &p){
    auto &g=pose_pager;
    if(p.resident)pose_page_unlink(p);
    p.previous=g.last;p.next=nullptr;
    if(g.last)g.last->next=&p;else g.first=&p;
    g.last=&p;
}
static void pose_page_release(PosePage *p){
    if(!p)return;
    auto &g=pose_pager;
    if(p->resident){pose_page_unlink(*p);g.resident-=p->bytes;}
    delete p;
    if(!--g.pages){
        std::printf("[Pose streaming] budget=%zu peak=%zu uploads=%llu hits=%llu evictions=%llu failures=%llu remaining=0\n",
            pose_cache_budget(),g.peak,(unsigned long long)g.uploads,(unsigned long long)g.hits,
            (unsigned long long)g.evictions,(unsigned long long)g.failures);
        if(g.backing)std::fclose(g.backing);
        g.backing=nullptr;g.end=0;g.scratch.clear();g.scratch.shrink_to_fit();
    }
}
using PosePageOwner=std::unique_ptr<PosePage,decltype(&pose_page_release)>;
static PosePageOwner pose_page_store(const void *data,std::size_t bytes){
    PosePageOwner out(nullptr,pose_page_release);
    if(!data||!bytes||std::getenv("VALWOOD_QA_EAGER_POSE_STORAGE"))return out;
    auto &g=pose_pager;
    // This portable stdio offset also works with the Windows CRT's 32-bit long.
    // A larger-than-supported bank safely retains its ordinary eager upload.
    if(bytes>std::size_t(LONG_MAX)||g.end>std::size_t(LONG_MAX)-bytes)return out;
    if(!g.backing)g.backing=std::tmpfile();
    if(!g.backing){++g.failures;return out;}
    try{if(g.scratch.size()<bytes)g.scratch.resize(bytes);}catch(...){++g.failures;return out;}
    out.reset(new(std::nothrow)PosePage);
    if(!out)return out;
    // Count the owner before any failing I/O so the deleter always balances it.
    ++g.pages;
    if(std::fseek(g.backing,long(g.end),SEEK_SET)||std::fwrite(data,1,bytes,g.backing)!=bytes){
        ++g.failures;std::clearerr(g.backing);out.reset();return out;
    }
    out->offset=g.end;out->bytes=bytes;g.end+=bytes;
    return out;
}
static bool pose_page_ready(PosePage &p){
    auto &g=pose_pager;
    if(p.resident){++g.hits;pose_page_recent(p);return true;}
    if(!g.backing||g.scratch.size()<p.bytes||std::fseek(g.backing,long(p.offset),SEEK_SET)||
       std::fread(g.scratch.data(),1,p.bytes,g.backing)!=p.bytes){++g.failures;return false;}
    while(g.resident+p.bytes>pose_cache_budget()){
        auto *victim=g.first;while(victim&&victim->pins)victim=victim->next;
        if(!victim)break; // A draw's exact A/B endpoints take precedence over the budget.
        glBindBuffer(GL_ARRAY_BUFFER,victim->buffer);
        glBufferData(GL_ARRAY_BUFFER,0,nullptr,GL_STATIC_DRAW);
        pose_page_unlink(*victim);victim->resident=false;g.resident-=victim->bytes;++g.evictions;
    }
    glBindBuffer(GL_ARRAY_BUFFER,p.buffer);
    glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(p.bytes),g.scratch.data(),GL_STATIC_DRAW);
    if(glGetError()!=GL_NO_ERROR){++g.failures;glBufferData(GL_ARRAY_BUFFER,0,nullptr,GL_STATIC_DRAW);return false;}
    pose_page_recent(p);p.resident=true;g.resident+=p.bytes;g.peak=std::max(g.peak,g.resident);++g.uploads;
    return true;
}
}
