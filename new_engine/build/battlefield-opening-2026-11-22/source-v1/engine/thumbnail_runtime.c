/* CPU decoding only. The owning UI thread creates and destroys GL textures. */
#include "engine/thumbnail_runtime.h"
#include "cgltf.h"
#include "stb_image.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#define THUMB_REQUESTS 512
#define THUMB_RESULTS 8
typedef struct{unsigned generation;int index;char path[512];}ThumbnailRequest;
static struct{SDL_Thread *thread;SDL_mutex *mutex;SDL_cond *wake;bool stop;unsigned generation;ThumbnailRequest requests[THUMB_REQUESTS];int head,count;ThumbnailResult results[THUMB_RESULTS];int result_head,result_count;} worker;
static uint8_t *thumbnail_decode(const char *path,int *w,int *h){
    char candidate[512];snprintf(candidate,sizeof(candidate),"%s",path);char *dot=strrchr(candidate,'.');int channels;
    if(dot){const char *ext[]={".png",".jpg"};for(int i=0;i<2;i++){snprintf(dot,(size_t)(candidate+sizeof(candidate)-dot),"%s",ext[i]);uint8_t *px=stbi_load(candidate,w,h,&channels,4);if(px)return px;}}
    dot=strrchr(path,'.');if(!dot||strcasecmp(dot,".glb"))return NULL;
    struct stat info;if(stat(path,&info)||info.st_size>128*1024*1024)return NULL;
    cgltf_options options={0};cgltf_data *data=NULL;uint8_t *pixels=NULL;
    if(cgltf_parse_file(&options,path,&data)!=cgltf_result_success)return NULL;
    if(cgltf_load_buffers(&options,data,path)==cgltf_result_success){
        // Prefer a base-color image, never a normal or packed material map.
        for(size_t i=0;i<data->materials_count&&!pixels;i++){
            cgltf_texture *texture=data->materials[i].pbr_metallic_roughness.base_color_texture.texture;
            cgltf_image *img=texture?texture->image:NULL;
            if(img&&img->buffer_view&&img->buffer_view->size<32*1024*1024){
                const void *bytes=cgltf_buffer_view_data(img->buffer_view);
                if(bytes)pixels=stbi_load_from_memory(bytes,(int)img->buffer_view->size,w,h,&channels,4);
            }
        }
    }
    cgltf_free(data);return pixels;
}
static uint8_t *thumbnail_reduce(uint8_t *pixels,int *w,int *h){
    while(pixels&&(*w>192||*h>192)){
        int nw=(*w+1)/2,nh=(*h+1)/2;uint8_t *small=malloc((size_t)nw*nh*4);if(!small){stbi_image_free(pixels);return NULL;}
        for(int y=0;y<nh;y++)for(int x=0;x<nw;x++)for(int c=0;c<4;c++){
            unsigned sum=0,n=0;for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++){int sx=x*2+dx,sy=y*2+dy;if(sx<*w&&sy<*h){sum+=pixels[((size_t)sy*(*w)+sx)*4+c];n++;}}
            small[((size_t)y*nw+x)*4+c]=(uint8_t)(sum/n);
        }
        stbi_image_free(pixels);pixels=small;*w=nw;*h=nh;
    }return pixels;
}
static int thumbnail_thread(void *unused){
    (void)unused;SDL_LockMutex(worker.mutex);
    for(;;){
        while(!worker.stop&&(!worker.count||worker.result_count==THUMB_RESULTS))SDL_CondWait(worker.wake,worker.mutex);
        if(worker.stop)break;
        ThumbnailRequest request=worker.requests[worker.head];worker.head=(worker.head+1)%THUMB_REQUESTS;worker.count--;
        bool current=request.generation==worker.generation;SDL_UnlockMutex(worker.mutex);
        ThumbnailResult result={.generation=request.generation,.index=request.index};
        if(current){result.pixels=thumbnail_decode(request.path,&result.width,&result.height);result.pixels=thumbnail_reduce(result.pixels,&result.width,&result.height);}
        SDL_LockMutex(worker.mutex);
        if(!worker.stop&&request.generation==worker.generation){int tail=(worker.result_head+worker.result_count)%THUMB_RESULTS;worker.results[tail]=result;worker.result_count++;}
        else stbi_image_free(result.pixels);
    }SDL_UnlockMutex(worker.mutex);return 0;
}
static bool thumbnail_start(void){
    if(worker.thread)return true;
    worker.mutex=SDL_CreateMutex();worker.wake=SDL_CreateCond();
    if(!worker.mutex||!worker.wake){thumbnail_shutdown();return false;}
    worker.thread=SDL_CreateThread(thumbnail_thread,"asset-thumbnails",NULL);
    if(!worker.thread){thumbnail_shutdown();return false;}return true;
}
unsigned thumbnail_new_generation(void){
    if(worker.mutex)SDL_LockMutex(worker.mutex);
    worker.generation++;worker.count=worker.head=0;
    for(int i=0;i<worker.result_count;i++)stbi_image_free(worker.results[(worker.result_head+i)%THUMB_RESULTS].pixels);
    worker.result_count=worker.result_head=0;unsigned generation=worker.generation;
    if(worker.mutex){SDL_CondSignal(worker.wake);SDL_UnlockMutex(worker.mutex);}return generation;
}
bool thumbnail_request(unsigned generation,int index,const char *path){
    if(!path||!thumbnail_start())return false;
    SDL_LockMutex(worker.mutex);
    bool accepted=generation==worker.generation&&worker.count<THUMB_REQUESTS;
    if(accepted){ThumbnailRequest *r=&worker.requests[(worker.head+worker.count)%THUMB_REQUESTS];r->generation=generation;r->index=index;snprintf(r->path,sizeof(r->path),"%s",path);worker.count++;SDL_CondSignal(worker.wake);}
    SDL_UnlockMutex(worker.mutex);return accepted;
}
bool thumbnail_poll(ThumbnailResult *result){
    if(!worker.mutex||!result)return false;
    SDL_LockMutex(worker.mutex);bool available=worker.result_count>0;
    if(available){*result=worker.results[worker.result_head];worker.result_head=(worker.result_head+1)%THUMB_RESULTS;worker.result_count--;SDL_CondSignal(worker.wake);}
    SDL_UnlockMutex(worker.mutex);return available;
}
void thumbnail_shutdown(void){
    if(worker.mutex){SDL_LockMutex(worker.mutex);worker.stop=true;if(worker.wake)SDL_CondBroadcast(worker.wake);SDL_UnlockMutex(worker.mutex);}
    if(worker.thread)SDL_WaitThread(worker.thread,NULL);
    for(int i=0;i<worker.result_count;i++)stbi_image_free(worker.results[(worker.result_head+i)%THUMB_RESULTS].pixels);
    if(worker.wake)SDL_DestroyCond(worker.wake);
    if(worker.mutex)SDL_DestroyMutex(worker.mutex);
    memset(&worker,0,sizeof(worker));
}
