// ImGui frame is active. Projection uses the actual rendered camera.
if(!launch->no_hud&&!studio.open&&!preview_blocked()&&!show_map&&!show_graphics&&!ui_loading&&!opening_camera_locked&&!cam.fly&&!first_journey_dialogue_active&&!(opening_enabled&&opening_state.camp().active())&&!(studio.components_runtime&&studio.components_runtime->current_line())){
 struct Label{const char *name;double x,y,z,d;};std::array<Label,240> labels{};unsigned count=0;
 auto add=[&](const char *name,double x,double y,double z){const double d=std::hypot(std::hypot(x-cam.x,z-cam.z),0.);if(name&&*name&&d<world_npc_nameplate::range&&count<labels.size())labels[count++]={name,x,y,z,d};};
 for(unsigned i=0;i<world_village_resident_count+WORLD_EDITED_ACTOR_CAPACITY;++i){WorldVillageResident a;if(world_village_population_resident(population,i,&a))add(a.name,a.x,a.y+a.visual_y_offset+(a.seated?1.22:1.98)*a.appearance_scale,a.z);}
 for(unsigned i=0;i<world_road_traffic_count(road_traffic);++i){WorldRoadTrafficActor a;if(world_road_traffic_actor(road_traffic,i,&a)&&a.active)add(a.name,a.x,a.y+(a.kind==WorldRoadTrafficKind::Rider?3.15:1.98),a.z);}
 const char *camp_names[]={"Rowan","Bram","Ellis"};const char *town_names[]={"Hugh","Townsperson","Townsperson"};
 for(unsigned i=0;i<3;++i){WorldTrailCamper a;if(world_trail_camp_actor(arrival_camp,i,&a))add(camp_names[i],a.x,a.y+(a.seated?1.22:1.98),a.z);if(world_trail_camp_actor(first_journey_townsfolk,i,&a))add(town_names[i],a.x,a.y+(a.seated?1.22:1.98),a.z);}
 std::sort(labels.begin(),labels.begin()+count,[](const Label&a,const Label&b){return a.d<b.d;});
 auto *dl=ImGui::GetForegroundDrawList();const float w=ImGui::GetIO().DisplaySize.x,h=ImGui::GetIO().DisplaySize.y,u=std::clamp(std::min(w/1600,h/900),.65f,1.5f)*preview_settings.text_scale;const auto f=render_cam.forward(),r=norm(cross(f,{0,1,0})),up=cross(r,f);
 std::array<ImVec4,24> occupied{};unsigned visible=0;
 for(unsigned i=0;i<count&&visible<occupied.size();++i){const auto &a=labels[i];const V3 delta={float(a.x-render_cam.x),float(a.y-render_cam.y),float(a.z-render_cam.z)};
  const auto p=world_npc_nameplate::project(dot(delta,r),dot(delta,up),dot(delta,f),render_cam.fov,w,h);if(!p.visible||p.y<130*u)continue;
  world_physics::Hit hit;if(active_playtest_walk&&(!active_playtest_walk->raycast({render_cam.x,render_cam.y,render_cam.z},{delta.x,delta.y,delta.z},hit)||(hit.hit&&hit.fraction<.96)))continue;
  const auto size=ImGui::GetFont()->CalcTextSizeA(16*u,1000,0,a.name);const ImVec4 box={p.x-size.x*.5f-10*u,p.y-size.y-12*u,p.x+size.x*.5f+10*u,p.y};bool overlap=false;
  if(opening_enabled&&opening_hud_regions.covers({box.x,box.y,box.z,box.w},4*u))continue;
  for(unsigned j=0;j<visible;++j){const auto &b=occupied[j];if(box.x<b.z&&box.z>b.x&&box.y<b.w&&box.w>b.y){overlap=true;break;}}if(overlap)continue;occupied[visible++]=box;
  const int alpha=int(255*world_npc_nameplate::opacity(a.d));const bool focus=a.d<=world_npc_nameplate::talk_range;const ImU32 color=focus?IM_COL32(255,215,134,alpha):IM_COL32(239,237,222,alpha);
  dl->AddRectFilled({box.x,box.y},{box.z,box.w},IM_COL32(8,14,18,int(alpha*.78f)),4*u);if(focus)dl->AddRect({box.x,box.y},{box.z,box.w},IM_COL32(230,183,95,alpha),4*u,0,1.3f*u);
  dl->AddText(ImGui::GetFont(),16*u,{p.x-size.x*.5f,box.y+5*u},color,a.name);
 }
 npc_nameplate_frames+=visible>0;npc_nameplate_labels+=visible;
}
