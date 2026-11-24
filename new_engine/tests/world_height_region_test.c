#define _POSIX_C_SOURCE 200809L
#include "engine/world_atlas.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void u32(FILE *file,uint32_t value){
    unsigned char bytes[4];for(unsigned i=0;i<4;i++)bytes[i]=(unsigned char)(value>>(8*i));
    assert(fwrite(bytes,1,4,file)==4);
}
static void f64(FILE *file,double value){
    uint64_t bits;memcpy(&bits,&value,8);u32(file,(uint32_t)bits);u32(file,(uint32_t)(bits>>32));
}
static void fixture(const char *path,uint32_t columns,uint32_t rows,double xmin,double zmin,
                    double stepx,double stepz,const float *values,size_t count){
    FILE *file=fopen(path,"wb");assert(file);
    assert(fwrite("VWHGT1\0\0",1,8,file)==8);u32(file,columns);u32(file,rows);
    f64(file,xmin);f64(file,zmin);f64(file,stepx);f64(file,stepz);
    for(size_t i=0;i<count;i++){uint32_t bits;memcpy(&bits,values+i,4);u32(file,bits);}
    assert(fclose(file)==0);
}
static void close_enough(float actual,double expected){assert(fabs(actual-expected)<.0005);}
static void retained(double x,double z){
    WorldAtlasHeightRegionInfo info;assert(world_atlas_height_region_info(&info));
    assert(info.columns==3&&info.rows==3&&info.bytes==9*sizeof(float));
    close_enough(world_atlas_base_height(x,z),104.25);
    close_enough(world_atlas_sample(x,z).height,104.25);
}
static void rejected(const char *path,char *error,size_t capacity,double x,double z){
    assert(world_atlas_load_height_region(path,error,capacity)==-1&&error[0]);retained(x,z);
}
static float studio_delta(double x,double z,const void *data){(void)x;(void)z;return *(const float*)data;}

int main(void){
    char path[]="/tmp/valwood-height-region-test.XXXXXX";int fd=mkstemp(path);assert(fd>=0);assert(close(fd)==0);
    char surrounding[]="/tmp/valwood-surrounding-height-test.XXXXXX";fd=mkstemp(surrounding);assert(fd>=0);assert(close(fd)==0);
    char missing[sizeof(path)+16],error[128];snprintf(missing,sizeof(missing),"%s.missing",path);
    const double xmin=-43344.25,zmin=-23988.75,sx=2,sz=4;
    const double x=xmin+.25*sx,z=zmin+.5*sz;
    const float values[9]={100,103,106,107,110,113,114,117,120};
    fixture(path,3,3,xmin,zmin,sx,sz,values,9);
    assert(world_atlas_load_height_region(path,error,sizeof(error))==-1&&error[0]);
    assert(world_atlas_load_fallback_height_region(path,error,sizeof(error))==-1&&error[0]);
    assert(!world_atlas_height_region_info(NULL));assert(world_atlas_init());
    const double outside_points[4][2]={{xmin-.01,zmin+2},{xmin+4.01,zmin+2},{xmin+1,zmin-.01},{xmin+1,zmin+8.01}};
    float outside[4];for(unsigned i=0;i<4;i++)outside[i]=world_atlas_height(outside_points[i][0],outside_points[i][1]);
    const float original=world_atlas_base_height(x,z);
    WorldAtlasSample prior=world_atlas_sample(x,z);
    assert(world_atlas_load_height_region(path,error,sizeof(error))==1&&!error[0]);
    WorldAtlasHeightRegionInfo info;assert(world_atlas_height_region_info(&info));
    assert(info.columns==3&&info.rows==3&&info.xmin==xmin&&info.zmin==zmin&&info.stepx==sx&&info.stepz==sz);
    assert(world_atlas_height_region_info(NULL));
    for(unsigned iz=0;iz<3;iz++)for(unsigned ix=0;ix<3;ix++){
        const double wx=xmin+ix*sx,wz=zmin+iz*sz;double u,v;world_atlas_world_to_uv(wx,wz,&u,&v);
        close_enough(world_atlas_base_height(wx,wz),values[iz*3+ix]);
        close_enough(world_atlas_sample(wx,wz).height,values[iz*3+ix]);
        close_enough(world_atlas_sample_uv(u,v).height,values[iz*3+ix]);
    }
    retained(x,z);close_enough(world_atlas_walk_height(x,z),104.25);
    close_enough(world_atlas_landscape_sample(x,z).height,104.25);
    for(unsigned i=0;i<4;i++)close_enough(world_atlas_height(outside_points[i][0],outside_points[i][1]),outside[i]);
    assert(world_atlas_height(NAN,z)==-85&&world_atlas_height(x,INFINITY)==-85);
    // Both axes are measured in metres, including anisotropic source spacing.
    close_enough((world_atlas_height(x+.25,z)-world_atlas_height(x-.25,z))/.5,1.5);
    close_enough((world_atlas_height(x,z+.25)-world_atlas_height(x,z-.25))/.5,1.75);
    WorldAtlasSample after=world_atlas_sample(x,z);
    assert(after.region==prior.region&&after.shore_distance==prior.shore_distance&&after.moisture==prior.moisture);
    const float delta=4;world_atlas_set_height_edit(studio_delta,&delta);
    close_enough(world_atlas_base_height(x,z),104.25);close_enough(world_atlas_height(x,z),108.25);
    close_enough(world_atlas_sample_uv(x/world_atlas_width()+.5,z/world_atlas_depth()+.5).height,108.25);
    world_atlas_set_height_edit(NULL,NULL);
    // A coarser surrounding landscape must never resample/overwrite original
    // source corners or bilinear heights. Only uncovered points use fallback.
    const float country[9]={220,220,220,220,220,220,220,220,220};
    fixture(surrounding,3,3,xmin-sx,zmin-sz,sx*2,sz*2,country,9);
    assert(world_atlas_load_fallback_height_region(surrounding,error,sizeof(error))==1);
    assert(world_atlas_fallback_height_region_info(&info)&&info.columns==3&&info.stepx==sx*2);
    retained(x,z);close_enough(world_atlas_height(xmin,zmin),100);
    close_enough(world_atlas_base_height(xmin-.5,zmin),220);
    close_enough(world_atlas_sample(xmin-.5,zmin).height,220);
    close_enough(world_atlas_walk_height(xmin-.5,zmin),220);
    world_atlas_set_height_edit(studio_delta,&delta);
    close_enough(world_atlas_height(x,z),108.25);
    close_enough(world_atlas_walk_height(xmin-.5,zmin),224);
    world_atlas_set_height_edit(NULL,NULL);
    assert(world_atlas_load_fallback_height_region(missing,error,sizeof(error))==0);
    close_enough(world_atlas_height(xmin-.5,zmin),220);
    fixture(surrounding,3,3,xmin-sx,zmin-sz,sx*2,sz*2,country,8);
    assert(world_atlas_load_fallback_height_region(surrounding,error,sizeof(error))==-1&&error[0]);
    close_enough(world_atlas_height(xmin-.5,zmin),220);retained(x,z);
    world_atlas_clear_height_region();close_enough(world_atlas_height(x,z),220);
    assert(world_atlas_load_height_region(path,error,sizeof(error))==1);retained(x,z);
    world_atlas_clear_fallback_height_region();assert(!world_atlas_fallback_height_region_info(&info));
    for(unsigned i=0;i<4;i++)close_enough(world_atlas_height(outside_points[i][0],outside_points[i][1]),outside[i]);
    strcpy(error,"old error");assert(world_atlas_load_height_region(missing,error,sizeof(error))==0&&!error[0]);retained(x,z);
    rejected(NULL,error,sizeof(error),x,z);
    rejected("/dev/null/invalid-height-region",error,sizeof(error),x,z);
    // Every malformed case must leave the valid committed terrain untouched.
    FILE *file=fopen(path,"wb");assert(file);assert(fwrite("VWHG",1,4,file)==4);assert(fclose(file)==0);
    rejected(path,error,sizeof(error),x,z);
    fixture(path,3,3,xmin,zmin,sx,sz,values,8);rejected(path,error,sizeof(error),x,z);
    fixture(path,3,3,xmin,zmin,sx,sz,values,9);
    file=fopen(path,"r+b");assert(file);assert(fputc('X',file)!=EOF);assert(fclose(file)==0);rejected(path,error,sizeof(error),x,z);
    fixture(path,3,3,xmin,zmin,sx,sz,values,9);
    file=fopen(path,"ab");assert(file);assert(fputc(0,file)!=EOF);assert(fclose(file)==0);rejected(path,error,sizeof(error),x,z);
    float nonfinite[9];memcpy(nonfinite,values,sizeof(values));nonfinite[4]=NAN;
    fixture(path,3,3,xmin,zmin,sx,sz,nonfinite,9);rejected(path,error,sizeof(error),x,z);
    nonfinite[4]=INFINITY;fixture(path,3,3,xmin,zmin,sx,sz,nonfinite,9);rejected(path,error,sizeof(error),x,z);
    fixture(path,1,3,xmin,zmin,sx,sz,values,0);rejected(path,error,sizeof(error),x,z);
    fixture(path,2050,3,xmin,zmin,sx,sz,values,0);rejected(path,error,sizeof(error),x,z);
    fixture(path,3,2050,xmin,zmin,sx,sz,values,0);rejected(path,error,sizeof(error),x,z);
    fixture(path,UINT32_MAX,UINT32_MAX,xmin,zmin,sx,sz,values,0);rejected(path,error,sizeof(error),x,z);
    fixture(path,3,3,xmin,zmin,0,sz,values,0);rejected(path,error,sizeof(error),x,z);
    fixture(path,3,3,xmin,zmin,sx,-1,values,0);rejected(path,error,sizeof(error),x,z);
    fixture(path,3,3,NAN,zmin,sx,sz,values,0);rejected(path,error,sizeof(error),x,z);
    fixture(path,3,3,xmin,zmin,sx,INFINITY,values,0);rejected(path,error,sizeof(error),x,z);
    fixture(path,3,3,DBL_MAX,zmin,DBL_MAX,sz,values,0);rejected(path,error,sizeof(error),x,z);
    char tiny[1]={'x'};assert(world_atlas_load_height_region(path,tiny,sizeof(tiny))==-1&&tiny[0]=='\0');retained(x,z);
    // Accept the maximum supported side without allocating an oversized square.
    float *wide=malloc(2049*2*sizeof(float));assert(wide);for(unsigned i=0;i<2049*2;i++)wide[i]=(float)(i%2049)*.25f;
    fixture(path,2049,2,xmin,zmin,1,1,wide,2049*2);free(wide);
    assert(world_atlas_load_height_region(path,error,sizeof(error))==1);
    assert(world_atlas_height_region_info(&info)&&info.columns==2049&&info.rows==2);
    close_enough(world_atlas_height(xmin+2048,zmin+1),512);
    // A publisher ground can cover the atlas village's former bridge. Its
    // source height must also win in walking queries, while Studio edits stay
    // above the source base. Clearing it restores the legacy bridge surface.
    double village_x,village_z;world_atlas_village_origin(&village_x,&village_z);
    const double bridge_x=village_x,bridge_z=village_z+219;
    world_atlas_clear_height_region();
    const float old_bridge=world_atlas_walk_height(bridge_x,bridge_z);
    const float city_values[4]={48,48,48,48};
    fixture(path,2,2,bridge_x-1,bridge_z-1,2,2,city_values,4);
    assert(world_atlas_load_height_region(path,error,sizeof(error))==1);
    close_enough(world_atlas_walk_height(bridge_x,bridge_z),48);
    world_atlas_set_height_edit(studio_delta,&delta);
    close_enough(world_atlas_walk_height(bridge_x,bridge_z),52);
    world_atlas_set_height_edit(NULL,NULL);world_atlas_clear_height_region();
    close_enough(world_atlas_walk_height(bridge_x,bridge_z),old_bridge);
    fixture(surrounding,2,2,bridge_x-1,bridge_z-1,2,2,city_values,4);
    assert(world_atlas_load_fallback_height_region(surrounding,error,sizeof(error))==1);
    close_enough(world_atlas_walk_height(bridge_x,bridge_z),48);
    world_atlas_clear_fallback_height_region();close_enough(world_atlas_walk_height(bridge_x,bridge_z),old_bridge);
    world_atlas_clear_height_region();assert(!world_atlas_height_region_info(&info)&&info.columns==0&&info.bytes==0);
    close_enough(world_atlas_base_height(x,z),original);world_atlas_clear_height_region();
    fixture(path,3,3,xmin,zmin,sx,sz,values,9);assert(world_atlas_load_height_region(path,error,sizeof(error))==1);
    assert(world_atlas_load_fallback_height_region(surrounding,error,sizeof(error))==1);
    world_atlas_shutdown();assert(!world_atlas_height_region_info(NULL)&&!world_atlas_fallback_height_region_info(NULL));
    assert(world_atlas_init());close_enough(world_atlas_base_height(x,z),original);world_atlas_shutdown();
    assert(unlink(path)==0&&unlink(surrounding)==0);
    puts("PASS height region: exact source priority over coarse countryside, walking/Studio order, fallback missing/malformed atomicity and clearing, little-endian heights, corners/UV paths, metre slopes, outside preservation, 2049-side bound and shutdown reset");
    return 0;
}
