#!/usr/bin/env python3
"""Headless actual-ImGui Audio controls plus real authoring/history operations.

Extract the production member methods into a minimal owner, instrumenting only
widget rectangles so SDL-free input can click their real controls. No renderer,
audio device, terrain build, or accepted project is opened.
"""
from pathlib import Path
import hashlib
import json
import subprocess
import sys

SOURCE=Path(__file__).resolve().parents[1]
VIEW=SOURCE/"engine/world_studio_audio_view.inc"


def method(text,signature):
    start=text.index(signature);begin=text.index("{",start);depth=1;end=begin+1
    while depth:
        depth+=(text[end]=="{")-(text[end]=="}");end+=1
    return text[start:end]


def main():
    out=Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True)
    data=Path(sys.argv[2]).resolve()
    text=VIEW.read_text()
    signatures=["bool sound_can_edit()","std::string sound_name(","void sound_preview_apply()","void sound_property_changed()","void sound_finish_properties()","bool sound_select(","void sound_monitor_changed()","bool sound_add(","bool sound_set_clip(","void sound_delete()","bool sound_prepare_now()","void sound_listen(","bool sound_asset_budget()","void sound_panel("]
    methods="\n".join(method(text,s) for s in signatures)
    methods=methods.replace("ImGui::Button(","tracked_button(").replace("ImGui::SliderFloat(","tracked_slider(")
    harness=r'''
#include "engine/world_studio.h"
#include "engine/world_designer.h"
#include "engine/world_authoring_identity.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
struct Camera{};
std::map<std::string,ImVec2> controls;
std::map<std::string,ImVec4> control_rects;float panel_width=540;
bool tracked_button(const char*label,ImVec2 size={0,0}){bool r=ImGui::Button(label,size);auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();controls[label]={(a.x+b.x)/2,(a.y+b.y)/2};return r;}
bool tracked_slider(const char*label,float*v,float low,float high,const char*format){bool r=ImGui::SliderFloat(label,v,low,high,format);auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();const std::string key=std::string(label)=="##sound-volume"?"Volume":label;controls[key]={(a.x+b.x)/2,(a.y+b.y)/2};control_rects[key]={a.x,a.y,b.x,b.y};return r;}
unsigned checks=0;
void require(bool ok,const char*label){++checks;if(!ok){std::cerr<<"FAIL: "<<label<<'\n';std::exit(1);}}
struct Host {
 world_studio::Document draft,runtime_before;
 world_studio::History history;
 world_designer::Selection selection;
 world_designer::Context context;
 std::string message,sound_clip="sounds/imported/test.wav";
 std::vector<std::string>sound_catalog={sound_clip};
 std::uint64_t sound_solo_entity=0;
 int sound_selection=-1,sound_solo=-1,sound_arm_frame=-1,tab=25,requested_tab=25;
 bool sound_muted=false,sound_placing=false,sound_moving=false,sound_catalog_scanned=true,sound_editing=false,sound_preview_pending=false,designer_audio_browser=false,designer_content=true,designer_tools=true;
 bool editable=true,playing=false,frozen=false,apply=false,dirty=false;
 unsigned prepares=0,stops=0,auditions=0,monitors=0,syncs=0;bool host_muted=false;int host_solo=-1;bool prepare_ok=true;
 char sound_import_path[1024]{};
 std::function<bool(const world_studio::Document&,std::string&)>sound_prepare;
 std::function<bool(unsigned,std::string&)>sound_audition;
 std::function<void()>sound_stop;
 std::function<void(bool,int)>sound_monitor;
 Host(){sound_prepare=[&](const auto&doc,std::string&){++prepares;std::vector<world_soundscape::Emitter>v;std::string e;return prepare_ok&&world_authoring::sound_emitters(doc,v,e);};sound_audition=[&](unsigned i,std::string&){++auditions;return i<draft.sounds.size();};sound_stop=[&]{++stops;host_muted=true;};sound_monitor=[&](bool muted,int solo){++monitors;host_muted=muted;host_solo=solo;};}
 bool production_can_save(){return editable;}bool production_playing()const{return playing;}bool production_freeze()const{return frozen;}
 bool production_sync(std::string&e){++syncs;auto graph=draft.entities;if(!world_authoring::synchronize(draft,graph,{}, {},nullptr,e))return false;draft.entities=std::move(graph);return true;}
 void checkpoint(){history.checkpoint(draft);dirty=true;}void changed(bool geometry){require(!geometry,"Audio host never requests geometry rebuild");dirty=true;}
 void sound_cancel(){sound_placing=sound_moving=false;sound_arm_frame=-1;}void designer_asset_cancel(){}void designer_frame_selection(){}void designer_finish_edit(){}
 world_designer::Ref designer_binding_ref(world_entity::Binding b){if(!production_sync(message))return{};return world_designer::from_binding(draft,b);}
 bool designer_select(world_designer::Ref r){return world_designer::select(draft,context,selection,r,false,message);}
 bool designer_run(const world_designer::Command&c){world_studio::Document next;world_designer::Result r;if(!world_designer::execute(draft,selection,context,c,next,r,message))return false;if(r.changed){require(!r.geometry,"Sound delete uses nongeometry common command");history.checkpoint(draft);draft=std::move(next);selection=std::move(r.selection);dirty=true;}return true;}
 void sound_catalog_refresh(){sound_catalog_scanned=true;}bool sound_import_file(){return false;}
 PRODUCTION_METHODS
};
void frame(Host&h){ImGui::NewFrame();ImGui::SetNextWindowPos({10,10});ImGui::SetNextWindowSize({panel_width,1350});ImGui::Begin("Actual Audio controls",nullptr,ImGuiWindowFlags_NoSavedSettings);ImGui::BeginTabBar("Authoring");h.sound_panel({});ImGui::EndTabBar();ImGui::End();h.sound_finish_properties();ImGui::Render();}
void click(Host&h,const char*label){require(controls.count(label),label);auto p=controls[label];auto&io=ImGui::GetIO();io.AddMousePosEvent(p.x,p.y);io.AddMouseButtonEvent(0,true);frame(h);io.AddMouseButtonEvent(0,false);frame(h);}
int main(){
 ImGui::CreateContext();auto&io=ImGui::GetIO();io.DisplaySize={1200,1500};io.DeltaTime=1.f/60;io.IniFilename=nullptr;io.LogFilename=nullptr;io.ConfigInputTrickleEventQueue=false;unsigned char*p;int w,h;io.Fonts->GetTexDataAsRGBA32(&p,&w,&h);
 Host host;require(host.sound_add(14,3,28,host.sound_clip),"production click-placement append");require(host.draft.sounds.size()==1&&host.sound_selection==0&&host.selection.active.id!=0,"placed Sound has stable common selection");require(host.history.back.size()==1&&host.history.back[0].sounds.empty(),"placement has one complete Undo snapshot");host.apply=false;
 require(host.sound_select(0),"production speaker selection");const auto first=host.selection.active.id;
 world_designer::Clipboard clipboard;require(clipboard.copy(host.draft,host.selection,host.context,{},host.message),"common Clipboard copies Sound");world_studio::Document next;world_designer::Result pasted;require(clipboard.paste(host.draft,host.context,{},next,pasted,host.message),"common Clipboard pastes Sound");require(!pasted.geometry&&next.sounds.size()==2&&pasted.selection.active.id!=first,"Sound paste has fresh identity and no geometry rebuild");
 host.runtime_before=host.draft;frame(host);frame(host);
 click(host,"Stop");require(host.stops==1&&host.sound_muted&&host.host_muted&&host.sound_solo==-1,"real ImGui Stop preserves UI and host mute");
 click(host,"Audition clip");require(host.prepares==1&&host.auditions==1&&!host.sound_muted&&!host.host_muted&&host.syncs>=2,"real ImGui Audition syncs persistent ID and unmutes");
 host.prepare_ok=false;host.sound_muted=true;host.host_muted=true;const auto count=host.auditions;click(host,"Audition clip");require(host.auditions==count&&host.sound_muted&&host.host_muted,"failed prepare preserves previous mix/mute and skips audition");host.prepare_ok=true;
 host.apply=false;const auto history_before=host.history.back.size();const float old_volume=host.draft.sounds[0].volume;host.runtime_before=host.draft;
 auto slider=controls.at("Volume");io.AddMousePosEvent(slider.x-55,slider.y);io.AddMouseButtonEvent(0,true);frame(host);
 require(ImGui::IsAnyItemActive()&&host.sound_editing&&!host.apply,"active real slider defers Apply");
 io.AddMousePosEvent(slider.x-95,slider.y);frame(host);io.AddMousePosEvent(slider.x-120,slider.y);frame(host);
 require(host.history.back.size()==history_before+1&&host.history.back.back().sounds[0].volume==old_volume,"multiple drag frames retain one pre-edit history snapshot");
 io.AddMouseButtonEvent(0,false);frame(host);require(host.apply&&!host.sound_editing&&host.draft.sounds[0].volume!=old_volume,"slider release previews edited values once");
 const auto edited_volume=host.draft.sounds[0].volume;require(host.history.undo(host.draft)&&host.draft.sounds[0].volume==old_volume,"real shared History Undo restores property");require(host.history.redo(host.draft)&&host.draft.sounds[0].volume==edited_volume,"real shared History Redo restores edited value");
 host.apply=false;frame(host);click(host,"Delete sound");require(host.draft.sounds.empty()&&host.sound_selection==-1&&host.history.back.back().sounds.size()==1,"real ImGui Delete uses common entity operation and history");
 require(host.history.undo(host.draft)&&host.draft.sounds.size()==1,"deleted Sound is restored by shared Undo");host.sound_select(0);host.playing=true;frame(host);const auto before=host.draft.sounds.size();click(host,"Delete sound");require(host.draft.sounds.size()==before,"real controls disable edits during Play");
 host.playing=false;panel_width=280;frame(host);frame(host);require(control_rects.count("Volume")&&control_rects["Volume"].x>=18&&control_rects["Volume"].z<=282,"stacked Audio control fits 280px inspector without right label crop");
 ImGui::DestroyContext();std::cout<<"PASS "<<checks<<" headless ImGui/authoring/history checks\n";
}
'''.replace("PRODUCTION_METHODS",methods)
    cpp=out/"actual-audio-controls.cpp";cpp.write_text(harness);binary=out/"actual-audio-controls-test"
    names=["world_studio","world_entity_graph","world_authoring_identity","world_prefab","world_designer","world_studio_tools","world_surface_authoring","world_shader_graph","world_navigation"]
    objects=[data/(name+".o")for name in names]
    imgui=SOURCE/"third_party/imgui"
    command=["/usr/bin/x86_64-linux-gnu-g++-13","-std=c++17","-O0","-I"+str(SOURCE),"-I"+str(imgui),str(cpp),*[str(o)for o in objects],*[str(imgui/(name+".cpp"))for name in ["imgui","imgui_draw","imgui_widgets","imgui_tables"]],"-o",str(binary)]
    compiled=subprocess.run(command,text=True,capture_output=True);(out/"compile.log").write_text(compiled.stdout+compiled.stderr);compiled.check_returncode()
    run=subprocess.run([str(binary)],text=True,capture_output=True);(out/"run.log").write_text(run.stdout+run.stderr);print(run.stdout+run.stderr,end="")
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    receipt={"passed":run.returncode==0,"scope":"Headless actual ImGui widgets with exact extracted production Audio member methods, rectangle-only instrumentation; shared real authoring/History/Clipboard backend; no GPU/audio device/full viewer runtime", "source":str(VIEW),"source_sha256":sha(VIEW),"test_sha256":sha(__file__),"backend_objects":{str(p):sha(p)for p in objects},"command":command,"stdout":run.stdout,"stderr":run.stderr,"returncode":run.returncode,"limits":"Owner/runtime_before is supplied explicitly, so full designer baseline producer, 3D mouse picking/gizmos and audio playback still require ordinary editor verification."}
    (out/"RESULTS.json").write_text(json.dumps(receipt,indent=2)+"\n");run.check_returncode()


if __name__=="__main__":main()
