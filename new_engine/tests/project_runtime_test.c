#define _POSIX_C_SOURCE 200809L
#include "engine/project_runtime.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void put32(FILE *f,unsigned value){
    unsigned char b[4]={value,value>>8,value>>16,value>>24};
    assert(fwrite(b,1,4,f)==4);
}

int main(void){
    EngineLaunch launch;
    char *valid[]={"engine","--editor","--width","1280","--frames","12","--view","river"};
    assert(engine_launch_parse(8,valid,&launch)==1);
    assert(launch.editor&&launch.direct&&launch.width==1280&&launch.frames==12);
    assert(!strcmp(launch.view,"river"));
    assert(!launch.graphics_preset[0]&&launch.auto_quality==-1&&launch.render_scale==0&&!launch.graphics_menu&&launch.target_fps==0);
    char *graphics[]={"engine","--play","--graphics-preset","balanced","--auto-quality","on","--render-scale",".85","--graphics-menu"};
    assert(engine_launch_parse(9,graphics,&launch)==1&&launch.direct&&launch.graphics_menu);
    assert(!strcmp(launch.graphics_preset,"balanced")&&launch.auto_quality==1&&launch.render_scale==.85f);
    const char *presets[]={"performance","balanced","cinematic"};
    for(size_t i=0;i<sizeof(presets)/sizeof(presets[0]);i++){
        char *argv[]={"engine","--graphics-preset",(char*)presets[i]};
        assert(engine_launch_parse(3,argv,&launch)==1&&!strcmp(launch.graphics_preset,presets[i]));
        assert(launch.auto_quality==-1&&launch.render_scale==0&&!launch.graphics_menu);
    }
    const char *valid_scales[]={".70","0.7","1","1.0","0.700001",".999999","7e-1"};
    for(size_t i=0;i<sizeof(valid_scales)/sizeof(valid_scales[0]);i++){
        char *argv[]={"engine","--render-scale",(char*)valid_scales[i]};
        assert(engine_launch_parse(3,argv,&launch)==1&&isfinite(launch.render_scale));
        assert(launch.render_scale>=.70f&&launch.render_scale<=1.f);
        assert(!launch.graphics_preset[0]&&launch.auto_quality==-1&&!launch.graphics_menu);
    }
    const char *invalid_scales[]={"","0","-0.7",".699999","0.6999999999","1.0000000001","1.01","nan","NaN","nan(1)","inf","-inf","1e999","1e-999",".8f",".8junk",".8 .9"," .8",".8 "};
    for(size_t i=0;i<sizeof(invalid_scales)/sizeof(invalid_scales[0]);i++){
        char *argv[]={"engine","--render-scale",(char*)invalid_scales[i]};
        assert(engine_launch_parse(3,argv,&launch)==-1&&launch.render_scale==0);
    }
    const char *invalid_presets[]={"","high","low","ultra","custom","Balanced","balanced-extra","performance "};
    for(size_t i=0;i<sizeof(invalid_presets)/sizeof(invalid_presets[0]);i++){
        char *argv[]={"engine","--graphics-preset",(char*)invalid_presets[i]};
        assert(engine_launch_parse(3,argv,&launch)==-1&&!launch.graphics_preset[0]);
    }
    const char *quality_values[]={"on","off"};
    for(size_t i=0;i<sizeof(quality_values)/sizeof(quality_values[0]);i++){
        char *argv[]={"engine","--auto-quality",(char*)quality_values[i]};
        assert(engine_launch_parse(3,argv,&launch)==1&&launch.auto_quality==(i==0));
    }
    const char *invalid_quality[]={"","true","false","1","0","ON","off-extra","on "};
    for(size_t i=0;i<sizeof(invalid_quality)/sizeof(invalid_quality[0]);i++){
        char *argv[]={"engine","--auto-quality",(char*)invalid_quality[i]};
        assert(engine_launch_parse(3,argv,&launch)==-1&&launch.auto_quality==-1);
    }
    const char *valid_targets[]={"45","60","90","120"};
    for(size_t i=0;i<sizeof(valid_targets)/sizeof(valid_targets[0]);i++){
        char *argv[]={"engine","--target-fps",(char*)valid_targets[i]};
        assert(engine_launch_parse(3,argv,&launch)==1&&launch.target_fps==atoi(valid_targets[i]));
        assert(!launch.graphics_preset[0]&&launch.auto_quality==-1&&launch.render_scale==0);
    }
    const char *invalid_targets[]={"","0","44","46","59","61","89","91","119","121","2147483648","nan","60.0","60fps","060","+60"," 60","60 "};
    for(size_t i=0;i<sizeof(invalid_targets)/sizeof(invalid_targets[0]);i++){
        char *argv[]={"engine","--target-fps",(char*)invalid_targets[i]};
        assert(engine_launch_parse(3,argv,&launch)==-1&&launch.target_fps==0);
    }
    const char *graphics_value_flags[]={"--graphics-preset","--auto-quality","--render-scale","--target-fps"};
    for(size_t i=0;i<sizeof(graphics_value_flags)/sizeof(graphics_value_flags[0]);i++){
        char *argv[]={"engine",(char*)graphics_value_flags[i]};
        assert(engine_launch_parse(2,argv,&launch)==-1);
    }
    char *repeated_graphics[]={"engine","--auto-quality","on","--auto-quality","off","--render-scale",".7","--render-scale","1","--graphics-preset","performance","--graphics-preset","cinematic"};
    assert(engine_launch_parse(13,repeated_graphics,&launch)==1&&launch.auto_quality==0&&launch.render_scale==1.f&&!strcmp(launch.graphics_preset,"cinematic"));
    char *defaults[]={"engine"};
    assert(engine_launch_parse(1,defaults,&launch)==1&&!launch.graphics_preset[0]&&launch.auto_quality==-1&&launch.render_scale==0&&!launch.graphics_menu&&launch.target_fps==0);
    const char *city_views[]={"city-town","city-walk","city-approach","city-meadow","city-woodland","city-camera-02","city-camera-03","city-camera-04","city-camera-05","city-camera-06","city-camera-07","city-camera-08","city-camera-09","city-camera-10","city-camera-11"};
    for(size_t i=0;i<sizeof(city_views)/sizeof(city_views[0]);i++){
        char *argv[]={"engine","--scenic",(char*)city_views[i]};
        assert(engine_launch_parse(3,argv,&launch)==1&&launch.direct&&!strcmp(launch.scenic,city_views[i]));
    }
    const char *invalid_city_views[]={"city","city-camera-01","city-camera-00","city-camera-12","city-camera-2","city-camera-002","city-camera-1x","city-camera--2","city-town-extra","city-meadow-extra","city-woodlands"};
    for(size_t i=0;i<sizeof(invalid_city_views)/sizeof(invalid_city_views[0]);i++){
        char *argv[]={"engine","--scenic",(char*)invalid_city_views[i]};
        assert(engine_launch_parse(3,argv,&launch)==-1&&!launch.scenic[0]);
    }
    char *meadow[]={"engine","--play","--view","meadow"};
    assert(engine_launch_parse(4,meadow,&launch)==1&&!strcmp(launch.view,"meadow"));
    char *bad[]={"engine","--frames","-2"};
    assert(engine_launch_parse(3,bad,&launch)==-1);
    char *missing[]={"engine","--project"};
    assert(engine_launch_parse(2,missing,&launch)==-1);
    char *unknown[]={"engine","--view","missing"};
    assert(engine_launch_parse(3,unknown,&launch)==-1);
    char *capture[]={"engine","--capture","sample.bmp"};
    assert(engine_launch_parse(3,capture,&launch)==1&&launch.frames==24&&launch.capture[0]=='/');
    char *atlas_launch[]={"engine","--landmark","gorshett_keep","--tour","--frames","120"};
    assert(engine_launch_parse(6,atlas_launch,&launch)==1);
    assert(launch.direct&&launch.tour&&launch.frames==120&&!strcmp(launch.landmark,"gorshett_keep"));
    char *tour[]={"engine","--tour"};
    assert(engine_launch_parse(2,tour,&launch)==1&&launch.tour&&launch.direct&&launch.frames==0);
    const char *bad_landmarks[]={"","../seldonne","name/child","white knight","north.east","name;echo","\n","\xc3\xa9"};
    for(size_t i=0;i<sizeof(bad_landmarks)/sizeof(bad_landmarks[0]);i++){
        char *argv[]={"engine","--landmark",(char*)bad_landmarks[i]};
        assert(engine_launch_parse(3,argv,&launch)==-1);
    }
    char longest_landmark[65];memset(longest_landmark,'a',sizeof(longest_landmark));longest_landmark[63]=0;
    char *long_argv[]={"engine","--landmark",longest_landmark};
    assert(engine_launch_parse(3,long_argv,&launch)==1&&strlen(launch.landmark)==63);
    longest_landmark[63]='a';longest_landmark[64]=0;
    assert(engine_launch_parse(3,long_argv,&launch)==-1);
    char *missing_landmark[]={"engine","--landmark"};
    assert(engine_launch_parse(2,missing_landmark,&launch)==-1);

    char *scenic[]={"engine","--scenic","lake"};
    assert(engine_launch_parse(3,scenic,&launch)==1&&launch.direct&&!strcmp(launch.scenic,"lake"));
    char *waterline[]={"engine","--scenic","shore"};
    assert(engine_launch_parse(3,waterline,&launch)==1&&launch.direct&&!strcmp(launch.scenic,"shore"));
    const char *new_views[]={"start","courtyard","workshop","overlook","grove","rainforest","rainforest-deep","rainforest-pool","rainforest-camp","rainforest-purchased-understory","castle-purchased-gardens","castle-finished-arcade","castle-finished-west","castle-finished-east","castle-creek-garden","castle-village","castle-market","castle-courtyard","castle-stalls","castle-alley","castle-demo","castle-housing","castle-marketrow","castle-forecourt","castle-hall","castle-hero-entry","castle-hero-mid","castle-hero-reverse","castle-drawbridge","castle-creek","castle-training","castle-statue","castle-armory","castle-farms","castle-farms-reverse","castle-watch","castle-watch-close","castle-farm-tools","wildlife-farm","wildlife-woodland","wildlife-rainforest","bandit-camp","bandit-supplies","bandit-lookout","bandit-fire","castle-west-street","castle-tavern","castle-tavern-interior","castle-shelter","castle-alleys"};
    for(size_t i=0;i<sizeof(new_views)/sizeof(new_views[0]);i++){
        char *argv[]={"engine","--scenic",(char*)new_views[i],"--benchmark","--view","arrival"};
        assert(engine_launch_parse(6,argv,&launch)==1&&launch.direct&&launch.benchmark&&launch.frames==360);
        assert(!strcmp(launch.scenic,new_views[i]));
    }
    char *starting_route[]={"engine","--scenic","start","--benchmark","--view","sweep","--warmup","40"};
    assert(engine_launch_parse(8,starting_route,&launch)==1&&launch.frames==6140&&launch.warmup==40);
    char *short_route[]={"engine","--scenic","start","--benchmark","--view","sweep","--frames","24"};
    assert(engine_launch_parse(8,short_route,&launch)==1&&launch.frames==24);
    char *profile_only[]={"engine","--scenic","start","--profile","timing.json","--view","sweep"};
    assert(engine_launch_parse(7,profile_only,&launch)==1&&!launch.benchmark&&launch.frames==360);
    char *village_route[]={"engine","--scenic","castle-village","--benchmark","--view","river","--warmup","120"};
    assert(engine_launch_parse(8,village_route,&launch)==1&&launch.frames==18120);
    char *demo_route[]={"engine","--scenic","castle-demo","--benchmark","--view","river","--warmup","120"};
    assert(engine_launch_parse(8,demo_route,&launch)==1&&launch.frames==18120);
    char *hero_route[]={"engine","--scenic","castle-hero-entry","--benchmark","--view","river","--warmup","120"};
    assert(engine_launch_parse(8,hero_route,&launch)==1&&launch.frames==1820);
    char *castle_route[]={"engine","--scenic","castle-drawbridge","--benchmark","--view","river","--warmup","120"};
    assert(engine_launch_parse(8,castle_route,&launch)==1&&launch.frames==18120);
    char *farm_route[]={"engine","--scenic","castle-farms","--benchmark","--view","river","--warmup","120"};
    assert(engine_launch_parse(8,farm_route,&launch)==1&&launch.frames==3318);
    char *bad_scenic[]={"engine","--scenic","../lake"};
    assert(engine_launch_parse(3,bad_scenic,&launch)==-1);
    assert(engine_launch_parse(2,scenic,&launch)==-1);

    char directory[]="/tmp/valwood-project-test-XXXXXX";
    assert(mkdtemp(directory)); assert(chdir(directory)==0);
    FILE *f=fopen("source model.glb","wb"); assert(f);
    const char payload[]="{\"asset\":{\"version\":\"2.0\"}} ";
    unsigned bytes=(unsigned)strlen(payload), padded=(bytes+3)&~3u;
    put32(f,0x46546c67); put32(f,2); put32(f,20+padded);
    put32(f,padded); put32(f,0x4e4f534a);
    fwrite(payload,1,bytes,f); for(unsigned i=bytes;i<padded;i++) fputc(' ',f); fclose(f);
    char destination[256], message[256];
    assert(engine_project_import_glb("source model.glb",destination,sizeof(destination),message,sizeof(message)));
    assert(!strcmp(destination,"imports/models/source_model.glb"));
    struct stat first,source; assert(stat(destination,&first)==0&&stat("source model.glb",&source)==0);
    assert(first.st_size==source.st_size);
    assert(engine_project_import_glb("source model.glb",destination,sizeof(destination),message,sizeof(message)));
    assert(!strcmp(destination,"imports/models/source_model_2.glb"));
    f=fopen("broken.glb","wb"); fputs("not a model",f); fclose(f);
    assert(!engine_project_import_glb("broken.glb",destination,sizeof(destination),message,sizeof(message)));
    assert(!engine_project_import_glb("source.fbx",destination,sizeof(destination),message,sizeof(message)));
    char small[2];
    assert(!engine_project_import_glb("source model.glb",small,sizeof(small),message,sizeof(message)));

    f=fopen("project.valwood","w"); assert(f);
    fputs("name=Unit Test\nspawn=4 -12 3.14\ncamera=3.14 0.25 9\nday_fraction=0.3\n",f); fclose(f);
    memset(&launch,0,sizeof(launch)); snprintf(launch.project,sizeof(launch.project),"%s",directory);
    EngineProject project;
    assert(engine_project_open(&launch,&project));
    assert(project.loaded&&!project.atlas_world&&!strcmp(project.name,"Unit Test")&&project.spawn_z==-12);
    f=fopen("project.valwood","w"); fputs("spawn=nan 0 0\n",f); fclose(f);
    assert(!engine_project_open(&launch,&project));
    const char *invalid_settings[]={"spawn=4 bad 0\n","camera=0 0\n","day_fraction=0.3junk\n","spawn=4 0 0 extra\n","day_fraction=1\n","camera=0 0 500\n","spawn=246 0 0\n","spawn=0 -246 0\n","terrain=unknown\n"};
    for(size_t i=0;i<sizeof(invalid_settings)/sizeof(invalid_settings[0]);i++){
        f=fopen("project.valwood","w");fputs(invalid_settings[i],f);fclose(f);
        assert(!engine_project_open(&launch,&project));
    }
    /* The atlas opts into its own bounds; manifests can place terrain last. */
    f=fopen("project.valwood","w");assert(f);
    fputs("name=Valwood Atlas\nspawn=-11692.8 6294.6773 0\nterrain=valwood_atlas\n",f);fclose(f);
    assert(engine_project_open(&launch,&project)&&project.loaded&&project.atlas_world);
    assert(project.spawn_x==-11692.8f&&fabsf(project.spawn_z-6294.6773f)<.001f);
    f=fopen("project.valwood","w");assert(f);
    fputs("terrain=valwood_atlas\nspawn=50400 -28354 0\n",f);fclose(f);
    assert(engine_project_open(&launch,&project)&&project.atlas_world);
    const char *invalid_atlas[]={"spawn=50401 0 0\n","spawn=-50401 0 0\n","spawn=0 28355 0\n","spawn=0 -28355 0\n","spawn=inf 0 0\n"};
    for(size_t i=0;i<sizeof(invalid_atlas)/sizeof(invalid_atlas[0]);i++){
        f=fopen("project.valwood","w");assert(f);fputs("terrain=valwood_atlas\n",f);fputs(invalid_atlas[i],f);fclose(f);
        assert(!engine_project_open(&launch,&project));
    }
    /* An atlas opened earlier must not expand the next legacy project's bounds. */
    f=fopen("project.valwood","w");assert(f);fputs("spawn=300 0 0\n",f);fclose(f);
    assert(!engine_project_open(&launch,&project)&&!project.atlas_world);
    puts("PASS project launch/graphics/landmark validation, isolated atlas bounds, import copies, duplicate protection, malformed assets, project state");
    return 0;
}
