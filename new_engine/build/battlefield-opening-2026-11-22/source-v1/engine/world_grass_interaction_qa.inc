// Opt-in private-project integration fixture. Drives the real SDL virtual pad
// through the ordinary gameplay controller and collision path. Only the
// observer is fixed; player position and shader contacts are never injected.
unsigned grass_qa_phase=0,grass_qa_checks=0,grass_qa_captures=0,grass_qa_pending=0;
bool grass_qa_passed=false,grass_qa_failed=false,grass_qa_observer_ready=false;
unsigned grass_qa_first_gl_error=0;
double grass_qa_age=0,grass_qa_x=0,grass_qa_y=0,grass_qa_z=0,grass_qa_distance=0;
uint64_t grass_qa_query_start=0;
Camera grass_qa_observer;
std::array<std::array<double,5>,3> grass_qa_observations{};
auto grass_qa_check=[&](bool condition,const char *message){
    ++grass_qa_checks;if(condition)return true;
    grass_qa_failed=true;launch->exit_code=1;running=false;
    controller_test_pad.axis(SDL_CONTROLLER_AXIS_LEFTY,0);
    std::fprintf(stderr,"[Grass interaction QA] FAIL phase%u: %s\n",grass_qa_phase,message);return false;
};
auto grass_interaction_workflow=[&](float dt){
    if(!qa_grass||grass_qa_passed||grass_qa_failed)return;
    controller_test_pad.tick();
    if(frame>uint64_t(launch->frames)-10){grass_qa_check(false,"Frame budget exhausted");return;}
    if(destination_loading||frame<=uint64_t(launch->warmup)||!terrain.view_ready(cam.x,cam.z,false))return;
    if(grass_qa_phase==0){
        SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);SDL_PumpEvents();preview_pad.focused=true;
        if(!grass_qa_check(!source_review.active&&!cam.fly&&cam.grounded&&!studio.open&&!studio.cinematic_preview&&!preview_blocked(),"Grounded gameplay, no editor/cinematic/route"))return;
        if(!grass_qa_check(preview_pad.id()==SDL_JoystickInstanceID(controller_test_pad.joystick),"Actual owned SDL virtual controller"))return;
        if(!grass_qa_check(active_playtest_walk||active_city_physics,"Production source physics available"))return;
        grass_qa_x=cam.x;grass_qa_y=cam.y-1.75;grass_qa_z=cam.z;
        grass_qa_query_start=grass_player_movement_queries;grass_qa_phase=1;grass_qa_age=0;return;
    }
    if(!grass_qa_check(!cam.fly&&cam.grounded&&!studio.open&&!studio.cinematic_preview&&!preview_blocked(),"Controller stays in ordinary grounded gameplay"))return;
    grass_qa_age+=dt;
    if(grass_qa_phase==1&&grass_qa_age>=.6&&grass_qa_captures==0)grass_qa_pending=1;
    if(grass_qa_phase==2&&grass_qa_age>=1.25){
        controller_test_pad.axis(SDL_CONTROLLER_AXIS_LEFTY,0);
        grass_qa_distance=std::hypot(cam.x-grass_qa_x,cam.z-grass_qa_z);
        if(!grass_qa_check(grass_qa_distance>=.8&&grass_qa_distance<=4.,"Real SDL stick moves player through physics"))return;
        if(!grass_qa_check(grass_interaction.history_samples>=3&&grass_interaction.peak_contacts==4,"Bounded contact trail sampled from real movement"))return;
        grass_qa_pending=2;grass_qa_phase=3;grass_qa_age=0;
    }
    if(grass_qa_phase==4&&grass_qa_age>=1.){
        if(!grass_qa_check(grass_interaction.active_history==0&&grass_interaction.recovery_frames>0,"Released trail recovers exactly"))return;
        if(!grass_qa_check(grass_player_movement_queries>grass_qa_query_start+5&&grass_player_movement_query_failures==0,"Production movement queries executed without failure"))return;
        grass_qa_pending=3;
    }
};
auto grass_interaction_capture=[&](int width,int height){
    if(!grass_qa_pending)return;
    const unsigned index=grass_qa_pending-1;
    const char *names[]={"grass-before.bmp","grass-contact.bmp","grass-recovered.bmp"};
    std::vector<unsigned char> pixels(size_t(width)*height*3);
    GLint alignment=4;glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);
    glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());glPixelStorei(GL_PACK_ALIGNMENT,alignment);
    const size_t stride=size_t(width)*3;
    for(int row=0;row<height/2;++row)std::swap_ranges(pixels.data()+row*stride,pixels.data()+(row+1)*stride,pixels.data()+(height-1-row)*stride);
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels.data(),width,height,24,int(stride),SDL_PIXELFORMAT_RGB24);
    const bool saved=surface&&SDL_SaveBMP(surface,names[index])==0;if(surface)SDL_FreeSurface(surface);
    const GLenum error=glGetError();if(error&&!grass_qa_first_gl_error)grass_qa_first_gl_error=error;
    if(!grass_qa_check(saved&&error==GL_NO_ERROR,"Rendered observer capture"))return;
    grass_qa_observations[index]={cam.x,cam.y-1.75,cam.z,double(grass_interaction.active_history),grass_interaction.clock};
    ++grass_qa_captures;grass_qa_pending=0;grass_qa_age=0;
    if(index==0){controller_test_pad.axis(SDL_CONTROLLER_AXIS_LEFTY,-22000);grass_qa_phase=2;}
    else if(index==1)grass_qa_phase=4;
    else {grass_qa_phase=100;grass_qa_passed=true;running=false;}
};
auto grass_interaction_report=[&](){
    if(!qa_grass)return;
    if(!grass_qa_passed){grass_qa_failed=true;launch->exit_code=1;}
    FILE *f=std::fopen("grass-interaction-qa.json","wb");if(!f){launch->exit_code=1;return;}
    std::fprintf(f,"{\"schema\":\"valwood.grass-interaction.runtime-qa.v1\",\"passed\":%s,\"failed\":%s,\"phase\":%u,\"checks\":%u,\"first_gl_error\":%u,\"actual_sdl_controller\":true,\"injected_player_pose\":false,\"fixed_observer\":true,\"captures\":%u,\"distance_m\":%.8f,\"movement_queries\":%llu,\"movement_query_failures\":%llu,\"active_frames\":%llu,\"moving_frames\":%llu,\"history_samples\":%llu,\"recovery_frames\":%llu,\"peak_contacts\":%u,\"active_history\":%u,\"observations\":[",
        grass_qa_passed?"true":"false",grass_qa_failed?"true":"false",grass_qa_phase,grass_qa_checks,grass_qa_first_gl_error,grass_qa_captures,grass_qa_distance,
        (unsigned long long)(grass_player_movement_queries-grass_qa_query_start),(unsigned long long)grass_player_movement_query_failures,
        (unsigned long long)grass_interaction.active_frames,(unsigned long long)grass_interaction.moving_frames,(unsigned long long)grass_interaction.history_samples,
        (unsigned long long)grass_interaction.recovery_frames,grass_interaction.peak_contacts,grass_interaction.active_history);
    for(unsigned i=0;i<3;++i){const auto &o=grass_qa_observations[i];std::fprintf(f,"%s{\"feet\":[%.8f,%.8f,%.8f],\"active_history\":%.0f,\"clock_seconds\":%.8f}",i?",":"",o[0],o[1],o[2],o[3],o[4]);}
    std::fprintf(f,"]}\n");if(std::fclose(f))launch->exit_code=1;
};
