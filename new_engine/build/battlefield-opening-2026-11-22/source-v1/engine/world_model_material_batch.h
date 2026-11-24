#ifndef VALWOOD_WORLD_MODEL_MATERIAL_BATCH_H
#define VALWOOD_WORLD_MODEL_MATERIAL_BATCH_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void world_model_material_batch_begin(void);
void world_model_material_batch_end(void);
uint64_t world_model_material_batch_hits(void);
#ifdef __cplusplus
}
// Only synchronous loads on the GL owner thread may share this scope. The
// material/configuration context cannot change until the loading call returns.
struct WorldModelMaterialBatch {
    WorldModelMaterialBatch(){world_model_material_batch_begin();}
    ~WorldModelMaterialBatch(){world_model_material_batch_end();}
    WorldModelMaterialBatch(const WorldModelMaterialBatch&)=delete;
    WorldModelMaterialBatch& operator=(const WorldModelMaterialBatch&)=delete;
};
#endif
#endif
