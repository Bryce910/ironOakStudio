#include "engine/world_graphics_settings.h"
#include "engine/world_quality_controller.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
namespace {
unsigned checks=0;
void check(bool ok){++checks;if(!ok){std::fprintf(stderr,"quality check %u failed\n",checks);std::abort();}}
void frames(QualityController &q,unsigned count,double gpu,double cpu,double dt,std::uint64_t epoch=1){
 auto serial=q.status().last_serial;
 for(unsigned i=0;i<count;i++)q.submit({++serial,epoch,gpu,cpu,true,dt});
}
}
int main(){
 auto s=world_graphics_settings_default();
 check(s.foliage_density==WorldFoliageDensity::High&&s.reflection_scale==.25f);
 const WorldFoliageDensity densities[]={WorldFoliageDensity::Low,WorldFoliageDensity::Medium,WorldFoliageDensity::High,WorldFoliageDensity::Ultra};
 float last=0;
 for(auto density:densities){const float amount=world_foliage_density_scale(density);check(world_foliage_density_name(density)&&amount>last&&amount<=1);last=amount;}
 check(world_foliage_density_name(static_cast<WorldFoliageDensity>(99))==nullptr);
 for(int cap:{0,30,45,60,90,120,144}){s.frame_rate_limit=cap;check(world_graphics_settings_valid(s)&&world_graphics_frame_limit_name(cap));}
 check(world_graphics_frame_limit_name(59)==nullptr);
 for(int side:{1024,2048,3072,4096})check(world_graphics_shadow_quality_name(side)!=nullptr);
 check(world_graphics_shadow_quality_name(1536)==nullptr);
 s.target_fps=144;s.frame_rate_limit=0;s.vsync=false;
 check(world_graphics_effective_target_fps(s,60)==144);
 s.vsync=true;check(world_graphics_effective_target_fps(s,60)==60);
 check(world_graphics_effective_target_fps(s,0)==144);
 check(world_graphics_effective_target_fps(s,-1)==144);
 s.frame_rate_limit=45;check(world_graphics_effective_target_fps(s,60)==45);
 s.frame_rate_limit=120;check(world_graphics_effective_target_fps(s,59)==59);
 s.target_fps=30;s.frame_rate_limit=144;check(world_graphics_effective_target_fps(s,144)==30);
 s.target_fps=144;s.frame_rate_limit=30;s.auto_quality=false;s.display_mode=WorldDisplayMode::Borderless;s.window_width=1920;s.window_height=1080;
 const auto prior=s;
 for(auto preset:{WorldGraphicsPreset::Performance,WorldGraphicsPreset::Balanced,WorldGraphicsPreset::Cinematic}){
  check(world_graphics_settings_apply_preset(&s,preset));
  check(s.target_fps==prior.target_fps&&s.frame_rate_limit==prior.frame_rate_limit&&s.auto_quality==prior.auto_quality&&s.vsync==prior.vsync&&s.display_mode==prior.display_mode&&s.window_width==prior.window_width&&s.window_height==prior.window_height);
  check(world_graphics_effective_target_fps(s,60)==30);
 }
 QualityController q;
 // The previous fixed45FPS emergency path degraded this healthy30FPS scene.
 q.reset({true,1,.7f,30},1);frames(q,300,25,4,1./30.);
 check(q.scale()==1&&q.status().resizes==0&&!q.status().floor_limited&&q.status().target_fps==30);
 q.reset({true,.7f,.7f,30},1);frames(q,100,25,4,1./30.);
 check(!q.status().floor_limited&&q.status().resizes==0);
 // A real24Hz VSync mode is also an intentional cadence, not a missed30FPS
 // target. Saved target choices remain30+; only runtime refresh can lower it.
 s.target_fps=60;s.frame_rate_limit=0;s.vsync=true;
 check(world_graphics_effective_target_fps(s,24)==24);
 q.reset({true,1,.7f,float(world_graphics_effective_target_fps(s,24))},1);
 frames(q,240,34,4,1./24.);
 check(q.status().target_fps==24&&q.scale()==1&&q.status().resizes==0&&!q.status().floor_limited);
 // A real GPU miss at that same lower target still causes bounded adaptation.
 q.reset({true,1,.7f,30},1);frames(q,100,40,4,1./24.);
 check(q.scale()<1&&q.scale()>=.7f&&q.status().resizes>0);
 q.reset({true,.7f,.7f,30},1);frames(q,100,40,4,1./24.);
 check(q.status().floor_limited&&q.scale()==.7f);
 // Lowering pixel count cannot repair CPU-bound frames. A high GPU number by
 // itself must not trigger the emergency branch while CPU work is dominant.
 q.reset({true,1,.7f,60},1);frames(q,300,25,38,1./24.);
 check(q.status().cpu_limited&&q.scale()==1&&q.status().resizes==0);
 q.reset({true,.7f,.7f,60},1);frames(q,100,25,38,1./24.);
 check(q.status().cpu_limited&&!q.status().floor_limited);
 // GPU-limited60FPS behavior, monotone serials and resize epochs remain active.
 q.reset({true,1,.7f,60},1);frames(q,120,35,4,1./30.);check(q.scale()<1&&q.scale()>=.7f);
 auto before=q.status();check(!q.submit({before.last_serial,1,35,4,true,1./30.}));
 check(q.status().duplicate_samples==before.duplicate_samples+1);
 check(!q.submit({before.last_serial+1,0,35,4,true,1./30.}));
 check(q.status().stale_samples==before.stale_samples+1);
 check(q.reconfigure(2)&&!q.reconfigure(2));const float reduced=q.scale();
 check(!q.submit({999,1,8,3,true,1./60.})&&q.scale()==reduced);
 frames(q,1800,8,3,1./60.,2);check(q.scale()==1&&q.status().resizes>before.resizes);
 q.reset({false,1,.7f,60},1);frames(q,120,50,2,.05);check(q.scale()==1&&q.status().excluded_samples==120);
 q.reset({true,1,.7f,std::numeric_limits<float>::quiet_NaN()},1);check(q.status().target_fps==60);
 q.reset({true,1,.7f,144},1);check(q.status().target_fps==144&&q.status().frame_budget_ms<7);
 check(!q.submit({1,1,std::numeric_limits<double>::infinity(),1,true,.01}));
 check(q.status().invalid_samples==1&&q.scale()==1);
 std::printf("PASS quality/settings CPU: %u checks; independent cap/VSync targets, presets, healthy30FPS, real GPU pressure, CPU-bound suppression, recovery and epoch/invalid-sample guards\n",checks);
}
