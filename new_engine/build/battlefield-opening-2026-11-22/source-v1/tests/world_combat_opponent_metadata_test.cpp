#include "engine/world_combat_opponent_metadata.h"
#include <cassert>
#include <limits>
#include <string>

using namespace world_combat_opponent_metadata;
static void word(std::string &bytes,std::uint32_t n){for(unsigned i=0;i<4;++i)bytes.push_back(char((n>>(8*i))&255));}
static void scalar(std::string &bytes,float f){std::uint32_t bits=0;std::memcpy(&bits,&f,4);word(bytes,bits);}
static std::string fixture(){std::string bytes="VWGRP172";word(bytes,pose_count);
    for(unsigned i=0;i<pose_count;++i){scalar(bytes,float(i)*.001f);scalar(bytes,1.2f);scalar(bytes,-.17f);}return bytes;}
static bool parse(const std::string &bytes,Pivots &out){FILE *f=std::tmpfile();assert(f);assert(std::fwrite(bytes.data(),1,bytes.size(),f)==bytes.size());std::rewind(f);
    const bool ok=read_pivots(f,out);assert(!std::fclose(f));return ok;}
int main(){
    const auto bytes=fixture();Pivots out;for(auto &point:out)point={9,8,7};const auto sentinel=out;
    assert(parse(bytes,out));assert(out[0][0]==0&&out[0][1]==1.2f&&out[171][0]==.171f&&out[171][2]==-.17f);
    unsigned rejected=0;
    auto rejects=[&](const std::string &bad){out=sentinel;assert(!parse(bad,out));assert(out==sentinel);++rejected;};
    rejects("");rejects(bytes.substr(0,7));rejects(bytes.substr(0,11));rejects(bytes.substr(0,bytes.size()-1));
    auto bad=bytes;bad[0]='X';rejects(bad);bad=bytes;bad[8]=char(171);rejects(bad);
    bad=bytes;bad[9]=char(1);rejects(bad);rejects(bytes+"x");
    for(float value:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),16.1f,-16.1f}){
        bad=bytes;std::string encoded;scalar(encoded,value);bad.replace(12,4,encoded);rejects(bad);}
    assert(rejected==13);out=sentinel;assert(!read_pivots(nullptr,out)&&out==sentinel);
    float a[3]={0,1,0},b[3]={0,1,1};assert(valid_edge(a,b));assert(!valid_edge(a,a));
    b[2]=4.1f;assert(!valid_edge(a,b));b[2]=.01f;assert(!valid_edge(a,b));b[2]=std::numeric_limits<float>::quiet_NaN();assert(!valid_edge(a,b));
    std::printf("Opponent metadata: measured172pivots,13malformedpayloads rejected transactionally; collapsed/oversize/nonfinite edges rejected\n");
}
