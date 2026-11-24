// Owned by the viewer/context. Two cubes prevent sampling partially refreshed
// faces. After the first complete map, two faces/frame bound the steady cost.
struct LocalLightShadow {
    GLuint fbo=0,cube[2]={};
    int side=0,front=0,next_face=0;
    bool ready=false,enabled=false;
    std::array<float,4> active={},pending={};
    float strength=0;
    uint64_t faces=0,cycles=0,selections=0,first_refreshes=0,failures=0;
    double cpu_ms=0;
    static constexpr float near_plane=.08f;
    bool allocate(int wanted) {
        if(fbo&&side==wanted)return true;
        GLint draw=0,read=0,unit=0,binding=0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);
        glGetIntegerv(GL_ACTIVE_TEXTURE,&unit);glActiveTexture(GL_TEXTURE0+16);glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP,&binding);
        GLuint next_fbo=0,next_cube[2]={};glGenFramebuffers(1,&next_fbo);glGenTextures(2,next_cube);
        glBindFramebuffer(GL_FRAMEBUFFER,next_fbo);glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);
        bool ok=true;
        for(auto texture:next_cube){
            glBindTexture(GL_TEXTURE_CUBE_MAP,texture);
            for(unsigned face=0;face<6;face++){
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,0,GL_DEPTH_COMPONENT24,wanted,wanted,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);
                glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,texture,0);
                ok=(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE)&&ok;
            }
            glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
        }
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,draw);glBindFramebuffer(GL_READ_FRAMEBUFFER,read);
        glBindTexture(GL_TEXTURE_CUBE_MAP,GLuint(binding));glActiveTexture(GLenum(unit));
        if(!ok){glDeleteFramebuffers(1,&next_fbo);glDeleteTextures(2,next_cube);failures++;return false;}
        release();fbo=next_fbo;cube[0]=next_cube[0];cube[1]=next_cube[1];side=wanted;return true;
    }
    bool select(const WorldTorchLights &lights,const Camera &eye,bool allowed,int wanted) {
        enabled=false;
        if(!allowed){ready=false;next_face=0;strength=0;return true;}
        int best=-1;float best_score=0,old_score=0;int previous=-1;
        for(unsigned i=0;i<lights.count;i++){
            const auto &p=lights.position_radius[i];const auto &c=lights.color_intensity[i];
            // Calibrated apartment windows need real curtain/furniture shadows.
            // Other authored spot fills retain their previous budget policy.
            if(lights.direction_cone[i][3]>-.999f&&!world_apartment_window_light::shadow_source(p))continue;
            const float dx=float(p[0]-eye.x),dy=float(p[1]-eye.y),dz=float(p[2]-eye.z),distance2=dx*dx+dy*dy+dz*dz;
            if(distance2>(p[3]+8)*(p[3]+8)||p[3]<=.25f||c[3]<=.001f)continue;
            const float score=c[3]*std::max({c[0],c[1],c[2]})/(1+distance2);
            if(score>best_score){best=int(i);best_score=score;}
            float difference=0;for(unsigned a=0;a<3;a++)difference+=(p[a]-pending[a])*(p[a]-pending[a]);
            if(difference<.0004f&&std::abs(std::clamp(p[3],.5f,32.f)-pending[3])<.01f){previous=int(i);old_score=score;}
        }
        if(best<0){ready=false;next_face=0;strength=0;return true;}
        if(previous>=0&&old_score>=best_score*.65f)best=previous;
        if(!allocate(wanted))return false;
        auto chosen=lights.position_radius[unsigned(best)];chosen[3]=std::clamp(chosen[3],.5f,32.f);
        float difference=0;for(unsigned a=0;a<4;a++)difference+=(chosen[a]-pending[a])*(chosen[a]-pending[a]);
        if(difference>.0004f){pending=chosen;next_face=0;selections++;}
        enabled=true;return true;
    }
    M4 face_matrix(int face,const Camera &eye)const {
        static const V3 directions[]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
        static const V3 ups[]={{0,-1,0},{0,-1,0},{0,0,1},{0,0,-1},{0,-1,0},{0,-1,0}};
        const V3 origin={float(pending[0]-eye.x),float(pending[1]-eye.y),float(pending[2]-eye.z)};
        return product(perspective(1.57079632679f,1,near_plane,pending[3]),look(origin,add(origin,directions[face]),ups[face]));
    }
    template<class Draw> bool update(const Camera &eye,Draw draw) {
        if(!enabled)return true;
        const auto started=SDL_GetPerformanceCounter();
        const int count=ready?2:6;
        if(!ready)first_refreshes++;
        glBindFramebuffer(GL_FRAMEBUFFER,fbo);glViewport(0,0,side,side);glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);
        glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDisable(GL_BLEND);glDisable(GL_SCISSOR_TEST);glDisable(GL_CULL_FACE);
        glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(1.1f,1.5f);
        for(int i=0;i<count;i++){
            const int face=next_face;
            glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,cube[1-front],0);
            glClearDepth(1.);glClear(GL_DEPTH_BUFFER_BIT);
            if(!draw(face_matrix(face,eye))){glDisable(GL_POLYGON_OFFSET_FILL);return false;}
            next_face++;faces++;
            if(next_face==6){float changed=0;for(unsigned a=0;a<4;a++)changed+=(active[a]-pending[a])*(active[a]-pending[a]);if(!ready||changed>.0004f)strength=0;next_face=0;front=1-front;active=pending;ready=true;cycles++;break;}
        }
        // A completed cube stays fully shadowed during later refresh cycles.
        // Only new source selection needs a short brightness transition.
        strength=std::min(1.f,strength+.2f);
        glDisable(GL_POLYGON_OFFSET_FILL);glEnable(GL_CULL_FACE);
        cpu_ms+=1000.*double(SDL_GetPerformanceCounter()-started)/SDL_GetPerformanceFrequency();
        return true;
    }
    void bind(Program &p,bool supported)const {
        if(!supported)return;
        p.i("uLocalLightShadow",16);p.f("uLocalLightShadowEnabled",enabled&&ready?strength:0.f);
        p.f("uLocalLightShadowSide",float(std::max(side,1)));
        glUniform4fv(p.at("uLocalLightShadowPositionRadius"),1,active.data());
        p.v2("uLocalLightShadowNearFar",near_plane,std::max(active[3],.5f));
        glActiveTexture(GL_TEXTURE0+16);glBindSampler(16,0);glBindTexture(GL_TEXTURE_CUBE_MAP,ready?cube[front]:0);glActiveTexture(GL_TEXTURE0);
    }
    uint64_t bytes()const{return fbo?12ull*side*side*4:0;}
    void release(){glDeleteFramebuffers(1,&fbo);glDeleteTextures(2,cube);fbo=0;cube[0]=cube[1]=0;side=0;front=next_face=0;ready=enabled=false;strength=0;}
};
LocalLightShadow local_light_shadow;
