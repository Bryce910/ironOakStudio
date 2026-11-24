// Included inside the viewer namespace after Program/Camera/M4. All geometry
// comes from the exact successful CPU collision capture, never a second bake.
struct WorldStructureGPU {
    struct Range {world_structure::Point origin;std::array<double,6> bounds;int first=0,count=0;};
    GLuint vao=0,vbo=0;Program surface,depth;std::vector<Range> ranges;
    std::uint64_t bytes=0,fingerprint=0,triangles=0;unsigned walls=0,bridges=0,segments=0,draws=0,shadow_draws=0;
    void release(){if(vbo)glDeleteBuffers(1,&vbo);if(vao)glDeleteVertexArrays(1,&vao);surface.free();depth.free();vbo=vao=0;ranges.clear();bytes=fingerprint=triangles=0;walls=bridges=segments=0;}
    bool prepare(const world_structure_runtime::Prepared &source,std::string &error){
        WorldStructureGPU next;next.fingerprint=source.fingerprint;next.triangles=source.triangles;next.walls=source.walls;next.bridges=source.bridges;next.segments=source.segments;
        if(source.meshes.empty()){release();*this=std::move(next);return true;}
        static const char *vs=R"GLSL(#version 330 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aMaterialUV;
uniform mat4 uVP,uLightVP;uniform vec3 uOffset;
out vec3 vNormal,vRelative;out vec3 vUV;out vec4 vShadow;
void main(){vec3 p=aPosition+uOffset;gl_Position=uVP*vec4(p,1);vRelative=p;vNormal=aNormal;vUV=aMaterialUV;vShadow=uLightVP*vec4(p,1);}
)GLSL";
        static const char *fs=R"GLSL(#version 330 core
in vec3 vNormal,vRelative,vUV;in vec4 vShadow;
layout(location=0)out vec4 FragColor;layout(location=1)out vec4 IndirectColor;
uniform vec3 uSun,uSunColor,uAmbient,uFog;uniform float uHaze,uShadowTexel;
uniform sampler2D uShadow;uniform bool uHasShadow,uClipEnabled;uniform float uClipRelativeY;
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float shadowing(vec3 n){if(!uHasShadow||vShadow.w<=0.)return 1.;vec3 p=vShadow.xyz/vShadow.w*.5+.5;if(any(lessThan(p,vec3(0)))||any(greaterThan(p,vec3(1))))return 1.;float a=0.;float bias=max(.00035,.0011*(1.-max(dot(n,uSun),0.)));for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++)a+=p.z-bias<=texture(uShadow,p.xy+vec2(x,y)*uShadowTexel).r?1.:0.;return a/9.;}
void main(){if(uClipEnabled&&vRelative.y<uClipRelativeY)discard;vec3 n=normalize(vNormal);if(!gl_FrontFacing)n=-n;vec2 uv=vUV.xy;vec3 base;float rough;
 if(vUV.z>.5){float row=floor(uv.y/.42);vec2 q=vec2(uv.x+mod(row,2.)*.48,uv.y);vec2 cell=floor(q/vec2(.96,.42));vec2 f=fract(q/vec2(.96,.42));float mortar=1.-smoothstep(.012,.035,min(min(f.x,1.-f.x),min(f.y,1.-f.y)));float tone=hash(cell);float grain=sin(uv.x*53.+sin(uv.y*27.)*2.)*.017+sin(uv.y*97.)*.01;base=mix(vec3(.27,.29,.28),vec3(.43,.44,.40),tone)+grain;base=mix(base,vec3(.13,.14,.13),mortar*.8);rough=.89;
 }else{vec2 f=fract(vec2(uv.x/.24,uv.y));float seam=1.-smoothstep(.009,.028,min(f.x,1.-f.x));float plank=hash(vec2(floor(uv.x/.24),0));float grain=sin(uv.y*48.+sin(uv.x*54.)*3.)*.018+sin(uv.y*153.+uv.x*19.)*.008;base=mix(vec3(.20,.12,.065),vec3(.38,.255,.135),plank*.65+.22)+grain;base*=1.-seam*.62;vec2 nail=vec2(min(f.x,1.-f.x)*.24,abs(fract(uv.y/.8)-.5)*.8);if(nail.x<.028&&length(nail-vec2(.018,0))<.009)base=vec3(.075,.08,.075);rough=.76;}
 base=pow(max(base,vec3(.01)),vec3(2.2));vec3 v=normalize(-vRelative),h=normalize(v+uSun);float ndl=max(dot(n,uSun),0.),ndv=max(dot(n,v),.001),ndh=max(dot(n,h),0.);float a=rough*rough,a2=a*a,den=ndh*ndh*(a2-1.)+1.;float D=a2/(3.14159265*den*den);float k=(rough+1.)*(rough+1.)*.125;float G=(ndv/(ndv*(1.-k)+k))*(ndl/(ndl*(1.-k)+k));vec3 F=vec3(.04)+(1.-vec3(.04))*pow(1.-max(dot(h,v),0.),5.);float sh=shadowing(n);vec3 indirect=base*uAmbient*(.42+.58*max(n.y*.5+.5,0.));vec3 direct=(base/3.14159265+D*G*F/max(4.*ndv*max(ndl,.001),.001))*uSunColor*ndl*sh;vec3 color=indirect+direct;float fog=1.-exp(-length(vRelative)*max(uHaze,0.)*.000035);FragColor=vec4(mix(color,uFog,clamp(fog,0.,.94)),.995);IndirectColor=vec4(indirect*(1.-fog),0.);}
)GLSL";
        static const char *dvs=R"GLSL(#version 330 core
layout(location=0)in vec3 aPosition;uniform mat4 uVP;uniform vec3 uOffset;void main(){gl_Position=uVP*vec4(aPosition+uOffset,1);}
)GLSL";
        static const char *dfs="#version 330 core\nvoid main(){}\n";
        next.surface.id=GLH_program(vs,fs);next.depth.id=GLH_program(dvs,dfs);
        GLint ok=0;glGetProgramiv(next.surface.id,GL_LINK_STATUS,&ok);if(ok)glGetProgramiv(next.depth.id,GL_LINK_STATUS,&ok);
        if(!ok){next.release();error="Structure material shader failed; previous structures retained";return false;}
        std::vector<world_structure_runtime::Vertex> vertices;vertices.reserve(source.vertices);
        for(const auto &m:source.meshes){next.ranges.push_back({m.origin,m.bounds,int(vertices.size()),int(m.vertices.size())});vertices.insert(vertices.end(),m.vertices.begin(),m.vertices.end());}
        GLint oldvao=0,oldvbo=0;glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&oldvao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&oldvbo);
        glGenVertexArrays(1,&next.vao);glGenBuffers(1,&next.vbo);glBindVertexArray(next.vao);glBindBuffer(GL_ARRAY_BUFFER,next.vbo);glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(vertices.size()*sizeof(world_structure_runtime::Vertex)),vertices.data(),GL_STATIC_DRAW);
        for(unsigned i=0;i<3;i++){glEnableVertexAttribArray(i);glVertexAttribPointer(i,3,GL_FLOAT,GL_FALSE,sizeof(world_structure_runtime::Vertex),reinterpret_cast<void*>(std::size_t(i)*3*sizeof(float)));}
        glBindVertexArray(GLuint(oldvao));glBindBuffer(GL_ARRAY_BUFFER,GLuint(oldvbo));
        if(glGetError()!=GL_NO_ERROR){next.release();error="Structure mesh upload failed; previous structures retained";return false;}
        next.bytes=vertices.size()*sizeof(world_structure_runtime::Vertex);release();*this=std::move(next);error.clear();return true;
    }
    void draw(const Camera &camera,M4 vp,M4 lightVP,V3 sun,V3 sunColor,V3 ambient,V3 fog,float haze,GLuint shadowTexture,unsigned shadowSide,bool shadow=false,double clipRelativeY=std::numeric_limits<double>::quiet_NaN()){
        if(!vao||ranges.empty())return;
        GLint oldprogram=0,oldvao=0,oldactive=0,oldtexture=0;glGetIntegerv(GL_CURRENT_PROGRAM,&oldprogram);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&oldvao);glGetIntegerv(GL_ACTIVE_TEXTURE,&oldactive);
        auto &p=shadow?depth:surface;p.use();p.m4("uVP",vp);
        if(!shadow){p.m4("uLightVP",lightVP);p.v3("uSun",sun);p.v3("uSunColor",sunColor);p.v3("uAmbient",ambient);p.v3("uFog",fog);p.f("uHaze",haze);p.f("uShadowTexel",shadowSide?1.f/shadowSide:0);p.i("uHasShadow",shadowTexture&&shadowSide?1:0);p.i("uClipEnabled",std::isfinite(clipRelativeY)?1:0);p.f("uClipRelativeY",std::isfinite(clipRelativeY)?float(clipRelativeY):0.f);glActiveTexture(GL_TEXTURE7);glGetIntegerv(GL_TEXTURE_BINDING_2D,&oldtexture);glBindTexture(GL_TEXTURE_2D,shadowTexture);p.i("uShadow",7);}
        glBindVertexArray(vao);
        for(const auto &r:ranges){
            const double x=(r.bounds[0]+r.bounds[1])*.5-camera.x,y=(r.bounds[2]+r.bounds[3])*.5-camera.y,z=(r.bounds[4]+r.bounds[5])*.5-camera.z;
            if(!world_terrain_box_visible(vp.m,x,y,z,(r.bounds[1]-r.bounds[0])*.5,(r.bounds[3]-r.bounds[2])*.5,(r.bounds[5]-r.bounds[4])*.5))continue;
            p.v3("uOffset",{float(r.origin.x-camera.x),float(r.origin.y-camera.y),float(r.origin.z-camera.z)});glDrawArrays(GL_TRIANGLES,r.first,r.count);if(shadow)++shadow_draws;else ++draws;
        }
        if(!shadow){glActiveTexture(GL_TEXTURE7);glBindTexture(GL_TEXTURE_2D,GLuint(oldtexture));}glActiveTexture(GLenum(oldactive));glBindVertexArray(GLuint(oldvao));glUseProgram(GLuint(oldprogram));
    }
};
