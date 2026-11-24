// Fixed-buffer, serial-varied discharge into the existing HDR color/depth.
// Geometry is generated once per event; steady draws allocate no memory.
#include "engine/world_storm_lighting.h"
struct WorldLightningVisual {
    GLuint program=0,vao=0,vbo=0,fbo=0,color=0,depth=0;
    GLint vp_location=-1,gain_location=-1;
    unsigned long long draws=0,failures=0,geometry_builds=0;
    unsigned maximum_segments=0;
    std::uint64_t geometry_serial=0;
    world_storm_lighting::Bolt geometry;
    bool create(){
        const char *vs="#version 330 core\nlayout(location=0)in vec3 p;layout(location=1)in float edge;uniform mat4 uVP;out float vEdge;void main(){vEdge=edge;gl_Position=uVP*vec4(p,1);}";
        const char *fs="#version 330 core\nin float vEdge;uniform float uGain;out vec4 outColor;void main(){float halo=exp(-3.8*vEdge*vEdge),core=exp(-40.*vEdge*vEdge);vec3 color=mix(vec3(.38,.58,1),vec3(.96,.98,1),core);outColor=vec4(color*(.32*halo+2.*core)*uGain,0);}";
        program=GLH_program(vs,fs);GLint linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);if(!linked){free();return false;}
        vp_location=glGetUniformLocation(program,"uVP");gain_location=glGetUniformLocation(program,"uGain");
        glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);glGenFramebuffers(1,&fbo);
        glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,288*4*sizeof(float),nullptr,GL_STREAM_DRAW);
        glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,4*sizeof(float),nullptr);
        glEnableVertexAttribArray(1);glVertexAttribPointer(1,1,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)(3*sizeof(float)));return vao&&vbo&&fbo;
    }
    bool draw(const world_storm::Output &storm,GLuint c,GLuint d,int w,int h,const M4 &vp,double cx,double cy,double cz,V3 camera_right,float sun_y=.5f){
        const auto flash=world_storm_lighting::flash(storm,sun_y);
        if(!flash.active)return true;
        if(!program||!c||!d||c==d||w<=0||h<=0){++failures;return false;}
        glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        if(color!=c||depth!=d){glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,c,0);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,d,0);color=c;depth=d;if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){++failures;return false;}}
        const auto &event=storm.event;
        if(geometry_serial!=event.serial){geometry=world_storm_lighting::bolt(event,world_atlas_height(event.x,event.z));geometry_serial=event.serial;++geometry_builds;maximum_segments=std::max(maximum_segments,geometry.count);}
        struct Vertex{float x,y,z,e;};std::array<Vertex,288> vertices{};unsigned n=0;
        const float distance=float(std::hypot(event.x-cx,event.z-cz));const float width=std::clamp(distance*.0015f,.28f,3.2f);
        auto segment=[&](V3 a,V3 b,float thickness){
            V3 direction={b.x-a.x,b.y-a.y,b.z-a.z},toward={-(a.x+b.x)*.5f,-(a.y+b.y)*.5f,-(a.z+b.z)*.5f};V3 across=cross(direction,toward);
            const float length2=across.x*across.x+across.y*across.y+across.z*across.z;
            if(length2>1.e-8f)across=mul(across,thickness/std::sqrt(length2));else across=mul(camera_right,thickness);
            auto put=[&](V3 p,float e){vertices[n++]={p.x,p.y,p.z,e};};
            put({a.x-across.x,a.y-across.y,a.z-across.z},-1);put({a.x+across.x,a.y+across.y,a.z+across.z},1);put({b.x-across.x,b.y-across.y,b.z-across.z},-1);put({b.x-across.x,b.y-across.y,b.z-across.z},-1);put({a.x+across.x,a.y+across.y,a.z+across.z},1);put({b.x+across.x,b.y+across.y,b.z+across.z},1);
        };
        for(unsigned i=0;i<geometry.count&&i<geometry.segments.size();++i){const auto&s=geometry.segments[i];segment({float(s.a.x-cx),float(s.a.y-cy),float(s.a.z-cz)},{float(s.b.x-cx),float(s.b.y-cy),float(s.b.z-cz)},width*s.width);}
        glViewport(0,0,w,h);glDrawBuffer(GL_COLOR_ATTACHMENT0);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDisable(GL_SCISSOR_TEST);glDisable(GL_CULL_FACE);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_FALSE);glEnable(GL_BLEND);glBlendFuncSeparate(GL_ONE,GL_ONE,GL_ZERO,GL_ZERO);glBlendEquation(GL_FUNC_ADD);
        glUseProgram(program);glUniformMatrix4fv(vp_location,1,GL_FALSE,vp.m);glUniform1f(gain_location,flash.gain*4.f);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferSubData(GL_ARRAY_BUFFER,0,n*sizeof(Vertex),vertices.data());glDrawArrays(GL_TRIANGLES,0,n);glDisable(GL_BLEND);++draws;return true;
    }
    void free(){if(fbo)glDeleteFramebuffers(1,&fbo);if(vbo)glDeleteBuffers(1,&vbo);if(vao)glDeleteVertexArrays(1,&vao);if(program)glDeleteProgram(program);program=vao=vbo=fbo=color=depth=0;geometry_serial=0;geometry={};}
};
