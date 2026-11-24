#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#include "engine/texture_cache.h"
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#define TEXTURE_SOURCE_FILE_BACKING 1
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

#define TEXTURE_CACHE_CAPACITY TEXTURE_CACHE_ENTRY_CAPACITY
#ifdef TEXTURE_CACHE_TESTING
static size_t source_budget=TEXTURE_CACHE_SOURCE_BUDGET_BYTES;
#else
static const size_t source_budget=TEXTURE_CACHE_SOURCE_BUDGET_BYTES;
#endif

typedef struct {
    unsigned texture;
    size_t references, length;
    uint64_t fingerprint;
    uint8_t *encoded;
    TextureCachePolicy policy;
    TextureCacheDelete destroy;
    bool file_backed;
    size_t source_offset, source_span;
    size_t gpu_bytes;
} TextureCacheEntry;
static TextureCacheEntry entries[TEXTURE_CACHE_CAPACITY];
static TextureCacheStats stats;
typedef struct {unsigned texture;size_t bytes;} ExternalImage;
static ExternalImage external_images[1024];
#ifdef TEXTURE_SOURCE_FILE_BACKING
static FILE *source_file;
static uint8_t *source_arena;
static size_t arena_length,source_page_size;
static bool backing_unavailable;
#endif
#ifdef TEXTURE_CACHE_TESTING
static bool force_collision;
static unsigned backing_failure;
void texture_cache_test_backing_failure(unsigned stage) {backing_failure=stage;}
void texture_cache_test_force_hash_collision(bool enabled) { force_collision=enabled; }
bool texture_cache_test_set_source_budget(size_t bytes) {
    if(stats.entries||bytes>TEXTURE_CACHE_SOURCE_BUDGET_BYTES)return false;
    source_budget=bytes;return true;
}
#endif

static bool fail_backing(unsigned stage) {
#ifdef TEXTURE_CACHE_TESTING
    return backing_failure==stage;
#else
    (void)stage;return false;
#endif
}
static void close_source_file(void) {
#ifdef TEXTURE_SOURCE_FILE_BACKING
    if(source_arena)munmap(source_arena,arena_length);
    if(source_file)fclose(source_file);
    source_arena=NULL;source_file=NULL;arena_length=source_page_size=0;backing_unavailable=false;
#endif
}
#ifdef TEXTURE_SOURCE_FILE_BACKING
static bool open_source_file(void) {
    if(source_arena)return true;
    if(backing_unavailable)return false;
    const long page=sysconf(_SC_PAGESIZE);
    if(page<=0||fail_backing(1)) {backing_unavailable=true;return false;}
    source_page_size=(size_t)page;
    arena_length=source_budget+source_page_size*TEXTURE_CACHE_CAPACITY;
    source_file=tmpfile();
    if(!source_file||fcntl(fileno(source_file),F_SETFD,FD_CLOEXEC)<0||fail_backing(2)||
       ftruncate(fileno(source_file),(off_t)arena_length))goto fail;
    if(fail_backing(3))goto fail;
    void *mapping=mmap(NULL,arena_length,PROT_READ,MAP_SHARED,fileno(source_file),0);
    if(mapping==MAP_FAILED)goto fail;
    source_arena=mapping;return true;
fail:
    close_source_file();backing_unavailable=true;return false;
}
#endif
typedef struct {uint8_t *bytes;size_t offset,span;bool file_backed;} SourceCopy;
static SourceCopy copy_source(const void *encoded,size_t length) {
    SourceCopy result={0};
#ifdef TEXTURE_SOURCE_FILE_BACKING
    if(open_source_file()&&length<=SIZE_MAX-(source_page_size-1)) {
        const size_t span=(length+source_page_size-1)/source_page_size*source_page_size;
        size_t offset=0;
        /* At most512 live intervals. Reuse holes after regional unloads;
           the one sparse temporary file can never grow past the fixed budget
           plus one padding page per entry. This runs only while loading. */
        for(unsigned attempt=0;attempt<=TEXTURE_CACHE_CAPACITY&&span<=arena_length&&offset<=arena_length-span;attempt++) {
            size_t next=offset;
            for(unsigned i=0;i<TEXTURE_CACHE_CAPACITY;i++) {
                const TextureCacheEntry *e=&entries[i];
                if(e->texture&&e->file_backed&&offset<e->source_offset+e->source_span&&offset+span>e->source_offset)
                    if(next<e->source_offset+e->source_span)next=e->source_offset+e->source_span;
            }
            if(next!=offset) {offset=next;continue;}
            size_t written=0;
            while(written<length&&!fail_backing(4)) {
                const ssize_t n=pwrite(fileno(source_file),(const uint8_t *)encoded+written,length-written,(off_t)(offset+written));
                if(n<0&&errno==EINTR)continue;
                if(n<=0)break;
                written+=(size_t)n;
            }
            if(written==length)return (SourceCopy){source_arena+offset,offset,span,true};
            break;
        }
    }
#endif
    result.bytes=malloc(length);
    if(result.bytes)memcpy(result.bytes,encoded,length);
    if(!stats.source_file_entries)close_source_file();
    return result;
}
bool texture_cache_trim_source_pages(void) {
#ifdef TEXTURE_SOURCE_FILE_BACKING
    if(source_arena&&stats.source_file_entries) {
        /* Make pages clean before reclaiming them. No sync/read/write is
           performed by ordinary texture binds or animation updates. */
        bool ok=!fail_backing(5)&&fdatasync(fileno(source_file))==0;
        if(madvise(source_arena,arena_length,MADV_DONTNEED))ok=false;
        if(posix_fadvise(fileno(source_file),0,(off_t)arena_length,POSIX_FADV_DONTNEED))ok=false;
        if(ok)stats.source_page_trims++;else stats.source_page_trim_failures++;
        return ok;
    }
#endif
    return true;
}

static uint64_t texture_fingerprint(const void *data,size_t length) {
#ifdef TEXTURE_CACHE_TESTING
    if(force_collision) return 1;
#endif
    const uint8_t *bytes=data;
    uint64_t hash=0x9e3779b97f4a7c15ull ^ (uint64_t)length;
    size_t left=length;
    /* Hash words without alignment assumptions; byte comparison below is the
       authority, so a fingerprint collision can never substitute an image. */
    while(left>=sizeof(uint64_t)) {
        uint64_t word;
        memcpy(&word,bytes,sizeof(word));
        hash^=word;hash*=0xbf58476d1ce4e5b9ull;hash^=hash>>29;
        bytes+=sizeof(word);left-=sizeof(word);
    }
    while(left--) { hash^=*bytes++;hash*=0x100000001b3ull; }
    return hash;
}

static bool same_policy(const TextureCachePolicy *a,const TextureCachePolicy *b) {
    return a->srgb==b->srgb&&a->min_filter==b->min_filter&&a->mag_filter==b->mag_filter
        &&a->wrap_s==b->wrap_s&&a->wrap_t==b->wrap_t&&a->mip_policy==b->mip_policy;
}
struct TextureCachePrepared {size_t length;uint64_t fingerprint;uint8_t *encoded;};
TextureCachePrepared *texture_cache_prepare(const void *encoded,size_t length) {
    if(!encoded||!length||length>TEXTURE_CACHE_SOURCE_BUDGET_BYTES)return NULL;
    TextureCachePrepared *p=malloc(sizeof(*p));if(!p)return NULL;
    p->encoded=malloc(length);if(!p->encoded){free(p);return NULL;}
    memcpy(p->encoded,encoded,length);p->length=length;p->fingerprint=texture_fingerprint(encoded,length);return p;
}
void texture_cache_prepared_free(TextureCachePrepared *p){if(p){free(p->encoded);free(p);}}
unsigned texture_cache_find_prepared(const TextureCachePrepared *p,const TextureCachePolicy *policy) {
    if(!p||!policy)return 0;
    for(unsigned i=0;i<TEXTURE_CACHE_CAPACITY;i++){TextureCacheEntry *e=&entries[i];if(e->texture&&e->length==p->length&&e->fingerprint==p->fingerprint&&same_policy(&e->policy,policy)&&!memcmp(e->encoded,p->encoded,p->length)){e->references++;stats.references++;stats.hits++;return e->texture;}}
    return 0;
}
unsigned texture_cache_adopt_prepared(TextureCachePrepared *p,const TextureCachePolicy *policy,unsigned texture,TextureCacheDelete destroy) {
    if(!p||!policy||!texture||!destroy){texture_cache_prepared_free(p);return 0;}
    unsigned existing=texture_cache_find_prepared(p,policy);
    if(existing){destroy(texture);texture_cache_prepared_free(p);return existing;}
    stats.misses++;
    int slot=-1;for(unsigned i=0;i<TEXTURE_CACHE_CAPACITY;i++)if(!entries[i].texture){slot=(int)i;break;}
    if(slot>=0&&stats.source_bytes<=source_budget&&p->length<=source_budget-stats.source_bytes){
        entries[slot]=(TextureCacheEntry){.texture=texture,.references=1,.length=p->length,.fingerprint=p->fingerprint,.encoded=p->encoded,.policy=*policy,.destroy=destroy};
        stats.entries++;stats.references++;stats.source_bytes+=p->length;stats.source_heap_bytes+=p->length;p->encoded=NULL;
    }else stats.unmanaged_uploads++;
    texture_cache_prepared_free(p);return texture;
}
bool texture_cache_track_gpu_bytes(unsigned texture,size_t bytes){
    if(!texture)return false;
    for(unsigned i=0;i<TEXTURE_CACHE_CAPACITY;i++)if(entries[i].texture==texture){if(!entries[i].gpu_bytes){entries[i].gpu_bytes=bytes;stats.gpu_bytes+=bytes;}return true;}
    int slot=-1;for(unsigned i=0;i<1024;i++){if(external_images[i].texture==texture)return true;if(!external_images[i].texture&&slot<0)slot=(int)i;}
    if(slot<0){stats.untracked_gpu_images++;return false;}
    external_images[slot]=(ExternalImage){texture,bytes};stats.gpu_bytes+=bytes;stats.unmanaged_gpu_bytes+=bytes;return true;
}
void texture_cache_forget_gpu_bytes(unsigned texture){for(unsigned i=0;i<1024;i++)if(external_images[i].texture==texture){stats.gpu_bytes-=external_images[i].bytes;stats.unmanaged_gpu_bytes-=external_images[i].bytes;external_images[i]=(ExternalImage){0};return;}}

unsigned texture_cache_acquire(const void *encoded,size_t length,
                               const TextureCachePolicy *policy,
                               TextureCacheUpload upload,TextureCacheDelete destroy) {
    if(!encoded||!length||!policy||!upload||!destroy) return 0;
    uint64_t fingerprint=texture_fingerprint(encoded,length);
    int free_slot=-1;
    for(unsigned i=0;i<TEXTURE_CACHE_CAPACITY;i++) {
        TextureCacheEntry *entry=&entries[i];
        if(!entry->texture) { if(free_slot<0)free_slot=i;continue; }
        if(entry->length==length&&entry->fingerprint==fingerprint
            &&same_policy(&entry->policy,policy)&&!memcmp(entry->encoded,encoded,length)) {
            entry->references++;stats.references++;stats.hits++;
            return entry->texture;
        }
    }
    stats.misses++;
    SourceCopy copy={0};
    if(free_slot>=0&&stats.source_bytes<=source_budget&&length<=source_budget-stats.source_bytes) {
        copy=copy_source(encoded,length);
    }
    unsigned texture=upload(encoded,length,policy);
    if(!texture) {if(!copy.file_backed)free(copy.bytes);if(!stats.source_file_entries)close_source_file();return 0;}
    if(copy.bytes) {
        entries[free_slot]=(TextureCacheEntry){.texture=texture,.references=1,.length=length,
            .fingerprint=fingerprint,.encoded=copy.bytes,.policy=*policy,.destroy=destroy,
            .file_backed=copy.file_backed,.source_offset=copy.offset,.source_span=copy.span};
        stats.entries++;stats.source_bytes+=length;stats.references++;
        if(copy.file_backed) {stats.source_file_bytes+=length;stats.source_file_entries++;}
        else stats.source_heap_bytes+=length;
    } else stats.unmanaged_uploads++;
    return texture;
}

bool texture_cache_retain(unsigned texture) {
    if(!texture)return false;
    for(unsigned i=0;i<TEXTURE_CACHE_CAPACITY;i++)if(entries[i].texture==texture) {
        entries[i].references++;stats.references++;return true;
    }
    return false;
}

bool texture_cache_release(unsigned texture) {
    if(!texture)return false;
    for(unsigned i=0;i<TEXTURE_CACHE_CAPACITY;i++)if(entries[i].texture==texture) {
        TextureCacheEntry *entry=&entries[i];
        entry->references--;stats.references--;
        if(!entry->references) {
            entry->destroy(entry->texture);
            stats.gpu_bytes-=entry->gpu_bytes;
            stats.entries--;stats.source_bytes-=entry->length;
            if(entry->file_backed) {stats.source_file_bytes-=entry->length;stats.source_file_entries--;}
            else {stats.source_heap_bytes-=entry->length;free(entry->encoded);}
            memset(entry,0,sizeof(*entry));
            if(!stats.source_file_entries)close_source_file();
        }
        return true;
    }
    return false;
}

TextureCacheStats texture_cache_stats(void) { return stats; }
void texture_cache_shutdown(void) {
    for(unsigned i=0;i<TEXTURE_CACHE_CAPACITY;i++)if(entries[i].texture) {
        entries[i].destroy(entries[i].texture);if(!entries[i].file_backed)free(entries[i].encoded);
    }
    memset(entries,0,sizeof(entries));memset(external_images,0,sizeof(external_images));memset(&stats,0,sizeof(stats));
    close_source_file();
}
