// Marked private settings rehearsal. Commands are real SDL input events;
// the selected preference and saved file are observed, never assigned here.
const char *ultra_ui_dir_env=std::getenv("VALWOOD_QA_ULTRA_UI_DIR");
const bool ultra_ui_qa=launch->frames&&!launch->benchmark&&ultra_ui_dir_env;
const std::filesystem::path ultra_ui_dir=ultra_ui_dir_env?ultra_ui_dir_env:"";
struct UltraUICommand {std::uint64_t frame=0;std::string action,value;int x=0,y=0;};
std::vector<UltraUICommand> ultra_ui_commands;
std::vector<std::string> ultra_ui_captures;
std::string ultra_ui_error,ultra_ui_pending;
unsigned ultra_ui_events=0,ultra_ui_gl_errors=0;
std::uint64_t ultra_ui_frame=0;
std::size_t ultra_ui_next=0;
bool ultra_ui_started=false,ultra_ui_done=false,ultra_ui_selected=false;
const auto ultra_ui_original_resident=resident_character_textures;
auto ultra_ui_fail=[&](const std::string &error){if(ultra_ui_error.empty())ultra_ui_error=error;launch->exit_code=1;running=false;};
auto ultra_ui_key=[&](SDL_Keycode key){
    SDL_Event event{};event.type=SDL_KEYDOWN;event.key.windowID=SDL_GetWindowID(window);event.key.state=SDL_PRESSED;
    event.key.keysym.sym=key;event.key.keysym.scancode=SDL_GetScancodeFromKey(key);event.key.keysym.unused=0x56414c54u;
    if(SDL_PushEvent(&event)!=1){ultra_ui_fail("SDL rejected private settings key");return;}
    event.type=SDL_KEYUP;event.key.state=SDL_RELEASED;if(SDL_PushEvent(&event)!=1)ultra_ui_fail("SDL rejected private settings release");ultra_ui_events+=2;
};
auto ultra_ui_step=[&](std::uint64_t frame){
    if(!ultra_ui_qa||ultra_ui_done||!ultra_ui_error.empty())return;
    ultra_ui_frame=frame;
    if(!ultra_ui_started){
        std::ifstream marker(".valwood-ultra-ui-qa");std::string token,trailing;marker>>token;
        if(!ultra_ui_dir.is_absolute()||token!="VALWOOD_PRIVATE_ULTRA_UI_V1"||(marker>>trailing)||
           std::filesystem::current_path().parent_path()!=ultra_ui_dir||
           std::filesystem::weakly_canonical(graphics_settings_path)!=std::filesystem::weakly_canonical("graphics.settings")){
            ultra_ui_fail("Missing isolated settings project marker");return;
        }
        std::ifstream script(ultra_ui_dir/"inputs.txt");std::string magic;unsigned version=0;script>>magic>>version;
        if(magic!="VALWOOD_ULTRA_UI"||version!=1){ultra_ui_fail("Invalid private settings input script");return;}
        std::uint64_t previous=0;UltraUICommand command;
        while(script>>command.frame>>command.action){
            command.value.clear();command.x=command.y=0;
            if(command.frame<previous||command.frame<2||command.frame>=std::uint64_t(launch->frames)-2||ultra_ui_commands.size()>=64){ultra_ui_fail("Unbounded private settings input script");return;}
            previous=command.frame;
            if(command.action=="key"||command.action=="button"||command.action=="capture")script>>command.value;
            else if(command.action=="move")script>>command.x>>command.y;
            else if(command.action!="finish"){ultra_ui_fail("Unknown settings input action");return;}
            if(!script){ultra_ui_fail("Incomplete settings input action");return;}
            if(command.action=="capture")for(char c:command.value)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-')){ultra_ui_fail("Invalid native capture name");return;}
            ultra_ui_commands.push_back(command);
        }
        if(!script.eof()||ultra_ui_commands.empty()||ultra_ui_commands.back().action!="finish"){ultra_ui_fail("Incomplete private settings input script");return;}
        ultra_ui_started=true;
        SDL_Event focus{};focus.type=SDL_WINDOWEVENT;focus.window.windowID=SDL_GetWindowID(window);focus.window.event=SDL_WINDOWEVENT_FOCUS_GAINED;
        if(SDL_PushEvent(&focus)!=1){ultra_ui_fail("SDL rejected private settings focus");return;}++ultra_ui_events;
    }
    ultra_ui_selected|=unsigned(graphics_draft.character_textures)==3;
    while(ultra_ui_next<ultra_ui_commands.size()&&ultra_ui_commands[ultra_ui_next].frame<=frame){
        const auto &command=ultra_ui_commands[ultra_ui_next++];
        if(command.action=="key"){
            const SDL_Keycode key=SDL_GetKeyFromName(command.value.c_str());
            if(key==SDLK_UNKNOWN){ultra_ui_fail("Unknown private settings key");return;}ultra_ui_key(key);
        }else if(command.action=="move"){
            if(command.x<0||command.x>1600||command.y<0||command.y>900){ultra_ui_fail("Private settings pointer outside window");return;}
            SDL_Event event{};event.type=SDL_MOUSEMOTION;event.motion.windowID=SDL_GetWindowID(window);event.motion.which=0x56414c54u;event.motion.x=command.x;event.motion.y=command.y;
            if(SDL_PushEvent(&event)!=1){ultra_ui_fail("SDL rejected private settings pointer");return;}++ultra_ui_events;
        }else if(command.action=="button"){
            if(command.value!="down"&&command.value!="up"){ultra_ui_fail("Invalid private settings pointer button");return;}
            const bool down=command.value=="down";SDL_Event event{};event.type=down?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;
            event.button.windowID=SDL_GetWindowID(window);event.button.which=0x56414c54u;event.button.button=SDL_BUTTON_LEFT;event.button.state=down?SDL_PRESSED:SDL_RELEASED;
            if(SDL_PushEvent(&event)!=1){ultra_ui_fail("SDL rejected private settings click");return;}++ultra_ui_events;
        }else if(command.action=="capture")ultra_ui_pending=command.value;
        else if(command.action=="finish"){ultra_ui_done=true;running=false;}
    }
};
auto ultra_ui_capture=[&](int width,int height){
    if(!ultra_ui_qa||!ultra_ui_started||ultra_ui_pending.empty()||!ultra_ui_error.empty())return;
    std::vector<unsigned char> pixels(std::size_t(width)*height*3);GLint alignment=4;glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());glPixelStorei(GL_PACK_ALIGNMENT,alignment);
    if(glGetError()!=GL_NO_ERROR){++ultra_ui_gl_errors;ultra_ui_fail("Native settings framebuffer GL error");return;}
    const auto stride=std::size_t(width)*3;for(int row=0;row<height/2;++row)std::swap_ranges(pixels.data()+std::size_t(row)*stride,pixels.data()+std::size_t(row+1)*stride,pixels.data()+std::size_t(height-1-row)*stride);
    auto *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels.data(),width,height,24,width*3,SDL_PIXELFORMAT_RGB24);const auto path=ultra_ui_dir/(ultra_ui_pending+".bmp");
    const bool saved=!std::filesystem::exists(path)&&surface&&SDL_SaveBMP(surface,path.u8string().c_str())==0;if(surface)SDL_FreeSurface(surface);
    if(!saved){ultra_ui_fail("Could not preserve unique native settings capture");return;}
    ultra_ui_captures.push_back(path.u8string());ultra_ui_pending.clear();
};
auto ultra_ui_finish=[&](){
    if(!ultra_ui_qa)return;
    WorldGraphicsSettings saved;char error[256]={};const auto loaded=world_graphics_settings_load(graphics_settings_path,&saved,error,sizeof(error));
    const bool persisted=loaded==WorldGraphicsSettingsLoadResult::Loaded&&unsigned(saved.character_textures)==3;
    const bool unchanged=resident_character_textures==ultra_ui_original_resident&&unsigned(resident_character_textures)==2;
    const bool passed=ultra_ui_started&&ultra_ui_done&&ultra_ui_error.empty()&&ultra_ui_selected&&persisted&&unchanged&&unsigned(graphics.character_textures)==3&&ultra_ui_captures.size()>=3&&ultra_ui_gl_errors==0;
    if(!passed&&ultra_ui_error.empty())ultra_ui_error="Actual GUI selection/save or restart boundary was not observed";
    std::ofstream out(ultra_ui_dir/"ULTRA_UI_RAW.json");out<<"{\"schema\":\"valwood.ultra4k.actual-settings-ui.v1\",\"passed\":"<<(passed?"true":"false")<<",\"error\":"<<world_json::quote(ultra_ui_error)
       <<",\"actual_SDL_input\":true,\"setting_injected\":false,\"private_project\":true,\"draft_ultra_observed\":"<<(ultra_ui_selected?"true":"false")<<",\"saved_ultra\":"<<(persisted?"true":"false")
       <<",\"resident_unchanged_until_restart\":"<<(unchanged?"true":"false")<<",\"resident_quality\":"<<unsigned(resident_character_textures)<<",\"active_preference\":"<<unsigned(graphics.character_textures)<<",\"last_frame\":"<<ultra_ui_frame<<",\"input_events\":"<<ultra_ui_events<<",\"GL_errors\":"<<ultra_ui_gl_errors<<",\"capture_paths\":[";
    for(unsigned i=0;i<ultra_ui_captures.size();++i)out<<(i?",":"")<<world_json::quote(ultra_ui_captures[i]);out<<"]}\n";
    if(!out||!passed)launch->exit_code=1;
    std::fprintf(stderr,"[Ultra UI QA] %s selected%d saved%d resident%d events%u: %s\n",passed?"PASS":"FAIL",int(ultra_ui_selected),int(persisted),int(unchanged),ultra_ui_events,ultra_ui_error.c_str());
};
