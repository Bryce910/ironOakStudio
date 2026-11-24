// CPU-only coverage of the optional resident parser and absolute-height update.
// Unused renderer sections are discarded by --gc-sections; no GL context exists.
#include "../engine/world_stronghold.cpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <unistd.h>

extern "C" float world_atlas_height(double,double){return 25.f;}

int main(int argc,char **argv){
    const auto source=argc>1?std::filesystem::absolute(argv[1]):std::filesystem::path{};
    char temporary[]="/tmp/valwood-residents-XXXXXX";
    const char *directory=mkdtemp(temporary);assert(directory);
    const auto original=std::filesystem::current_path();std::filesystem::current_path(directory);
    unsigned checks=0;
    auto write=[](const std::string &text){std::ofstream file("showcase.residents");file<<text;};
    auto station=[](){Station s;s.x=991;s.y=992;s.z=993;return s;};
    auto rejected=[&](const std::string &text){write(text);std::vector<Station> rows={station()};std::string error;assert(!load_stations(rows,error));assert(!error.empty());assert(rows.size()==1&&rows[0].x==991&&rows[0].y==992);++checks;};
    if(!source.empty()){
        std::filesystem::copy_file(source,"showcase.residents");WorldStronghold w;std::string error;
        assert(load_stations(w.stations,error)&&!w.stations.empty()&&w.stations.size()<=capacity);
        world_stronghold_update(&w,17);assert(w.stats.actors==w.stations.size());
        for(const auto &row:w.stations)assert(row.absolute_y&&row.family!=1);
        std::filesystem::remove("showcase.residents");++checks;
    }
    {
        std::vector<Station> rows={station()};std::string error;
        assert(load_stations(rows,error)&&rows.size()==1&&rows[0].x==991);++checks;
    }
    const std::string valid="2 10 1200 20 1.5 1 .2 10 1200 20 0";
    write("# source residents\n\n"+valid); // A final newline is optional.
    {
        WorldStronghold w;std::string error;assert(load_stations(w.stations,error));
        assert(w.stations.size()==1&&w.stations[0].absolute_y);
        world_stronghold_update(&w,17);assert(w.stats.actors==1&&!w.actors[0].walking&&w.actors[0].y==1200.f);++checks;
    }
    write("1 0 1 0 0 1 0 0 1 0 0\n"+valid+"\n");
    {
        std::vector<Station> rows;std::string error;assert(load_stations(rows,error));
        assert(rows.size()==1&&rows[0].family==2);++checks;
    }
    write("4 0 5 0 0 1 0 7.5 15 0 1\n");
    {
        WorldStronghold w;std::string error;assert(load_stations(w.stations,error));
        world_stronghold_update(&w,7);assert(w.stats.actors==1&&w.actors[0].walking);
        assert(std::abs(w.actors[0].x-3.75)<1e-6&&std::abs(w.actors[0].y-10.f)<1e-6);++checks;
    }
    {
        WorldStronghold w;w.stations.push_back(station());
        world_stronghold_update(&w,0);assert(w.stats.actors==1&&std::abs(w.actors[0].y-25.015f)<1e-5);++checks;
    }
    rejected(valid+"\n5 0 1 0 0 1 0 0 1 0 0\n");
    rejected("2 nan 1 0 0 1 0 0 1 0 0\n");
    rejected("2 0 inf 0 0 1 0 0 1 0 0\n");
    rejected("2 0 1 0 0 .1 0 0 1 0 0\n");
    rejected("2 0 1 0 0 1 0 0 1 0 2\n");
    rejected("2 0 1 0 0 1 0 0 1 0 1\n");
    rejected(valid+" extra\n");
    rejected("2 0 1\n");
    rejected("# no residents\n");
    rejected(std::string(600,'x')+"\n");
    std::string too_many;for(unsigned i=0;i<33;i++)too_many+=valid+"\n";rejected(too_many);
    std::filesystem::current_path(original);std::filesystem::remove_all(directory);
    std::printf("PASS: %u CPU resident parser/update checks; no GPU initialization\n",checks);
}
