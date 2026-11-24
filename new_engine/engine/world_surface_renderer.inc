// Included after Program inside the viewer namespace. Owns a derived texture
// array; the original six landscape layers and their owner remain untouched.
#include "engine/world_surface_rivers.inc"
struct WorldSurfaceGPU {
    WorldSurfaceRivers rivers;
    GLuint texture=0;world_surface::Data data;world_surface::PaintField paint;
    std::uint64_t bytes=0;unsigned prepares=0;
    void release(){rivers.release();if(texture)glDeleteTextures(1,&texture);texture=0;data={};paint={};bytes=0;}
    GLuint choose(GLuint original)const{return texture?texture:original;}
    static bool features(const world_surface::Data &d){return !d.images.empty()||!d.layers.empty()||!d.projections.empty()||!d.paint.empty();}
    bool prepare(GLuint original,const world_surface::Data &d,std::string &error){
        using namespace world_surface;
        if(!validate(d,error))return false;
        WorldSurfaceGPU next;next.data=d;
        if(!bake_paint(d,next.paint,error)||!next.rivers.prepare(d,error))return false;
        if(!features(d)){next.prepares=prepares+1;release();*this=std::move(next);return true;}
        const unsigned depth=6+unsigned(d.images.size())*image_layers+(next.paint.tiles.empty()?0:4);
        GLint maximum=0;glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS,&maximum);
        if(depth>unsigned(maximum)){error="Surface library exceeds device array-layer budget";next.release();return false;}
        GLint old=0,pack=0,unpack=0;glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY,&old);glGetIntegerv(GL_PACK_ALIGNMENT,&pack);glGetIntegerv(GL_UNPACK_ALIGNMENT,&unpack);
        struct Restore{GLint old,pack,unpack;~Restore(){glBindTexture(GL_TEXTURE_2D_ARRAY,GLuint(old));glPixelStorei(GL_PACK_ALIGNMENT,pack);glPixelStorei(GL_UNPACK_ALIGNMENT,unpack);}} restore{old,pack,unpack};
        WorldVillageUnpackScope unpack_scope;glPixelStorei(GL_PACK_ALIGNMENT,1);
        glBindTexture(GL_TEXTURE_2D_ARRAY,original);GLint w=0,h=0,layers=0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY,0,GL_TEXTURE_WIDTH,&w);glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY,0,GL_TEXTURE_HEIGHT,&h);glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY,0,GL_TEXTURE_DEPTH,&layers);
        if(w!=int(array_side)||h!=int(array_side)||layers!=6){error="Unexpected source landscape array; surface transaction cancelled";next.release();return false;}
        std::vector<std::uint8_t> fields(size_t(array_side)*array_side*6*4);glGetTexImage(GL_TEXTURE_2D_ARRAY,0,GL_RGBA,GL_UNSIGNED_BYTE,fields.data());
        glGenTextures(1,&next.texture);glBindTexture(GL_TEXTURE_2D_ARRAY,next.texture);
        glTexImage3D(GL_TEXTURE_2D_ARRAY,0,GL_RGBA8,array_side,array_side,depth,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY,0,0,0,0,array_side,array_side,6,GL_RGBA,GL_UNSIGNED_BYTE,fields.data());
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        for(unsigned i=0;i<d.images.size();i++){
            const auto &im=d.images[i];std::error_code ec;auto path=std::filesystem::canonical(im.path,ec),root=std::filesystem::canonical("surfaces",ec);
            if(ec||path.generic_string().rfind(root.generic_string()+"/",0)!=0||std::filesystem::file_size(path,ec)>32*1024*1024||ec){error="Surface image is missing or outside the project library: "+im.name;next.release();return false;}
            int width=0,height=0,channels=0;
            if(!stbi_info(path.u8string().c_str(),&width,&height,&channels)||width<1||height<1||width>8192||height>8192||std::uint64_t(width)*height>16*1024*1024){error="Surface image exceeds16million decoded pixels: "+im.name;next.release();return false;}
            auto *pixels=stbi_load(path.u8string().c_str(),&width,&height,&channels,4);PackedImage packed;
            const bool valid=pixels&&pack_image(pixels,unsigned(width),unsigned(height),im.role,packed,error);stbi_image_free(pixels);
            if(!valid){if(error.empty())error="Cannot decode surface image: "+im.name;next.release();return false;}
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY,0,0,0,6+i*image_layers,array_side,array_side,image_layers,GL_RGBA,GL_UNSIGNED_BYTE,packed.rgba.data());
        }
        if(!next.paint.tiles.empty()){
            std::vector<std::uint8_t> pixels;if(!pack_paint(next.paint,pixels,error)){next.release();return false;}
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY,0,0,0,6+d.images.size()*image_layers,array_side,array_side,4,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        }
        if(glGetError()!=GL_NO_ERROR){error="GPU rejected surface library; previous surfaces retained";next.release();return false;}
        next.bytes=std::uint64_t(array_side)*array_side*depth*4;next.prepares=prepares+1;
        release();*this=std::move(next);error.clear();return true;
    }
    double wind_time=0;
    static void particle_wind(void *userdata,double x,double y,double z,double time,float out[2]){
        const auto &data=*static_cast<const world_surface::Data *>(userdata);auto v=world_surface::wind(data,{x,y,z},time,{out[0],out[1]});out[0]=v[0];out[1]=v[1];
    }
    void bind_wind(Program &p)const{
        if(!studio_wind_features)return;
        std::array<float,64> centers={},radii={},directions={};
        for(unsigned i=0;i<data.winds.size();i++){
            const auto &w=data.winds[i];for(unsigned k=0;k<3;k++){centers[i*4+k]=float(w.center[k]);radii[i*4+k]=w.radius[k];}
            const double phase=std::remainder(wind_time,w.period)*6.283185307179586/w.period+(w.seed%65536)*.0000958738;
            centers[i*4+3]=float(w.speed*(1+w.gust*.5*(std::sin(phase)+.35*std::sin(phase*2))));double length=std::hypot(w.direction[0],w.direction[1]);directions[i*4]=float(w.direction[0]/length);directions[i*4+1]=float(w.direction[1]/length);
        }
        p.i("uSurfaceWindCount",int(data.winds.size()));if(!data.winds.empty()){glUniform4fv(p.at("uSurfaceWindCenter[0]"),data.winds.size(),centers.data());glUniform4fv(p.at("uSurfaceWindRadius[0]"),data.winds.size(),radii.data());glUniform4fv(p.at("uSurfaceWindDirection[0]"),data.winds.size(),directions.data());}
    }
    void bind(Program &p,double x,double y,double z)const{
        using namespace world_surface;
        if(!studio_imported_features)return;
        p.i("uSurfaceImageCount",int(data.images.size()));p.i("uSurfaceLayerCount",int(data.layers.size()));p.i("uSurfaceBinding",-1);
        p.i("uSurfacePaintCount",int(paint.tiles.size()));p.i("uSurfacePaintBase",int(6+data.images.size()*image_layers));
        std::array<GLint,image_limit> roles={};for(unsigned i=0;i<data.images.size();i++)roles[i]=GLint(data.images[i].role);
        glUniform1iv(p.at("uSurfaceImageRole[0]"),image_limit,roles.data());
        std::array<GLint,layer_limit*4> maps={};std::array<float,layer_limit*4> tint={},params={};
        for(unsigned i=0;i<data.layers.size();i++){const auto &l=data.layers[i];maps[i*4]=l.color;maps[i*4+1]=l.normal;maps[i*4+2]=l.arm;for(unsigned k=0;k<3;k++)tint[i*4+k]=l.tint[k];tint[i*4+3]=l.metres;params[i*4]=l.roughness;params[i*4+1]=l.metallic;params[i*4+2]=l.normal_strength;params[i*4+3]=l.flip_normal_y?-1.f:1.f;}
        glUniform4iv(p.at("uSurfaceLayerMaps[0]"),layer_limit,maps.data());glUniform4fv(p.at("uSurfaceLayerTint[0]"),layer_limit,tint.data());glUniform4fv(p.at("uSurfaceLayerParams[0]"),layer_limit,params.data());
        std::array<GLint,64*4> bindings={};std::array<float,64*4> blend={};
        for(unsigned i=0;i<data.bindings.size();i++){const auto &b=data.bindings[i];bindings[i*4]=b.first;bindings[i*4+1]=b.second;bindings[i*4+2]=GLint(b.mode);blend[i*4]=b.amount;blend[i*4+1]=b.scale;blend[i*4+2]=b.threshold;blend[i*4+3]=b.softness;}
        glUniform4iv(p.at("uSurfaceBindings[0]"),64,bindings.data());glUniform4fv(p.at("uSurfaceBlend[0]"),64,blend.data());
        std::array<GLint,image_mips*4> rects={};const auto src=image_rects();for(unsigned i=0;i<image_mips;i++){rects[i*4]=src[i].x;rects[i*4+1]=src[i].y;rects[i*4+2]=src[i].side;rects[i*4+3]=src[i].layer;}
        glUniform4iv(p.at("uSurfaceMipRect[0]"),image_mips,rects.data());
        std::array<float,4> bounds={1e9,1e9,-1e9,-1e9};for(const auto &t:paint.tiles){bounds[0]=std::min(bounds[0],t.x*128.f);bounds[1]=std::min(bounds[1],t.z*128.f);bounds[2]=std::max(bounds[2],(t.x+1)*128.f);bounds[3]=std::max(bounds[3],(t.z+1)*128.f);}
        glUniform4fv(p.at("uSurfacePaintBounds"),1,bounds.data());
        struct Pick {double distance;unsigned index;};std::array<Pick,projection_limit> picks;unsigned count=0;
        for(unsigned i=0;i<data.projections.size();i++){const auto &a=data.projections[i];double dx=x-a.position[0],dy=y-a.position[1],dz=z-a.position[2],d=dx*dx+dy*dy+dz*dz;if(d<180.*180.)picks[count++]={d,i};}
        std::sort(picks.begin(),picks.begin()+count,[](auto a,auto b){return a.distance==b.distance?a.index<b.index:a.distance<b.distance;});count=std::min(count,8u);
        std::array<float,32> positions={},sizes={},projection_params={};std::array<float,72> bases={};std::array<float,3> minimum={1e9,1e9,1e9},maximum={-1e9,-1e9,-1e9};
        for(unsigned i=0;i<count;i++){
            const auto &a=data.projections[picks[i].index];for(unsigned k=0;k<3;k++){positions[i*4+k]=float(a.position[k]);sizes[i*4+k]=a.size[k]*.5f;}
            positions[i*4+3]=a.opacity;sizes[i*4+3]=a.feather;projection_params[i*4]=float(a.layer);projection_params[i*4+1]=a.normal_cutoff;
            for(unsigned column=0;column<3;column++){
                std::array<double,3> v={};v[column]=1;
                for(unsigned axis=0;axis<3;axis++){unsigned j=(axis+1)%3,k=(axis+2)%3;double angle=a.rotation[axis]*.017453292519943295,c=std::cos(angle),s=std::sin(angle),u=v[j],w=v[k];v[j]=c*u-s*w;v[k]=s*u+c*w;}
                for(unsigned row=0;row<3;row++)bases[i*9+column*3+row]=float(v[row]);
            }
            for(unsigned row=0;row<3;row++){float r=0;for(unsigned column=0;column<3;column++)r+=std::abs(bases[i*9+column*3+row])*sizes[i*4+column];minimum[row]=std::min(minimum[row],positions[i*4+row]-r);maximum[row]=std::max(maximum[row],positions[i*4+row]+r);}
        }
        p.i("uSurfaceProjectionCount",int(count));p.v3("uSurfaceProjectionMinimum",{minimum[0],minimum[1],minimum[2]});p.v3("uSurfaceProjectionMaximum",{maximum[0],maximum[1],maximum[2]});
        if(count){glUniform4fv(p.at("uSurfaceProjectionPosition[0]"),count,positions.data());glUniform4fv(p.at("uSurfaceProjectionSize[0]"),count,sizes.data());glUniform4fv(p.at("uSurfaceProjectionParams[0]"),count,projection_params.data());glUniformMatrix3fv(p.at("uSurfaceProjectionBasis[0]"),count,GL_FALSE,bases.data());}
    }
};
WorldSurfaceGPU studio_surface_gpu;
