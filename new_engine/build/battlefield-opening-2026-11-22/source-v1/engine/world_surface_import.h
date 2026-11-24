#ifndef VALWOOD_WORLD_SURFACE_IMPORT_H
#define VALWOOD_WORLD_SURFACE_IMPORT_H
#include "engine/world_surface_authoring.h"
namespace world_surface {
struct ImportResult {bool ok=false;Image image;unsigned width=0,height=0;std::string error;};
// Validates and copies a content-addressed immutable source. Does not execute
// scripts, resample purchased originals, mutate a document or allocate GL.
ImportResult import_image(const std::string &project,const std::string &source,ImageRole);
}
#endif
