#ifndef VALWOOD_WORLD_HEIGHT_REGION_H
#define VALWOOD_WORLD_HEIGHT_REGION_H

#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Private, header-only immutable data service with explicit owner storage. This
 * keeps standalone atlas consumers independent of another link dependency.
 * Owner joins sampling workers before loading/clearing; queries never mutate.
 * File: 8 bytes VWHGT1\0\0, little-endian uint32 columns/rows, float64
 * xmin/zmin/stepx/stepz, then row-major float32 absolute world heights.
 * Exactly 48 header bytes; X varies fastest, both axes increase in world space.
 */
enum { WORLD_HEIGHT_REGION_SIDE_LIMIT=2049 };
typedef struct {
    uint32_t columns,rows;
    double xmin,zmin,stepx,stepz;
    float *heights;
} WorldHeightRegion;

static inline void world_height_region_clear(WorldHeightRegion *region) {
    if(region){free(region->heights);memset(region,0,sizeof(*region));}
}
static inline uint32_t world_height_region_u32(const unsigned char *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static inline double world_height_region_f64(const unsigned char *p) {
    uint64_t bits=(uint64_t)world_height_region_u32(p)|((uint64_t)world_height_region_u32(p+4)<<32);
    double result;memcpy(&result,&bits,sizeof(result));return result;
}
static inline int world_height_region_error(char *error,size_t capacity,const char *message) {
    if(error&&capacity)snprintf(error,capacity,"%s",message);
    return -1;
}
/* 1 loaded, 0 absent, -1 malformed/read/allocation error. Missing/failed loads
 * preserve every byte of the previously committed region and its samples. */
static inline int world_height_region_load(WorldHeightRegion *region,const char *path,char *error,size_t capacity) {
    if(error&&capacity)error[0]='\0';
    if(!region||!path||!path[0])return world_height_region_error(error,capacity,"Invalid height region path/destination");
    if(sizeof(float)!=4||sizeof(double)!=8||FLT_RADIX!=2||FLT_MANT_DIG!=24||DBL_MANT_DIG!=53)
        return world_height_region_error(error,capacity,"Height regions require IEEE float32/float64");
    FILE *file=fopen(path,"rb");
    if(!file){if(errno==ENOENT)return 0;return world_height_region_error(error,capacity,"Cannot open height region");}
    unsigned char header[48];
    if(fread(header,1,sizeof(header),file)!=sizeof(header)){
        fclose(file);return world_height_region_error(error,capacity,"Truncated height region header");
    }
    if(memcmp(header,"VWHGT1\0\0",8)!=0){
        fclose(file);return world_height_region_error(error,capacity,"Invalid height region magic/version");
    }
    WorldHeightRegion next;memset(&next,0,sizeof(next));
    next.columns=world_height_region_u32(header+8);next.rows=world_height_region_u32(header+12);
    next.xmin=world_height_region_f64(header+16);next.zmin=world_height_region_f64(header+24);
    next.stepx=world_height_region_f64(header+32);next.stepz=world_height_region_f64(header+40);
    const double xmax=next.xmin+(next.columns?next.columns-1:0)*next.stepx;
    const double zmax=next.zmin+(next.rows?next.rows-1:0)*next.stepz;
    if(next.columns<2||next.rows<2||next.columns>WORLD_HEIGHT_REGION_SIDE_LIMIT||next.rows>WORLD_HEIGHT_REGION_SIDE_LIMIT||
       !isfinite(next.xmin)||!isfinite(next.zmin)||!isfinite(next.stepx)||!isfinite(next.stepz)||
       next.stepx<=0||next.stepz<=0||!isfinite(xmax)||!isfinite(zmax)||xmax<=next.xmin||zmax<=next.zmin){
        fclose(file);return world_height_region_error(error,capacity,"Invalid height region dimensions/bounds/spacing");
    }
    const size_t count=(size_t)next.columns*next.rows;
    next.heights=(float*)malloc(count*sizeof(float));
    if(!next.heights){fclose(file);return world_height_region_error(error,capacity,"Height region allocation failed");}
    if(fread(next.heights,sizeof(float),count,file)!=count){
        fclose(file);world_height_region_clear(&next);return world_height_region_error(error,capacity,"Truncated height region samples");
    }
    const int trailing=fgetc(file),read_error=ferror(file),close_error=fclose(file);
    if(trailing!=EOF||read_error||close_error){
        world_height_region_clear(&next);return world_height_region_error(error,capacity,"Height region has trailing data/read failure");
    }
    for(size_t i=0;i<count;i++){
        const uint32_t bits=world_height_region_u32((const unsigned char*)next.heights+i*4);
        memcpy(&next.heights[i],&bits,sizeof(bits));
        if(!isfinite(next.heights[i])){
            world_height_region_clear(&next);return world_height_region_error(error,capacity,"Non-finite height region sample");
        }
    }
    world_height_region_clear(region);*region=next;return 1;
}
/* Closed bounds retain endpoint samples. The tiny world-coordinate tolerance
 * only absorbs atlas UV round trips (a few double ULPs), not a terrain feather.
 * On false, the caller's output remains unchanged. */
static inline bool world_height_region_sample(const WorldHeightRegion *region,double x,double z,float *height) {
    if(!region||!region->heights||!height||!isfinite(x)||!isfinite(z))return false;
    const double xmax=region->xmin+(region->columns-1)*region->stepx;
    const double zmax=region->zmin+(region->rows-1)*region->stepz;
    const double epsilon_x=fmin(region->stepx*.000001,8*DBL_EPSILON*fmax(1,fmax(fabs(region->xmin),fabs(xmax))));
    const double epsilon_z=fmin(region->stepz*.000001,8*DBL_EPSILON*fmax(1,fmax(fabs(region->zmin),fabs(zmax))));
    if(x<region->xmin-epsilon_x||x>xmax+epsilon_x||z<region->zmin-epsilon_z||z>zmax+epsilon_z)return false;
    double gx=fmax(0,fmin((double)region->columns-1,(x-region->xmin)/region->stepx));
    double gz=fmax(0,fmin((double)region->rows-1,(z-region->zmin)/region->stepz));
    const uint32_t ix=(uint32_t)gx,iz=(uint32_t)gz;
    const uint32_t jx=ix+1<region->columns?ix+1:ix,jz=iz+1<region->rows?iz+1:iz;
    const double tx=gx-ix,tz=gz-iz;
    const size_t a=(size_t)iz*region->columns+ix,b=(size_t)iz*region->columns+jx;
    const size_t c=(size_t)jz*region->columns+ix,d=(size_t)jz*region->columns+jx;
    const double near=region->heights[a]*(1-tx)+region->heights[b]*tx;
    const double far=region->heights[c]*(1-tx)+region->heights[d]*tx;
    *height=(float)(near*(1-tz)+far*tz);return true;
}

#endif
