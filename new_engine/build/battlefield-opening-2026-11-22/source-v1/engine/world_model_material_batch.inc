// A synchronous animation bank repeats the same encoded atlases thousands of
// times. Compare exact bytes before repeating expensive texture substitutions,
// fingerprints and sampler queries. Entries borrow IDs; every hit retains the
// ordinary texture cache reference. Any model texture release invalidates them.
struct MaterialBatchEntry {
    std::array<unsigned,11> request={};
    std::vector<unsigned char> encoded;
    GLuint texture=0;
    bool hand=false;
};
thread_local unsigned material_batch_depth=0;
thread_local std::uint64_t material_batch_hits=0;
thread_local std::size_t material_batch_bytes=0;
thread_local std::vector<MaterialBatchEntry> material_batch;
static void material_batch_clear(){material_batch.clear();material_batch_bytes=0;}
static GLuint material_batch_find(const std::array<unsigned,11>&key,const unsigned char *data,std::size_t bytes,bool *hand){
    if(!material_batch_depth||std::getenv("VALWOOD_QA_UNCACHED_MATERIAL_BATCH"))return 0;
    for(const auto &e:material_batch)if(e.request==key&&e.encoded.size()==bytes&&
        !std::memcmp(e.encoded.data(),data,bytes)&&texture_cache_retain(e.texture)){
        if(hand)*hand=e.hand;++material_batch_hits;return e.texture;
    }
    return 0;
}
static void material_batch_remember(const std::array<unsigned,11>&key,const unsigned char *data,std::size_t bytes,GLuint texture,bool hand){
    constexpr std::size_t budget=64u*1024u*1024u;
    if(!material_batch_depth||!texture||bytes>budget||std::getenv("VALWOOD_QA_UNCACHED_MATERIAL_BATCH"))return;
    try{
        if(material_batch.size()>=32||material_batch_bytes+bytes>budget)material_batch_clear();
        MaterialBatchEntry entry;entry.request=key;entry.encoded.assign(data,data+bytes);entry.texture=texture;entry.hand=hand;
        material_batch.push_back(std::move(entry));material_batch_bytes+=bytes;
    }catch(...){material_batch_clear();} // Correctness never depends on this optimization.
}
