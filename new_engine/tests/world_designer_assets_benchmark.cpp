// Real ImGui/browser microbenchmark, runnable without SDL, files, or a GPU.
// Compile with the same optimization flags as the editor and link imgui.
#define main designer_assets_regression_main
#include "world_designer_assets_test.cpp"
#undef main
#include <chrono>
#include <iostream>

int main(){
    assert(designer_assets_regression_main()==0);
    auto *context=ImGui::CreateContext();auto &io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={1200,800};io.DeltaTime=1.f/60;
    unsigned char *pixels=nullptr;int w=0,h=0;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    std::vector<std::string> names;names.reserve(7000);registry.clear();registry.reserve(7000);
    for(unsigned i=0;i<7000;++i)names.push_back("purchased_house_"+std::to_string(i));
    for(const auto &name:names)registry.push_back(asset(name.c_str()));
    ++registry_revision;
    Browser browser;browser.designer_asset_selected=names.back();
    for(unsigned i=0;i<16;++i)browser.designer_asset_recent.push_back(names[6999-i]);
    for(unsigned frame=0;frame<16;++frame){begin_frame();browser.designer_assets_panel();end_frame();}
    constexpr unsigned frames=1200;std::vector<double> elapsed;elapsed.reserve(frames);int vertices=0;
    for(unsigned frame=0;frame<frames;++frame){
        const auto start=std::chrono::steady_clock::now();
        begin_frame();browser.designer_assets_panel();end_frame();
        elapsed.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count());
        vertices=std::max(vertices,ImGui::GetDrawData()->TotalVtxCount);
    }
    std::sort(elapsed.begin(),elapsed.end());double total=0;for(double value:elapsed)total+=value;
    std::cout<<"{\"records\":"<<registry.size()<<",\"frames\":"<<frames<<",\"mean_us\":"<<total/frames
             <<",\"median_us\":"<<elapsed[frames/2]<<",\"p95_us\":"<<elapsed[frames*95/100]
             <<",\"max_vertices\":"<<vertices<<"}\n";
    ImGui::DestroyContext(context);
}
