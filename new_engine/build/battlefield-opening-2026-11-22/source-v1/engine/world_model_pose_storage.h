#ifndef VALWOOD_WORLD_MODEL_POSE_STORAGE_H
#define VALWOOD_WORLD_MODEL_POSE_STORAGE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint64_t vertex_bytes_saved,index_bytes_saved,index_hits,resident_index_buffers;} WorldModelPoseStorageStats;
WorldModelPoseStorageStats world_model_pose_storage_stats(void);
typedef struct {
    uint64_t pages,backing_bytes,resident_bytes,peak_bytes,budget_bytes;
    uint64_t uploads,hits,evictions,failures;
} WorldModelPoseStreamStats;
WorldModelPoseStreamStats world_model_pose_stream_stats(void);
#ifdef __cplusplus
}
#endif
#endif
