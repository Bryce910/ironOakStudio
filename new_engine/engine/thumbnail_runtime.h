#ifndef VALWOOD_THUMBNAIL_RUNTIME_H
#define VALWOOD_THUMBNAIL_RUNTIME_H
#include <stdint.h>
#include <stdbool.h>
typedef struct{unsigned generation;int index,width,height;uint8_t *pixels;}ThumbnailResult;
unsigned thumbnail_new_generation(void);
bool thumbnail_request(unsigned generation,int index,const char *path);
bool thumbnail_poll(ThumbnailResult *result);
void thumbnail_shutdown(void);
#endif
