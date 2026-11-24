#ifndef VALWOOD_TEXTURE_CACHE_H
#define VALWOOD_TEXTURE_CACHE_H

#include <stdbool.h>
#include <stddef.h>

/* Encoded comparison bytes use reclaimable temporary-file pages on POSIX,
   with an exact heap-backed fallback, independently of GPU texture storage.
   The full scenery plus all animation poses must fit here so repeated
   image bytes continue sharing one upload after architecture has loaded. */
#define TEXTURE_CACHE_ENTRY_CAPACITY 512u
#define TEXTURE_CACHE_SOURCE_BUDGET_BYTES ((size_t)768 * 1024 * 1024)

/* One render-thread cache shared by every loader translation unit. The policy
   describes the effective upload/sampler state, rather than an asset's name. */
typedef struct {
    bool srgb;
    unsigned min_filter, mag_filter, wrap_s, wrap_t;
    unsigned mip_policy;
} TextureCachePolicy;
typedef unsigned (*TextureCacheUpload)(const void *encoded, size_t length,
                                       const TextureCachePolicy *policy);
typedef void (*TextureCacheDelete)(unsigned texture);
typedef struct {
    size_t entries, source_bytes, references;
    size_t hits, misses;
    /* Successful capacity/allocation fallbacks, retained by their callers. */
    size_t unmanaged_uploads;
    size_t source_file_bytes, source_heap_bytes, source_file_entries;
    size_t source_page_trims, source_page_trim_failures;
    size_t gpu_bytes; /* Registered image storage, counted once per shared ID. */
    size_t unmanaged_gpu_bytes,untracked_gpu_images;
} TextureCacheStats;

/* Each successful call owns one texture-field reference. On capacity/allocation
   failure, upload still runs and returns an unmanaged ID so content is retained. */
unsigned texture_cache_acquire(const void *encoded, size_t length,
                               const TextureCachePolicy *policy,
                               TextureCacheUpload upload, TextureCacheDelete destroy);
bool texture_cache_retain(unsigned texture);
/* Returns false for legacy/unmanaged IDs, whose caller retains deletion duty. */
bool texture_cache_release(unsigned texture);
TextureCacheStats texture_cache_stats(void);
/* Worker preparation for incremental uploads. Copies/hashes the exact encoded
   key on the worker; no shared cache or GL access occurs. The owner thread can
   retain an existing match or adopt a newly uploaded image without decoding,
   allocating source memory or writing the temporary comparison file. Adopt
   consumes the prepared key even when capacity forces an unmanaged texture. */
typedef struct TextureCachePrepared TextureCachePrepared;
TextureCachePrepared *texture_cache_prepare(const void *encoded,size_t length);
void texture_cache_prepared_free(TextureCachePrepared *);
unsigned texture_cache_find_prepared(const TextureCachePrepared *,const TextureCachePolicy *);
unsigned texture_cache_adopt_prepared(TextureCachePrepared *,const TextureCachePolicy *,unsigned texture,TextureCacheDelete);
/* Owner-thread metadata only. First registration wins; no GL query or IO. */
bool texture_cache_track_gpu_bytes(unsigned texture,size_t bytes);
void texture_cache_forget_gpu_bytes(unsigned texture); /* unmanaged caller deletion */
/* Loading boundary only: finish backing-file writes and discard comparison
   pages from the working set. GPU images, ownership and exact bytes survive.
   Failure is nonfatal and preserves the cache; no draw path calls this. */
bool texture_cache_trim_source_pages(void);
/* Teardown only: call after scene owners have released, before GL destruction. */
void texture_cache_shutdown(void);

#ifdef TEXTURE_CACHE_TESTING
void texture_cache_test_force_hash_collision(bool enabled);
bool texture_cache_test_set_source_budget(size_t bytes);
/* 1=open,2=resize,3=map,4=write,5=trim sync. Zero restores ordinary storage. */
void texture_cache_test_backing_failure(unsigned stage);
#endif
#endif
