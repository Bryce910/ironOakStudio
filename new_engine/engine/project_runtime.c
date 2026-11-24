#define _POSIX_C_SOURCE 200809L
#include "engine/world_platform.h"
#include "engine/project_runtime.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>

EngineLaunch g_launch;
EngineProject g_project;

static bool copy_path(char *out,size_t capacity,const char *path) {
    char cwd[1024];
    int n;
    if(!path||!path[0]) return false;
    if(path[0]=='/' || (isalpha((unsigned char)path[0]) && path[1]==':') || path[0]=='\\') n=snprintf(out,capacity,"%s",path);
    else {
        if(!getcwd(cwd,sizeof(cwd))) return false;
        n=snprintf(out,capacity,"%s/%s",cwd,path);
    }
    return n>=0&&(size_t)n<capacity;
}

static bool number(const char *s,int low,int high,int *out) {
    char *end=NULL;
    long v;
    errno=0; v=strtol(s,&end,10);
    if(errno||!s[0]||*end||v<low||v>high) return false;
    *out=(int)v; return true;
}

static bool render_scale_value(const char *s,float *out) {
    char *end=NULL;
    if(!s[0]||isspace((unsigned char)s[0])) return false;
    errno=0;
    double value=strtod(s,&end);
    /* Check the double before narrowing, so values just beyond a boundary
       cannot become valid when rounded to the nearest float. */
    if(errno||end==s||*end||!isfinite(value)||value<.70||value>1.) return false;
    *out=(float)value;
    return true;
}

static bool landmark_name(const char *s) {
    size_t length=strlen(s);
    if(!length||length>=sizeof(((EngineLaunch*)0)->landmark)) return false;
    for(size_t i=0;i<length;i++){
        unsigned char c=(unsigned char)s[i];
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
             (c>='0'&&c<='9')||c=='_'||c=='-')) return false;
    }
    return true;
}

static bool city_scenic_name(const char *value) {
    if(!strcmp(value,"city-town")||!strcmp(value,"city-walk")||!strcmp(value,"city-approach")||!strcmp(value,"city-meadow")||!strcmp(value,"city-woodland"))return true;
    if(strlen(value)!=14||strncmp(value,"city-camera-",12)||value[12]<'0'||value[12]>'9'||value[13]<'0'||value[13]>'9')return false;
    const unsigned station=(unsigned)(value[12]-'0')*10+(unsigned)(value[13]-'0');
    return station>=2&&station<=11;
}

int engine_launch_parse(int argc,char **argv,EngineLaunch *launch) {
    memset(launch,0,sizeof(*launch));
    launch->width=1600; launch->height=900; launch->seed=4217; launch->warmup=60;
    launch->auto_quality=-1;
    snprintf(launch->view,sizeof(launch->view),"arrival");
    for(int i=1;i<argc;i++) {
        const char *arg=argv[i];
        if(!strcmp(arg,"--help")||!strcmp(arg,"-h")) {
            puts("Valwood Engine\n"
                 "  --project PATH   Open a project.valwood directory\n"
                 "  --editor         Enter the authoring workspace directly\n"
                 "  --play           Enter the starting area directly\n"
                 "  --view NAME      arrival, village, meadow, river, overlook; sweep benchmarks rotate or follow the starting-area loop\n"
                 "  --landmark NAME  Atlas landmark to inspect (for example seldonne or callieut)\n"
                 "  --scenic NAME    lake, forest, ridge, coast, shore, rainforest, rainforest-deep, rainforest-pool, rainforest-camp, start, courtyard, workshop, overlook, grove, castle-village, castle-market, castle-stalls, castle-alley, castle-courtyard, castle-demo, castle-housing, castle-marketrow, castle-forecourt, castle-hall, castle-hero-entry, castle-hero-mid or castle-hero-reverse, castle-drawbridge, castle-creek, castle-training, castle-training-start, castle-statue, castle-armory, castle-farms, castle-farms-reverse, castle-watch, castle-watch-close or castle-farm-tools, wildlife-farm, wildlife-woodland, wildlife-rainforest, bandit-camp, bandit-supplies, bandit-lookout, bandit-fire, castle-west-street, castle-tavern, castle-tavern-interior, castle-shelter, castle-alleys, castle-purchased-gardens, castle-creek-garden, rainforest-purchased-understory, castle-milestone, rainforest-milestone, castle-hero-portrait, castle-finished-arcade, castle-finished-west, castle-finished-east, castle-dragons, castle-frost-dragon, castle-dragon-flight, castle-dragon-overlook, city-town, city-walk, city-approach, city-meadow, city-woodland, city-camera-02..11\n"
                 "  --tour           Tour the atlas landmarks\n"
                 "  --width N --height N   Window / capture size\n"
                 "  --headless       Render with SDL's offscreen driver\n"
                 "  --capture PATH   Save a BMP and JSON rendering report\n"
                 "  --frames N       Exit after N game frames (capture defaults to 24)\n"
                 "  --no-hud         Hide gameplay overlays in captures\n"
                 "  --seed N         Reproducible world seed\n"
                 "  --profile PATH   Frame timing JSON and CSV, with --warmup N\n"
                 "  --benchmark      Repeatable moving-camera run\n"
                 "  --hidden         Keep the benchmark window hidden\n"
                 "  --graphics-preset NAME   performance, balanced or cinematic\n"
                 "  --auto-quality on|off   Override adaptive graphics quality\n"
                 "  --target-fps N   Adaptive target: 45, 60, 90 or 120\n"
                 "  --render-scale N   Internal render scale (.70 through 1.0)\n"
                 "  --graphics-menu   Open the graphics settings panel\n"
                 "  MODEL            Optional character model override");
            return 0;
        }
        if(!strcmp(arg,"--editor")) { launch->editor=true; launch->direct=true; continue; }
        if(!strcmp(arg,"--play")) { launch->editor=false; launch->direct=true; continue; }
        if(!strcmp(arg,"--headless")) { launch->headless=true; continue; }
        if(!strcmp(arg,"--benchmark")){launch->benchmark=true;launch->direct=true;continue;}
        if(!strcmp(arg,"--tour")){launch->tour=true;launch->direct=true;continue;}
        if(!strcmp(arg,"--hidden")){launch->hidden=true;continue;}
        if(!strcmp(arg,"--no-hud")) { launch->no_hud=true; continue; }
        if(!strcmp(arg,"--graphics-menu")) { launch->graphics_menu=true; continue; }
        if(!strcmp(arg,"--project")||!strcmp(arg,"--capture")||!strcmp(arg,"--profile")||!strcmp(arg,"--warmup")||!strcmp(arg,"--view")||!strcmp(arg,"--landmark")||!strcmp(arg,"--scenic")||
           !strcmp(arg,"--frames")||!strcmp(arg,"--width")||!strcmp(arg,"--height")||!strcmp(arg,"--seed")||
           !strcmp(arg,"--graphics-preset")||!strcmp(arg,"--auto-quality")||!strcmp(arg,"--render-scale")||!strcmp(arg,"--target-fps")) {
            if(i+1>=argc) { fprintf(stderr,"Missing value for %s\n",arg); return -1; }
            const char *value=argv[++i];
            bool ok=true;
            if(!strcmp(arg,"--project")) ok=copy_path(launch->project,sizeof(launch->project),value);
            else if(!strcmp(arg,"--capture")) ok=copy_path(launch->capture,sizeof(launch->capture),value);
            else if(!strcmp(arg,"--profile")) ok=copy_path(launch->profile,sizeof(launch->profile),value);
            else if(!strcmp(arg,"--warmup")) ok=number(value,0,100000,&launch->warmup);
            else if(!strcmp(arg,"--graphics-preset")) {
                ok=!strcmp(value,"performance")||!strcmp(value,"balanced")||!strcmp(value,"cinematic");
                if(ok) snprintf(launch->graphics_preset,sizeof(launch->graphics_preset),"%s",value);
            }
            else if(!strcmp(arg,"--auto-quality")) {
                ok=!strcmp(value,"on")||!strcmp(value,"off");
                if(ok) launch->auto_quality=!strcmp(value,"on");
            }
            else if(!strcmp(arg,"--render-scale")) ok=render_scale_value(value,&launch->render_scale);
            else if(!strcmp(arg,"--target-fps")) {
                ok=!strcmp(value,"45")||!strcmp(value,"60")||!strcmp(value,"90")||!strcmp(value,"120");
                if(ok) launch->target_fps=atoi(value);
            }
            else if(!strcmp(arg,"--landmark")) {
                ok=landmark_name(value);
                if(ok){snprintf(launch->landmark,sizeof(launch->landmark),"%s",value);launch->direct=true;}
            }
            else if(!strcmp(arg,"--scenic")) {
                ok=!strcmp(value,"lake")||!strcmp(value,"forest")||!strcmp(value,"ridge")||!strcmp(value,"coast")||!strcmp(value,"shore")||
                   !strcmp(value,"start")||!strcmp(value,"courtyard")||!strcmp(value,"workshop")||!strcmp(value,"overlook")||!strcmp(value,"grove")||
                   !strcmp(value,"rainforest")||!strcmp(value,"rainforest-deep")||!strcmp(value,"rainforest-pool")||!strcmp(value,"rainforest-camp")||!strcmp(value,"rainforest-purchased-understory")||
                   !strcmp(value,"castle-dragons")||!strcmp(value,"castle-frost-dragon")||!strcmp(value,"castle-dragon-flight")||!strcmp(value,"castle-dragon-overlook")||!strcmp(value,"castle-quarter")||!strcmp(value,"castle-quarter-tavern")||!strcmp(value,"castle-milestone")||!strcmp(value,"rainforest-milestone")||!strcmp(value,"castle-hero-portrait")||!strcmp(value,"castle-finished-arcade")||!strcmp(value,"castle-finished-west")||!strcmp(value,"castle-finished-east")||!strcmp(value,"castle-purchased-gardens")||!strcmp(value,"castle-creek-garden")||
                   !strcmp(value,"castle-village")||!strcmp(value,"castle-market")||!strcmp(value,"castle-courtyard")||!strcmp(value,"castle-stalls")||!strcmp(value,"castle-alley")||
                   !strcmp(value,"castle-demo")||!strcmp(value,"castle-housing")||!strcmp(value,"castle-marketrow")||!strcmp(value,"castle-forecourt")||!strcmp(value,"castle-hall")||
                   !strcmp(value,"castle-hero-entry")||!strcmp(value,"castle-hero-mid")||!strcmp(value,"castle-hero-reverse")||!strcmp(value,"castle-drawbridge")||!strcmp(value,"castle-creek")||!strcmp(value,"castle-training")||!strcmp(value,"castle-training-start")||!strcmp(value,"castle-statue")||!strcmp(value,"castle-armory")||!strcmp(value,"castle-farms")||!strcmp(value,"castle-farms-reverse")||!strcmp(value,"castle-watch")||!strcmp(value,"castle-watch-close")||!strcmp(value,"castle-farm-tools")||!strcmp(value,"wildlife-farm")||!strcmp(value,"wildlife-woodland")||!strcmp(value,"wildlife-rainforest")||!strcmp(value,"bandit-camp")||!strcmp(value,"bandit-supplies")||!strcmp(value,"bandit-lookout")||!strcmp(value,"bandit-fire")||!strcmp(value,"castle-west-street")||!strcmp(value,"castle-tavern")||!strcmp(value,"castle-tavern-interior")||!strcmp(value,"castle-shelter")||!strcmp(value,"castle-alleys");
                ok=ok||!strcmp(value,"stronghold")||!strcmp(value,"stronghold-gate")||!strcmp(value,"stronghold-drifts")||!strcmp(value,"stronghold-snow")||!strcmp(value,"stronghold-hall")||!strcmp(value,"stronghold-forge")||!strcmp(value,"stronghold-throne");
                ok=ok||!strcmp(value,"stronghold-town")||!strcmp(value,"stronghold-market")||!strcmp(value,"stronghold-sanctuary")||!strcmp(value,"stronghold-overlook")||!strcmp(value,"stronghold-forge-wing");
                ok=ok||!strcmp(value,"stronghold-waterfall")||!strcmp(value,"stronghold-trees")||city_scenic_name(value);
                if(ok){snprintf(launch->scenic,sizeof(launch->scenic),"%s",value);launch->direct=true;}
            }
            else if(!strcmp(arg,"--view")) {
                ok=!strcmp(value,"arrival")||!strcmp(value,"village")||!strcmp(value,"river")||!strcmp(value,"meadow")||!strcmp(value,"sweep")||!strcmp(value,"overlook");
                if(ok) snprintf(launch->view,sizeof(launch->view),"%s",value);
            } else if(!strcmp(arg,"--frames")) ok=number(value,1,1000000,&launch->frames);
            else if(!strcmp(arg,"--width")) ok=number(value,320,3840,&launch->width);
            else if(!strcmp(arg,"--height")) ok=number(value,240,2160,&launch->height);
            else { int seed=0; ok=number(value,1,INT_MAX,&seed); launch->seed=(unsigned)seed; }
            if(!ok) { fprintf(stderr,"Invalid value for %s: %s\n",arg,value); return -1; }
            continue;
        }
        if(arg[0]=='-'||launch->character[0]||strlen(arg)>=sizeof(launch->character)) {
            fprintf(stderr,"Unknown argument: %s\n",arg); return -1;
        }
        snprintf(launch->character,sizeof(launch->character),"%s",arg);
    }
    if((launch->profile[0]||launch->benchmark)&&!launch->frames) {
        /* The350.257m start+sweep route needs about6005 simulated walking
           frames at60Hz, plus warmup and a small destination-streaming margin. */
        launch->frames=launch->benchmark&&!strcmp(launch->scenic,"castle-west-street")&&!strcmp(launch->view,"river") ? launch->warmup+10400 :
            launch->benchmark&&!strcmp(launch->scenic,"bandit-camp")&&!strcmp(launch->view,"river") ? launch->warmup+2400 :
            launch->benchmark&&!strcmp(launch->scenic,"start")&&!strcmp(launch->view,"sweep") ? launch->warmup+6100 :
            (launch->benchmark&&!strcmp(launch->scenic,"rainforest")&&!strcmp(launch->view,"river") ? launch->warmup+7500 :
             (launch->benchmark&&(!strcmp(launch->scenic,"castle-village")||!strcmp(launch->scenic,"castle-demo"))&&!strcmp(launch->view,"river") ? launch->warmup+18000 :
              (launch->benchmark&&!strcmp(launch->scenic,"castle-hero-entry")&&!strcmp(launch->view,"river") ? launch->warmup+1700 : (launch->benchmark&&!strcmp(launch->scenic,"castle-drawbridge")&&!strcmp(launch->view,"river") ? launch->warmup+18000 : (launch->benchmark&&!strcmp(launch->scenic,"castle-farms")&&!strcmp(launch->view,"river") ? launch->warmup+3198 : 360)))));
    }
    if(launch->capture[0]&&!launch->frames) launch->frames=24;
    if(launch->frames||launch->capture[0]) launch->direct=true;
    if(launch->headless) {
        SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
        SDL_setenv("SDL_AUDIODRIVER","dummy",1);
    }
    return 1;
}

static char *trim(char *s) {
    while(isspace((unsigned char)*s)) s++;
    char *end=s+strlen(s);
    while(end>s&&isspace((unsigned char)end[-1])) *--end=0;
    return s;
}

bool engine_project_open(EngineLaunch *launch,EngineProject *project) {
    memset(project,0,sizeof(*project));
    snprintf(project->name,sizeof(project->name),"Valwood Engine");
    project->camera_pitch=0.28f; project->camera_distance=9.0f; project->day_fraction=0.32f;
    if(!launch->project[0]) return true;
    char manifest[1200];
    snprintf(manifest,sizeof(manifest),"%s/project.valwood",launch->project);
    FILE *f=fopen(manifest,"r");
    if(!f) { fprintf(stderr,"[Project] Cannot open %s: %s\n",manifest,strerror(errno)); return false; }
    char line[512];
    bool valid=true;
    while(fgets(line,sizeof(line),f)) {
        char *key=trim(line), *eq=strchr(key,'=');
        if(!key[0]||key[0]=='#'||!eq) continue;
        *eq++=0; key=trim(key); char *value=trim(eq);
        if(!strcmp(key,"name")) snprintf(project->name,sizeof(project->name),"%s",value);
        else if(!strcmp(key,"terrain")){
            if(!strcmp(value,"valwood_atlas")) project->atlas_world=true;
            else valid=false;
        }
        else if(!strcmp(key,"spawn")){char extra; if(sscanf(value,"%f %f %f %c",&project->spawn_x,&project->spawn_z,&project->yaw,&extra)!=3) valid=false;}
        else if(!strcmp(key,"camera")){char extra; if(sscanf(value,"%f %f %f %c",&project->camera_yaw,&project->camera_pitch,&project->camera_distance,&extra)!=3) valid=false;}
        else if(!strcmp(key,"day_fraction")){char *end; project->day_fraction=strtof(value,&end); if(end==value||*trim(end)) valid=false;}
    }
    fclose(f);
    if(!valid){fprintf(stderr,"[Project] Malformed project settings in %s\n",manifest);return false;}
    const float values[]={project->spawn_x,project->spawn_z,project->yaw,project->camera_yaw,
        project->camera_pitch,project->camera_distance,project->day_fraction};
    for(size_t i=0;i<sizeof(values)/sizeof(values[0]);i++) if(!isfinite(values[i])) return false;
    const float spawn_limit_x=project->atlas_world?50400.0f:245.0f;
    const float spawn_limit_z=project->atlas_world?(100800.0f*2818.0f/5009.0f*0.5f):245.0f;
    if(fabsf(project->spawn_x)>spawn_limit_x||fabsf(project->spawn_z)>spawn_limit_z||project->camera_distance<2||
       project->camera_distance>180||project->camera_pitch<-1.4f||project->camera_pitch>1.4f||
       project->day_fraction<0||project->day_fraction>=1) {
        fprintf(stderr,"[Project] Invalid spawn, camera, or clock in %s\n",manifest); return false;
    }
    if(chdir(launch->project)) { perror("[Project] chdir"); return false; }
    project->loaded=true;
    printf("[Project] Opened %s at %s\n",project->name,launch->project);
    return true;
}

static void json_string(FILE *f,const char *s) {
    fputc('"',f);
    for(;s&&*s;s++) {
        unsigned char c=(unsigned char)*s;
        if(c=='"'||c=='\\') fputc('\\',f);
        if(c>=32) fputc(c,f);
    }
    fputc('"',f);
}

void engine_launch_frame(SDL_Window *window) {
    if(!g_launch.active||g_launch.finished) return;
    if(!g_launch.started) g_launch.started=SDL_GetPerformanceCounter();
    g_launch.rendered_frames++;
    if(!g_launch.frames||g_launch.rendered_frames<g_launch.frames) return;
    if(g_launch.capture[0]) {
        int w,h; SDL_GL_GetDrawableSize(window,&w,&h);
        size_t stride=(size_t)w*3;
        unsigned char *pixels=malloc(stride*(size_t)h), *row=malloc(stride);
        bool ok=pixels&&row;
        if(ok) {
            glPixelStorei(GL_PACK_ALIGNMENT,1);
            glReadBuffer(GL_BACK);
            glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,pixels);
            for(int y=0;y<h/2;y++) {
                unsigned char *a=pixels+(size_t)y*stride,*b=pixels+(size_t)(h-1-y)*stride;
                memcpy(row,a,stride); memcpy(a,b,stride); memcpy(b,row,stride);
            }
            SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels,w,h,24,(int)stride,SDL_PIXELFORMAT_RGB24);
            ok=surface&&SDL_SaveBMP(surface,g_launch.capture)==0;
            if(surface) SDL_FreeSurface(surface);
            glPixelStorei(GL_PACK_ALIGNMENT,4);
        }
        free(pixels); free(row);
        if(!ok) { fprintf(stderr,"[Capture] Failed: %s (%s)\n",g_launch.capture,SDL_GetError()); g_launch.exit_code=1; }
        else printf("[Capture] Saved %s (%dx%d)\n",g_launch.capture,w,h);
        char report[1100]; snprintf(report,sizeof(report),"%s.json",g_launch.capture);
        FILE *f=fopen(report,"w");
        if(f) {
            double elapsed=(double)(SDL_GetPerformanceCounter()-g_launch.started)/(double)SDL_GetPerformanceFrequency();
            fprintf(f,"{\n  \"project\": "); json_string(f,g_project.name);
            fprintf(f,",\n  \"renderer\": "); json_string(f,(const char*)glGetString(GL_RENDERER));
            fprintf(f,",\n  \"gl_version\": "); json_string(f,(const char*)glGetString(GL_VERSION));
            fprintf(f,",\n  \"view\": "); json_string(f,g_launch.view);
            fprintf(f,",\n  \"width\": %d, \"height\": %d, \"frames\": %d, \"elapsed_seconds\": %.4f,\n  \"offscreen\": %s, \"capture_ok\": %s\n}\n",
                w,h,g_launch.rendered_frames,elapsed,g_launch.headless?"true":"false",ok?"true":"false");
            fclose(f);
        }
    }
    g_launch.finished=true;
}

static unsigned read_u32(const unsigned char *p) {
    return (unsigned)p[0]|((unsigned)p[1]<<8)|((unsigned)p[2]<<16)|((unsigned)p[3]<<24);
}

bool engine_project_import_glb(const char *source,char *destination,size_t capacity,
                               char *message,size_t message_capacity) {
    if(destination&&capacity) destination[0]=0;
    if(!source||!source[0]) { snprintf(message,message_capacity,"Choose a GLB file first."); return false; }
    const char *ext=strrchr(source,'.');
    if(!ext||strcasecmp(ext,".glb")) {
        snprintf(message,message_capacity,"Import a self contained .glb. Existing FBX and glTF assets remain available in the library."); return false;
    }
    FILE *in=fopen(source,"rb");
    unsigned char header[20];
    struct stat st;
    if(!in||fstat(fileno(in),&st)||st.st_size<20||st.st_size>1024LL*1024*1024||
       fread(header,1,sizeof(header),in)!=sizeof(header)||memcmp(header,"glTF",4)||
       read_u32(header+4)!=2||read_u32(header+8)!=(unsigned)st.st_size||
       read_u32(header+16)!=0x4e4f534a||read_u32(header+12)>(unsigned)st.st_size-20) {
        if(in) fclose(in);
        snprintf(message,message_capacity,"File is not a valid GLB 2.0 container (maximum 1 GiB)."); return false;
    }
    const char *base=strrchr(source,'/'); base=base?base+1:source;
    char name[96]; size_t n=0;
    for(const char *s=base;s<ext&&n<sizeof(name)-1;s++) name[n++]=isalnum((unsigned char)*s)?*s:'_';
    name[n]=0; if(!n) snprintf(name,sizeof(name),"model");
    if((vw_mkdir("imports",0755)&&errno!=EEXIST)||(vw_mkdir("imports/models",0755)&&errno!=EEXIST)) {
        fclose(in); snprintf(message,message_capacity,"Cannot create project import directory."); return false;
    }
    char path[256]; int version;
    for(version=1;version<10000;version++) {
        if(version==1) snprintf(path,sizeof(path),"imports/models/%s.glb",name);
        else snprintf(path,sizeof(path),"imports/models/%s_%d.glb",name,version);
        if(access(path,F_OK)!=0) break;
    }
    if(version==10000||strlen(path)+1>capacity) {
        fclose(in); snprintf(message,message_capacity,"Import path is full."); return false;
    }
    char temp[280]; snprintf(temp,sizeof(temp),"%s.importing",path);
    FILE *out=fopen(temp,"wb");
    bool ok=out!=NULL;
    rewind(in); unsigned char buffer[65536]; size_t count;
    while(ok&&(count=fread(buffer,1,sizeof(buffer),in))>0) if(fwrite(buffer,1,count,out)!=count) ok=false;
    if(ferror(in)) ok=false;
    fclose(in);
    if(out&&fclose(out)) ok=false;
    if(ok&&vw_replace(temp,path)) ok=false;
    if(!ok) { unlink(temp); snprintf(message,message_capacity,"Could not copy imported model."); return false; }
    snprintf(destination,capacity,"%s",path);
    snprintf(message,message_capacity,"Imported %s. Select it in the library and place it in the viewport.",name);
    return true;
}
