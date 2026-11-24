// Textured purchased forearms, hands, shield and sword. A private depth buffer
// preserves scene depth; the same HDR post pipeline exposes and tones both.
struct TrainingWeapon {
    Program program;GLuint fbo=0,depth=0;int width=0,height=0;
    bool init(){if(!program.load("shaders/world_combat_view.vs.glsl","shaders/world_combat_view.fs.glsl"))return false;if(glGetUniformLocation(program.id,"uRigidWeaponPose")<0||glGetUniformLocation(program.id,"uVertexOverrideEnabled")<0)return false;glGenFramebuffers(1,&fbo);glGenTextures(1,&depth);return fbo&&depth;}
    bool resize(int w,int h){if(width==w&&height==h)return true;if(w<=0||h<=0||w>8192||h>8192)return false;
        glBindTexture(GL_TEXTURE_2D,depth);glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,w,h,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);width=w;height=h;return glGetError()==GL_NO_ERROR;}
    bool draw(WorldCombatMotion *motion,GLuint color,int w,int h,V3 sun,V3 sun_color,V3 sky,V3 ground,V3 world_up={0,1,0}){
        if(!motion||!color||!resize(w,h))return false;
        GLint previous=0,viewport[4];glGetIntegerv(GL_FRAMEBUFFER_BINDING,&previous);glGetIntegerv(GL_VIEWPORT,viewport);
        glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depth,0);glDrawBuffer(GL_COLOR_ATTACHMENT0);
        const bool ready=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
        if(ready){glViewport(0,0,w,h);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glDepthMask(GL_TRUE);glClear(GL_DEPTH_BUFFER_BIT);glDisable(GL_BLEND);glEnable(GL_CULL_FACE);
            program.use();program.m4("uProjection",perspective(1.08f,float(w)/h,.025f,10.f));program.v3("uSun",sun);program.v3("uSunColor",sun_color);program.v3("uSkyLight",sky);program.v3("uGroundLight",ground);program.v3("uWorldUp",world_up);world_combat_motion_draw_arms(motion,program.id);}
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,0,0);glBindFramebuffer(GL_FRAMEBUFFER,GLuint(previous));glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);return ready;
    }
    template<class Draw> bool draw_skeletal(GLuint color,int w,int h,Draw &&draw){
        if(!color||!resize(w,h))return false;
        GLint previous=0,viewport[4],prior_program=0,prior_depth_func=0;
        GLboolean prior_depth_mask=GL_TRUE;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING,&previous);glGetIntegerv(GL_VIEWPORT,viewport);
        glGetIntegerv(GL_CURRENT_PROGRAM,&prior_program);glGetIntegerv(GL_DEPTH_FUNC,&prior_depth_func);
        glGetBooleanv(GL_DEPTH_WRITEMASK,&prior_depth_mask);
        const bool prior_depth=glIsEnabled(GL_DEPTH_TEST),prior_blend=glIsEnabled(GL_BLEND),prior_cull=glIsEnabled(GL_CULL_FACE);
        glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depth,0);glDrawBuffer(GL_COLOR_ATTACHMENT0);
        const bool complete=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
        bool ok=complete;
        if(complete){
            glViewport(0,0,w,h);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glDepthMask(GL_TRUE);
            glClear(GL_DEPTH_BUFFER_BIT);glDisable(GL_BLEND);glEnable(GL_CULL_FACE);
            ok=draw();
        }
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,0,0);
        glBindFramebuffer(GL_FRAMEBUFFER,GLuint(previous));glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
        glUseProgram(GLuint(prior_program));glDepthFunc(GLenum(prior_depth_func));glDepthMask(prior_depth_mask);
        if(prior_depth)glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);
        if(prior_blend)glEnable(GL_BLEND);else glDisable(GL_BLEND);
        if(prior_cull)glEnable(GL_CULL_FACE);else glDisable(GL_CULL_FACE);
        return ok;
    }
    bool prepare(WorldCombatMotion *motion,int w,int h){
        GLuint color=0;glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,w,h,0,GL_RGBA,GL_HALF_FLOAT,nullptr);
        const bool ready=draw(motion,color,w,h,{.3f,.8f,.4f},{1,1,1},{.2f,.2f,.2f},{.04f,.04f,.04f});glFinish();glDeleteTextures(1,&color);return ready&&resize(w,h);
    }
    void release(){if(depth)glDeleteTextures(1,&depth);if(fbo)glDeleteFramebuffers(1,&fbo);depth=fbo=0;program.free();}
};

void training_panel(const WorldTrainingCombat &c,bool nearby,bool in_yard,bool paused,int width,int height,bool first_person=false){
    if(!nearby&&!in_yard&&!c.engaged())return;
    const bool result=c.state==TrainingState::Won||c.state==TrainingState::Lost||c.state==TrainingState::Cancelled;
    if(first_person)ImGui::SetNextWindowPos({18,18},ImGuiCond_Always,{0,0});
    else ImGui::SetNextWindowPos({float(width)*.5f,float(height)-20},ImGuiCond_Always,{.5f,1.f});
    ImGui::SetNextWindowSize({std::min(first_person?440.f:640.f,float(width)-30),0});ImGui::SetNextWindowBgAlpha(.88f);
    ImGui::Begin("West court | Sparring",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings);
    if(!c.engaged()){
        ImGui::TextColored({.96f,.80f,.48f,1},"WEST COURT | KNIGHT TRAINING");
        if(result){ImGui::TextWrapped("%s",c.feedback);ImGui::Text("Hits %u  Perfect parries %u  Blocks %u",c.hits,c.parries,c.blocks);}
        ImGui::TextWrapped("%s",result?"Press E to retry here, or walk away.":nearby?"Press E to start three non-lethal sparring rounds":"Approach the master of arms beside the practice pairs. Press E nearby.");
        ImGui::TextWrapped("Mouse flick / arrows: angle | LMB: swing | RMB: guard | MMB: thrust\nWASD: footwork | Space: quick step | Esc: leave training");
    }else{
        ImGui::TextColored({.96f,.80f,.48f,1},"ROUND %u / 3 | %s%s",c.round+1,c.opponent_name(),paused?" | PAUSED":"");
        ImGui::ProgressBar(c.health/100.f,{195,13},"Your health");ImGui::SameLine();ImGui::ProgressBar(c.stamina/100.f,{195,13},"Stamina");
        ImGui::ProgressBar(c.enemy_health/c.enemy_max_health,{400,13},"Knight health");
        ImGui::Text("Your angle: %s%s  |  Knight guard: %s",world_training_direction_name(c.selected),c.guard?" [GUARD]":"",world_training_direction_name(c.enemy_guard));
        if(c.enemy_phase==TrainingEnemyPhase::Windup&&c.state==TrainingState::Fighting){
            const bool parry_now=c.enemy_duration-c.enemy_time<=.24;
            ImGui::TextColored(parry_now?ImVec4(.45f,1,.65f,1):ImVec4(1,.66f,.25f,1),"%s %s - match guard or step out!",parry_now?"PARRY NOW:":"INCOMING",world_training_direction_name(c.enemy_direction));
            ImGui::ProgressBar(float(c.enemy_time/c.enemy_duration),{400,8},"");
        }
        if(c.state==TrainingState::Countdown||c.state==TrainingState::RoundBreak)ImGui::Text("Ready in %.0f...",std::ceil(std::max(0.,c.phase_time)));
        if(c.feedback_time>0)ImGui::TextWrapped("%s",c.feedback);
        ImGui::Separator();
        ImGui::TextWrapped("Mouse flick / arrow keys: angle | LMB: swing | MMB: thrust\nHold RMB: directional guard | Space: quick step | WASD: footwork\nT: select thrust guard | Esc: leave safely | F10: settings / pause");
    }
    ImGui::End();
}
