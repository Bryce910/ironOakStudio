#ifndef VALWOOD_PERFORMANCE_H
#define VALWOOD_PERFORMANCE_H
#include <stddef.h>
typedef struct {
    float update_ms,render_ms,editor_ms,shadow_gpu_ms,terrain_gpu_ms,props_gpu_ms,grass_gpu_ms,postfx_gpu_ms;
    /* Raw CPU durations identify spikes hidden by debugger smoothing.
       present_ms is the preceding SDL_GL_SwapWindow call. GPU values remain
       asynchronous smoothed estimates, including the extra stages below. */
    float raw_update_ms,raw_render_cpu_ms,raw_editor_ms,present_ms,shadow_cpu_ms;
    float render_total_gpu_ms,forest_gpu_ms,reflection_gpu_ms;
    unsigned static_assets,static_instances,cache_hits,cache_misses;
    size_t static_gpu_bytes;
} EnginePerfSample;
extern EnginePerfSample g_engine_perf;
void engine_perf_begin(const char *path,int warmup);
/* Set before the first frame; unavailable GPU stages remain zero in CSV/JSON. */
void engine_perf_set_gpu_stage_timing_available(int available);
void engine_perf_frame(void);
void engine_perf_finish(void);
#endif
