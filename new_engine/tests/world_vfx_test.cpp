#include "engine/world_vfx.h"
#include "engine/world_studio.h"
#include "engine/world_editor_session.h"
#include <filesystem>
#include <cassert>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <unistd.h>
int main(int argc,char **argv){
 unsigned checks=0;std::string error;world_studio::Document doc;
 for(unsigned i=0;i<world_vfx::presets.size();++i){auto p=world_vfx::make(i,12,24,-18,121+i);doc.particles.push_back(p);assert(world_studio::valid(doc,error));++checks;assert(p.ceiling>=p.y+p.height*(p.kind==2?3.2:1.));++checks;}
 char path[]="/tmp/valwood-vfx-XXXXXX";int fd=mkstemp(path);assert(fd>=0);close(fd);
 assert(world_studio::save(path,doc,error));++checks;world_studio::Document loaded;assert(world_studio::load(path,loaded,error));++checks;
 for(unsigned i=0;i<doc.particles.size();++i){const auto &a=doc.particles[i],&b=loaded.particles[i];assert(a.kind==b.kind&&a.tint==b.tint&&a.glow==b.glow&&a.seed==b.seed&&a.ceiling==b.ceiling);++checks;}
 auto p=doc.particles[0];const auto height=p.ceiling-p.y;world_vfx::move(p,7,82,9);assert(std::abs((p.ceiling-p.y)-height)<1e-10&&p.x==7&&p.z==9);++checks;
 world_vfx::Clock clock;clock.tick(.1);assert(std::abs(clock.time-.1)<1e-6);++checks;clock.playing=false;clock.tick(2);assert(std::abs(clock.time-.1)<1e-6);++checks;clock.restart();assert(clock.time==0);++checks;clock.playing=true;clock.speed=.5;clock.tick(.2);assert(std::abs(clock.time-.1)<1e-6);++checks;clock.tick(std::numeric_limits<double>::quiet_NaN());assert(std::abs(clock.time-.1)<1e-6);++checks;
 // Old saves receive neutral tint and no glow; schema seven rejects bad values.
 {world_studio::Document old;old.particles.push_back(world_vfx::make(3,0,1,0,123));std::string encoded;assert(world_studio::encode(old,encoded,error));std::istringstream in(encoded);std::ofstream f(path);std::string line;while(std::getline(in,line)){if(line.rfind("VALWOOD_STUDIO",0)==0)line="VALWOOD_STUDIO 6";if(line.rfind("particle ",0)==0){std::istringstream fields(line);line.clear();std::string token;for(unsigned i=0;i<18;++i){fields>>token;if(i)line+=' ';line+=token;}}f<<line<<'\n';}}
 assert((world_studio::load(path,loaded,error)&&loaded.particles[0].tint==std::array<float,3>({1,1,1})&&loaded.particles[0].glow==0));++checks;
 {
  std::string canonical;assert(world_studio::encode(loaded,canonical,error)&&canonical.rfind("VALWOOD_STUDIO 6\n",0)==0);++checks;
  world_editor::Session session;assert(session.open(path,loaded,0,error));++checks;
  auto draft=loaded;draft.particles.push_back(world_vfx::make(0,4,2,0,98101));assert(session.flush_recovery(draft,1,error));++checks;
  world_editor::Session restarted;assert(restarted.open(path,loaded,0,error)&&restarted.probe(error)==world_editor::Recovery::Available);++checks;
  world_studio::Document recovered;assert(restarted.recover(recovered,1,error)&&recovered.particles.size()==2&&recovered.particles[1].kind==4&&recovered.particles[1].glow==4);++checks;
 }
 for(unsigned i=0;i<3;++i)std::filesystem::remove(std::string(path)+".recovery"+(i?"."+std::to_string(i):""));
 if(argc==3){
  char folder[]="/tmp/valwood-legacy-vfx-XXXXXX";assert(mkdtemp(folder));const auto scene=std::string(folder)+"/world.studio";
  std::filesystem::copy_file(argv[1],scene);std::filesystem::copy_file(argv[2],scene+".recovery");world_studio::Document base,recovered;assert(world_studio::load(scene.c_str(),base,error));++checks;
  {world_editor::Session session;assert(session.open(scene,base,0,error)&&session.probe(error)==world_editor::Recovery::Available);++checks;assert(session.recover(recovered,1,error));++checks;}
  std::filesystem::remove_all(folder);std::cout<<"Existing version-six user recovery opens and stages successfully in an isolated copy\n";
 }
 doc.particles[0].glow=9;assert(!world_studio::valid(doc,error));++checks;doc.particles[0].glow=4;doc.particles[0].tint[1]=std::numeric_limits<float>::quiet_NaN();assert(!world_studio::valid(doc,error));++checks;doc.particles[0].tint[1]=.5;doc.particles[0].kind=5;assert(!world_studio::valid(doc,error));++checks;
 unlink(path);std::cout<<"VFX authoring: "<<checks<<" checks passed (presets, persistence, migration, bounds, preview clock)\n";
}
