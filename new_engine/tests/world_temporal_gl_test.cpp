/* Synthetic shader/resolve validation, not a frame-rate benchmark. Default
 * SDL offscreen context; VALWOOD_TEST_NATIVE_GL=1 keeps the native driver.
 * Run from the engine root or pass that directory as the first argument.
 */
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_temporal.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <vector>

namespace {
constexpr int extent = 64;
constexpr float near_plane = .4f, far_plane = 1000.f;
using Pixel = std::array<float, 4>;

void projection(WorldTemporalFrame &frame, float yaw = 0) {
    float p[16] = {}, view[16] = {};
    p[0] = p[5] = 1.f / std::tan(.54f);
    p[8] = -2 * frame.jitter_pixels[0] / frame.width;
    p[9] = -2 * frame.jitter_pixels[1] / frame.height;
    p[10] = (frame.far_plane + frame.near_plane) / (frame.near_plane - frame.far_plane);
    p[11] = -1; p[14] = 2 * frame.far_plane * frame.near_plane / (frame.near_plane - frame.far_plane);
    view[0] = view[10] = std::cos(yaw); view[5] = view[15] = 1;
    view[2] = -std::sin(yaw); view[8] = std::sin(yaw);
    std::fill(std::begin(frame.view_projection), std::end(frame.view_projection), 0.f);
    for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r) for (int k = 0; k < 4; ++k)
        frame.view_projection[c * 4 + r] += p[k * 4 + r] * view[c * 4 + k];
}
float device_depth(float distance, float far = far_plane) {
    if (distance == 0) return 1;
    return .5f * ((far + near_plane) / (far - near_plane) -
                  2 * far * near_plane / ((far - near_plane) * distance)) + .5f;
}
struct Inputs {
    GLuint color = 0, depth = 0;
    int size;
    std::vector<Pixel> pixels;
    std::vector<float> depths;
    explicit Inputs(int width) : size(width), pixels(width * width), depths(width * width) {
        glGenTextures(1, &color); glGenTextures(1, &depth);
        for (GLuint id : {color, depth}) {
            glBindTexture(GL_TEXTURE_2D, id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, id == color ? GL_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, id == color ? GL_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
    }
    ~Inputs() { glDeleteTextures(1, &color); glDeleteTextures(1, &depth); }
    void pattern(int phase, float distance, float stability = 1) {
        for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
            float value = ((x + y + phase) & 1) ? .8f : .2f;
            pixels[y * size + x] = {value, value, value, stability};
        }
        std::fill(depths.begin(), depths.end(), device_depth(distance));
        upload();
    }
    void upload() {
        glBindTexture(GL_TEXTURE_2D, color);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, size, size, 0, GL_RGBA, GL_FLOAT, pixels.data());
        glBindTexture(GL_TEXTURE_2D, depth);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, size, size, 0, GL_RED, GL_FLOAT, depths.data());
    }
};
std::vector<Pixel> read_color(GLuint texture, int size) {
    GLuint fbo; glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    std::vector<Pixel> pixels(size * size);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, size, size, GL_RGBA, GL_FLOAT, pixels.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0); glDeleteFramebuffers(1, &fbo);
    for (const Pixel &pixel : pixels) for (float value : pixel) assert(std::isfinite(value));
    return pixels;
}
std::vector<float> read_depth(GLuint texture, int size) {
    std::vector<float> values(size * size);
    glBindTexture(GL_TEXTURE_2D, texture);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, values.data());
    return values;
}
Pixel center(WorldTemporal *temporal, WorldTemporalFrame &frame) {
    auto pixels = read_color(world_temporal_resolve(temporal, frame), frame.width);
    assert(glGetError() == GL_NO_ERROR);
    return pixels[frame.width * (frame.height / 2) + frame.width / 2];
}
void expect_current(Pixel pixel, const Inputs &input, float tolerance = .002f) {
    float value = input.pixels[input.size * (input.size / 2) + input.size / 2][0];
    assert(std::abs(pixel[0] - value) < tolerance);
    assert(std::abs(pixel[3] - 1.f) < .001f);
}
void check_sampler_contract(WorldTemporal *temporal) {
    Inputs input(extent);WorldTemporalFrame frame;
    frame.color=input.color;frame.depth=input.depth;frame.width=frame.height=extent;
    frame.near_plane=near_plane;frame.far_plane=far_plane;frame.reactive_alpha=true;
    const char *names[]={"opaque","reactive actor","static rigid","foliage/sky","oblique3D plane"};
    const float stability[]={1.f,.15f,.995f,.98f,1.f};
    constexpr unsigned samples=16;
    std::array<std::vector<Pixel>,samples> reference;
    std::array<std::vector<float>,samples> reference_depth;
    GLuint hostile[4]={};glGenSamplers(4,hostile);
    for(GLuint sampler:hostile){
        glSamplerParameteri(sampler,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glSamplerParameteri(sampler,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glSamplerParameteri(sampler,GL_TEXTURE_WRAP_S,GL_REPEAT);
        glSamplerParameteri(sampler,GL_TEXTURE_WRAP_T,GL_MIRRORED_REPEAT);
    }
    auto filters=[](GLuint texture,GLint wanted){
        glBindTexture(GL_TEXTURE_2D,texture);GLint minimum=0,maximum=0;
        glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,&minimum);
        glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,&maximum);
        assert(minimum==wanted&&maximum==wanted);
    };
    auto unbound=[](){for(GLuint unit=0;unit<4;unit++){GLint sampler=-1;glGetIntegeri_v(GL_SAMPLER_BINDING,unit,&sampler);assert(sampler==0);}};
    double largest_delta=0,largest_depth_delta=0;
    for(unsigned scene=0;scene<5;scene++)for(unsigned policy=0;policy<3;policy++){
        world_temporal_reset(temporal);
        const GLint color_filter=policy==0?GL_LINEAR:GL_NEAREST;
        const GLint depth_filter=policy==2?GL_LINEAR:GL_NEAREST;
        glBindTexture(GL_TEXTURE_2D,input.color);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,color_filter);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,color_filter);
        glBindTexture(GL_TEXTURE_2D,input.depth);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,depth_filter);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,depth_filter);
        for(unsigned n=0;n<samples;n++){
            world_temporal_jitter(n,frame.jitter_pixels);projection(frame);
            for(int y=0;y<extent;y++)for(int x=0;x<extent;x++){
                const float px=x+.5f-frame.jitter_pixels[0],py=y+.5f-frame.jitter_pixels[1];const int k=y*extent+x;
                const float value=.08f+px*.008f+py*.003f;
                input.pixels[k]={value,.15f+.32f*std::sin(px*.73f+py*.31f)*std::sin(py*.27f)+.32f,.25f+.1f*std::cos(px*.29f-py*.61f),stability[scene]};
                input.depths[k]=device_depth(8);
                if(scene==3&&std::sin(px*2.1f+py*.83f)<.25f){input.pixels[k]={.6f,.8f,1.f,1.f};input.depths[k]=1.;}
                if(scene==4)input.depths[k]+=((px-extent*.5f)*.00045f+(py-extent*.5f)*.00023f);
            }
            input.upload();
            // Upstream effects are permitted to leave arbitrary sampler
            // objects, not just texture filters. In particular history color
            // must remain bilinear even if unit2 previously sampled a mask.
            if(policy==2)for(GLuint unit=0;unit<4;unit++)glBindSampler(unit,hostile[unit]);
            const auto result=read_color(world_temporal_resolve(temporal,frame),extent);
            const auto depth=read_depth(world_temporal_depth(temporal,0),extent);
            unbound();filters(input.color,color_filter);filters(input.depth,depth_filter);
            if(policy==0){reference[n]=result;reference_depth[n]=depth;}
            else{
                double color_delta=0,depth_delta=0;
                for(std::size_t i=0;i<result.size();i++){
                    for(unsigned c=0;c<4;c++)color_delta=std::max(color_delta,double(std::abs(result[i][c]-reference[n][i][c])));
                    depth_delta=std::max(depth_delta,double(std::abs(depth[i]-reference_depth[n][i])));
                }
                largest_delta=std::max(largest_delta,color_delta);largest_depth_delta=std::max(largest_depth_delta,depth_delta);
                if(color_delta||depth_delta){std::fprintf(stderr,"Temporal sampler regression %s policy%u phase%u: color%.9f depth%.9f\n",names[scene],policy,n%8,color_delta,depth_delta);std::fflush(stderr);}
                assert(color_delta==0&&depth_delta==0);
            }
        }
    }
    // The failure path also releases borrowed unit state instead of leaving
    // samplers that could override the next scene's material filters.
    for(GLuint unit=0;unit<4;unit++)glBindSampler(unit,hostile[unit]);
    projection(frame);frame.view_projection[0]=NAN;
    assert(world_temporal_resolve(temporal,frame)==frame.color);unbound();
    for(GLuint sampler:hostile){GLint filter=0;glGetSamplerParameteriv(sampler,GL_TEXTURE_MIN_FILTER,&filter);assert(filter==GL_NEAREST&&glIsSampler(sampler));}
    glDeleteSamplers(4,hostile);for(GLuint sampler:hostile)assert(!glIsSampler(sampler));
    assert(glGetError()==GL_NO_ERROR);
    std::printf("PASS temporal sampler contract: five3D/source classes,16frames/all8jitter phases, NEAREST/LINEAR upstream color and depth plus hostile samplers produce identical pixels (maxRGBA%.9f/depth%.9f), input filters preserved, units0–3 unbound on success/failure\n",largest_delta,largest_depth_delta);
}
void check_rigid_edges(WorldTemporal *temporal) {
    Inputs input(extent);WorldTemporalFrame frame;
    frame.color=input.color;frame.depth=input.depth;frame.width=frame.height=extent;
    frame.near_plane=near_plane;frame.far_plane=far_plane;frame.reactive_alpha=true;
    constexpr float rigid=.995f;
    const Pixel wall={.08f,.25f,.04f,rigid},sky={.6f,.8f,1.f,1.f},actor={1.f,.03f,.02f,.15f};
    auto fill=[&](Pixel value,float distance){std::fill(input.pixels.begin(),input.pixels.end(),value);std::fill(input.depths.begin(),input.depths.end(),device_depth(distance));};
    auto put=[&](int x,int y,Pixel value,float distance){input.pixels[y*extent+x]=value;input.depths[y*extent+x]=device_depth(distance);};
    auto render=[&](){input.upload();return read_color(world_temporal_resolve(temporal,frame),extent);};
    auto exact=[&](const Pixel &actual,const Pixel &wanted){for(int c=0;c<3;c++)assert(std::fabs(actual[c]-wanted[c])<.001f);assert(actual[3]==1.f);};
    const int middle=extent*(extent/2)+extent/2;
    double variances[2]={};
    for(int tag=0;tag<2;tag++) {
        world_temporal_reset(temporal);std::vector<double> sums(extent*extent),squares(extent*extent);
        for(int n=0;n<48;n++) {
            world_temporal_jitter(n,frame.jitter_pixels);projection(frame);
            for(int y=0;y<extent;y++)for(int x=0;x<extent;x++) {
                bool covered=x+.5f-frame.jitter_pixels[0]+.37f*(y+.5f-frame.jitter_pixels[1])<extent*.683f;
                Pixel value=covered?wall:sky;if(covered&&!tag)value[3]=1;
                put(x,y,value,covered?6.f:0.f);
            }
            auto resolved=render();if(n<32)continue;
            for(int y=2;y<extent-2;y++)for(int x=2;x<extent-2;x++){int p=y*extent+x;sums[p]+=resolved[p][1];squares[p]+=double(resolved[p][1])*resolved[p][1];}
        }
        for(int p=0;p<extent*extent;p++)variances[tag]+=std::max(0.,squares[p]/16-sums[p]*sums[p]/256);
    }
    std::printf("Rigid wall/sky variance: ordinaryalpha1 %.9f; explicitstatic %.9f\n",variances[0],variances[1]);
    assert(variances[0]>0&&variances[1]<variances[0]*.20);
    frame.jitter_pixels[0]=frame.jitter_pixels[1]=0;projection(frame);

    // Reserved source metadata must not blur or change fully supported
    // opaque texture interiors. Decode its ordinary history confidence to1.
    std::vector<Pixel> plain_texture;
    for(int tag=0;tag<2;tag++) {
        world_temporal_reset(temporal);
        for(int n=0;n<12;n++) {
            input.pattern(n%2,8,tag?rigid:1.f);auto resolved=render();
            if(n==11){if(tag)assert(resolved==plain_texture);else plain_texture=resolved;}
        }
    }
    // Static old walls must disappear when their depth leaves current support.
    for(bool reveal_sky:{false,true}) {
        world_temporal_reset(temporal);fill(wall,4);for(int n=0;n<16;n++)render();
        const Pixel exposed=reveal_sky?sky:Pixel{.1f,.2f,.3f,1};fill(exposed,reveal_sky?0.f:12.f);
        exact(render()[middle],exposed);
    }
    // A one-pixel actor lies inside a rigid background's support footprint.
    // Its lower confidence cannot acquire old green wall color/depth.
    world_temporal_reset(temporal);fill(wall,8);for(int n=0;n<16;n++)render();
    put(extent/2,extent/2,actor,4);exact(render()[middle],actor);
    fill(wall,8);put(extent/2+4,extent/2,actor,4);exact(render()[middle],wall);

    // Turning a corner exposes a farther tagged wall. Old near-wall support
    // remains at the boundary, but a pixel two cells beyond it stays strict.
    world_temporal_reset(temporal);fill(wall,4);for(int n=0;n<16;n++)render();
    const Pixel beyond={.1f,.3f,.7f,rigid};fill(beyond,8);
    for(int y=0;y<extent;y++)for(int x=0;x<extent/2;x++)put(x,y,wall,4);
    auto corner=render();exact(corner[middle+2],beyond);
    for(int n=0;n<8;n++) {
        frame.camera[0]+=.002;projection(frame);corner=render();
        for(int c=0;c<3;c++)assert(std::fabs(corner[middle+2][c]-beyond[c])<.001f);
    }
    frame.camera[0]=0;projection(frame);

    // The old near layer still exists beside the new background, but a fast
    // translation/turn must disable local coverage reconstruction entirely.
    for(bool turn:{false,true}) {
        world_temporal_reset(temporal);frame.camera[0]=0;projection(frame);fill(wall,4);for(int n=0;n<16;n++)render();
        fill(beyond,8);put(extent/2+1,extent/2,wall,4);
        if(turn)projection(frame,.15f);else {frame.camera[0]=2*(2*8*std::tan(.54f)/extent);projection(frame);}
        exact(render()[middle],beyond);frame.camera[0]=0;projection(frame);
    }
    // Reject a bright old foreground's color even when a same-colored
    // CURRENT neighbor opens the local color box at a different depth.
    world_temporal_reset(temporal);fill(wall,8);
    put(extent/2+1,extent/2,Pixel{10,0,0,rigid},4);render();
    frame.camera[0]=.25*(2*8*std::tan(.54f)/extent);projection(frame);
    fill(wall,8);input.pixels[middle][1]=.15f;
    put(extent/2-1,extent/2-1,Pixel{10,0,0,rigid},8);
    const Pixel no_halo=render()[middle];assert(no_halo[3]>1.5f);
    assert(std::fabs(no_halo[0]-wall[0])<.002f&&std::fabs(no_halo[2]-wall[2])<.002f);
    frame.camera[0]=0;projection(frame);
    // Sky/cloud alpha can numerically equal the reserved source tag. Without
    // non-sky depth it cannot opt an ordinary opaque neighbor into coverage.
    world_temporal_reset(temporal);fill(wall,4);for(int n=0;n<16;n++)render();
    Pixel cloud={.2f,.4f,.8f,rigid};fill(cloud,0);Pixel ordinary={1,0,0,1};put(extent/2+1,extent/2,ordinary,4);
    exact(render()[middle],cloud);
    frame.reset=true;fill(beyond,8);exact(render()[middle],beyond);frame.reset=false;
    // Cloud confidence can equal the reserved rigid tag. With a lower base
    // history limit the ordinary sky cap no longer conceals a bad decode.
    world_temporal_reset(temporal);frame.history_weight=.5f;
    input.pattern(0,0,rigid);render();const Pixel old_cloud=read_color(input.color,extent)[middle];
    input.pattern(1,0,rigid);input.upload();const Pixel new_cloud=read_color(input.color,extent)[middle];
    const Pixel cloud_history=read_color(world_temporal_resolve(temporal,frame),extent)[middle];
    const float cloud_weight=std::min(.5f,.15f+(.5f-.15f)*new_cloud[3]);
    for(int c=0;c<3;c++)assert(std::fabs(cloud_history[c]-(new_cloud[c]*(1-cloud_weight)+old_cloud[c]*cloud_weight))<.0003f);
    std::puts("PASS rigid edge containment: reserved source tag, unsupported wall/sky rejection, movingactor onrigidbackground and no trail, parallax corner, fasttranslation/turn exclusion, ordinaryopaque/cloud isolation includinglowhistoryweight, reset");
}

}

int main(int argc, char **argv) {
    if (argc > 1) assert(chdir(argv[1]) == 0);
    if (!std::getenv("VALWOOD_TEST_NATIVE_GL")) SDL_setenv("SDL_VIDEODRIVER", "offscreen", 1);
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window = SDL_CreateWindow("Temporal resolve test", 0, 0, extent, extent, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    assert(window);
    SDL_GLContext context = SDL_GL_CreateContext(window); assert(context);
    std::printf("Temporal test renderer: %s\n", glGetString(GL_RENDERER));
    WorldTemporal *temporal = world_temporal_create("shaders/world_temporal.vs.glsl", "shaders/world_temporal.fs.glsl");
    assert(temporal);
    check_sampler_contract(temporal);
    check_rigid_edges(temporal);
    {
        Inputs input(extent);
        WorldTemporalFrame frame;
        frame.color = input.color; frame.depth = input.depth;
        frame.width = frame.height = extent;
        frame.near_plane = near_plane; frame.far_plane = far_plane;
        frame.camera[0] = 15000; frame.camera[1] = 45; frame.camera[2] = -12000;
        projection(frame);
        input.pattern(0, 4); expect_current(center(temporal, frame), input);
        input.pattern(1, 4);
        Pixel static_history = center(temporal, frame);
        assert(std::abs(static_history[0] - .5f) < .01f && static_history[3] > 1.5f);

        world_temporal_reset(temporal);
        frame.reactive_alpha = true;
        input.pattern(0, 4, 0); center(temporal, frame);
        input.pattern(1, 4, 0);
        Pixel reactive = center(temporal, frame);
        assert(std::abs(reactive[0] - .8f) < std::abs(static_history[0] - .8f) * .5f);
        frame.reactive_alpha = false;

        // Newly exposed geometry and sky never inherit an occluder's history.
        input.pattern(0, 8); expect_current(center(temporal, frame), input);
        input.pattern(1, 0); expect_current(center(temporal, frame), input);
        input.pattern(0, 4); expect_current(center(temporal, frame), input);

        // Recover matching background history at a mixed-depth footprint,
        // without importing the foreground tap's bright red color. Ordinary
        // bilinear color + nearest-depth validation produces a red halo here;
        // rejecting the whole footprint throws away the valid green history.
        for (bool vegetation_tag : {false, true}) {
            frame.reactive_alpha = vegetation_tag;
            float tagged_alpha = vegetation_tag ? .65f : 1.f;
            world_temporal_reset(temporal);
            input.pattern(0, 8);
            for (Pixel &pixel : input.pixels) pixel = {.1f, .6f, .1f, tagged_alpha};
            int foreground = extent * (extent / 2) + extent / 2 + 1;
            input.depths[foreground] = device_depth(4);
            input.pixels[foreground] = {10.f, 0, 0, tagged_alpha};
            input.upload(); center(temporal, frame);
            frame.camera[0] += .25 * (2 * 8 * std::tan(.54) / extent);
            input.pattern(1, 8);
            for (Pixel &pixel : input.pixels) pixel = {.1f, .6f, .1f, tagged_alpha};
            input.pixels[extent * (extent / 2) + extent / 2][1] = .3f;
            // A nearby current red surface opens the neighborhood color bounds;
            // color clipping alone must not conceal a depth-validation mistake.
            input.pixels[extent * (extent / 2 - 1) + extent / 2 - 1] = {10.f, 0, 0, tagged_alpha};
            input.upload();
            Pixel partial = center(temporal, frame);
            assert(partial[3] > 1.5f && partial[1] > .35f && partial[1] < .61f);
            assert(std::abs(partial[0] - .1f) < .002f && std::abs(partial[2] - .1f) < .002f);

        }
        frame.reactive_alpha = false;

        // No valid depth tap still rejects history completely, including the
        // far background exposed after the foreground leaves the entire area.
        input.pattern(0, 16); expect_current(center(temporal, frame), input);

        // Production clipping planes and far 24-bit-like device-depth values
        // still accumulate a stationary distant surface, rather than rejecting
        // every frame due to loss of precision near depth=1.
        frame.far_plane = 15000; projection(frame);
        input.pattern(0, 8);
        std::fill(input.depths.begin(), input.depths.end(), device_depth(12000, 15000));
        input.upload(); expect_current(center(temporal, frame), input);
        input.pattern(1, 8);
        std::fill(input.depths.begin(), input.depths.end(), device_depth(12000, 15000));
        input.upload();
        Pixel distant_history = center(temporal, frame);
        assert(distant_history[3] > 1.5f && std::abs(distant_history[0] - .5f) < .01f);
        frame.far_plane = far_plane; projection(frame);

        // Explicit cuts, large translations, turns and resize must copy current.
        frame.reset = true; input.pattern(1, 4); expect_current(center(temporal, frame), input); frame.reset = false;
        frame.camera[0] += 81; input.pattern(0, 4); expect_current(center(temporal, frame), input);
        projection(frame, 1.57f); input.pattern(1, 4); expect_current(center(temporal, frame), input);
        projection(frame);
        input.pattern(0, 4); center(temporal, frame);
        frame.camera[0] += 10; input.pattern(1, 4); expect_current(center(temporal, frame), input); // outside old image

        // A large color change is clipped to the new local neighborhood.
        for (Pixel &pixel : input.pixels) pixel = {.95f, .3f, .1f, 1};
        input.upload();
        Pixel clipped = center(temporal, frame);
        assert(std::abs(clipped[0] - .95f) < .002f && std::abs(clipped[1] - .3f) < .002f);

        // Source coverage changes with the actual Halton projection. Resolve
        // lands on a stable grid and reduces diagonal high-frequency flicker.
        world_temporal_reset(temporal);
        constexpr int frames = 40;
        std::vector<double> raw_sum(extent * extent), raw_square(extent * extent), resolved_sum(extent * extent), resolved_square(extent * extent);
        for (int n = 0; n < frames; ++n) {
            world_temporal_jitter(n, frame.jitter_pixels); projection(frame);
            for (int y = 0; y < extent; ++y) for (int x = 0; x < extent; ++x) {
                float value = (x + .5f - frame.jitter_pixels[0]) + .41f * (y + .5f - frame.jitter_pixels[1]) > extent * .707f + .23f ? 1.f : 0.f;
                input.pixels[y * extent + x] = {value, value, value, 1};
            }
            input.upload();
            auto resolved = read_color(world_temporal_resolve(temporal, frame), extent);
            if (n < 8) continue;
            for (int k = 0; k < extent * extent; ++k) {
                raw_sum[k] += input.pixels[k][0]; raw_square[k] += input.pixels[k][0] * input.pixels[k][0];
                resolved_sum[k] += resolved[k][0]; resolved_square[k] += resolved[k][0] * resolved[k][0];
            }
        }
        double raw_variance = 0, resolved_variance = 0;
        for (int k = 0; k < extent * extent; ++k) {
            raw_variance += raw_square[k] / 32 - std::pow(raw_sum[k] / 32, 2);
            resolved_variance += resolved_square[k] / 32 - std::pow(resolved_sum[k] / 32, 2);
        }
        assert(raw_variance > 1 && resolved_variance < raw_variance * .4);
        assert(resolved_sum[0] < .001 && resolved_sum.back() > 31.99);
        std::printf("Diagonal temporal variance: raw %.6f, resolved %.6f\n", raw_variance, resolved_variance);

        // Dense subpixel leaves repeatedly expose the ground between blades.
        // Use the production Halton sequence, distinct blade/ground depth,
        // foliage reactivity and a small wind displacement. The unfiltered
        // 2x2-depth rule used to reject almost every history sample here.
        world_temporal_reset(temporal);
        frame.reactive_alpha = true;
        std::fill(raw_sum.begin(), raw_sum.end(), 0);
        std::fill(raw_square.begin(), raw_square.end(), 0);
        std::fill(resolved_sum.begin(), resolved_sum.end(), 0);
        std::fill(resolved_square.begin(), resolved_square.end(), 0);
        double accumulated_history = 0;
        for (int n = 0; n < frames; ++n) {
            world_temporal_jitter(n, frame.jitter_pixels); projection(frame);
            float wind = .12f * std::sin(n * .13f);
            for (int y = 0; y < extent; ++y) for (int x = 0; x < extent; ++x) {
                float px = x + .5f - frame.jitter_pixels[0], py = y + .5f - frame.jitter_pixels[1];
                float stripe = (px + .23f * py + wind) / 2.3f;
                bool blade = stripe - std::floor(stripe) < .34f + .12f * std::sin(py * .37f);
                float value = blade ? .65f : .15f;
                int k = y * extent + x;
                input.pixels[k] = {value * .5f, value, value * .25f, blade ? .45f : 1.f};
                input.depths[k] = device_depth(blade ? 6.f : 8.f);
            }
            input.upload();
            auto resolved = read_color(world_temporal_resolve(temporal, frame), extent);
            if (n < 8) continue;
            for (int y = 2; y < extent - 2; ++y) for (int x = 2; x < extent - 2; ++x) {
                int k = y * extent + x;
                raw_sum[k] += input.pixels[k][1]; raw_square[k] += input.pixels[k][1] * input.pixels[k][1];
                resolved_sum[k] += resolved[k][1]; resolved_square[k] += resolved[k][1] * resolved[k][1];
                accumulated_history += resolved[k][3] > 1.5f ? 1 : 0;
            }
        }
        raw_variance = resolved_variance = 0;
        for (int k = 0; k < extent * extent; ++k) {
            raw_variance += raw_square[k] / 32 - std::pow(raw_sum[k] / 32, 2);
            resolved_variance += resolved_square[k] / 32 - std::pow(resolved_sum[k] / 32, 2);
        }
        std::printf("Dense coverage temporal variance: raw %.6f, resolved %.6f; history reuse %.2f%%\n",
                    raw_variance, resolved_variance, accumulated_history / (32 * (extent - 4) * (extent - 4)) * 100);
        assert(raw_variance > 10 && resolved_variance < raw_variance * .12);
        assert(accumulated_history > 32 * (extent - 4) * (extent - 4) * .95);
        frame.reactive_alpha = false;

        // Frozen, high-contrast leaves against sky still receive the real
        // eight projection phases. Their supported subpixel area must converge
        // rather than alternate whole leaf/sky histories on every sample.
        world_temporal_reset(temporal);
        std::fill(raw_sum.begin(), raw_sum.end(), 0);
        std::fill(raw_square.begin(), raw_square.end(), 0);
        std::fill(resolved_sum.begin(), resolved_sum.end(), 0);
        std::fill(resolved_square.begin(), resolved_square.end(), 0);
        frame.reactive_alpha = true;
        for (int n = 0; n < frames; ++n) {
            world_temporal_jitter(n, frame.jitter_pixels); projection(frame);
            for (int y = 0; y < extent; ++y) for (int x = 0; x < extent; ++x) {
                float px=x+.5f-frame.jitter_pixels[0],py=y+.5f-frame.jitter_pixels[1];
                float stripe=(px+.23f*py)/2.3f;
                bool leaf=stripe-std::floor(stripe)<.34f+.12f*std::sin(py*.37f);
                int k=y*extent+x;
                input.pixels[k]=leaf?Pixel{.08f,.25f,.04f,.98f}:Pixel{.6f,.8f,1.f,1.f};
                input.depths[k]=device_depth(leaf?6.f:0.f);
            }
            input.upload();auto resolved=read_color(world_temporal_resolve(temporal,frame),extent);
            if(n<8)continue;
            for(int y=2;y<extent-2;++y)for(int x=2;x<extent-2;++x) {
                int k=y*extent+x;
                raw_sum[k]+=input.pixels[k][1];raw_square[k]+=input.pixels[k][1]*input.pixels[k][1];
                resolved_sum[k]+=resolved[k][1];resolved_square[k]+=resolved[k][1]*resolved[k][1];
            }
        }
        raw_variance=resolved_variance=0;
        for(int k=0;k<extent*extent;++k) {
            raw_variance+=raw_square[k]/32-std::pow(raw_sum[k]/32,2);
            resolved_variance+=resolved_square[k]/32-std::pow(resolved_sum[k]/32,2);
        }
        std::printf("Frozen leaf/sky coverage variance: raw %.6f, resolved %.6f\n",raw_variance,resolved_variance);
        // This held subpixel coverage must converge through all eight phases.
        // A 1.15-sigma box and ordinary sky confidence pass the looser2%
        // bound while still producing a visible periodic crawl.
        assert(raw_variance>10&&resolved_variance<raw_variance*.003);
        // Once vegetation disappears from the support footprint, its former
        // layer must be rejected immediately even when tagged foliage before.
        for(Pixel &pixel:input.pixels)pixel={.1f,.2f,.3f,1.f};
        std::fill(input.depths.begin(),input.depths.end(),device_depth(12));
        input.upload();expect_current(center(temporal,frame),input);
        frame.jitter_pixels[0]=frame.jitter_pixels[1]=0;projection(frame);
        world_temporal_reset(temporal);
        for(int n=0;n<17;++n) {
            for(int y=0;y<extent;++y)for(int x=0;x<extent;++x) {
                bool leaf=x>=(n<16?20:24)&&x<=(n<16?30:34);
                int k=y*extent+x;
                input.pixels[k]=leaf?Pixel{.08f,.25f,.04f,.45f}:Pixel{.6f,.8f,1.f,1.f};
                input.depths[k]=device_depth(leaf?6.f:0.f);
            }
            input.upload();auto resolved=read_color(world_temporal_resolve(temporal,frame),extent);
            if(n==16) {
                const auto &trail=resolved[(extent/2)*extent+20];
                for(int k=0;k<3;++k)assert(std::fabs(trail[k]-input.pixels[(extent/2)*extent+20][k])<.001f);
                assert(trail[3]==1.f);
            }
        }
        // Animated cloud alpha also lies between .68 and1. It must never
        // tag sky or an opaque wall/sky color interpolation as vegetation.
        world_temporal_reset(temporal);
        for(Pixel &pixel:input.pixels)pixel={1.f,0,0,1.f};
        std::fill(input.depths.begin(),input.depths.end(),device_depth(4));
        input.upload();center(temporal,frame);
        for(Pixel &pixel:input.pixels)pixel={0,.2f,1.f,.7f};
        std::fill(input.depths.begin(),input.depths.end(),1.f);
        input.pixels[(extent/2)*extent+extent/2+1]={1.f,0,0,1.f};
        input.depths[(extent/2)*extent+extent/2+1]=device_depth(4);
        input.upload();expect_current(center(temporal,frame),input);
        frame.reactive_alpha=false;
        std::puts("PASS cutout responsiveness: unsupported old foliage disappears immediately; a four-pixel moving leaf leaves no trail beyond its one-pixel support");

        // Raster depth is linear in screen coordinates on an oblique plane.
        // The color resolve reconstructs the unjittered grid; its depth must
        // reconstruct that identical grid as well. Nearest jittered depth
        // incorrectly changes every sample phase and rejects static history.
        world_temporal_reset(temporal);
        double plane_depth_error = 0, plane_history = 0;
        std::fill(raw_sum.begin(), raw_sum.end(), 0);
        std::fill(raw_square.begin(), raw_square.end(), 0);
        std::fill(resolved_sum.begin(), resolved_sum.end(), 0);
        std::fill(resolved_square.begin(), resolved_square.end(), 0);
        constexpr float depth_dx = .0011f, depth_dy = .00047f;
        for (int n = 0; n < frames; ++n) {
            world_temporal_jitter(n, frame.jitter_pixels); projection(frame);
            for (int y = 0; y < extent; ++y) for (int x = 0; x < extent; ++x) {
                float px = x + .5f - extent * .5f - frame.jitter_pixels[0];
                float py = y + .5f - extent * .5f - frame.jitter_pixels[1];
                float value = .4f + .25f * std::sin(px * 1.3f + py * .6f);
                int k = y * extent + x;
                input.pixels[k] = {value, value, value, 1};
                input.depths[k] = device_depth(8) + px * depth_dx + py * depth_dy;
            }
            input.upload();
            auto resolved = read_color(world_temporal_resolve(temporal, frame), extent);
            auto depths = read_depth(world_temporal_depth(temporal, 0), extent);
            if (n < 8) continue;
            for (int y = 12; y < extent - 12; ++y) for (int x = 12; x < extent - 12; ++x) {
                int k = y * extent + x;
                double expected = device_depth(8) + (x + .5f - extent * .5f) * depth_dx + (y + .5f - extent * .5f) * depth_dy;
                plane_depth_error = std::max(plane_depth_error, std::abs(depths[k] - expected));
                plane_history += resolved[k][3] > 1.5f ? 1 : 0;
                raw_sum[k] += input.pixels[k][0]; raw_square[k] += input.pixels[k][0] * input.pixels[k][0];
                resolved_sum[k] += resolved[k][0]; resolved_square[k] += resolved[k][0] * resolved[k][0];
            }
        }
        raw_variance = resolved_variance = 0;
        for (int k = 0; k < extent * extent; ++k) {
            raw_variance += raw_square[k] / 32 - std::pow(raw_sum[k] / 32, 2);
            resolved_variance += resolved_square[k] / 32 - std::pow(resolved_sum[k] / 32, 2);
        }
        std::printf("Oblique plane canonical depth max error %.9f, history reuse %.2f%%, raw variance %.6f resolved %.6f\n", plane_depth_error, plane_history / (32 * 40 * 40) * 100, raw_variance, resolved_variance);
        assert(plane_depth_error < 2.e-6);
        assert(plane_history > 32 * 40 * 40 * .95);

        // Depth reconstruction must not fabricate surfaces at silhouettes,
        // sky boundaries or a one-pixel-wide leaf. A flat wall beside ground
        // takes its zero same-surface slope; an isolated leaf takes none.
        world_temporal_reset(temporal);
        double silhouette_error = 0;
        for (int n = 0; n < 8; ++n) {
            world_temporal_jitter(n, frame.jitter_pixels); projection(frame);
            for (int y = 0; y < extent; ++y) for (int x = 0; x < extent; ++x) {
                float distance = y < 16 ? (x < 32 ? 4.f : 0.f) :
                                 y < 32 ? (x < 32 ? 4.f : 8.f) :
                                 (x == 32 ? 4.f : 8.f);
                int k = y * extent + x;
                input.pixels[k] = {distance == 4.f ? 1.f : 0.f, distance == 8.f ? 1.f : 0.f, distance == 0.f ? 1.f : 0.f, 1};
                input.depths[k] = device_depth(distance);
            }
            input.upload(); world_temporal_resolve(temporal, frame);
            auto depths = read_depth(world_temporal_depth(temporal, 0), extent);
            for (int y = 2; y < extent - 2; ++y) for (int x = 2; x < extent - 2; ++x) {
                int k = y * extent + x;
                silhouette_error = std::max(silhouette_error, double(std::abs(depths[k] - input.depths[k])));
            }
        }
        assert(silhouette_error < 2.e-7);
        assert(resolved_variance < raw_variance * .001);
        std::printf("Silhouette/sky/one-pixel leaf depth max error %.9f\n", silhouette_error);

        Inputs small(32);
        small.pattern(1, 4); frame.color = small.color; frame.depth = small.depth;
        frame.width = frame.height = 32; frame.jitter_pixels[0] = frame.jitter_pixels[1] = 0;
        projection(frame); expect_current(center(temporal, frame), small);
        GLuint last_color = world_temporal_output(temporal, 0), last_depth = world_temporal_depth(temporal, 0);
        assert(last_color && last_depth && glIsTexture(last_color) && glIsTexture(last_depth));
        assert(world_temporal_output(temporal, 99) == last_color); // held view: no resolve
        std::fill(std::begin(frame.view_projection), std::end(frame.view_projection), 0.f);
        assert(world_temporal_resolve(temporal, frame) == frame.color);
        assert(world_temporal_output(temporal, 99) == 99 && world_temporal_depth(temporal, 88) == 88);
        world_temporal_free(temporal); temporal = nullptr;
        assert(!glIsTexture(last_color) && !glIsTexture(last_depth));
    }
    world_temporal_reset(nullptr); world_temporal_free(nullptr);
    assert(glGetError() == GL_NO_ERROR);
    SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
    std::puts("PASS temporal GL: actual shaders, history accumulation, reactive pixels, valid-depth reconstruction, foreground halo rejection, depth/sky disocclusion, dense coverage stability, cuts, bounds, clamp, textured oblique-plane convergence, layer-safe canonical depth, resize, held output and teardown");
}
