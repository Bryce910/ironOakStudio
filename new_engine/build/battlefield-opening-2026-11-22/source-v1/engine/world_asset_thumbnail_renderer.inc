// Included in the viewer's namespace after M4/Program. All GL calls stay on
// the owner thread. Worker preparation never accesses the shared GL cache.
class AssetThumbnailCache {
public:
    enum class Status {Queued,Preparing,Uploading,Ready,Unavailable};
    struct Result {unsigned texture=0;Status status=Status::Queued;const char *message="Preview queued";};
    struct Stats {unsigned tiles=0,pending=0,unavailable=0;std::size_t bytes=0;std::uint64_t rendered=0;};
private:
    struct Entry {
        std::string stem,error;GLuint texture=0;Status status=Status::Queued;
        std::uint64_t touched=0,registry=0,fingerprint=0;bool priority=false;
        std::uintmax_t file_bytes=0;std::filesystem::file_time_type modified{};
    };
    struct Prepared {std::shared_ptr<const world_model_cpu::Asset> asset;std::string error;std::uint64_t fingerprint=0;};
    struct GenericAttributes {
        // Draws can leave current values of enabled arrays unspecified. The
        // engine's corresponding source attributes are floating point. Index0
        // is always the enabled position stream; querying its current value is
        // not portable in compatibility contexts, so it is deliberately omitted.
        static constexpr unsigned indices[8]={1,2,3,4,5,6,7,12};
        GLfloat values[8][4]{};
        PFNGLGETVERTEXATTRIBFVPROC get=nullptr;
        PFNGLVERTEXATTRIB4FVPROC set=nullptr;
        GenericAttributes(){
            get=reinterpret_cast<PFNGLGETVERTEXATTRIBFVPROC>(SDL_GL_GetProcAddress("glGetVertexAttribfv"));
            set=reinterpret_cast<PFNGLVERTEXATTRIB4FVPROC>(SDL_GL_GetProcAddress("glVertexAttrib4fv"));
            if(get&&set)for(unsigned i=0;i<8;++i)get(indices[i],GL_CURRENT_VERTEX_ATTRIB,values[i]);
        }
        bool valid()const{return get&&set;}
        ~GenericAttributes(){if(valid())for(unsigned i=0;i<8;++i)set(indices[i],values[i]);}
    };
    // Every binding altered by stage_step/world_model_draw is restored. The
    // thumbnail pass owns its program, FBO and renderbuffer; it never mutates
    // scene programs/uniforms, existing instance streams, camera or materials.
    struct GLState {
        GLint program=0,vao=0,array=0,draw_fbo=0,read_fbo=0,renderbuffer=0,active=0,unpack_buffer=0;
        GLint viewport[4]{},scissor[4]{},depth_func=0,blend_src_rgb=0,blend_dst_rgb=0,blend_src_alpha=0,blend_dst_alpha=0,blend_eq_rgb=0,blend_eq_alpha=0;
        GLint unpack[8]{},textures[8]{},samplers[8]{};
        GLfloat clear_color[4]{};GLdouble clear_depth=1,depth_range[2]{};GLboolean depth_mask=GL_TRUE,color_mask[4]{};
        GLint polygon_mode[2]{};
        GLboolean depth=0,blend=0,cull=0,scissor_on=0,srgb=0,multisample=0,dither=0,raster_discard=0,polygon_offset=0,alpha_coverage=0,sample_coverage=0,sample_mask=0;
        static constexpr GLenum units[8]={GL_TEXTURE0,GL_TEXTURE1,GL_TEXTURE2,GL_TEXTURE3,GL_TEXTURE4,GL_TEXTURE8,GL_TEXTURE9,GL_TEXTURE10};
        static constexpr GLenum unpack_names[8]={GL_UNPACK_ALIGNMENT,GL_UNPACK_ROW_LENGTH,GL_UNPACK_SKIP_PIXELS,GL_UNPACK_SKIP_ROWS,GL_UNPACK_IMAGE_HEIGHT,GL_UNPACK_SKIP_IMAGES,GL_UNPACK_SWAP_BYTES,GL_UNPACK_LSB_FIRST};
        GLState(){
            glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&array);
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw_fbo);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read_fbo);glGetIntegerv(GL_RENDERBUFFER_BINDING,&renderbuffer);
            glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&unpack_buffer);
            glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_SCISSOR_BOX,scissor);glGetIntegerv(GL_DEPTH_FUNC,&depth_func);
            glGetBooleanv(GL_DEPTH_WRITEMASK,&depth_mask);glGetBooleanv(GL_COLOR_WRITEMASK,color_mask);glGetFloatv(GL_COLOR_CLEAR_VALUE,clear_color);glGetDoublev(GL_DEPTH_CLEAR_VALUE,&clear_depth);glGetDoublev(GL_DEPTH_RANGE,depth_range);glGetIntegerv(GL_POLYGON_MODE,polygon_mode);
            glGetIntegerv(GL_BLEND_SRC_RGB,&blend_src_rgb);glGetIntegerv(GL_BLEND_DST_RGB,&blend_dst_rgb);glGetIntegerv(GL_BLEND_SRC_ALPHA,&blend_src_alpha);glGetIntegerv(GL_BLEND_DST_ALPHA,&blend_dst_alpha);
            glGetIntegerv(GL_BLEND_EQUATION_RGB,&blend_eq_rgb);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&blend_eq_alpha);
            depth=glIsEnabled(GL_DEPTH_TEST);blend=glIsEnabled(GL_BLEND);cull=glIsEnabled(GL_CULL_FACE);scissor_on=glIsEnabled(GL_SCISSOR_TEST);srgb=glIsEnabled(GL_FRAMEBUFFER_SRGB);multisample=glIsEnabled(GL_MULTISAMPLE);dither=glIsEnabled(GL_DITHER);raster_discard=glIsEnabled(GL_RASTERIZER_DISCARD);polygon_offset=glIsEnabled(GL_POLYGON_OFFSET_FILL);alpha_coverage=glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE);sample_coverage=glIsEnabled(GL_SAMPLE_COVERAGE);sample_mask=glIsEnabled(GL_SAMPLE_MASK);
            for(unsigned i=0;i<8;++i)glGetIntegerv(unpack_names[i],&unpack[i]);
            for(unsigned i=0;i<8;++i){glActiveTexture(units[i]);glGetIntegerv(GL_TEXTURE_BINDING_2D,&textures[i]);glGetIntegerv(GL_SAMPLER_BINDING,&samplers[i]);}
            glActiveTexture(GLenum(active));
        }
        static void enable(GLenum cap,GLboolean on){if(on)glEnable(cap);else glDisable(cap);}
        ~GLState(){
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(draw_fbo));glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(read_fbo));glBindRenderbuffer(GL_RENDERBUFFER,GLuint(renderbuffer));
            glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glScissor(scissor[0],scissor[1],scissor[2],scissor[3]);
            glDepthFunc(GLenum(depth_func));glDepthMask(depth_mask);glDepthRange(depth_range[0],depth_range[1]);glPolygonMode(GL_FRONT_AND_BACK,GLenum(polygon_mode[0]));glColorMask(color_mask[0],color_mask[1],color_mask[2],color_mask[3]);glClearColor(clear_color[0],clear_color[1],clear_color[2],clear_color[3]);glClearDepth(clear_depth);
            glBlendFuncSeparate(GLenum(blend_src_rgb),GLenum(blend_dst_rgb),GLenum(blend_src_alpha),GLenum(blend_dst_alpha));glBlendEquationSeparate(GLenum(blend_eq_rgb),GLenum(blend_eq_alpha));
            enable(GL_DEPTH_TEST,depth);enable(GL_BLEND,blend);enable(GL_CULL_FACE,cull);enable(GL_SCISSOR_TEST,scissor_on);enable(GL_FRAMEBUFFER_SRGB,srgb);enable(GL_MULTISAMPLE,multisample);enable(GL_DITHER,dither);enable(GL_RASTERIZER_DISCARD,raster_discard);enable(GL_POLYGON_OFFSET_FILL,polygon_offset);enable(GL_SAMPLE_ALPHA_TO_COVERAGE,alpha_coverage);enable(GL_SAMPLE_COVERAGE,sample_coverage);enable(GL_SAMPLE_MASK,sample_mask);
            for(unsigned i=0;i<8;++i)glPixelStorei(unpack_names[i],unpack[i]);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER,GLuint(unpack_buffer));
            for(unsigned i=0;i<8;++i){glActiveTexture(units[i]);glBindTexture(GL_TEXTURE_2D,GLuint(textures[i]));glBindSampler(unsigned(units[i]-GL_TEXTURE0),GLuint(samplers[i]));}
            glActiveTexture(GLenum(active));glBindVertexArray(GLuint(vao));glBindBuffer(GL_ARRAY_BUFFER,GLuint(array));glUseProgram(GLuint(program));
        }
    };
    std::vector<Entry> entries;
    std::string project,busy,placement;
    std::future<Prepared> worker;
    std::atomic<bool> stopping{false};std::atomic<std::uint64_t> generation{1};
    std::uint64_t frame=0,placement_frame=0,rendered=0,busy_registry=0,busy_generation=0;
    WorldModelStaged *stage=nullptr;WorldModelAsset *model=nullptr;
    std::string model_stem;
    GLuint program=0,fbo=0,depth_buffer=0,depth_neutral=0,coverage_fbo=0,coverage_color=0;
    bool shader_failed=false,invalidate_pending=false;
    Entry *find(std::string_view stem){for(auto &e:entries)if(e.stem==stem)return &e;return nullptr;}
    static const char *status_text(Status s){switch(s){case Status::Preparing:return "Preparing model";case Status::Uploading:return "Uploading preview";case Status::Ready:return "Textured model preview";case Status::Unavailable:return "Preview unavailable";default:return "Preview queued";}}
    static std::uint64_t source_fingerprint(const std::string &path,const world_model_cpu::Options &options){
        std::error_code error;const auto size=std::filesystem::file_size(path,error);if(error||size<20||size>128ull*1024*1024)return 0;
        std::ifstream file(path,std::ios::binary);if(!file)return 0;std::array<char,65536> bytes{};std::uint64_t hash=14695981039346656037ull;
        while(file){if(options.cancelled())return 0;file.read(bytes.data(),bytes.size());for(std::streamsize i=0;i<file.gcount();++i){hash^=static_cast<unsigned char>(bytes[std::size_t(i)]);hash*=1099511628211ull;}}
        return file.eof()?hash:0;
    }
    static Prepared prepare(const std::string &path,world_model_cpu::Options options){
        Prepared out;out.fingerprint=source_fingerprint(path,options);if(!out.fingerprint){out.error="Cannot read model source";return out;}
        out.asset=world_model_cpu::prepare(path.c_str(),options,out.error);if(!out.asset)return out;
        if(source_fingerprint(path,options)!=out.fingerprint){out.asset.reset();out.error="Model changed while preparing. Refresh its registry to retry.";return out;}
        // This transaction owns the newly prepared asset exclusively. Thumbnail
        // images use real source mipmaps, reduced to256px. Different prepared
        // comparison bytes prevent accidentally replacing a full-size scene map.
        auto asset=std::const_pointer_cast<world_model_cpu::Asset>(out.asset);
        asset->texture_bytes=0;
        std::vector<bool> masked(asset->textures.size(),false);
        for(const auto &mesh:asset->meshes)if(mesh.facade.mode==0&&mesh.cutoff>0&&mesh.factor[3]>=mesh.cutoff&&mesh.textures[0]>=0)
            masked[std::size_t(mesh.textures[0])]=true;
        for(std::size_t texture_index=0;texture_index<asset->textures.size();++texture_index){
            auto &t=asset->textures[texture_index];
            if(options.cancelled()){out.asset.reset();out.error="Preview cancelled";return out;}
            if(t.compressed||t.levels.empty()){out.asset.reset();out.error="Unsupported preview image";return out;}
            const auto alpha=masked[texture_index]?world_asset_thumbnail::alpha_reference(t.levels.front().rgba):world_asset_thumbnail::AlphaReference{};
            if(masked[texture_index])for(std::size_t mip=1;mip<t.levels.size();++mip){
                const auto &from=t.levels[mip-1];auto &to=t.levels[mip];
                if(options.cancelled()||!world_asset_thumbnail::alpha_weighted_mip(from.rgba,from.width,from.height,to.rgba,to.width,to.height,t.srgb)){out.asset.reset();out.error="Preview color coverage preparation failed";return out;}
            }
            std::size_t first=0;while(first+1<t.levels.size()&&(t.levels[first].width>world_asset_thumbnail::texture_side||t.levels[first].height>world_asset_thumbnail::texture_side))++first;
            if(first)t.levels.erase(t.levels.begin(),t.levels.begin()+std::ptrdiff_t(first));
            if(masked[texture_index])for(auto &level:t.levels){
                if(options.cancelled()||!world_asset_thumbnail::preserve_alpha(level.rgba,alpha,std::uint32_t(t.fingerprint))){out.asset.reset();out.error="Preview alpha coverage preparation failed";return out;}
            }
            // Destroy erased level buffers/capacity before queueing GL work.
            t.levels.shrink_to_fit();
            const unsigned char marker[]={0,'V','A','L','W','O','O','D','_','T','H','U','M','B',3,0,1,std::uint8_t(masked[texture_index])};
            // Domain prefix cannot be a valid source PNG/JPEG header, even
            // when a publisher leaves arbitrary trailing bytes in an image.
            t.encoded.insert(t.encoded.begin(),std::begin(marker),std::end(marker));
            t.cache_key=std::make_shared<world_model_cpu::CacheKey>();t.cache_key->prepared=texture_cache_prepare(t.encoded.data(),t.encoded.size());
            if(!t.cache_key->prepared){out.asset.reset();out.error="Preview image key budget exceeded";return out;}
            for(const auto &l:t.levels)asset->texture_bytes+=l.rgba.size();
        }
        asset->gpu_bytes=asset->upload_bytes=asset->geometry_bytes+asset->texture_bytes;
        if(asset->gpu_bytes>world_asset_thumbnail::gpu_job_limit){out.asset.reset();out.error="Preview exceeds the 64 MiB model budget. Inspect still works.";}
        return out;
    }
    bool shaders(){
        if(program)return true;
        if(shader_failed)return false;
        const char *vs=R"GLSL(#version 330 core
layout(location=0)in vec3 aPos;layout(location=1)in vec3 aNormal;layout(location=2)in vec2 aUV;layout(location=3)in vec4 aTangent;
layout(location=4)in vec4 aInstance;layout(location=5)in float aYaw;layout(location=7)in float aFacadePaint;
uniform mat4 uVP;uniform bool uSourcePaintEnabled;out vec3 vPosition,vSourcePosition,vNormal,vTangent,vBitangent,vPaint;out vec2 vUV;out float vFacadePaint;
void main(){float c=cos(aYaw),s=sin(aYaw);mat3 r=mat3(c,0,-s,0,1,0,s,0,c);vec3 n=normalize(r*aNormal);vec3 t=normalize(r*aTangent.xyz);vPosition=r*aPos*aInstance.w+aInstance.xyz;vSourcePosition=aPos;vNormal=n;vTangent=t;vBitangent=cross(n,t)*aTangent.w;vUV=aUV;uint p=uint(aFacadePaint);vPaint=uSourcePaintEnabled?vec3(float(p&255u),float((p>>8u)&255u),float((p>>16u)&255u))/255.:vec3(0);vFacadePaint=uSourcePaintEnabled?vPaint.r:aFacadePaint;gl_Position=uVP*vec4(vPosition,1);}
)GLSL";
        const char *fs=R"GLSL(#version 330 core
in vec3 vPosition,vSourcePosition,vNormal,vTangent,vBitangent,vPaint;in vec2 vUV;in float vFacadePaint;out vec4 color;
uniform sampler2D uBase,uNormal,uARM,uEmissive,uFacadeAux0,uFacadeAux1,uFacadeAux2,uSceneDepth;
uniform bool uHasBase,uHasNormal,uHasARM,uHasEmissive,uDisplayGamma,uDepthOcclusion,uThumbnailCoverage;uniform vec4 uBaseFactor,uPreviewViewport;uniform vec2 uPreviewNearFar;uniform float uRoughness,uMetallic,uAlphaCutoff,uNormalScale,uOpacity;
uniform vec3 uEmissiveFactor,uEye,uFacadeUVScale,uWinterSurface;uniform int uFacadeMode;
vec2 uv(float k){return vec2(vUV.x*k,1.-(1.-vUV.y)*k);}vec4 tex(sampler2D t,float k){return texture(t,uv(k));}
float heightWeight(float h,float phase,float contrast){return clamp((h+2.*clamp(phase,0.,1.)-1.)*(1.+2.*contrast)-contrast,0.,1.);}
vec3 nxy(vec2 xy){vec2 n=xy*2.-1.;return vec3(n,sqrt(max(1.-dot(n,n),.00001)));}
float hash(vec2 p){vec3 q=fract(vec3(p.xyx)*.1031);q+=dot(q,q.yzx+33.33);return fract((q.x+q.y)*q.z);}
float noise(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+1.),f.x),f.y);}
// Source winter finish in neutral studio coordinates. The scene's mountain
// elevation mask and world lighting are intentionally outside this inspection.
void winterMaterial(vec3 geometricN,inout vec3 normal,inout vec3 base,inout float rough,inout float metal,inout float ao){
 vec3 p=vSourcePosition;float coverage=uWinterSurface.x,ice=uWinterSurface.y;
 if(coverage>0.){float patch=noise(p.xz*.47+vec2(p.y*.28,-p.y*.13));float cap=smoothstep(.40,.76,geometricN.y+(patch-.5)*.31)*coverage;vec3 powder=vec3(.74,.81,.88)*mix(.94,1.02,noise(p.xz*.16));base=mix(base,powder,cap);rough=mix(rough,.89,cap);metal*=1.-cap;normal=normalize(mix(normal,geometricN,cap*.76));ao=mix(ao,1.,cap*.6);}
 if(ice>0.){float veins=noise(vec2(p.x*.71+p.z*.47,p.y*.13)),bubbles=noise(p.xy*19.1+p.z*3.7);float frost=smoothstep(.49,.82,veins*.55+bubbles*.45)*.42;frost=max(frost,smoothstep(.37,.85,geometricN.y)*.72);float fractures=0.;for(int i=0;i<3;i++){float k=float(i);float line=abs(sin(dot(p,vec3(.83+k*.41,.16+k*.071,.37-k*.11))*4.1+sin(p.y*.73+k)*.4));float aa=max(fwidth(line),.008);fractures=max(fractures,(1.-smoothstep(.008,.018+aa,line))*(.35+.35*noise(p.xy*.7+k)));}frost=max(frost,fractures*.66);vec3 interior=mix(vec3(.23,.35,.39),vec3(.36,.49,.52),veins);base=mix(base,mix(interior,vec3(.66,.77,.81),frost),ice);normal=normalize(normal+vec3(sin(p.x*27.+p.y*.4),cos(p.y*19.+p.z*11.),sin(p.z*24.+p.y*.7))*.034*ice);rough=mix(rough,mix(.21,.57,frost),ice);metal*=1.-ice;ao=mix(ao,.91+.09*bubbles,ice);}
}
void main(){
 if(uDepthOcclusion){vec2 at=(gl_FragCoord.xy-uPreviewViewport.xy)/uPreviewViewport.zw;ivec2 size=textureSize(uSceneDepth,0);ivec2 pixel=clamp(ivec2(at*vec2(size)),ivec2(0),size-1);float depth=texelFetch(uSceneDepth,pixel,0).r;if(depth<1.){float n=uPreviewNearFar.x,f=uPreviewNearFar.y;float sceneRange=n*f/max(f-depth*(f-n),.000001),ghostRange=n*f/max(f-gl_FragCoord.z*(f-n),.000001);if(ghostRange>sceneRange+max(.02,sceneRange*.00001))discard;}}
 vec4 b=(uHasBase?texture(uBase,vUV):vec4(1))*uBaseFactor;vec3 normalMap=uHasNormal?texture(uNormal,vUV).xyz*2.-1.:vec3(0,0,1);vec3 arm=uHasARM?texture(uARM,vUV).rgb:vec3(1);
 if(uFacadeMode>0){
   b=tex(uBase,uFacadeUVScale.x)*uBaseFactor;b.a=1.;normalMap=tex(uNormal,uFacadeUVScale.x).xyz*2.-1.;arm=tex(uARM,uFacadeMode==2?uFacadeUVScale.y:uFacadeUVScale.x).rgb;
   if(uFacadeMode==1){float w=clamp(vFacadePaint,0.,1.)*tex(uFacadeAux1,uFacadeUVScale.z).g;b.rgb=mix(b.rgb,tex(uFacadeAux0,uFacadeUVScale.y).rgb*uBaseFactor.rgb,w);}
   if(uFacadeMode==2){float w=clamp((tex(uFacadeAux2,uFacadeUVScale.z).b+2.*clamp(vFacadePaint,0.,1.)-1.)*3.-1.,0.,1.);normalMap=mix(normalMap,tex(uFacadeAux0,uFacadeUVScale.y).xyz*2.-1.,w);arm=mix(arm,tex(uFacadeAux1,uFacadeUVScale.x).rgb,w);}
   if(uFacadeMode==3){
     float red=texture(uFacadeAux2,vUV).r;float w=heightWeight(tex(uFacadeAux2,uFacadeUVScale.x).b,vPaint.g,red),w2=heightWeight(red,vPaint.g,1.);
     b.rgb=mix(mix(tex(uBase,uFacadeUVScale.x).rgb,tex(uARM,uFacadeUVScale.y).rgb,w),tex(uFacadeAux1,uFacadeUVScale.z).rgb,w2)*uBaseFactor.rgb;
     float nr=heightWeight(tex(uFacadeAux2,uFacadeUVScale.x).b,vPaint.r,1.);normalMap=mix(nxy(tex(uNormal,uFacadeUVScale.x).rg),nxy(tex(uFacadeAux0,uFacadeUVScale.y).rg),nr);normalMap=mix(normalMap,nxy(tex(uFacadeAux0,uFacadeUVScale.z).rg),w2);
     vec4 ma=tex(uNormal,uFacadeUVScale.y),mb=tex(uFacadeAux0,uFacadeUVScale.x),mc=tex(uNormal,uFacadeUVScale.z);arm=mix(mix(vec3(ma.a,ma.b,0),vec3(mb.a,mb.b,0),nr),vec3(mc.a,mc.b,0),w2);
   }
   if(uFacadeMode==4){float h=tex(uEmissive,uFacadeUVScale.y).r;float r=heightWeight(h,vPaint.r,-.047619),g=heightWeight(h,vPaint.g,-.3),cg=heightWeight(.2158605,vPaint.g,-.3);vec3 w=vec3((1.-r)*(1.-g),r*(1.-g),g),cw=vec3((1.-r)*(1.-cg),r*(1.-cg),cg);vec4 a=tex(uNormal,uFacadeUVScale.x),d=tex(uFacadeAux0,uFacadeUVScale.y),e=tex(uFacadeAux2,uFacadeUVScale.x);b.rgb=(tex(uBase,uFacadeUVScale.x).rgb*cw.x+tex(uARM,uFacadeUVScale.y).rgb*cw.y+tex(uFacadeAux1,uFacadeUVScale.y).rgb*cw.z)*uBaseFactor.rgb;normalMap=nxy(a.rg)*w.x+nxy(d.rg)*w.y+nxy(e.rg)*w.z;arm=vec3(a.a*w.x+d.a*w.y+e.a*w.z,a.b*w.x+e.b*(w.y+w.z),0);}
 }
 float maskCoverage=1.;
 if(uAlphaCutoff>0.){float width=clamp(fwidth(b.a),1./255.,.20);float coverage=clamp((b.a-uAlphaCutoff)/width+.5,0.,1.);if(uHasBase&&uFacadeMode==0&&uBaseFactor.a>=uAlphaCutoff){vec2 texels=vec2(textureSize(uBase,0));vec2 dx=dFdx(vUV)*texels,dy=dFdy(vUV)*texels;float subpixel=smoothstep(64.,1024.,max(dot(dx,dx),dot(dy,dy)));coverage=mix(coverage,clamp(b.a,0.,1.),subpixel);}maskCoverage=coverage;if(uThumbnailCoverage){if(coverage<=0.)discard;}else{float rank=fract(52.9829189*fract(dot(floor(gl_FragCoord.xy),vec2(.06711056,.00583715))));if(coverage<=0.||rank>=coverage)discard;}}
 vec3 geometricN=normalize(vNormal),n=geometricN;vec3 t=normalize(vTangent-n*dot(vTangent,n));vec3 bt=normalize(vBitangent);normalMap.xy*=uNormalScale;n=normalize(mat3(t,bt,n)*normalize(normalMap));if(!gl_FrontFacing){n=-n;geometricN=-geometricN;}
 float rough=clamp(uRoughness*arm.g,.2,1.),metal=clamp(uMetallic*arm.b,0.,1.),ao=clamp(arm.r,0.,1.);winterMaterial(geometricN,n,b.rgb,rough,metal,ao);
 vec3 l=normalize(vec3(-.5,.8,.65)),fill=normalize(vec3(.75,.3,-.6)),v=normalize(uEye-vPosition),halfV=normalize(l+v);float diffuse=max(dot(n,l),0.),secondary=max(dot(n,fill),0.);vec3 f0=mix(vec3(mix(.04,.018,uWinterSurface.y)),b.rgb,metal);float spec=pow(max(dot(n,halfV),0.),mix(100.,5.,rough))*(1.-rough*.6);vec3 lit=b.rgb*(.36+.80*diffuse+.20*secondary)*mix(.58,1.,ao)+f0*spec*.85;
 if(uWinterSurface.y>0.){float edge=pow(1.-max(dot(n,v),0.),2.),wrap=max((dot(-n,l)+.6)/1.6,0.);vec3 absorption=exp(-vec3(1.35,.48,.24)*uWinterSurface.z);lit+=b.rgb*absorption*(vec3(.3)*.32+vec3(.55)*wrap*.22)*uWinterSurface.y;lit+=vec3(.3)*absorption*edge*.12*uWinterSurface.y;}
 if(uFacadeMode==5){float mask=tex(uFacadeAux0,1.).r;float opacity=clamp((mask+uFacadeUVScale.z)*uFacadeUVScale.y,0.,1.);float through=max(dot(-geometricN,l),0.)*(1.-opacity)*.5;lit+=b.rgb*vec3(.55)*through;}
 if(uHasEmissive)lit+=texture(uEmissive,vUV).rgb*uEmissiveFactor;else lit+=uEmissiveFactor;
 if(uDisplayGamma)lit=pow(max(lit,vec3(0)),vec3(1./2.2));color=vec4(lit,uOpacity*(uThumbnailCoverage?maskCoverage:1.));
}
)GLSL";
        program=GLH_program(vs,fs);GLint ok=0;if(program)glGetProgramiv(program,GL_LINK_STATUS,&ok);
        if(!ok){if(program)glDeleteProgram(program);program=0;shader_failed=true;return false;}
        glUseProgram(program);glUniform1i(glGetUniformLocation(program,"uFacadeAux0"),8);glUniform1i(glGetUniformLocation(program,"uFacadeAux1"),9);glUniform1i(glGetUniformLocation(program,"uFacadeAux2"),10);glUniform1i(glGetUniformLocation(program,"uSceneDepth"),4);
        // Always complete the active depth sampler, including thumbnail draws
        // where the occlusion branch is disabled. This texture is never a draw
        // attachment and cannot create a framebuffer feedback loop.
        glActiveTexture(GL_TEXTURE4);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
        for(unsigned i=0;i<8;++i)glPixelStorei(GLState::unpack_names[i],i==0?4:0);
        glGenTextures(1,&depth_neutral);glBindTexture(GL_TEXTURE_2D,depth_neutral);const float sky=1;
        glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,1,1,0,GL_RED,GL_FLOAT,&sky);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);return depth_neutral!=0;
    }
    void uniforms(const M4 &vp,V3 eye,float opacity,bool gamma){
        glUseProgram(program);glUniformMatrix4fv(glGetUniformLocation(program,"uVP"),1,GL_FALSE,vp.m);glUniform3f(glGetUniformLocation(program,"uEye"),eye.x,eye.y,eye.z);glUniform1f(glGetUniformLocation(program,"uOpacity"),opacity);glUniform1i(glGetUniformLocation(program,"uDisplayGamma"),gamma);glUniform1i(glGetUniformLocation(program,"uDepthOcclusion"),0);glUniform1i(glGetUniformLocation(program,"uThumbnailCoverage"),0);glActiveTexture(GL_TEXTURE4);glBindTexture(GL_TEXTURE_2D,depth_neutral);
    }
    static void clear_samplers(){for(const auto unit:GLState::units)glBindSampler(unsigned(unit-GL_TEXTURE0),0);}
    bool tile(Entry &e){
        if(!model||!shaders())return false;
        GenericAttributes restore_attributes;
        if(!restore_attributes.valid()){e.error="Required preview GL entry points unavailable";return false;}
        float lo[3],hi[3];world_asset_thumbnail::Fit framing;if(!world_model_bounds(model,lo,hi)||!world_asset_thumbnail::fit(lo,hi,framing)){e.error="Invalid source bounds";return false;}
        auto storage=reinterpret_cast<PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC>(SDL_GL_GetProcAddress("glRenderbufferStorageMultisample"));
        auto blit=reinterpret_cast<PFNGLBLITFRAMEBUFFERPROC>(SDL_GL_GetProcAddress("glBlitFramebuffer"));
        GLint samples=0;glGetIntegerv(GL_MAX_SAMPLES,&samples);
        if(!storage||!blit||samples<4){e.error="Four-sample preview antialiasing unavailable";return false;}
        if(!fbo)glGenFramebuffers(1,&fbo);
        if(!coverage_fbo){
            glGenFramebuffers(1,&coverage_fbo);glBindFramebuffer(GL_FRAMEBUFFER,coverage_fbo);
            glGenRenderbuffers(1,&coverage_color);glBindRenderbuffer(GL_RENDERBUFFER,coverage_color);
            storage(GL_RENDERBUFFER,4,GL_RGBA8,world_asset_thumbnail::tile_side,world_asset_thumbnail::tile_side);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,coverage_color);
            glGenRenderbuffers(1,&depth_buffer);glBindRenderbuffer(GL_RENDERBUFFER,depth_buffer);
            storage(GL_RENDERBUFFER,4,GL_DEPTH_COMPONENT24,world_asset_thumbnail::tile_side,world_asset_thumbnail::tile_side);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth_buffer);
            glDrawBuffer(GL_COLOR_ATTACHMENT0);glReadBuffer(GL_COLOR_ATTACHMENT0);
            if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){e.error="Multisample preview framebuffer unavailable";return false;}
        }
        glGenTextures(1,&e.texture);glActiveTexture(GL_TEXTURE0);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glBindTexture(GL_TEXTURE_2D,e.texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,world_asset_thumbnail::tile_side,world_asset_thumbnail::tile_side,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,e.texture,0);glDrawBuffer(GL_COLOR_ATTACHMENT0);glReadBuffer(GL_COLOR_ATTACHMENT0);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){e.error="Preview resolve framebuffer unavailable";glDeleteTextures(1,&e.texture);e.texture=0;return false;}
        glBindFramebuffer(GL_FRAMEBUFFER,coverage_fbo);
        glViewport(0,0,world_asset_thumbnail::tile_side,world_asset_thumbnail::tile_side);glDisable(GL_SCISSOR_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_FRAMEBUFFER_SRGB);glEnable(GL_MULTISAMPLE);glDisable(GL_SAMPLE_COVERAGE);glDisable(GL_SAMPLE_MASK);glDisable(GL_DITHER);glDisable(GL_RASTERIZER_DISCARD);glDisable(GL_POLYGON_OFFSET_FILL);glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glDepthRange(0,1);glDepthMask(GL_TRUE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClearColor(.105f,.13f,.16f,1);glClearDepth(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        const V3 center{framing.center[0],framing.center[1],framing.center[2]},direction=norm(V3{.68f,.38f,.78f}),eye=add(center,mul(direction,framing.distance));
        const M4 vp=product(perspective(32.f*.017453292519943295f,1,framing.near_plane,framing.far_plane),look(eye,center,{0,1,0}));
        WorldModelInstance one={0,0,0,0,1};if(!world_model_upload_instances(model,&one,1)){e.error="Preview instance upload failed";return false;}uniforms(vp,eye,1,true);glUniform1i(glGetUniformLocation(program,"uThumbnailCoverage"),1);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_FALSE);clear_samplers();world_model_draw(model,program);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,coverage_fbo);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,fbo);
        blit(0,0,world_asset_thumbnail::tile_side,world_asset_thumbnail::tile_side,0,0,world_asset_thumbnail::tile_side,world_asset_thumbnail::tile_side,GL_COLOR_BUFFER_BIT,GL_NEAREST);
        ++rendered;return true;
    }
    bool room(){
        if(entries.size()<world_asset_thumbnail::record_limit)return true;
        auto candidate=entries.end();for(auto it=entries.begin();it!=entries.end();++it)if(it->touched!=frame&&it->stem!=busy&&it->stem!=model_stem&&(candidate==entries.end()||it->touched<candidate->touched))candidate=it;
        if(candidate==entries.end())return false;
        if(candidate->texture)glDeleteTextures(1,&candidate->texture);
        entries.erase(candidate);return true;
    }
    bool trim_tiles(){
        unsigned count=0;for(const auto &e:entries)count+=e.texture!=0;
        while(count>=world_asset_thumbnail::tile_limit){auto candidate=entries.end();for(auto it=entries.begin();it!=entries.end();++it)if(it->texture&&it->touched!=frame&&it->stem!=busy&&it->stem!=placement&&(candidate==entries.end()||it->touched<candidate->touched))candidate=it;if(candidate==entries.end())return false;glDeleteTextures(1,&candidate->texture);candidate->texture=0;candidate->status=Status::Queued;--count;}return true;
    }
public:
    AssetThumbnailCache(){entries.reserve(world_asset_thumbnail::record_limit);}
    AssetThumbnailCache(const AssetThumbnailCache&)=delete;AssetThumbnailCache&operator=(const AssetThumbnailCache&)=delete;
    ~AssetThumbnailCache(){shutdown();}
    // Call once per frame even when the browser is hidden; old placement
    // requests then expire and release their source model automatically.
    void begin_frame(const std::string &directory,std::uint64_t next_frame){
        frame=next_frame;
        if(project!=directory){shutdown();stopping.store(false);project=directory;}
        if(invalidate_pending){
            // The refresh button only schedules this boundary. ImGui may have
            // already queued earlier images in the frame where it was pressed.
            // Retiring here keeps every texture alive until that frame renders.
            if(stage)world_model_stage_free(stage);
            stage=nullptr;world_model_free(model);model=nullptr;model_stem.clear();placement.clear();
            for(auto &e:entries){if(e.texture)glDeleteTextures(1,&e.texture);e.texture=0;e.registry=0;e.fingerprint=0;e.error.clear();e.status=Status::Queued;}
            invalidate_pending=false;shader_failed=false;
            if(!worker.valid())busy.clear();
        }
        if(placement_frame+1<frame)placement.clear();
        for(auto &e:entries)e.priority=false;
    }
    // Nonblocking UI action. A running CPU job observes its generation change;
    // service discards its result. Existing ImGui images retire next frame.
    void invalidate_all(){generation.fetch_add(1);invalidate_pending=true;}
    Result request(std::string_view stem,std::uint64_t registry,bool priority=false){
        if(!world_asset_thumbnail::safe_stem(stem))return {0,Status::Unavailable,"Invalid model path"};
        auto *e=find(stem);if(!e){if(!room())return {0,Status::Queued,"Visible preview budget reached"};entries.emplace_back();e=&entries.back();e->stem=std::string(stem);}
        if(e->registry!=registry){
            // Clear old visual ownership even if the new source has vanished.
            if(e->texture)glDeleteTextures(1,&e->texture);
            e->texture=0;e->file_bytes=0;e->modified={};e->fingerprint=0;
            if(model_stem==stem){world_model_free(model);model=nullptr;model_stem.clear();}
            e->registry=registry;std::error_code error;const auto path=std::filesystem::path(project)/"models"/(e->stem+".glb");const auto size=std::filesystem::file_size(path,error);auto modified=std::filesystem::file_time_type{};if(!error)modified=std::filesystem::last_write_time(path,error);
            if(error){e->status=Status::Unavailable;e->error="Model file unavailable";}
            else {e->file_bytes=size;e->modified=modified;e->status=Status::Queued;e->error.clear();}
        }
        e->touched=frame;e->priority|=priority;
        return {e->texture,e->status,e->error.empty()?status_text(e->status):e->error.c_str()};
    }
    void touch_placement(std::string_view stem,std::uint64_t registry){
        const auto result=request(stem,registry,true);if(result.status==Status::Unavailable)return;placement=std::string(stem);placement_frame=frame;
    }
    bool placement_ready(std::string_view stem)const{return model&&model_stem==stem;}
    bool placement_bounds(std::string_view stem,float lo[3],float hi[3])const{return placement_ready(stem)&&world_model_bounds(model,lo,hi);}
    // With scene_depth!=0 this is an explicit post-temporal display overlay:
    // draw after compose_post, before ImGui rendering, never into history.
    // Pass the current matching world_temporal_depth() normalized OpenGL depth
    // (R32F or raw depth fallback), current UNJITTERED projection*view, actual
    // bottom-left drawable viewport, current near/far, and camera-relative
    // eye/offset. Suppress while showing held/loading scene frames. Depth comes
    // from that texture, not the default framebuffer's unrelated depth buffer.
    bool draw_placement(std::string_view stem,const M4 &vp,V3 eye,V3 offset,float yaw,float scale,int vx,int vy,int vw,int vh,bool gamma=true,unsigned scene_depth=0,float near_plane=.4f,float far_plane=15000.f){
        if(!placement_ready(stem)||!std::isfinite(scale)||scale<=0||vw<=0||vh<=0)return false;
        if(scene_depth&&(!std::isfinite(near_plane)||!std::isfinite(far_plane)||near_plane<=0||far_plane<=near_plane))return false;
        GenericAttributes restore_attributes;
        if(!restore_attributes.valid())return false;
        GLState restore;if(!shaders())return false;WorldModelInstance one={offset.x,offset.y,offset.z,yaw,scale};if(!world_model_upload_instances(model,&one,1))return false;
        if(scene_depth){glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);glViewport(vx,vy,vw,vh);glDisable(GL_DEPTH_TEST);glDepthRange(0,1);}else glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);if(gamma)glDisable(GL_FRAMEBUFFER_SRGB);glEnable(GL_BLEND);glBlendEquation(GL_FUNC_ADD);glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);glEnable(GL_SCISSOR_TEST);glScissor(vx,vy,vw,vh);uniforms(vp,eye,.68f,gamma);clear_samplers();
        if(scene_depth){glUniform1i(glGetUniformLocation(program,"uDepthOcclusion"),1);glUniform4f(glGetUniformLocation(program,"uPreviewViewport"),float(vx),float(vy),float(vw),float(vh));glUniform2f(glGetUniformLocation(program,"uPreviewNearFar"),near_plane,far_plane);glActiveTexture(GL_TEXTURE4);glBindTexture(GL_TEXTURE_2D,scene_depth);}
        world_model_draw(model,program);return true;
    }
    // Only zero-time future polling, one upload budget or one model render per
    // call. CPU decoding and mip generation stay on the single bounded worker.
    void service(){
        if(stopping.load()||invalidate_pending)return;
        if(model&&model_stem!=placement){world_model_free(model);model=nullptr;model_stem.clear();}
        if(model)return;
        if(worker.valid()){
            if(worker.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)return;
            auto result=worker.get();auto *e=find(busy);if(!e){busy.clear();return;}
            if(e->registry!=busy_registry||busy_generation!=generation.load()){if(e->status!=Status::Unavailable)e->status=Status::Queued;busy.clear();return;}
            if(!result.asset){e->status=Status::Unavailable;e->error=result.error;busy.clear();return;}
            e->fingerprint=result.fingerprint;GLState restore;stage=world_model_stage(std::move(result.asset),1,1);if(!stage){e->status=Status::Unavailable;e->error="Preview staging failed";busy.clear();return;}e->status=Status::Uploading;return;
        }
        if(stage){
            GLState restore;std::size_t consumed=0;bool complete=false;const bool ok=world_model_stage_step(stage,world_asset_thumbnail::upload_budget,&consumed,&complete);
            auto *e=find(busy);if(!ok||!e||e->registry!=busy_registry||busy_generation!=generation.load()){world_model_stage_free(stage);stage=nullptr;if(e&&e->status!=Status::Unavailable){e->status=ok?Status::Queued:Status::Unavailable;e->error=ok?"":"Preview upload failed";}busy.clear();return;}
            if(!complete)return;
            model=world_model_stage_take(stage);world_model_stage_free(stage);stage=nullptr;model_stem=busy;
            if(e->texture)e->status=Status::Ready;
            else if(!trim_tiles()){e->status=Status::Queued;}
            else if(!tile(*e)){e->status=Status::Unavailable;if(e->texture)glDeleteTextures(1,&e->texture);e->texture=0;if(e->error.empty())e->error="Preview rendering failed";}else e->status=Status::Ready;
            busy.clear();if(model_stem!=placement){world_model_free(model);model=nullptr;model_stem.clear();}return;
        }
        Entry *next=nullptr;
        for(auto &e:entries){
            const bool need_model=e.stem==placement;
            if((e.status!=Status::Queued&&!need_model)||e.status==Status::Unavailable||e.touched+1<frame)continue;
            if(!next||need_model||(e.priority&&!next->priority)||(e.priority==next->priority&&e.touched>next->touched))next=&e;
        }
        if(!next)return;
        if(!next->texture&&next->stem!=placement&&!trim_tiles())return;
        busy=next->stem;busy_registry=next->registry;busy_generation=generation.load();next->status=Status::Preparing;const std::string path=(std::filesystem::path(project)/"models"/(busy+".glb")).u8string();
        world_model_cpu::Options options;options.cpu_bytes=384ull*1024*1024;options.max_texture_size=8192;options.bc7=false;options.reuse_decoder_scratch=true;options.generation=&generation;options.expected_generation=generation.load();options.stopping=&stopping;
        worker=std::async(std::launch::async,[path,options](){try{return prepare(path,options);}catch(const std::exception &e){return Prepared{{},e.what()};}catch(...){return Prepared{{},"Preview preparation failed"};}});
    }
    Stats stats()const{Stats out;out.rendered=rendered;for(const auto &e:entries){out.tiles+=e.texture!=0;out.unavailable+=e.status==Status::Unavailable;out.pending+=e.status==Status::Queued||e.status==Status::Preparing||e.status==Status::Uploading;}out.bytes=std::size_t(out.tiles)*world_asset_thumbnail::tile_side*world_asset_thumbnail::tile_side*4;return out;}
    // Must run before the owning SDL GL context is destroyed. The destructor
    // becomes an inert second call after this explicit resource boundary.
    void shutdown(){
        stopping.store(true);generation.fetch_add(1);if(worker.valid())worker.get();
        if(stage)world_model_stage_free(stage);
        stage=nullptr;world_model_free(model);model=nullptr;
        for(auto &e:entries)if(e.texture)glDeleteTextures(1,&e.texture);
        entries.clear();
        if(depth_buffer)glDeleteRenderbuffers(1,&depth_buffer);
        if(fbo)glDeleteFramebuffers(1,&fbo);
        if(coverage_fbo)glDeleteFramebuffers(1,&coverage_fbo);
        if(coverage_color)glDeleteRenderbuffers(1,&coverage_color);
        if(program)glDeleteProgram(program);
        if(depth_neutral)glDeleteTextures(1,&depth_neutral);
        coverage_fbo=coverage_color=depth_buffer=fbo=program=depth_neutral=0;busy.clear();placement.clear();model_stem.clear();shader_failed=false;invalidate_pending=false;
    }
};
