// Production surface decoder and 2m paint baker; no graphics context.
#include "engine/world_surface_authoring.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

int main(int argc,char **argv){
    if(argc!=3){std::cerr<<"Usage: check-opening-surface WORLD.STUDIO REPORT\n";return 1;}
    std::ifstream input(argv[1]);if(!input){std::cerr<<"Missing studio document\n";return 1;}
    std::string line,text;
    while(std::getline(input,line))if(line.rfind("surfaces ",0)==0){
        std::istringstream row(line.substr(9));std::string record,extra;
        if(!(row>>std::quoted(record))||(row>>extra)){std::cerr<<"Malformed quoted surface record\n";return 1;}
        text+=record+'\n';
    }
    world_surface::Data data;world_surface::PaintField field;std::string error;
    if(!world_surface::decode(text,data,error)||!world_surface::bake_paint(data,field,error)){
        std::cerr<<error<<'\n';return 1;
    }
    unsigned occupied=0;for(const auto &tile:field.tiles)for(unsigned i=0;i<world_surface::paint_side*world_surface::paint_side;i++){
        unsigned sum=0;for(unsigned k=0;k<world_surface::layer_limit;k++)sum+=tile.weights[i*world_surface::layer_limit+k];
        if(sum>255){std::cerr<<"Unnormalized paint weights\n";return 1;}occupied+=sum>0;
    }
    std::ofstream out(argv[2]);out<<"{\"passed\":true,\"actual_production_surface_decoder\":true,\"actual_2m_paint_baker\":true,\"gpu_used\":false,\"images\":"<<data.images.size()
        <<",\"layers\":"<<data.layers.size()<<",\"paint\":"<<data.paint.size()<<",\"ribbons\":"<<data.ribbons.size()<<",\"tiles\":"<<field.tiles.size()<<",\"occupied_samples\":"<<occupied<<"}\n";
    if(!out)return 1;
    std::cout<<"Surface decode/bake PASS: "<<data.layers.size()<<" layers, "<<data.paint.size()<<" paints, "<<data.ribbons.size()<<" ribbons, "<<field.tiles.size()<<" tiles\n";
}
