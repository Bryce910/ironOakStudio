#ifndef VALWOOD_WORLD_MODEL_FACADE_H
#define VALWOOD_WORLD_MODEL_FACADE_H
// cgltf's optional implementation sits outside its declaration guard.
#ifndef CGLTF_H_INCLUDED__
#include "cgltf.h"
#endif
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
namespace world_model_materials {
inline void winter_parameters(const cgltf_material *material,float (&result)[3]){
    result[0]=result[1]=result[2]=0;
    if(!material)return;
    for(size_t i=0;i<material->extensions_count;i++){
        const auto &e=material->extensions[i];
        if(!e.name||std::strcmp(e.name,"VALWOOD_materials_winter"))continue;
        if(!e.data||std::strlen(e.data)>192)throw "Invalid winter material extension";
        const char *p=e.data;
        auto ws=[&](){while(*p==' '||*p=='\n'||*p=='\r'||*p=='\t')++p;};
        auto token=[&](char c){ws();if(*p++!=c)throw "Invalid winter material schema";};
        token('{');ws();constexpr char key[]="\"parameters\"";
        if(std::strncmp(p,key,sizeof(key)-1))throw "Invalid winter material schema";
        p+=sizeof(key)-1;token(':');token('[');
        for(int k=0;k<4;k++){
            ws();char *end=nullptr;const double value=std::strtod(p,&end);
            if(end==p||!std::isfinite(value)||(k==0?value!=1:value<0||value>1))throw "Invalid winter material value";
            if(k)result[k-1]=float(value);
            p=end;if(k<3)token(',');
        }
        token(']');token('}');ws();if(*p)throw "Trailing winter material data";
        return;
    }
}
struct Facade {int mode=0,aux[3]={-1,-1,-1};float scale[3]={1,1,1};};
inline Facade facade_parameters(const cgltf_material *material,size_t textures){
    Facade result;
    if(!material)return result;
    for(size_t i=0;i<material->extensions_count;i++){
        const cgltf_extension &e=material->extensions[i];
        if(!e.name||std::strcmp(e.name,"VALWOOD_materials_facade"))continue;
        // Deliberately small, versioned numeric schema. Reject partial,
        // non-finite or out-of-range parameters rather than guessing a layer.
        if(!e.data||std::strlen(e.data)>512)throw "Invalid facade extension";
        const char *p=e.data;
        auto whitespace=[&](){while(*p==' '||*p=='\t'||*p=='\r'||*p=='\n')++p;};
        auto token=[&](char c){whitespace();if(*p++!=c)throw "Invalid facade parameters";};
        token('{');whitespace();const char name[]="\"parameters\"";
        if(std::strncmp(p,name,sizeof(name)-1))throw "Invalid facade schema";
        p+=sizeof(name)-1;token(':');token('[');
        double values[8]={};
        for(int j=0;j<8;j++){
            whitespace();char *end=nullptr;values[j]=std::strtod(p,&end);
            if(end==p||!std::isfinite(values[j]))throw "Invalid facade number";
            p=end;if(j<7)token(',');
        }
        token(']');token('}');whitespace();if(*p)throw "Trailing facade data";
        if(std::floor(values[1])!=values[1]||!((values[0]==1&&values[1]>=1&&values[1]<=2)||(values[0]==2&&values[1]>=3&&values[1]<=5)))throw "Unsupported facade version/mode";
        result.mode=int(values[1]);
        for(int j=0;j<3;j++){
            if(values[2+j]<-1||values[2+j]>=double(textures)||std::floor(values[2+j])!=values[2+j])throw "Invalid facade texture index";
            result.aux[j]=int(values[2+j]);
            const double minimum=result.mode<=2?.1:(result.mode==5&&j>0?0.:.00001);
            const double maximum=result.mode<=2?8.:(result.mode==5&&j==1?512.:128.);
            if(values[5+j]<minimum||values[5+j]>maximum)throw "Invalid facade UV scale";
            result.scale[j]=float(values[5+j]);
        }
        if(result.aux[0]<0||(result.mode!=5&&result.aux[1]<0)||((result.mode==2||result.mode==3||result.mode==4)&&result.aux[2]<0))throw "Missing facade source images";
        if((result.mode==4&&result.scale[2]>1)||(result.mode==5&&result.scale[2]>1))throw "Invalid source surface factor";
        return result;
    }
    return result;
}
// Only these auxiliary slots are RGB surface colors. Paint/grunge masks,
// normal and ARM auxiliaries retain their original uncompressed data.
inline bool facade_color_aux(const Facade &f,int slot) {
    return (f.mode==1&&slot==0)||(f.mode==2&&slot==2)||((f.mode==3||f.mode==4)&&slot==1);
}
inline bool facade_srgb_aux(const Facade &f,int slot){
    if(f.mode<=2)return f.mode==1||slot==2;
    return (f.mode==3&&(slot==1||slot==2))||(f.mode==4&&slot==1)||(f.mode==5&&slot==0);
}
inline bool facade_packed_layers(const Facade &f){return f.mode==3||f.mode==4;}
}
#endif
