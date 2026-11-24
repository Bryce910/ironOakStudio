#include "engine/world_platform.h"
#include "engine/world_designer_workspace.h"
#include "engine/world_designer_assets.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <cstdio>
#include <unistd.h>
namespace world_designer_workspace {
namespace {
bool valid(const Preferences &p){
    const auto &l=p.layout;
    return std::isfinite(l.left)&&std::isfinite(l.right)&&std::isfinite(l.bottom)&&
        l.left>=160&&l.left<=520&&l.right>=260&&l.right<=640&&l.bottom>=112&&l.bottom<=480&&
        l.top==72&&l.status==24&&p.tool>=0&&p.tool<=26&&p.category>=0&&p.category<=4&&
        p.favorites.size()<=world_designer_assets::favorite_limit&&p.recent.size()<=world_designer_assets::recent_limit;
}
}
bool encode(const Preferences &p,std::string &text,std::string &error){
    if(!valid(p)){error="Invalid workspace layout";return false;}
    std::ostringstream s;s<<"VALWOOD_DESIGNER_WORKSPACE 1\n"<<std::setprecision(9)
      <<p.layout.left<<' '<<p.layout.right<<' '<<p.layout.bottom<<' '<<p.tool<<' '<<p.category<<' '
      <<p.grid<<' '<<p.favorites_only<<' '<<p.ground_grid<<'\n';
    for(const auto &a:p.favorites){if(!world_designer_assets::valid_alias(a)){error="Invalid favorite";return false;}s<<"favorite "<<std::quoted(a)<<'\n';}
    for(const auto &a:p.recent){if(!world_designer_assets::valid_alias(a)){error="Invalid recent asset";return false;}s<<"recent "<<std::quoted(a)<<'\n';}
    text=s.str();error.clear();return true;
}
bool decode(const std::string &text,Preferences &out,std::string &error){
    if(text.size()>32768){error="Workspace file too large";return false;}
    std::istringstream s(text);std::string magic;int version=0;Preferences p;
    if(!(s>>magic>>version)||magic!="VALWOOD_DESIGNER_WORKSPACE"||version!=1||
       !(s>>p.layout.left>>p.layout.right>>p.layout.bottom>>p.tool>>p.category>>p.grid>>p.favorites_only>>p.ground_grid)||!valid(p)){
        error="Unsupported or invalid workspace";return false;
    }
    std::string kind,alias;
    while(s>>kind){if(!(s>>std::quoted(alias))||!world_designer_assets::valid_alias(alias)){error="Invalid workspace asset";return false;}
        if(kind=="favorite")p.favorites.push_back(alias);else if(kind=="recent")p.recent.push_back(alias);else{error="Unknown workspace entry";return false;}
        if(!valid(p)){error="Workspace preference limit exceeded";return false;}}
    if(!s.eof()){error="Truncated workspace";return false;}
    world_designer_assets::normalize_preferences(p.favorites,world_designer_assets::favorite_limit);
    world_designer_assets::normalize_preferences(p.recent,world_designer_assets::recent_limit);
    out=std::move(p);error.clear();return true;
}
bool load(const std::string &path,Preferences &out,std::string &error){
    std::ifstream f(path,std::ios::binary);if(!f){error="Workspace preferences unavailable";return false;}
    std::string text;char buffer[4096];while(f.read(buffer,sizeof buffer)||f.gcount()){
        text.append(buffer,size_t(f.gcount()));if(text.size()>32768){error="Workspace file too large";return false;}}
    if(f.bad()){error="Cannot read workspace preferences";return false;}
    return decode(text,out,error);
}
bool save(const std::string &path,const Preferences &p,std::string &error){
    std::string text;if(!encode(p,text,error))return false;
    std::string temporary=path+".tmp-XXXXXX";std::vector<char> name(temporary.begin(),temporary.end());name.push_back(0);
    int fd=mkstemp(name.data());if(fd<0){error="Cannot create workspace preferences";return false;}
    size_t done=0;bool ok=true;while(done<text.size()){const auto n=write(fd,text.data()+done,text.size()-done);if(n<=0){ok=false;break;}done+=size_t(n);}
    if(ok)ok=fsync(fd)==0;
    if(close(fd)!=0)ok=false;
    if(ok)ok=vw_replace(name.data(),path.c_str())==0;
    if(!ok){unlink(name.data());error="Cannot save workspace preferences";return false;}
    error.clear();return true;
}
bool matches(const std::string &name,const char *query){
    auto lower=[](unsigned char c){return c>='A'&&c<='Z'?unsigned(c+32):unsigned(c);};
    auto space=[](char c){return c==' '||c=='\t'||c=='\n'||c=='\r';};
    const std::string q=query?query:"";size_t i=0;
    while(i<q.size()){while(i<q.size()&&space(q[i]))i++;size_t begin=i;while(i<q.size()&&!space(q[i]))i++;
        if(i==begin)continue;
        bool found=false;
        for(size_t n=0;n+i-begin<=name.size();n++){size_t k=0;for(;k<i-begin;k++)if(lower(name[n+k])!=lower(q[begin+k]))break;if(k==i-begin){found=true;break;}}
        if(!found)return false;}
    return true;
}
}
