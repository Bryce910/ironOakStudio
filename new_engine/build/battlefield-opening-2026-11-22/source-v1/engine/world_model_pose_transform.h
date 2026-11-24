#ifndef WORLD_MODEL_POSE_TRANSFORM_H
#define WORLD_MODEL_POSE_TRANSFORM_H
#include "engine/world_models.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Loading-time opt-in for ordinary, undeformed pose pairs. Subsequent uploads
   honor instance pitch, roll and axis scale. Combat deformation/anatomy streams
   are mutually exclusive; existing batches keep their established behavior. */
bool world_model_pose_blend_enable_transform(WorldModelPoseBlend *batch);
#ifdef __cplusplus
}
#endif
#endif
