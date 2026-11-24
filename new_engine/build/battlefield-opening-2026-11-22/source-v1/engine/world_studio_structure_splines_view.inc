// Dedicated structure authoring. Definitions own generated meshes; an Update
// replaces one definition's geometry and never appends ordinary placements.
    int structure_spline_mode=0,structure_spline_point=0;
    world_structure::Id structure_spline_owner=0;
    world_structure::Definition structure_spline_new;
    bool structure_spline_drawing=false,structure_spline_move=false;
    bool structure_spline_grid=true,structure_spline_endpoints=true,structure_spline_keep_height=true;
    double structure_spline_grid_size=1,structure_spline_snap_radius=1.5;
    int structure_spline_drag=-1;
    world_structure::Point structure_spline_drag_position;
    unsigned structure_spline_created=0,structure_spline_updates=0,structure_spline_moves=0,structure_spline_cancelled=0;
    static bool structure_spline_same(const std::vector<world_structure::Definition> &a,const std::vector<world_structure::Definition> &b){
        if(a.size()!=b.size())return false;
        for(std::size_t i=0;i<a.size();++i){const auto &x=a[i],&y=b[i];
            if(x.id!=y.id||x.kind!=y.kind||x.alias!=y.alias||x.spacing!=y.spacing||x.width!=y.width||x.height!=y.height||x.thickness!=y.thickness||x.rail_height!=y.rail_height||x.support_depth!=y.support_depth||x.railings!=y.railings||x.supports!=y.supports||x.smooth!=y.smooth||x.follow_ground!=y.follow_ground||x.points.size()!=y.points.size())return false;
            for(std::size_t j=0;j<x.points.size();++j)if(x.points[j].x!=y.points[j].x||x.points[j].y!=y.points[j].y||x.points[j].z!=y.points[j].z)return false;
        }return true;
    }
    bool structure_spline_update(){
        std::string error;if(apply||structure_cooking||production_freeze()||!structure_spline_selected()||!world_studio::valid(draft,error)){if(!error.empty())message=error;return false;}
        designer_finish_edit();changed(true);apply=true;++structure_spline_updates;message="Updating this structure and matching collision.";return true;
    }
    bool structure_spline_delete(){
        if(apply||structure_cooking||production_freeze()||!structure_spline_selected())return false;
        const auto owner=structure_spline_owner;designer_finish_edit();checkpoint();
        draft.structures.erase(std::remove_if(draft.structures.begin(),draft.structures.end(),[&](const auto &v){return v.id==owner;}),draft.structures.end());
        structure_spline_owner=0;structure_spline_move=false;changed(true);apply=true;message="Removing the structure and its collision.";return true;
    }
    world_structure::Definition *structure_spline_selected(){
        for(auto &value:draft.structures)if(value.id==structure_spline_owner)return &value;
        return nullptr;
    }
    void structure_spline_cancel(){
        structure_spline_drawing=false;structure_spline_move=false;structure_spline_drag=-1;
        structure_spline_new={};++structure_spline_cancelled;
    }
    bool structure_spline_active()const{return tab==5&&structure_spline_mode!=2;}
    world_structure::Point structure_spline_snap(world_structure::Point point,world_structure::Id exclude=0,bool ground_height=false){
        if(structure_spline_grid)point=world_structure::snap_grid(point,structure_spline_grid_size);
        if(ground_height){float floor=0;const double reference=point.y;point.y=world_atlas_height(point.x,point.z);if(active_playtest_walk&&active_playtest_walk->sample(point.x,point.z,reference,floor))point.y=floor;}
        if(structure_spline_endpoints){const auto snap=world_structure::snap_point(point,draft.structures,structure_spline_snap_radius,exclude);if(snap.snapped)point=snap.point;}
        return point;
    }
    void structure_spline_begin(world_structure::Kind kind){
        landscape_cancel();designer_finish_edit();cancel_designer_drag();clear_selection();
        structure_spline_cancel();structure_spline_mode=int(kind);structure_spline_owner=0;structure_spline_point=0;
        structure_spline_new.id=draft.next_structure_id;structure_spline_new.kind=kind;
        structure_spline_new.follow_ground=kind==world_structure::Kind::Wall;
        structure_spline_drawing=true;message="Click terrain to add points. Finish builds the structure; Esc cancels.";
    }
    bool structure_spline_finish(){
        if(!structure_spline_drawing||apply||structure_cooking||production_freeze())return false;
        auto next=draft;world_structure::Id owner=0;std::string error;
        if(!world_structure::append(next.structures,next.next_structure_id,structure_spline_new,owner,error)||!world_studio::valid(next,error)){
            message=error;return false;
        }
        designer_finish_edit();checkpoint();draft=std::move(next);changed(true);apply=true;
        structure_spline_owner=owner;structure_spline_drawing=false;structure_spline_new={};++structure_spline_created;
        message="Building the structure and matching collision. Save All keeps its editable path.";return true;
    }
    bool structure_spline_commit_point(world_structure::Id owner,unsigned point,world_structure::Point position){
        if(apply||structure_cooking||production_freeze())return false;
        auto next=draft;bool found=false;
        for(auto &value:next.structures)if(value.id==owner&&point<value.points.size()){
            if(world_structure::distance(value.points[point],position)<.001){structure_spline_move=false;return true;}
            value.points[point]=position;found=true;break;
        }
        std::string error;if(!found||!world_studio::valid(next,error)){message=found?error:"Structure point is no longer available.";return false;}
        designer_finish_edit();cancel_designer_drag();checkpoint();draft=std::move(next);changed(true);apply=true;
        structure_spline_move=false;structure_spline_drag=-1;++structure_spline_moves;
        message="Point moved. Updating this structure and its collision; one Undo restores the previous path.";return true;
    }
    void structure_spline_frame(const world_structure::Definition &value){
        world_structure::Point lo=value.points.front(),hi=lo;
        for(const auto &point:value.points){lo.x=std::min(lo.x,point.x);lo.y=std::min(lo.y,point.y);lo.z=std::min(lo.z,point.z);hi.x=std::max(hi.x,point.x);hi.y=std::max(hi.y,point.y);hi.z=std::max(hi.z,point.z);}
        hi.y+=value.kind==world_structure::Kind::Wall?value.height:value.rail_height;
        designer_frame_center={(lo.x+hi.x)*.5,(lo.y+hi.y)*.5,(lo.z+hi.z)*.5};
        designer_frame_radius=std::max(5.,world_structure::distance(lo,hi)*.65);designer_frame_pending=true;
    }
    void structure_spline_properties(world_structure::Definition &value,bool &edited){
        auto number=[&](const char *label,double &v,double minimum,double maximum,double step){
            if(ImGui::InputDouble(label,&v,step,step*5,"%.2f")){if(std::isfinite(v))v=std::clamp(v,minimum,maximum);edited=true;}
        };
        ImGui::TextUnformatted(value.kind==world_structure::Kind::Wall?"Stone wall":"Timber bridge");
        if(value.kind==world_structure::Kind::Wall){
            number("Wall height (m)",value.height,.5,20,.25);
            number("Wall thickness (m)",value.thickness,.08,4,.1);
            edited|=ImGui::Checkbox("Follow terrain",&value.follow_ground);
            edited|=ImGui::Checkbox("Foundations",&value.supports);
        }else{
            number("Deck width (m)",value.width,.5,24,.25);
            number("Deck thickness (m)",value.thickness,.08,4,.05);
            edited|=ImGui::Checkbox("Railings",&value.railings);
            if(value.railings)number("Railing height (m)",value.rail_height,.5,2.5,.1);
            edited|=ImGui::Checkbox("Stone supports",&value.supports);
        }
        if(value.supports)number("Maximum support depth (m)",value.support_depth,0,64,1);
        edited|=ImGui::Checkbox("Smooth curve",&value.smooth);
        number("Section length (m)",value.spacing,.25,32,.25);
    }
    void structure_spline_panel(bool &edited,bool &structural){
        ImGui::PushID("structure-splines");
        if(ImGui::RadioButton("Walls",structure_spline_mode==0)){structure_spline_cancel();structure_spline_mode=0;}
        ImGui::SameLine();if(ImGui::RadioButton("Bridges",structure_spline_mode==1)){structure_spline_cancel();structure_spline_mode=1;}
        if(ImGui::RadioButton("Repeated props",structure_spline_mode==2)){structure_spline_cancel();structure_spline_mode=2;}
        if(structure_spline_mode==2){ImGui::PopID();return;}
        ImGui::Separator();
        ImGui::BeginDisabled(apply||structure_cooking||production_freeze());
        if(!structure_spline_drawing){
            ImGui::BeginDisabled(draft.structures.size()>=world_structure::definition_limit);
            if(ImGui::Button(structure_spline_mode==0?"Draw a wall":"Draw a bridge",{-1,32}))structure_spline_begin(structure_spline_mode==0?world_structure::Kind::Wall:world_structure::Kind::Bridge);
            ImGui::EndDisabled();
        }
        ImGui::Checkbox("Snap to grid",&structure_spline_grid);
        if(structure_spline_grid){ImGui::SetNextItemWidth(95);ImGui::InputDouble("Grid (m)",&structure_spline_grid_size,.25,1,"%.2f");structure_spline_grid_size=std::isfinite(structure_spline_grid_size)?std::clamp(structure_spline_grid_size,.1,16.):1.;}
        ImGui::Checkbox("Snap to existing points",&structure_spline_endpoints);
        if(structure_spline_drawing){
            ImGui::SeparatorText("New path");bool working_edit=false;structure_spline_properties(structure_spline_new,working_edit);
            ImGui::Text("%zu / 32 points",structure_spline_new.points.size());
            ImGui::TextWrapped("Click each bank or corner in the scene. The path previews before you build it.");
            ImGui::BeginDisabled(structure_spline_new.points.size()<2);
            if(ImGui::Button("Finish and build",{-1,32}))structure_spline_finish();
            ImGui::EndDisabled();
            if(ImGui::Button("Remove last point")&&!structure_spline_new.points.empty())structure_spline_new.points.pop_back();
            ImGui::SameLine();if(ImGui::Button("Cancel path")){structure_spline_cancel();message="Path cancelled; the world is unchanged.";}
        }else{
            ImGui::SeparatorText("Editable structures");
            for(const auto &value:draft.structures){
                const auto label=std::string(value.kind==world_structure::Kind::Wall?"Wall ":"Bridge ")+std::to_string(value.id);
                if(ImGui::Selectable(label.c_str(),structure_spline_owner==value.id)){structure_spline_owner=value.id;structure_spline_mode=int(value.kind);structure_spline_point=0;structure_spline_move=false;structure_spline_drag=-1;}
            }
            auto *selected_structure=structure_spline_selected();
            if(selected_structure){
                const auto owner=selected_structure->id;
                ImGui::Separator();bool property_edit=false;structure_spline_properties(*selected_structure,property_edit);
                if(property_edit){edited=true;structural=true;message="Structure changes staged. Update structure builds the new geometry and collision.";}
                structure_spline_point=std::clamp(structure_spline_point,0,int(selected_structure->points.size()-1));
                if(ImGui::Button("Frame structure",{-1,0}))structure_spline_frame(*selected_structure);
                ImGui::Checkbox("Keep point height when moving",&structure_spline_keep_height);
                if(ImGui::Button(structure_spline_move?"Cancel point move":"Pick selected point position",{-1,0}))structure_spline_move=!structure_spline_move;
                ImGui::TextWrapped("Drag numbered handles in the scene, or select a point and enter its coordinates.");
                for(unsigned i=0;i<selected_structure->points.size();++i){
                    ImGui::PushID(int(i));if(ImGui::Selectable(("Point "+std::to_string(i+1)).c_str(),structure_spline_point==int(i)))structure_spline_point=int(i);
                    double values[]={selected_structure->points[i].x,selected_structure->points[i].y,selected_structure->points[i].z};
                    if(ImGui::InputScalarN("XYZ",ImGuiDataType_Double,values,3,nullptr,nullptr,"%.2f")){selected_structure->points[i]={values[0],values[1],values[2]};edited=structural=true;}
                    ImGui::PopID();
                }
                if(ImGui::Button("Append picked target")&&target&&selected_structure->points.size()<world_structure::point_limit){
                    auto next=draft;for(auto &v:next.structures)if(v.id==owner)v.points.push_back(structure_spline_snap({x,y,z},owner,true));
                    std::string error;if(world_studio::valid(next,error)){checkpoint();draft=std::move(next);changed(true);}else message=error;
                }
                ImGui::SameLine();selected_structure=structure_spline_selected();
                if(ImGui::Button("Remove last point")&&selected_structure&&selected_structure->points.size()>2){
                    auto next=draft;for(auto &v:next.structures)if(v.id==owner)v.points.pop_back();
                    std::string error;if(world_studio::valid(next,error)){checkpoint();draft=std::move(next);changed(true);}else message=error;
                }
                // Commands above may replace the document, so reacquire by ID.
                selected_structure=structure_spline_selected();
                if(selected_structure){
                    world_structure::Generated generated;std::string error;
                    const bool valid=world_structure::build(*selected_structure,generated,error);
                    if(valid)ImGui::Text("%zu sections | editable path",generated.segments.size());else ImGui::TextWrapped("%s",error.c_str());
                    ImGui::BeginDisabled(!valid);
                    if(ImGui::Button("Update structure",{-1,32}))structure_spline_update();
                    ImGui::EndDisabled();
                    if(ImGui::Button("Delete structure",{-1,0}))structure_spline_delete();
                }
            }else ImGui::TextWrapped("Draw a path to create a wall or bridge. Existing paths remain editable after saving and reopening.");
        }
        ImGui::EndDisabled();
        ImGui::Separator();ImGui::TextWrapped("Updates replace this path's generated pieces. Undo / Redo restores the path and collision together. Save All keeps the editable structure.");
        ImGui::PopID();
    }
    void structure_spline_overlay(const Camera &cam){
        if(!open||!structure_spline_active()){structure_spline_move=false;structure_spline_drag=-1;return;}
        auto &io=ImGui::GetIO();
        if(ImGui::IsKeyPressed(ImGuiKey_Escape)){
            if(structure_spline_drawing||structure_spline_move||structure_spline_drag>=0){structure_spline_cancel();message="Structure gesture cancelled; the world is unchanged.";}
        }
        const auto viewport=designer_viewport(io.DisplaySize.x,io.DisplaySize.y);
        const bool eligible=viewport.contains(io.MousePos.x,io.MousePos.y)&&!io.WantCaptureMouse&&!io.MouseDown[1]&&!designer_blocks_viewport()&&!apply&&!structure_cooking;
        auto *selected_structure=structure_spline_selected();
        world_structure::Definition preview=structure_spline_drawing?structure_spline_new:selected_structure?*selected_structure:world_structure::Definition{};
        if(!preview.id)return;
        auto *draw=ImGui::GetBackgroundDrawList();
        draw->PushClipRect({viewport.x,viewport.y},{viewport.x+viewport.w,viewport.y+viewport.h},true);
        struct Clip{ImDrawList *draw;~Clip(){draw->PopClipRect();}} clip{draw};
        auto line=[&](world_structure::Point a,world_structure::Point b,ImU32 color,float width=2.f){ImVec2 p,q;if(project_point(cam,{a.x,a.y+.12,a.z},p)&&project_point(cam,{b.x,b.y+.12,b.z},q))draw->AddLine(p,q,color,width);};
        world_structure::Point picked;bool have_pick=false;
        if(eligible&&(structure_spline_drawing||structure_spline_move||structure_spline_drag>=0)){
            target=false;pick(cam,io.MousePos.x,io.MousePos.y);
            if(target){picked={x,y,z};if(!structure_spline_drawing&&structure_spline_keep_height&&selected_structure){const int i=structure_spline_drag>=0?structure_spline_drag:structure_spline_point;if(i>=0&&unsigned(i)<selected_structure->points.size())picked.y=selected_structure->points[unsigned(i)].y;}
                picked=structure_spline_snap(picked,structure_spline_drawing?0:preview.id,structure_spline_drawing||!structure_spline_keep_height);have_pick=true;
                if(preview.kind==world_structure::Kind::Wall&&preview.follow_ground)picked.y=world_atlas_height(picked.x,picked.z);
            }
        }
        if(structure_spline_drawing&&have_pick&&preview.points.size()<world_structure::point_limit){
            if(preview.points.empty()||std::hypot(preview.points.back().x-picked.x,preview.points.back().z-picked.z)>=.1)preview.points.push_back(picked);
            if(ImGui::IsMouseClicked(0)){
                if(structure_spline_new.points.size()<world_structure::point_limit&&(structure_spline_new.points.empty()||std::hypot(structure_spline_new.points.back().x-picked.x,structure_spline_new.points.back().z-picked.z)>=.1))structure_spline_new.points.push_back(picked);
                else message="Choose a distinct point at least 10 cm from the previous point.";
            }
        }
        if(!structure_spline_drawing&&(structure_spline_move||structure_spline_drag>=0)&&have_pick&&selected_structure){
            const int i=structure_spline_drag>=0?structure_spline_drag:structure_spline_point;
            if(i>=0&&unsigned(i)<preview.points.size()){preview.points[unsigned(i)]=picked;structure_spline_drag_position=picked;
                if(structure_spline_move&&ImGui::IsMouseClicked(0))structure_spline_commit_point(preview.id,unsigned(i),picked);
            }
        }
        world_structure::Generated geometry;std::string error;const bool valid=preview.points.size()>=2&&world_structure::build(preview,geometry,error);
        const auto color=valid?IM_COL32(125,238,202,240):IM_COL32(255,165,95,240);
        if(valid)for(auto segment:geometry.segments){
            if(preview.kind==world_structure::Kind::Wall&&preview.follow_ground){segment.left_a.y=segment.right_a.y=world_atlas_height(segment.a.x,segment.a.z);segment.left_b.y=segment.right_b.y=world_atlas_height(segment.b.x,segment.b.z);}
            line(segment.left_a,segment.left_b,color);line(segment.right_a,segment.right_b,color);
            if(preview.kind==world_structure::Kind::Wall){auto a=segment.left_a,b=segment.left_b;a.y+=preview.height;b.y+=preview.height;line(a,b,color);}
        }
        else for(unsigned i=1;i<preview.points.size();++i)line(preview.points[i-1],preview.points[i],color);
        int hovered=-1;float nearest=14*14;
        for(unsigned i=0;i<preview.points.size();++i){
            const auto &point=preview.points[i];ImVec2 at;if(!project_point(cam,{point.x,point.y+.2,point.z},at))continue;
            const auto tint=structure_spline_point==int(i)?IM_COL32(255,218,105,255):color;
            draw->AddCircleFilled(at,structure_spline_point==int(i)?7:5,tint);draw->AddText({at.x+9,at.y-7},tint,std::to_string(i+1).c_str());
            const float dx=at.x-io.MousePos.x,dy=at.y-io.MousePos.y,d=dx*dx+dy*dy;if(d<nearest){nearest=d;hovered=int(i);}
        }
        if(!structure_spline_drawing&&!structure_spline_move&&structure_spline_drag<0&&eligible&&hovered>=0&&ImGui::IsMouseClicked(0)){
            structure_spline_point=structure_spline_drag=hovered;structure_spline_drag_position=preview.points[unsigned(hovered)];
        }
        if(structure_spline_drag>=0&&ImGui::IsMouseReleased(0)){
            const unsigned point=unsigned(structure_spline_drag);structure_spline_drag=-1;
            if(have_pick)structure_spline_commit_point(preview.id,point,structure_spline_drag_position);
        }
        if(eligible&&(structure_spline_drawing||structure_spline_move||structure_spline_drag>=0))draw->AddText({io.MousePos.x+18,io.MousePos.y+18},color,valid?(structure_spline_drawing?"Click to add point | Finish builds | Esc cancels":"Move point | Esc cancels"):error.empty()?"Click terrain to start":error.c_str());
    }
