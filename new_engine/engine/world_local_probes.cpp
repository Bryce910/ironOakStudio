#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include "world_local_probes.h"
#include "engine/world_program_epoch.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <new>
#include <vector>

namespace {
constexpr float basis_forward[6][3]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
constexpr float basis_up[6][3]={{0,-1,0},{0,-1,0},{0,0,1},{0,0,-1},{0,-1,0},{0,-1,0}};
constexpr float basis_right[6][3]={{0,0,-1},{0,0,1},{1,0,0},{1,0,0},{1,0,0},{-1,0,0}};
bool valid_side(int side) {return side>=4&&side<=128&&(side&(side-1))==0;}
double area(double x,double y) {return std::atan2(x*y,std::sqrt(x*x+y*y+1.));}
void sh_basis(const double n[3],double b[9]) {
    const double x=n[0],y=n[1],z=n[2];
    b[0]=.28209479177387814;b[1]=.4886025119029199*y;b[2]=.4886025119029199*z;b[3]=.4886025119029199*x;
    b[4]=1.0925484305920792*x*y;b[5]=1.0925484305920792*y*z;b[6]=.31539156525252005*(3*z*z-1);
    b[7]=1.0925484305920792*x*z;b[8]=.5462742152960396*(x*x-y*y);
}
float smooth_edge(double value) {const double t=std::clamp(value/1.5,0.,1.);return float(t*t*(3.-2.*t));}
bool valid_def(const WorldLocalProbeDef &p) {
    for(int k=0;k<3;k++)if(!std::isfinite(p.position[k])||!std::isfinite(p.box_min[k])||
        !std::isfinite(p.box_max[k])||std::abs(p.position[k])>1.e7||p.box_min[k]>=p.box_max[k]||
        p.box_max[k]-p.box_min[k]<.1f||p.position[k]<p.box_min[k]||p.position[k]>p.box_max[k])return false;
    return true;
}
struct StartupState {
    GLint draw=0,read=0,viewport[4]={},renderbuffer=0,active=0,cube=0;
    GLint pack_buffer=0,pack_align=4,pack_row=0,pack_skip_pixels=0,pack_skip_rows=0;
    GLint unpack_buffer=0,unpack_align=4,unpack_row=0,unpack_image=0,unpack_pixels=0,unpack_rows=0,unpack_images=0;
    StartupState() {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);
        glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_RENDERBUFFER_BINDING,&renderbuffer);
        glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP,&cube);
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&pack_buffer);glGetIntegerv(GL_PACK_ALIGNMENT,&pack_align);
        glGetIntegerv(GL_PACK_ROW_LENGTH,&pack_row);glGetIntegerv(GL_PACK_SKIP_PIXELS,&pack_skip_pixels);glGetIntegerv(GL_PACK_SKIP_ROWS,&pack_skip_rows);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&unpack_buffer);glGetIntegerv(GL_UNPACK_ALIGNMENT,&unpack_align);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH,&unpack_row);glGetIntegerv(GL_UNPACK_IMAGE_HEIGHT,&unpack_image);
        glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&unpack_pixels);glGetIntegerv(GL_UNPACK_SKIP_ROWS,&unpack_rows);glGetIntegerv(GL_UNPACK_SKIP_IMAGES,&unpack_images);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
        glPixelStorei(GL_UNPACK_IMAGE_HEIGHT,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_IMAGES,0);
    }
    static void pack() {
        glBindBuffer(GL_PIXEL_PACK_BUFFER,0);glPixelStorei(GL_PACK_ALIGNMENT,1);glPixelStorei(GL_PACK_ROW_LENGTH,0);
        glPixelStorei(GL_PACK_SKIP_PIXELS,0);glPixelStorei(GL_PACK_SKIP_ROWS,0);
    }
    ~StartupState() {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(draw));glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(read));
        glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glBindRenderbuffer(GL_RENDERBUFFER,GLuint(renderbuffer));
        glBindBuffer(GL_PIXEL_PACK_BUFFER,GLuint(pack_buffer));glPixelStorei(GL_PACK_ALIGNMENT,pack_align);glPixelStorei(GL_PACK_ROW_LENGTH,pack_row);
        glPixelStorei(GL_PACK_SKIP_PIXELS,pack_skip_pixels);glPixelStorei(GL_PACK_SKIP_ROWS,pack_skip_rows);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER,GLuint(unpack_buffer));glPixelStorei(GL_UNPACK_ALIGNMENT,unpack_align);
        glPixelStorei(GL_UNPACK_ROW_LENGTH,unpack_row);glPixelStorei(GL_UNPACK_IMAGE_HEIGHT,unpack_image);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS,unpack_pixels);glPixelStorei(GL_UNPACK_SKIP_ROWS,unpack_rows);glPixelStorei(GL_UNPACK_SKIP_IMAGES,unpack_images);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_CUBE_MAP,GLuint(cube));glActiveTexture(GLenum(active));
    }
};
struct Probe {WorldLocalProbeDef def;GLuint texture=0;double sh[9][3]={};};
struct Locations {
    unsigned long long epoch=0;
    GLuint program=0;GLint cube[2]={-1,-1},count=-1,center=-1,lo=-1,hi=-1,weights=-1,mip=-1,scale=-1;
    GLint sh=-1,sh_weight=-1,sh_bounds=-1,sh_heights=-1;
    GLint sh_secondary=-1,sh_secondary_weight=-1,sh_secondary_bounds=-1,sh_secondary_heights=-1;
};
}

struct WorldLocalProbes {
    std::array<Probe,WORLD_LOCAL_PROBE_LIMIT> probes;
    std::array<Locations,8> locations;
    GLuint fbo=0,depth=0;
    std::vector<float> pixels; // loading-time readback only; never resized after create
    WorldLocalProbeStats stats;
    bool capturing=false,baked=false;
};

bool world_local_probe_face_basis(unsigned face,float forward[3],float up[3],float right[3]) {
    if(face>=6||!forward||!up||!right)return false;
    std::copy_n(basis_forward[face],3,forward);std::copy_n(basis_up[face],3,up);std::copy_n(basis_right[face],3,right);return true;
}
std::uint64_t world_local_probe_cube_bytes(unsigned count,int side) {
    if(count<1||count>WORLD_LOCAL_PROBE_LIMIT||!valid_side(side))return 0;
    std::uint64_t face=0;for(int n=side;n;n/=2)face+=std::uint64_t(n)*n*6;
    return count*6*face;
}
bool world_local_probe_add_face(unsigned face,int side,const float *rgb,std::size_t values,double coefficients[9][3]) {
    if(face>=6||!valid_side(side)||!rgb||!coefficients||values!=std::size_t(side)*side*3)return false;
    // Validate first: malformed readback cannot leave a partly accumulated SH.
    for(std::size_t i=0;i<values;i++)if(!std::isfinite(rgb[i])||rgb[i]<0||rgb[i]>65504.f)return false;
    double next[9][3]={};
    for(int y=0;y<side;y++)for(int x=0;x<side;x++) {
        const double x0=2.*x/side-1.,x1=2.*(x+1)/side-1.;
        const double y0=2.*y/side-1.,y1=2.*(y+1)/side-1.;
        const double solid=area(x1,y1)-area(x0,y1)-area(x1,y0)+area(x0,y0);
        const double u=(x0+x1)*.5,v=(y0+y1)*.5;
        double direction[3];for(int k=0;k<3;k++)direction[k]=basis_forward[face][k]+u*basis_right[face][k]+v*basis_up[face][k];
        const double inverse=1./std::sqrt(direction[0]*direction[0]+direction[1]*direction[1]+direction[2]*direction[2]);
        for(double &k:direction)k*=inverse;
        double b[9];sh_basis(direction,b);const auto *pixel=rgb+(std::size_t(y)*side+x)*3;
        for(int i=0;i<9;i++)for(int c=0;c<3;c++)next[i][c]+=pixel[c]*b[i]*solid;
    }
    for(int i=0;i<9;i++)for(int c=0;c<3;c++)coefficients[i][c]+=next[i][c];
    return true;
}
void world_local_probe_diffuse(const double coefficients[9][3],const double normal[3],double rgb[3]) {
    if(!rgb)return;
    rgb[0]=rgb[1]=rgb[2]=0;
    if(!coefficients||!normal)return;
    const double length=std::hypot(std::hypot(normal[0],normal[1]),normal[2]);
    if(!std::isfinite(length)||length<1.e-12)return;
    double n[3]={normal[0]/length,normal[1]/length,normal[2]/length},b[9];sh_basis(n,b);
    for(int i=0;i<9;i++)for(int c=0;c<3;c++)rgb[c]+=coefficients[i][c]*b[i]*(i==0?1.:(i<=3?2./3.:.25));
    for(int c=0;c<3;c++)rgb[c]=std::isfinite(rgb[c])?std::max(0.,rgb[c]):0;
}

WorldLocalProbes *world_local_probes_create(const WorldLocalProbeDef *defs,std::size_t count,int side) {
    if(!defs||count<1||count>WORLD_LOCAL_PROBE_LIMIT||!valid_side(side))return nullptr;
    for(std::size_t i=0;i<count;i++)if(!valid_def(defs[i]))return nullptr;
    auto *p=new(std::nothrow) WorldLocalProbes;if(!p)return nullptr;
    try {p->pixels.resize(std::size_t(side)*side*3);}catch(...){delete p;return nullptr;}
    p->stats.count=unsigned(count);p->stats.side=side;p->stats.cube_bytes=world_local_probe_cube_bytes(unsigned(count),side);
    p->stats.depth_bytes=std::uint64_t(side)*side*4;p->stats.bytes=p->stats.cube_bytes+p->stats.depth_bytes;
    if(p->stats.bytes>8u*1024u*1024u){delete p;return nullptr;}
    StartupState state;
    glGenFramebuffers(1,&p->fbo);glBindFramebuffer(GL_FRAMEBUFFER,p->fbo);
    glGenRenderbuffers(1,&p->depth);glBindRenderbuffer(GL_RENDERBUFFER,p->depth);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,side,side);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,p->depth);
    for(std::size_t i=0;i<count;i++) {
        auto &probe=p->probes[i];probe.def=defs[i];glGenTextures(1,&probe.texture);glBindTexture(GL_TEXTURE_CUBE_MAP,probe.texture);
        int levels=0;
        for(int n=side;n;n/=2,levels++)for(unsigned face=0;face<6;face++)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,levels,GL_RGB16F,n,n,0,GL_RGB,GL_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAX_LEVEL,levels-1);
        glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        for(GLenum axis:{GL_TEXTURE_WRAP_S,GL_TEXTURE_WRAP_T,GL_TEXTURE_WRAP_R})glTexParameteri(GL_TEXTURE_CUBE_MAP,axis,GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_CUBE_MAP_POSITIVE_X,probe.texture,0);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);glReadBuffer(GL_COLOR_ATTACHMENT0);
        if(!probe.texture||glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE||glGetError()!=GL_NO_ERROR) {
            world_local_probes_free(p);return nullptr;
        }
    }
    return p;
}

bool world_local_probes_capture(WorldLocalProbes *p,WorldLocalProbeCaptureFn callback,void *user) {
    if(!p)return false;
    p->stats.capture_calls++;
    if(!callback||p->capturing||p->baked){p->stats.failures++;return false;}
    StartupState state;p->capturing=true;p->stats.ready=0;
    bool okay=true;
    for(unsigned i=0;i<p->stats.count&&okay;i++) {
        auto &probe=p->probes[i];for(auto &row:probe.sh)std::fill_n(row,3,0.);
        for(unsigned face=0;face<6&&okay;face++) {
            glBindFramebuffer(GL_FRAMEBUFFER,p->fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,probe.texture,0);
            glDrawBuffer(GL_COLOR_ATTACHMENT0);glReadBuffer(GL_COLOR_ATTACHMENT0);glViewport(0,0,p->stats.side,p->stats.side);
            glDisable(GL_SCISSOR_TEST);glDisable(GL_BLEND);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);
            glClearColor(0,0,0,1);glClearDepth(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            WorldLocalProbeCapture frame;frame.framebuffer=p->fbo;frame.index=i;frame.face=face;frame.side=p->stats.side;
            std::copy_n(probe.def.position,3,frame.eye);world_local_probe_face_basis(face,frame.forward,frame.up,frame.right);
            if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){okay=false;break;}
            try {okay=callback(frame,user);}catch(...){okay=false;}
            if(!okay)break;
            p->stats.capture_faces++;
            glBindFramebuffer(GL_READ_FRAMEBUFFER,p->fbo);glReadBuffer(GL_COLOR_ATTACHMENT0);StartupState::pack();
            glReadPixels(0,0,p->stats.side,p->stats.side,GL_RGB,GL_FLOAT,p->pixels.data());p->stats.readbacks++;
            if(glGetError()!=GL_NO_ERROR||!world_local_probe_add_face(face,p->stats.side,p->pixels.data(),p->pixels.size(),probe.sh))okay=false;
        }
        if(okay) {
            glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_CUBE_MAP,probe.texture);glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
            if(glGetError()!=GL_NO_ERROR)okay=false;
            else p->stats.ready++;
        }
    }
    p->capturing=false;p->baked=okay;
    if(!okay){p->stats.ready=0;p->stats.failures++;}
    return okay;
}

bool world_local_probes_register_program(WorldLocalProbes *p,unsigned program) {
    if(!p||!program)return false;
    for(auto &l:p->locations)if(l.program==program&&l.epoch==world_program_epoch)return true;
    if(!glIsProgram(program)){p->stats.failures++;return false;}
    GLint linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);
    if(!linked){p->stats.failures++;return false;}
    for(auto &l:p->locations)if(!l.program||l.epoch!=world_program_epoch) {
        l={};l.program=program;l.epoch=world_program_epoch;
        l.cube[0]=glGetUniformLocation(program,"uLocalProbe0");l.cube[1]=glGetUniformLocation(program,"uLocalProbe1");
        l.count=glGetUniformLocation(program,"uLocalProbeCount");l.center=glGetUniformLocation(program,"uLocalProbeCenter[0]");
        l.lo=glGetUniformLocation(program,"uLocalProbeBoxMin[0]");l.hi=glGetUniformLocation(program,"uLocalProbeBoxMax[0]");
        l.weights=glGetUniformLocation(program,"uLocalProbeWeights");l.mip=glGetUniformLocation(program,"uLocalProbeMip");
        l.scale=glGetUniformLocation(program,"uLocalProbeLightScale");l.sh=glGetUniformLocation(program,"uLocalIrradiance[0]");
        l.sh_weight=glGetUniformLocation(program,"uLocalIrradianceWeight");l.sh_bounds=glGetUniformLocation(program,"uLocalIrradianceBounds");
        l.sh_heights=glGetUniformLocation(program,"uLocalIrradianceHeights");
        l.sh_secondary=glGetUniformLocation(program,"uLocalIrradianceSecondary[0]");
        l.sh_secondary_weight=glGetUniformLocation(program,"uLocalIrradianceSecondaryWeight");
        l.sh_secondary_bounds=glGetUniformLocation(program,"uLocalIrradianceSecondaryBounds");
        l.sh_secondary_heights=glGetUniformLocation(program,"uLocalIrradianceSecondaryHeights");return true;
    }
    p->stats.failures++;return false;
}

bool world_local_probes_bind(WorldLocalProbes *p,unsigned program,const double camera[3],bool enabled,float light_scale) {
    if(!p||!program)return false;
    p->stats.bind_calls++;if(!world_local_probes_register_program(p,program))return false;
    const Locations *location=nullptr;for(const auto &l:p->locations)if(l.program==program&&l.epoch==world_program_epoch)location=&l;
    if(!location)return false;
    const auto &l=*location;
    bool valid=camera&&std::isfinite(light_scale)&&light_scale>=0&&light_scale<=16;
    if(camera)for(int k=0;k<3;k++)valid=valid&&std::isfinite(camera[k]);
    if(!valid)p->stats.failures++;
    enabled=enabled&&valid&&p->baked&&!p->capturing;
    std::array<unsigned,2> selected={};std::array<double,2> scores={1.e100,1.e100};std::array<float,2> edges={};
    unsigned count=0;bool inside_interior=false;
    if(enabled) {
        for(unsigned i=0;i<p->stats.count;i++) {
            const auto &d=p->probes[i].def;bool inside=d.interior;
            for(int k=0;k<3;k++)inside=inside&&camera[k]>d.box_min[k]&&camera[k]<d.box_max[k];
            inside_interior=inside_interior||inside;
        }
        for(unsigned i=0;i<p->stats.count;i++) {
            const auto &d=p->probes[i].def;if(inside_interior&&!d.interior)continue;
            double edge=1.e100,score=0;bool inside=true;
            for(int k=0;k<3;k++) {
                inside=inside&&camera[k]>d.box_min[k]&&camera[k]<d.box_max[k];
                edge=std::min(edge,std::min(camera[k]-d.box_min[k],d.box_max[k]-camera[k]));
                const double delta=(camera[k]-d.position[k])/(.5*(d.box_max[k]-d.box_min[k]));score+=delta*delta;
            }
            if(!inside)continue;
            const float fade=smooth_edge(edge);if(fade<=0)continue;
            if(score<scores[0]) {scores[1]=scores[0];selected[1]=selected[0];edges[1]=edges[0];scores[0]=score;selected[0]=i;edges[0]=fade;count=std::min(2u,count+1);}
            else if(score<scores[1]) {scores[1]=score;selected[1]=i;edges[1]=fade;count=2;}
        }
    }
    float centers[6]={},low[6]={},high[6]={},weights[2]={},sh[2][27]={},bounds[2][4]={},heights[2][2]={};
    float irradiance_weight[2]={};
    if(count) {
        float total=0,raw[2]={};for(unsigned i=0;i<count;i++){raw[i]=edges[i]/float(.25+scores[i]);total+=raw[i];}
        for(unsigned i=0;i<count;i++) {
            const auto &probe=p->probes[selected[i]];const float share=raw[i]/total;weights[i]=share*edges[i];
            for(int k=0;k<3;k++){centers[i*3+k]=float(probe.def.position[k]);low[i*3+k]=probe.def.box_min[k];high[i*3+k]=probe.def.box_max[k];}
            for(int j=0;j<9;j++)for(int k=0;k<3;k++)sh[i][j*3+k]=float(probe.sh[j][k])*light_scale;
            const auto &d=probe.def;
            bounds[i][0]=d.box_min[0];bounds[i][1]=d.box_min[2];bounds[i][2]=d.box_max[0];bounds[i][3]=d.box_max[2];
            heights[i][0]=d.box_min[1];heights[i][1]=d.box_max[1];
            irradiance_weight[i]=(d.interior?.40f:.22f)*share*edges[i];
        }
    }
    glUniform1i(l.count,GLint(count));glUniform3fv(l.center,2,centers);glUniform3fv(l.lo,2,low);glUniform3fv(l.hi,2,high);
    glUniform2fv(l.weights,1,weights);glUniform1f(l.mip,float(std::log2(p->stats.side)));glUniform1f(l.scale,valid?light_scale:0);
    glUniform3fv(l.sh,9,sh[0]);glUniform1f(l.sh_weight,irradiance_weight[0]);glUniform4fv(l.sh_bounds,1,bounds[0]);glUniform2fv(l.sh_heights,1,heights[0]);
    glUniform3fv(l.sh_secondary,9,sh[1]);glUniform1f(l.sh_secondary_weight,irradiance_weight[1]);
    glUniform4fv(l.sh_secondary_bounds,1,bounds[1]);glUniform2fv(l.sh_secondary_heights,1,heights[1]);
    // Keep active sampler formats complete during ordinary play, including
    // count-zero passes. Counts/weights still disable all local contributions.
    // Capture/unbaked paths must bind zero to avoid a target feedback loop.
    const GLuint fallback=p->baked&&!p->capturing?p->probes[count?selected[0]:0].texture:0;
    for(unsigned i=0;i<2;i++)if(l.cube[i]>=0) {
        glUniform1i(l.cube[i],GLint(4+i));glActiveTexture(GL_TEXTURE0+4+i);
        glBindTexture(GL_TEXTURE_CUBE_MAP,i<count?p->probes[selected[i]].texture:fallback);
    }
    if(count&&(l.cube[0]>=0||l.cube[1]>=0))glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glActiveTexture(GL_TEXTURE0);return valid;
}

WorldLocalProbeStats world_local_probes_stats(const WorldLocalProbes *p) {return p?p->stats:WorldLocalProbeStats{};}
void world_local_probes_free(WorldLocalProbes *p) {
    if(!p)return;
    for(auto &probe:p->probes)glDeleteTextures(1,&probe.texture);
    glDeleteRenderbuffers(1,&p->depth);glDeleteFramebuffers(1,&p->fbo);delete p;
}
