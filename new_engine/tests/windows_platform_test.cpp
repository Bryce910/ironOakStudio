#include "engine/world_platform.h"
#include "engine/world_studio.h"
#include "engine/world_editor_session.h"
#include "engine/world_project_checkpoint.h"
#include "engine/world_assets.h"
#include "engine/world_npc_appearance.h"
#include <sys/file.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstring>

static unsigned checks=0;
static void check(bool good,const std::string &why){checks++;if(!good)throw std::runtime_error(why);}
int main(int argc,char **argv){
    namespace fs=std::filesystem;
    std::string pattern=vw_temp_pattern("valwood-windows-test-XXXXXX");
    try{
        check(mkdtemp(pattern.data())!=nullptr,"Windows private temporary directory");
        const auto root=fs::u8path(pattern)/fs::u8path(u8"Artist work é spaces");fs::create_directory(root);
        struct Cleanup{fs::path path;~Cleanup(){std::error_code ec;fs::remove_all(path,ec);}} cleanup{fs::u8path(pattern)};
        std::string error;auto file=(root/"world.studio").u8string();world_studio::Document doc;
        world_studio::Placement p;p.alias="starting_crate";p.x=12;p.z=34;p.pitch=12;p.roll=-7;p.sx=1.2f;p.sy=.8f;doc.placements.push_back(p);
        check(world_studio::save(file.c_str(),doc,error),"First save: "+error);
        doc.placements[0].x=15;
        check(world_studio::save(file.c_str(),doc,error),"Atomic overwrite: "+error);
        world_studio::Document reopened;check(world_studio::load(file.c_str(),reopened,error)&&reopened.placements.size()==1&&reopened.placements[0].x==15&&reopened.placements[0].pitch==12,"Reopen retains actual TRS: "+error);
        const auto appearance=(root/"appearance.vwlooks").u8string();
        std::vector<world_npc_appearance::Binding> bindings={{1,"ai/example.vwmemory"}};
        check(world_npc_appearance::save_bindings(appearance,bindings,error),"Initial appearance save: "+error);
        bindings[0].resident=2;
        check(world_npc_appearance::save_bindings(appearance,bindings,error),"Repeated appearance save: "+error);
        {
            world_editor::Session session;check(session.open(file,reopened,0,error),"Recovery session opens: "+error);
            auto draft=reopened;draft.placements[0].z=40;check(session.changed(1),"Completed authoring gesture");
            check(session.flush_recovery(draft,2,error),"Recovery flush: "+error);
        }
        {
            world_editor::Session session;check(session.open(file,reopened,3,error),"Fresh recovery session");
            check(session.probe(error)==world_editor::Recovery::Available,"Recovery available: "+error);
            world_studio::Document recovered;check(session.recover(recovered,4,error)&&recovered.placements[0].z==40,"Recover unsaved transform: "+error);
            check(world_studio::save(file.c_str(),recovered,error)&&session.clean_saved(recovered,error),"Explicit Save clears recovery: "+error);
        }
        world_project::Files before;
        check(world_project::read(root.u8string(),{"world.studio","world.gameplay"},before,error),"Checkpoint original set");
        auto after=before;after[1].bytes="test typed authoring text\n";after[1].exists=true;
        check(world_project::commit(root.u8string(),"world.vwproject",after,before,error),"Coordinated checkpoint: "+error);
        check(!world_project::pending(root.u8string(),"world.vwproject"),"Completed journal removed");
        world_project::Files loaded;check(world_project::load(root.u8string(),"world.vwproject",loaded,error)&&loaded.size()==2,"Checkpoint reload: "+error);
        auto newer=after;newer[1].bytes="second generation\n";
        check(world_project::stage(root.u8string(),"world.vwproject",newer,after,error),"Write-ahead staging: "+error);
        check(world_project::pending(root.u8string(),"world.vwproject"),"Interrupted transaction detectable");
        check(world_project::recover(root.u8string(),"world.vwproject",false,error),"Roll back interrupted transaction: "+error);
        const auto lock=(root/"test.lock").u8string();int a=vw_open(lock.c_str(),O_CREAT|O_RDWR|O_NOFOLLOW,0600),b=vw_open(lock.c_str(),O_RDWR|O_NOFOLLOW);
        check(a>=0&&b>=0&&flock(a,LOCK_EX|LOCK_NB)==0,"Acquire project lock");
        check(flock(b,LOCK_EX|LOCK_NB)!=0,"Second handle cannot take existing lock");
        check(flock(a,LOCK_UN)==0&&flock(b,LOCK_EX|LOCK_NB)==0,"Release then acquire lock");flock(b,LOCK_UN);close(a);close(b);
        std::string output;int code=vw_run_process({vw_python_executable(),"-c","import sys;print(repr(sys.argv[1:]));sys.exit(7)",u8"é space", "quote\"value", "tail\\"},output,error);
        check(code==7&&output.find("quote")!=std::string::npos&&output.find("tail")!=std::string::npos,"Worker quoting/exit code: "+error+output);
        output.clear();code=vw_run_process({vw_python_executable(),"-c","import sys;sys.stdout.write('x'*150000);sys.stderr.write('done')"},output,error);
        check(code==0&&output.size()<66000&&output.find("limited")!=std::string::npos,"Bounded worker output fully drained");
        if(argc==3){
            auto report=world_assets::inspect(argv[2]);check(report.supported,"Purchased static GLB inspect: "+report.error);
            auto imported=world_assets::import_model(argv[1],root.u8string(),"windows_test_crate",{argv[2],"",""},1);
            check(imported.ok&&imported.report.triangles==report.triangles,"Bundled Python real GLB import: "+imported.error);
        }
        std::cout<<"WINDOWS_PLATFORM_PASS "<<checks<<" checks: Unicode/space paths, repeated save/reopen/TRS, recovery, checkpoint/journal rollback, locks, workers and imports\n";
        return 0;
    }catch(const std::exception &e){std::cerr<<"WINDOWS_PLATFORM_FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}
}
