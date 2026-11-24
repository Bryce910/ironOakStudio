#!/usr/bin/env python3
"""Real headless ImGui controls and shared backend, without a renderer/device."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
sys.dont_write_bytecode=True
SOURCE=Path(__file__).resolve().parents[1]
VIEW=SOURCE/"engine/world_studio_weather_view.inc"


def method(text,signature):
    start=text.index(signature);begin=text.index("{",start);depth=1;end=begin+1
    while depth:
        depth+=(text[end]=="{")-(text[end]=="}");end+=1
    return text[start:end]


def main():
    out=Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True);data=Path(sys.argv[2]).resolve();text=VIEW.read_text()
    signatures=["void weather_preview_apply()","void weather_property_changed()","void weather_finish_properties()","static world_weather_authoring::Profile weather_preset(","world_weather_authoring::Profile weather_existing_profile()","bool weather_profile_controls(","bool weather_select(","bool weather_add(","void weather_delete()","void weather_panel("]
    methods="\n".join(method(text,s)for s in signatures).replace("ImGui::Button(","tracked_button(").replace("ImGui::Checkbox(","tracked_checkbox(").replace("ImGui::Combo(","tracked_combo(").replace("ImGui::DragFloatRange2(","tracked_range(")
    harness=r'''
#include "engine/world_studio.h"
#include "engine/world_designer.h"
#include "engine/world_authoring_identity.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <map>
struct Camera{};std::map<std::string,ImVec4>rects;
void tracked(const char*s){auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();rects[s]={a.x,a.y,b.x,b.y};}
bool tracked_button(const char*s,ImVec2 size={0,0}){bool r=ImGui::Button(s,size);tracked(s);return r;}
bool tracked_checkbox(const char*s,bool*v){bool r=ImGui::Checkbox(s,v);tracked(s);return r;}
bool tracked_combo(const char*s,int*v,const char*items){auto p=ImGui::GetCursorScreenPos();float w=ImGui::CalcItemWidth(),h=ImGui::GetFrameHeight();bool r=ImGui::Combo(s,v,items);rects[s]={p.x,p.y,p.x+w,p.y+h};return r;}
bool tracked_range(const char*s,float*a,float*b,float speed,float lo,float hi,const char*f,const char*g,ImGuiSliderFlags flags){bool r=ImGui::DragFloatRange2(s,a,b,speed,lo,hi,f,g,flags);tracked(s);return r;}
unsigned checks=0;void require(bool ok,const char*s){++checks;if(!ok){std::cerr<<"FAIL "<<s<<'\n';std::exit(1);}}
struct Host {
 world_studio::Document draft,runtime_before;world_studio::History history;world_designer::Selection selection;world_designer::Context context;
 std::string message;bool editable=true,playing=false,frozen=false,dirty=false,apply=false,designer_tools=true;
 bool weather_editing=false,weather_pending=false,weather_placing=false,weather_moving=false;int weather_selection=-1,weather_arm_frame=-1,tab=26,requested_tab=26;unsigned frames=0;
 bool sound_can_edit(){return editable&&!playing&&!frozen;}
 void checkpoint(){history.checkpoint(draft);dirty=true;}void changed(bool geometry){require(!geometry,"weather never requests terrain geometry");dirty=true;}
 void sound_cancel(){}void designer_asset_cancel(){}void weather_cancel(){weather_placing=weather_moving=false;weather_arm_frame=-1;}void designer_finish_edit(){}void designer_frame_selection(){++frames;}
 world_designer::Ref designer_binding_ref(world_entity::Binding b){auto g=draft.entities;if(!world_authoring::synchronize(draft,g,{}, {},nullptr,message))return{};draft.entities=std::move(g);return world_designer::from_binding(draft,b);}
 bool designer_select(world_designer::Ref r){return world_designer::select(draft,context,selection,r,false,message);}
 bool designer_run(const world_designer::Command&c){world_studio::Document next;world_designer::Result r;if(!world_designer::execute(draft,selection,context,c,next,r,message))return false;if(r.changed){require(!r.geometry,"common weather-only deletion is nongeometry");history.checkpoint(draft);draft=std::move(next);selection=std::move(r.selection);}return true;}
 PRODUCTION_METHODS
};
void frame(Host&h){ImGui::NewFrame();ImGui::SetNextWindowPos({10,10});ImGui::SetNextWindowSize({280,2600});ImGui::Begin("Actual Weather controls",nullptr,ImGuiWindowFlags_NoSavedSettings);ImGui::BeginTabBar("Authoring");h.weather_panel({});ImGui::EndTabBar();ImGui::End();h.weather_finish_properties();ImGui::Render();}
void click_at(Host&h,float x,float y){auto&io=ImGui::GetIO();io.AddMousePosEvent(x,y);io.AddMouseButtonEvent(0,true);frame(h);io.AddMouseButtonEvent(0,false);frame(h);}
void click(Host&h,const char*s){require(rects.count(s),s);auto r=rects[s];click_at(h,(r.x+r.z)/2,(r.y+r.w)/2);}
int main(){
 ImGui::CreateContext();auto&io=ImGui::GetIO();io.DisplaySize={1200,2800};io.DeltaTime=1.f/60;io.IniFilename=nullptr;io.LogFilename=nullptr;io.ConfigInputTrickleEventQueue=false;unsigned char*p;int w,h;io.Fonts->GetTexDataAsRGBA32(&p,&w,&h);
 Host host;require(!host.draft.weather_global.enabled&&host.draft.weather_areas.empty(),"untouched document preserves legacy weather");
 for(int i=1;i<=4;++i)require(world_weather_authoring::valid(host.weather_preset(i)),"each real preset is valid");
 auto storm=host.weather_preset(4);require(storm.rain_enabled&&storm.thunder_enabled&&storm.lightning_enabled&&storm.rain.minimum>=.75f,"Storm preset enables independent storm channels");
 require(!host.weather_preset(1).rain_enabled&&!host.weather_preset(2).thunder_enabled,"clear/overcast presets remain dry");
 host.runtime_before=host.draft;frame(host);frame(host);click(host,"Override existing world weather");
 require(host.draft.weather_global.enabled&&host.draft.weather_global.profile.rain.minimum==host.draft.look.rain&&host.draft.weather_global.profile.fog.minimum==host.draft.look.fog,"actual override checkbox initially inherits existing Look");
 require(host.history.back.size()==1&&!host.history.back.back().weather_global.enabled&&host.apply,"override has one Undo and automatic Apply");
 host.runtime_before=host.draft;click(host,"Rain");require(!host.draft.weather_global.profile.rain_enabled,"actual Rain toggle works independently");
 host.runtime_before=host.draft;click(host,"Lightning");require(host.draft.weather_global.profile.lightning_enabled&&!host.draft.weather_global.profile.thunder_enabled,"actual Lightning toggle leaves Thunder unchanged");
 host.runtime_before=host.draft;click(host,"Thunder");require(host.draft.weather_global.profile.thunder_enabled,"actual Thunder toggle works");
 host.runtime_before=host.draft;click(host,"##weather-preset");frame(host);ImGuiWindow*popup=nullptr;for(auto*window:ImGui::GetCurrentContext()->Windows)if(window->Active&&std::string(window->Name).rfind("##Combo_",0)==0)popup=window;
 require(popup!=nullptr,"actual preset Combo opens");float row=ImGui::GetTextLineHeightWithSpacing();click_at(host,popup->Pos.x+popup->Size.x*.5f,popup->DC.CursorStartPos.y+4*row+ImGui::GetTextLineHeight()*.5f);
 require(host.draft.weather_global.profile.rain_enabled&&host.draft.weather_global.profile.thunder_enabled&&host.draft.weather_global.profile.lightning_enabled&&host.draft.weather_global.profile.rain.minimum==storm.rain.minimum,"actual Storm Combo selection applies preset");
 host.apply=false;host.runtime_before=host.draft;const auto before=host.history.back.size();const float old=host.draft.weather_global.profile.rain.minimum;auto r=rects.at("##weather-Rain intensity · minimum / maximum");
 io.AddMousePosEvent(r.x+(r.z-r.x)*.25f,(r.y+r.w)/2);io.AddMouseButtonEvent(0,true);frame(host);io.AddMousePosEvent(r.x+(r.z-r.x)*.25f+12,(r.y+r.w)/2);frame(host);io.AddMousePosEvent(r.x+(r.z-r.x)*.25f+22,(r.y+r.w)/2);frame(host);
 require(host.weather_editing&&!host.apply&&host.history.back.size()==before+1,"real multi-frame range drag coalesces one Undo without Apply during drag");
 io.AddMouseButtonEvent(0,false);frame(host);require(host.apply&&!host.weather_editing&&host.history.back.back().weather_global.profile.rain.minimum==old,"range release automatically previews with original snapshot");
 require(host.history.undo(host.draft)&&host.draft.weather_global.profile.rain.minimum==old,"shared Undo restores original profile range");require(host.history.redo(host.draft),"shared Redo restores profile edit");
 host.runtime_before=host.draft;frame(host);click(host,"Place weather area on map…");require(host.weather_placing&&host.draft.weather_areas.empty(),"actual Place button arms terrain click without premature append");
 require(host.weather_add(14,3,28),"production area append after terrain hit");require(host.weather_selection==0&&host.selection.active.id!=0&&host.draft.weather_areas.size()==1,"new area has common stable selection");
 const auto id=host.selection.active.id;world_designer::Clipboard clipboard;require(clipboard.copy(host.draft,host.selection,host.context,{},host.message),"common Clipboard copies weather area");world_studio::Document next;world_designer::Result result;require(clipboard.paste(host.draft,host.context,{},next,result,host.message)&&!result.geometry&&next.weather_areas.size()==2&&result.selection.active.id!=id,"weather paste has fresh identity and no terrain rebuild");
 frame(host);frame(host);r=rects.at("##weather-Rain intensity · minimum / maximum");require(r.x>=18&&r.z<=290,"stacked Weather ranges fit narrow 280px inspector");click(host,"Frame area");require(host.frames==1,"actual Frame area calls common framing");click(host,"Delete area");require(host.draft.weather_areas.empty()&&host.weather_selection==-1,"actual Delete uses common typed operation");require(host.history.undo(host.draft)&&host.draft.weather_areas.size()==1,"shared Undo restores deleted weather area");
 host.weather_select(0);host.playing=true;frame(host);click(host,"Delete area");require(host.draft.weather_areas.size()==1,"Weather edits disabled during Play");
 ImGui::DestroyContext();std::cout<<"PASS "<<checks<<" actual headless ImGui Weather/History/Clipboard checks\n";
}
'''.replace("PRODUCTION_METHODS",methods)
    cpp=out/"actual-weather-controls.cpp";cpp.write_text(harness);binary=out/"actual-weather-controls-test";imgui=SOURCE/"third_party/imgui"
    names=["world_studio","world_entity_graph","world_authoring_identity","world_prefab","world_designer","world_studio_tools","world_surface_authoring","world_shader_graph","world_navigation"]
    objects=[data/(n+".o")for n in names]
    cmd=["/usr/bin/x86_64-linux-gnu-g++-13","-std=c++17","-O0","-I"+str(SOURCE),"-I"+str(imgui),str(cpp),*[str(p)for p in objects],*[str(imgui/(n+".cpp"))for n in ["imgui","imgui_draw","imgui_widgets","imgui_tables"]],"-o",str(binary)]
    p=subprocess.run(cmd,text=True,capture_output=True);(out/"compile.log").write_text(p.stdout+p.stderr);p.check_returncode();run=subprocess.run([str(binary)],text=True,capture_output=True);(out/"run.log").write_text(run.stdout+run.stderr);print(run.stdout+run.stderr,end="")
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    receipt={"passed":run.returncode==0,"source":str(VIEW),"source_sha256":sha(VIEW),"test_sha256":sha(__file__),"command":cmd,"backend_objects":{str(p):sha(p)for p in objects},"stdout":run.stdout,"stderr":run.stderr,"returncode":run.returncode,"scope":"Actual ImGui controls, exact extracted production Weather methods with rectangle-only instrumentation, real shared authoring/history/clipboard; no GPU or audio device","limits":"Owner baseline supplied explicitly; actual 3D picking/gizmo/rain/wetness/storm rendering and source-position audio conditions require separate runtime verification."}
    (out/"RESULTS.json").write_text(json.dumps(receipt,indent=2)+"\n");run.check_returncode()


if __name__=="__main__":main()
