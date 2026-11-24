#include <SDL2/SDL.h>
#include <iostream>
#include <stdexcept>
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": " #x);}while(0)
int main(){try{
 bool cutscene_enter=false,cutscene_space=false,cutscene_interact_down=false;struct Pad{int id()const{return 7;}}preview_pad;
 // Exact non-GPU input-latch prefix extracted from the frozen production host.
 #include "../review/cutscenes-host/input-latch-under-test.inc"
 SDL_Event e={};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_e;cutscene_event(e);CHECK(cutscene_interact_down);
 e.type=SDL_KEYUP;cutscene_event(e);CHECK(!cutscene_interact_down);
 cutscene_interact_down=true;e={};e.type=SDL_CONTROLLERBUTTONUP;e.cbutton.which=8;e.cbutton.button=SDL_CONTROLLER_BUTTON_A;cutscene_event(e);CHECK(cutscene_interact_down);
 e.cbutton.which=7;cutscene_event(e);CHECK(!cutscene_interact_down);
 cutscene_interact_down=true;e.cbutton.button=SDL_CONTROLLER_BUTTON_B;cutscene_event(e);CHECK(cutscene_interact_down);
 e={};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_RETURN;cutscene_event(e);CHECK(cutscene_enter);e.type=SDL_KEYUP;cutscene_event(e);CHECK(!cutscene_enter);
 e={};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_SPACE;cutscene_event(e);CHECK(cutscene_space);e.type=SDL_KEYUP;cutscene_event(e);CHECK(!cutscene_space);
 cutscene_enter=cutscene_space=cutscene_interact_down=true;e={};e.type=SDL_WINDOWEVENT;e.window.event=SDL_WINDOWEVENT_FOCUS_LOST;cutscene_event(e);CHECK(!cutscene_enter&&!cutscene_space&&!cutscene_interact_down);
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"actual_production_event_prefix\":true,\"controller_a_release\":true,\"other_device_ignored\":true,\"focus_loss_clears_holds\":true}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
