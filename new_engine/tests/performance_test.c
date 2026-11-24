/* Deterministic clock/renderer stubs exercise profiling without a GL context. */
#define _POSIX_C_SOURCE 200809L
#include "engine/performance.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static Uint64 test_clock=1000000;
static unsigned monotonic_reads;
Uint64 SDL_GetPerformanceCounter(void){return test_clock;}
Uint64 SDL_GetPerformanceFrequency(void){return 1000000;}
int clock_gettime(clockid_t clock_id,struct timespec *stamp){
    assert(clock_id==CLOCK_MONOTONIC);
    monotonic_reads++;
    stamp->tv_sec=7000+(time_t)(test_clock/1000000);
    stamp->tv_nsec=(long)(test_clock%1000000)*1000;
    return 0;
}
const GLubyte *glGetString(GLenum name){
    assert(name==GL_RENDERER);
    return (const GLubyte*)"Test \"renderer\"";
}

static void frame(double ms){
    test_clock+=(Uint64)llround(ms*1000.0);
    engine_perf_frame();
}

static char *read_file(const char *path){
    FILE *f=fopen(path,"rb");assert(f);
    assert(fseek(f,0,SEEK_END)==0);
    long length=ftell(f);assert(length>=0);
    rewind(f);
    char *text=malloc((size_t)length+1);assert(text);
    assert(fread(text,1,(size_t)length,f)==(size_t)length);
    text[length]=0;fclose(f);return text;
}

static double number(const char *json,const char *key){
    char needle[128];snprintf(needle,sizeof(needle),"\"%s\":",key);
    const char *at=strstr(json,needle);assert(at);
    return strtod(at+strlen(needle),NULL);
}

static void expect(const char *json,const char *key,double value){
    assert(fabs(number(json,key)-value)<.0011);
}

static void remove_profile(const char *path){
    char csv[256];snprintf(csv,sizeof(csv),"%s.csv",path);
    unlink(path);unlink(csv);
}

int main(void){
    char path[192],csv_path[256];
    snprintf(path,sizeof(path),"/tmp/valwood-performance-test-%ld.json",(long)getpid());
    snprintf(csv_path,sizeof(csv_path),"%s.csv",path);
    g_engine_perf=(EnginePerfSample){.update_ms=1.25f,.render_ms=2.5f,
        .editor_ms=.5f,.shadow_gpu_ms=3,.terrain_gpu_ms=4,.grass_gpu_ms=1,
        .raw_update_ms=1.5f,.raw_render_cpu_ms=2.75f,.raw_editor_ms=.75f,
        .present_ms=7,.shadow_cpu_ms=1,.render_total_gpu_ms=12,
        .forest_gpu_ms=2,.reflection_gpu_ms=.25f,
        .static_assets=7,.static_instances=42,.static_gpu_bytes=1024*1024};
    engine_perf_begin(path,2);
    assert(monotonic_reads==0);
    frame(100);frame(16);
    assert(monotonic_reads==1);
    const double samples[]={10,20,23,40,60,15,30,50,16,17};
    for(size_t i=0;i<sizeof(samples)/sizeof(samples[0]);i++) frame(samples[i]);
    engine_perf_finish();
    char *json=read_file(path);
    expect(json,"profile_version",2);expect(json,"frames",12);
    expect(json,"first_frame_monotonic_seconds",7001.1);
    assert(strstr(json,"\"gpu_stage_timing_available\": true"));
    assert(monotonic_reads==1);
    expect(json,"samples",10);expect(json,"warmup_frames",2);
    expect(json,"startup_ms",100);expect(json,"duration_seconds",.281);
    expect(json,"mean_ms",28.1);expect(json,"median_ms",20);
    expect(json,"p95_ms",60);expect(json,"p99_ms",60);expect(json,"max_ms",60);
    expect(json,"one_percent_low_fps",16.67);
    expect(json,"frames_over_45fps_budget",5);expect(json,"frames_over_30fps_budget",3);
    expect(json,"within_45fps_budget_percent",50);expect(json,"longest_over_45fps_streak",3);
    expect(json,"dropped_samples",0);expect(json,"mean_update_ms",1.25);
    expect(json,"mean_render_cpu_ms",2.5);expect(json,"static_gpu_mib",1);
    expect(json,"mean_raw_update_ms",1.5);expect(json,"mean_raw_render_cpu_ms",2.75);
    expect(json,"mean_raw_editor_ms",.75);expect(json,"mean_present_ms",7);
    expect(json,"mean_shadow_cpu_ms",1);expect(json,"mean_render_total_gpu_ms",12);
    expect(json,"mean_forest_gpu_ms",2);expect(json,"mean_reflection_gpu_ms",.25);
    assert(strstr(json,"Test \\\"renderer\\\""));
    assert(strstr(json,"{\"frame\": 7, \"frame_ms\": 60.000"));
    free(json);
    char *csv=read_file(csv_path);int lines=0;
    for(char *p=csv;*p;p++) if(*p=='\n') lines++;
    assert(lines==13);free(csv);

    /* Short captures stay in the requested buffer; a restart flushes every row
       before clearing the profile storage that owns that buffer. */
    engine_perf_begin(path,0);
    for(int i=0;i<200;i++) frame(16);
    struct stat csv_stat;
    assert(stat(csv_path,&csv_stat)==0);
    assert(csv_stat.st_size==0);
    engine_perf_begin(NULL,0);
    csv=read_file(csv_path);lines=0;
    for(char *p=csv;*p;p++) if(*p=='\n') lines++;
    assert(lines==201);free(csv);

    /* Cross several allocation capacities and retain every measured frame. */
    engine_perf_begin(path,1);frame(10);
    for(int i=0;i<10000;i++) frame(i%2?24:16);
    engine_perf_finish();json=read_file(path);
    expect(json,"frames",10001);expect(json,"samples",10000);
    expect(json,"mean_ms",20);expect(json,"p99_ms",24);
    expect(json,"frames_over_45fps_budget",5000);expect(json,"longest_over_45fps_streak",1);
    expect(json,"dropped_samples",0);free(json);
    csv=read_file(csv_path);lines=0;
    for(char *p=csv;*p;p++) if(*p=='\n') lines++;
    assert(lines==10002);free(csv);

    /* Frame-only diagnostics retain CPU/frame statistics, identify absent GPU
       measurements explicitly, and take just one monotonic timestamp. */
    engine_perf_begin(path,1);
    engine_perf_set_gpu_stage_timing_available(0);
    assert(g_engine_perf.raw_update_ms==1.5f&&g_engine_perf.present_ms==7);
    const unsigned reads_before=monotonic_reads;
    const double first_stamp=7000.0+(double)(test_clock+10000)/1000000.0;
    frame(10);frame(16);frame(24);
    assert(monotonic_reads==reads_before+1);
    engine_perf_finish();json=read_file(path);
    expect(json,"samples",2);expect(json,"mean_ms",20);
    expect(json,"frames_over_45fps_budget",1);
    expect(json,"first_frame_monotonic_seconds",first_stamp);
    expect(json,"mean_raw_update_ms",1.5);expect(json,"mean_present_ms",7);
    const char *gpu_keys[]={"mean_shadow_gpu_ms","mean_terrain_gpu_ms",
        "mean_props_gpu_ms","mean_grass_gpu_ms","mean_postfx_gpu_ms",
        "mean_render_total_gpu_ms","mean_forest_gpu_ms","mean_reflection_gpu_ms"};
    for(size_t i=0;i<sizeof(gpu_keys)/sizeof(*gpu_keys);i++)expect(json,gpu_keys[i],0);
    assert(strstr(json,"\"gpu_stage_timing_available\": false"));
    free(json);
    csv=read_file(csv_path);
    assert(strstr(csv,"shadow_gpu_ms,terrain_gpu_ms,props_gpu_ms,grass_gpu_ms,postfx_gpu_ms"));
    assert(strstr(csv,",0.00000,0.00000,0.00000,0.00000,0.00000,"));
    free(csv);

    /* Empty profiles and repeated begin/finish do not leak or emit NaNs. */
    engine_perf_begin(path,-1);engine_perf_begin(path,0);engine_perf_finish();
    json=read_file(path);expect(json,"samples",0);expect(json,"p99_ms",0);
    expect(json,"first_frame_monotonic_seconds",0);
    assert(strstr(json,"\"gpu_stage_timing_available\": true"));
    assert(!strstr(json,"nan")&&!strstr(json,"inf"));free(json);
    engine_perf_finish();engine_perf_begin(NULL,0);engine_perf_frame();engine_perf_finish();
    remove_profile(path);
    puts("PASS performance: warmup, exact tail/budget statistics, worst-frame IDs, long-run growth, frame-only GPU availability, one-shot monotonic alignment, CSV and profile lifecycle");
    return 0;
}
