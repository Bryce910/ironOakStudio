#include "engine/texture_cache.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static unsigned next_id=1,uploads,deletions;
static bool live[4096],fail_upload;
static unsigned upload(const void *bytes,size_t length,const TextureCachePolicy *policy){
    assert(bytes&&length&&policy);
    uploads++;
    if(fail_upload)return 0;
    assert(next_id<sizeof(live)/sizeof(*live));
    live[next_id]=true;
    return next_id++;
}
static void destroy(unsigned id){
    assert(id&&id<sizeof(live)/sizeof(*live)&&live[id]);
    live[id]=false;deletions++;
}
static void expect_empty(void){
    TextureCacheStats s=texture_cache_stats();
    assert(s.entries==0&&s.references==0&&s.source_bytes==0);
    assert(!s.source_file_bytes&&!s.source_heap_bytes&&!s.source_file_entries);
}

int main(void){
    TextureCachePolicy color={.srgb=true,.min_filter=1,.mag_filter=2,.wrap_s=3,.wrap_t=3,.mip_policy=1};
    const unsigned char original[]={1,2,3,4,5,6,7,8};
    unsigned char first[sizeof(original)];memcpy(first,original,sizeof(first));
    texture_cache_test_force_hash_collision(true);
    unsigned a=texture_cache_acquire(first,sizeof(first),&color,upload,destroy);
    // The cache owns its collision-comparison bytes, independently of scene data.
    memset(first,0,sizeof(first));
    unsigned b=texture_cache_acquire(original,sizeof(original),&color,upload,destroy);
    assert(a==b&&uploads==1);
    assert(texture_cache_stats().source_file_entries==1);
    assert(texture_cache_trim_source_pages());
    assert(texture_cache_stats().source_page_trims==1);
    assert(texture_cache_retain(a)); // Additional mesh field in the same scene.
    TextureCacheStats s=texture_cache_stats();assert(s.entries==1&&s.references==3&&s.hits==1);
    assert(texture_cache_release(a)&&live[a]);
    assert(texture_cache_release(b)&&live[a]);
    assert(texture_cache_release(a)&&!live[a]&&deletions==1);
    assert(!texture_cache_retain(a)&&!texture_cache_release(a));expect_empty();

    // Deliberately equal fingerprints/lengths must still compare full content.
    const unsigned char other[]={1,2,3,4,5,6,7,9};
    a=texture_cache_acquire(original,sizeof(original),&color,upload,destroy);
    b=texture_cache_acquire(other,sizeof(other),&color,upload,destroy);
    unsigned short_image=texture_cache_acquire(original,sizeof(original)-1,&color,upload,destroy);
    assert(a!=b&&a!=short_image&&b!=short_image);
    TextureCachePolicy linear=color;linear.srgb=false;
    unsigned n=texture_cache_acquire(original,sizeof(original),&linear,upload,destroy);
    assert(n!=a);
    TextureCachePolicy wrap=color;wrap.wrap_s++;
    unsigned w=texture_cache_acquire(original,sizeof(original),&wrap,upload,destroy);
    TextureCachePolicy mips=color;mips.mip_policy++;
    unsigned m=texture_cache_acquire(original,sizeof(original),&mips,upload,destroy);
    TextureCachePolicy compressed=color;compressed.mip_policy|=1u<<16;
    unsigned bc7=texture_cache_acquire(original,sizeof(original),&compressed,upload,destroy);
    assert(w!=a&&m!=a&&w!=m);
    assert(bc7!=a&&bc7!=m&&bc7!=w);
    assert(texture_cache_release(a)&&texture_cache_release(b)&&texture_cache_release(short_image));
    assert(texture_cache_release(n)&&texture_cache_release(w)&&texture_cache_release(m)&&texture_cache_release(bc7));expect_empty();

    fail_upload=true;
    assert(!texture_cache_acquire(original,sizeof(original),&color,upload,destroy));
    fail_upload=false;expect_empty();
    assert(!texture_cache_acquire(NULL,8,&color,upload,destroy));
    assert(!texture_cache_acquire(original,0,&color,upload,destroy));
    assert(!texture_cache_acquire(original,8,NULL,upload,destroy));
    assert(!texture_cache_acquire(original,8,&color,NULL,destroy));
    assert(!texture_cache_acquire(original,8,&color,upload,NULL));
    assert(!texture_cache_release(0)&&!texture_cache_retain(0));

    // Bounded storage must preserve content availability via unmanaged uploads.
    unsigned owners[513];
    for(unsigned i=0;i<513;i++){
        uint32_t image[2]={i,0x12345678u};
        owners[i]=texture_cache_acquire(image,sizeof(image),&color,upload,destroy);
        assert(owners[i]);
    }
    s=texture_cache_stats();assert(s.entries==512&&s.references==512&&s.source_bytes==512*8);
    assert(!texture_cache_retain(owners[512]));
    for(unsigned i=0;i<513;i++)if(!texture_cache_release(owners[i]))destroy(owners[i]);
    expect_empty();
    texture_cache_test_force_hash_collision(false);

    // Background-prepared keys adopt without temporary-file source writes;
    // exact byte/policy sharing and ownership survive both legacy directions.
    TextureCachePrepared *prepared=texture_cache_prepare(original,sizeof(original));assert(prepared);
    a=upload(original,sizeof(original),&color);assert(texture_cache_adopt_prepared(prepared,&color,a,destroy)==a);
    s=texture_cache_stats();assert(s.source_heap_bytes==sizeof(original)&&s.source_file_bytes==0);
    assert(texture_cache_track_gpu_bytes(a,4096));assert(texture_cache_track_gpu_bytes(a,8192));assert(texture_cache_stats().gpu_bytes==4096);
    prepared=texture_cache_prepare(original,sizeof(original));assert(texture_cache_find_prepared(prepared,&color)==a);
    texture_cache_prepared_free(prepared);assert(texture_cache_release(a));
    prepared=texture_cache_prepare(original,sizeof(original));b=upload(original,sizeof(original),&color);
    assert(texture_cache_adopt_prepared(prepared,&color,b,destroy)==a&&!live[b]);
    assert(texture_cache_release(a)&&texture_cache_release(a));expect_empty();assert(texture_cache_stats().gpu_bytes==0);
    assert(!texture_cache_prepare(NULL,4)&&!texture_cache_prepare(original,0));

    // Byte pressure is distinct from entry pressure. An existing image must
    // still deduplicate at a completely full source budget; only genuinely
    // new images fall back, with explicit caller ownership and one deletion.
    assert(texture_cache_test_set_source_budget(16));
    const size_t previous_unmanaged=texture_cache_stats().unmanaged_uploads;
    a=texture_cache_acquire(original,sizeof(original),&color,upload,destroy);
    b=texture_cache_acquire(other,sizeof(other),&color,upload,destroy);
    assert(!texture_cache_test_set_source_budget(8));
    unsigned fallback=texture_cache_acquire(original,sizeof(original),&linear,upload,destroy);
    unsigned shared=texture_cache_acquire(original,sizeof(original),&color,upload,destroy);
    s=texture_cache_stats();
    assert(a&&b&&fallback&&shared==a&&s.entries==2&&s.references==3&&s.source_bytes==16);
    assert(s.unmanaged_uploads==previous_unmanaged+1);
    assert(!texture_cache_retain(fallback)&&!texture_cache_release(fallback));destroy(fallback);
    assert(texture_cache_release(a)&&texture_cache_release(shared)&&texture_cache_release(b));expect_empty();
    assert(!texture_cache_test_set_source_budget(TEXTURE_CACHE_SOURCE_BUDGET_BYTES+1));
    assert(texture_cache_test_set_source_budget(TEXTURE_CACHE_SOURCE_BUDGET_BYTES));

    // Every backing-store failure preserves exact managed sharing via the
    // original heap path, rather than dropping an image or duplicating a GPU ID.
    for(unsigned stage=1;stage<=4;stage++) {
        texture_cache_test_backing_failure(stage);
        a=texture_cache_acquire(original,sizeof(original),&color,upload,destroy);
        s=texture_cache_stats();assert(a&&s.source_heap_bytes==8&&!s.source_file_bytes);
        texture_cache_test_backing_failure(0);
        b=texture_cache_acquire(original,sizeof(original),&color,upload,destroy);
        assert(a==b&&texture_cache_release(a)&&texture_cache_release(b));expect_empty();
    }
    a=texture_cache_acquire(original,sizeof(original),&color,upload,destroy);
    texture_cache_test_backing_failure(4);
    b=texture_cache_acquire(other,sizeof(other),&color,upload,destroy);
    s=texture_cache_stats();assert(s.source_file_bytes==8&&s.source_heap_bytes==8);
    texture_cache_test_backing_failure(5);
    assert(!texture_cache_trim_source_pages());
    texture_cache_test_backing_failure(0);
    assert(texture_cache_trim_source_pages());
    unsigned retained=texture_cache_acquire(original,sizeof(original),&color,upload,destroy);
    assert(retained==a);
    assert(texture_cache_release(a)&&texture_cache_release(retained));
    assert(live[b]&&texture_cache_stats().source_heap_bytes==8);
    assert(texture_cache_release(b));expect_empty();

    // A fixed sparse arena reuses released intervals while one entry stays
    // live. Append-only allocation would exhaust its padding after512 cycles.
    assert(texture_cache_test_set_source_budget(32));
    a=texture_cache_acquire(original,sizeof(original),&color,upload,destroy);
    for(unsigned i=0;i<1100;i++) {
        uint32_t image[2]={i+777,0x468dabc1u};
        b=texture_cache_acquire(image,sizeof(image),&color,upload,destroy);
        s=texture_cache_stats();assert(b&&s.source_file_entries==2&&s.source_file_bytes==16&&!s.source_heap_bytes);
        assert(texture_cache_release(b));
    }
    assert(texture_cache_trim_source_pages());
    b=texture_cache_acquire(original,sizeof(original),&color,upload,destroy);
    assert(a==b&&texture_cache_release(a)&&texture_cache_release(b));expect_empty();
    assert(texture_cache_test_set_source_budget(TEXTURE_CACHE_SOURCE_BUDGET_BYTES));

    a=texture_cache_acquire(original,sizeof(original),&color,upload,destroy);
    b=texture_cache_acquire(other,sizeof(other),&color,upload,destroy);
    assert(live[a]&&live[b]);texture_cache_shutdown();
    assert(!live[a]&&!live[b]);expect_empty();
    for(unsigned i=1;i<next_id;i++)assert(!live[i]);
    assert(deletions==next_id-1);
    puts("Texture cache: exact content/forced collisions after file-page trimming, sampler/image identity, references, failed uploads,512-entry/byte bounds, five backing failure paths with exact heap fallback,1100 reused file intervals and complete teardown passed.");
    return 0;
}
