#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "engine/performance.h"
#include <SDL2/SDL.h>
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

EnginePerfSample g_engine_perf;

#define PROFILE_INITIAL_SAMPLES 4096
#define PROFILE_WORST_FRAMES 10
#define PROFILE_METRICS(X) \
    X(update_ms) X(render_ms) X(editor_ms) X(shadow_gpu_ms) \
    X(terrain_gpu_ms) X(props_gpu_ms) X(grass_gpu_ms) X(postfx_gpu_ms) \
    X(raw_update_ms) X(raw_render_cpu_ms) X(raw_editor_ms) X(present_ms) \
    X(shadow_cpu_ms) X(render_total_gpu_ms) X(forest_gpu_ms) X(reflection_gpu_ms)

typedef struct {
    int frame;
    double ms;
    EnginePerfSample sample;
} ProfileWorstFrame;

typedef struct {
#define PROFILE_DOUBLE_FIELD(name) double name;
    PROFILE_METRICS(PROFILE_DOUBLE_FIELD)
#undef PROFILE_DOUBLE_FIELD
} ProfileTotals;

static struct {
    char path[1024];
    FILE *csv;
    /* The stream must close before profile_release clears its backing buffer. */
    char csv_buffer[64*1024];
    double *times;
    int count, capacity, frames, warmup, dropped_samples;
    int over_45, over_30, slow_streak, longest_slow_streak;
    Uint64 begin, last;
    double clock_ms, startup_ms, first_frame_monotonic_seconds;
    int gpu_stage_timing_available;
    ProfileTotals sum;
    ProfileWorstFrame worst[PROFILE_WORST_FRAMES];
    int worst_count;
} profile;

static int compare_double(const void *a,const void *b){
    double x=*(const double*)a,y=*(const double*)b;
    return (x>y)-(x<y);
}

static void json_text(FILE *f,const char *s){
    fputc('"',f);
    for(;s&&*s;s++){
        unsigned char c=(unsigned char)*s;
        if(c=='"'||c=='\\') fputc('\\',f);
        if(c>=32) fputc(c,f);
    }
    fputc('"',f);
}

static void profile_release(void){
    if(profile.csv) fclose(profile.csv);
    free(profile.times);
    memset(&profile,0,sizeof(profile));
}

void engine_perf_begin(const char *path,int warmup){
    profile_release();
    if(!path||!path[0]) return;
    snprintf(profile.path,sizeof(profile.path),"%s",path);
    profile.warmup=warmup>0?warmup:0;
    profile.gpu_stage_timing_available=1;
    profile.clock_ms=1000.0/(double)SDL_GetPerformanceFrequency();
    profile.begin=SDL_GetPerformanceCounter();
    /* Allocate before the measured loop, and keep logging off the syscall path
       for most frames. Growth is still supported for long-running soak tests. */
    profile.times=malloc(PROFILE_INITIAL_SAMPLES*sizeof(*profile.times));
    if(profile.times) profile.capacity=PROFILE_INITIAL_SAMPLES;
    char csv[1100];
    snprintf(csv,sizeof(csv),"%s.csv",path);
    profile.csv=fopen(csv,"w");
    if(profile.csv){
        /* A NULL buffer lets libc choose a smaller buffer despite this size. */
        setvbuf(profile.csv,profile.csv_buffer,_IOFBF,sizeof(profile.csv_buffer));
        fputs("frame,frame_ms,update_ms,render_cpu_ms,editor_ms,shadow_gpu_ms,terrain_gpu_ms,props_gpu_ms,grass_gpu_ms,postfx_gpu_ms,raw_update_ms,raw_render_cpu_ms,raw_editor_ms,present_ms,shadow_cpu_ms,render_total_gpu_ms,forest_gpu_ms,reflection_gpu_ms\n",profile.csv);
    }
}

void engine_perf_set_gpu_stage_timing_available(int available){
    profile.gpu_stage_timing_available=available!=0;
    if(!available){
        g_engine_perf.shadow_gpu_ms=g_engine_perf.terrain_gpu_ms=0;
        g_engine_perf.props_gpu_ms=g_engine_perf.grass_gpu_ms=0;
        g_engine_perf.postfx_gpu_ms=g_engine_perf.render_total_gpu_ms=0;
        g_engine_perf.forest_gpu_ms=g_engine_perf.reflection_gpu_ms=0;
    }
}

static void profile_record_worst(double ms){
    int at=profile.worst_count;
    if(at==PROFILE_WORST_FRAMES){
        if(ms<=profile.worst[at-1].ms) return;
        at--;
    }else profile.worst_count++;
    while(at>0&&profile.worst[at-1].ms<ms){
        profile.worst[at]=profile.worst[at-1];
        at--;
    }
    profile.worst[at]=(ProfileWorstFrame){profile.frames,ms,g_engine_perf};
}

void engine_perf_frame(void){
    if(!profile.path[0]) return;
    Uint64 now=SDL_GetPerformanceCounter();
    double ms=profile.last?(now-profile.last)*profile.clock_ms:0;
    profile.last=now;
    profile.frames++;
    if(profile.frames==1){
        profile.startup_ms=(now-profile.begin)*profile.clock_ms;
        struct timespec stamp;
        if(clock_gettime(CLOCK_MONOTONIC,&stamp)==0)
            profile.first_frame_monotonic_seconds=(double)stamp.tv_sec+(double)stamp.tv_nsec*1e-9;
    }
    if(profile.csv){
        fprintf(profile.csv,"%d,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f\n",
                profile.frames,ms,g_engine_perf.update_ms,g_engine_perf.render_ms,
                g_engine_perf.editor_ms,g_engine_perf.shadow_gpu_ms,
                g_engine_perf.terrain_gpu_ms,g_engine_perf.props_gpu_ms,
                g_engine_perf.grass_gpu_ms,g_engine_perf.postfx_gpu_ms,
                g_engine_perf.raw_update_ms,g_engine_perf.raw_render_cpu_ms,
                g_engine_perf.raw_editor_ms,g_engine_perf.present_ms,
                g_engine_perf.shadow_cpu_ms,g_engine_perf.render_total_gpu_ms,
                g_engine_perf.forest_gpu_ms,g_engine_perf.reflection_gpu_ms);
    }
    if(profile.frames<=profile.warmup||!isfinite(ms)||ms<=0) return;
    if(profile.count==profile.capacity){
        int cap=profile.capacity?profile.capacity*2:PROFILE_INITIAL_SAMPLES;
        double *p=realloc(profile.times,(size_t)cap*sizeof(*p));
        if(!p){profile.dropped_samples++;return;}
        profile.times=p;
        profile.capacity=cap;
    }
    profile.times[profile.count++]=ms;
#define PROFILE_ADD(name) profile.sum.name+=(double)g_engine_perf.name;
    PROFILE_METRICS(PROFILE_ADD)
#undef PROFILE_ADD
    if(ms>1000.0/45.0){
        profile.over_45++;
        profile.slow_streak++;
        if(profile.slow_streak>profile.longest_slow_streak)
            profile.longest_slow_streak=profile.slow_streak;
    }else profile.slow_streak=0;
    if(ms>1000.0/30.0) profile.over_30++;
    profile_record_worst(ms);
}

/* Nearest-rank percentiles keep the upper-tail samples in short runs. */
static double profile_percentile(double fraction){
    if(!profile.count) return 0;
    int at=(int)ceil(fraction*profile.count)-1;
    if(at<0) at=0;
    if(at>=profile.count) at=profile.count-1;
    return profile.times[at];
}

void engine_perf_finish(void){
    if(!profile.path[0]) return;
    if(profile.csv){fclose(profile.csv);profile.csv=NULL;}
    FILE *f=fopen(profile.path,"w");
    if(f){
        double sum=0;
        for(int i=0;i<profile.count;i++) sum+=profile.times[i];
        if(profile.count) qsort(profile.times,(size_t)profile.count,sizeof(*profile.times),compare_double);
        int n=profile.count;
        double mean=n?sum/n:0, inv=n?1.0/n:0;
        int low_count=n?(int)ceil(n*.01):0;
        double low_ms=0;
        for(int i=n-low_count;i<n;i++) low_ms+=profile.times[i];
        if(low_count) low_ms/=low_count;
        fputs("{\n  \"profile_version\": 2,\n  \"renderer\": ",f);
        json_text(f,(const char*)glGetString(GL_RENDERER));
        fprintf(f,",\n  \"first_frame_monotonic_seconds\": %.9f,\n  \"gpu_stage_timing_available\": %s",
                profile.first_frame_monotonic_seconds,profile.gpu_stage_timing_available?"true":"false");
        fprintf(f,",\n  \"frames\": %d, \"warmup_frames\": %d, \"samples\": %d, \"dropped_samples\": %d,\n"
                "  \"startup_ms\": %.3f, \"duration_seconds\": %.3f, \"mean_ms\": %.3f, \"mean_fps\": %.2f,\n"
                "  \"median_ms\": %.3f, \"p95_ms\": %.3f, \"p99_ms\": %.3f, \"max_ms\": %.3f,\n"
                "  \"one_percent_low_fps\": %.2f, \"frames_over_45fps_budget\": %d, \"frames_over_30fps_budget\": %d,\n"
                "  \"within_45fps_budget_percent\": %.3f, \"longest_over_45fps_streak\": %d,\n",
                profile.frames,profile.warmup,n,profile.dropped_samples,
                profile.startup_ms,sum*.001,mean,mean>0?1000.0/mean:0,
                profile_percentile(.5),profile_percentile(.95),profile_percentile(.99),
                n?profile.times[n-1]:0,low_ms>0?1000.0/low_ms:0,
                profile.over_45,profile.over_30,n?100.0*(n-profile.over_45)/n:0,
                profile.longest_slow_streak);
        fprintf(f,"  \"mean_update_ms\": %.3f, \"mean_render_cpu_ms\": %.3f, \"mean_editor_ms\": %.3f,\n"
                "  \"mean_shadow_gpu_ms\": %.3f, \"mean_terrain_gpu_ms\": %.3f, \"mean_props_gpu_ms\": %.3f, \"mean_grass_gpu_ms\": %.3f, \"mean_postfx_gpu_ms\": %.3f,\n",
                profile.sum.update_ms*inv,profile.sum.render_ms*inv,profile.sum.editor_ms*inv,
                profile.sum.shadow_gpu_ms*inv,profile.sum.terrain_gpu_ms*inv,
                profile.sum.props_gpu_ms*inv,profile.sum.grass_gpu_ms*inv,profile.sum.postfx_gpu_ms*inv);
        fprintf(f,"  \"mean_raw_update_ms\": %.3f, \"mean_raw_render_cpu_ms\": %.3f, \"mean_raw_editor_ms\": %.3f,\n"
                "  \"mean_present_ms\": %.3f, \"mean_shadow_cpu_ms\": %.3f,\n"
                "  \"mean_render_total_gpu_ms\": %.3f, \"mean_forest_gpu_ms\": %.3f, \"mean_reflection_gpu_ms\": %.3f,\n",
                profile.sum.raw_update_ms*inv,profile.sum.raw_render_cpu_ms*inv,
                profile.sum.raw_editor_ms*inv,profile.sum.present_ms*inv,
                profile.sum.shadow_cpu_ms*inv,profile.sum.render_total_gpu_ms*inv,
                profile.sum.forest_gpu_ms*inv,profile.sum.reflection_gpu_ms*inv);
        fputs("  \"stage_timing_note\": \"GPU stages are delayed estimates; stage snapshots are not exact per-frame attribution. GPU stage fields are zero when gpu_stage_timing_available is false. present_ms is the preceding swap duration. Legacy update/render CPU fields are smoothed; raw_* CPU fields are unsmoothed. first_frame_monotonic_seconds timestamps CSV frame 1; later timestamps add subsequent frame_ms intervals.\",\n",f);
        fprintf(f,"  \"static_assets\": %u, \"static_instances\": %u, \"cache_hits\": %u, \"cache_misses\": %u, \"static_gpu_mib\": %.3f,\n",
                g_engine_perf.static_assets,g_engine_perf.static_instances,
                g_engine_perf.cache_hits,g_engine_perf.cache_misses,
                (double)g_engine_perf.static_gpu_bytes/(1024*1024));
        fputs("  \"worst_frames\": [",f);
        for(int i=0;i<profile.worst_count;i++){
            const ProfileWorstFrame *w=&profile.worst[i];
            fprintf(f,"%s\n    {\"frame\": %d, \"frame_ms\": %.3f, \"update_ms\": %.3f, \"render_cpu_ms\": %.3f, \"editor_ms\": %.3f, \"raw_update_ms\": %.3f, \"raw_render_cpu_ms\": %.3f, \"raw_editor_ms\": %.3f, \"present_ms\": %.3f, \"shadow_cpu_ms\": %.3f}",
                    i?",":"",w->frame,w->ms,w->sample.update_ms,
                    w->sample.render_ms,w->sample.editor_ms,
                    w->sample.raw_update_ms,w->sample.raw_render_cpu_ms,
                    w->sample.raw_editor_ms,w->sample.present_ms,w->sample.shadow_cpu_ms);
        }
        fputs("\n  ]\n}\n",f);
        fclose(f);
        printf("[Profile] %s: %.2f FPS, p95 %.2f ms, %.2f%% within 45 FPS budget (%d samples)\n",
               profile.path,mean>0?1000.0/mean:0,profile_percentile(.95),
               n?100.0*(n-profile.over_45)/n:0,n);
    }
    profile_release();
}
