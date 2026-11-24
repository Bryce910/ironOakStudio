#include "engine/world_model_facade.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

int main(){
    float values[3]={1,1,1};
    world_model_materials::winter_parameters(nullptr,values);
    assert(values[0]==0&&values[1]==0&&values[2]==0);
    cgltf_extension extension={};
    extension.name=const_cast<char*>("VALWOOD_materials_winter");
    cgltf_material material={};material.extensions=&extension;material.extensions_count=1;
    std::string json=" { \"parameters\" : [1, .93, 1, .28] } ";
    extension.data=json.data();world_model_materials::winter_parameters(&material,values);
    assert(std::abs(values[0]-.93f)<.00001f&&values[1]==1&&std::abs(values[2]-.28f)<.00001f);
    for(const char *bad:{"", "{}", "{\"parameters\":[2,0,0,0]}",
        "{\"parameters\":[1,-.1,0,0]}","{\"parameters\":[1,0,1.01,0]}",
        "{\"parameters\":[1,0,0,nan]}","{\"parameters\":[1,0,0,1e999]}",
        "{\"parameters\":[1,0,0]}","{\"parameters\":[1,0,0,0,0]}",
        "{\"parameters\":[1,0,0,0]} trailing"}){
        json=bad;extension.data=json.data();bool rejected=false;
        try{world_model_materials::winter_parameters(&material,values);}catch(const char*){rejected=true;}
        assert(rejected);
    }
    extension.name=const_cast<char*>("unrelated_optional_extension");
    world_model_materials::winter_parameters(&material,values);
    assert(values[0]==0&&values[1]==0&&values[2]==0);
    std::puts("PASS: optional winter material defaults, valid values, schema/version/range/nonfinite/trailing rejection");
}
