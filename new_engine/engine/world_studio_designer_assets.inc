// Included inside Studio. Asset cards use registry metadata and type icons;
// actual image/model inspection remains the explicit asynchronous Inspect tool.
    char designer_asset_search[128]={};
    int designer_asset_category=0;
    bool designer_asset_grid=true,designer_asset_favorites_only=false;
    std::vector<std::string> designer_asset_favorites,designer_asset_recent;
    std::string designer_asset_selected,designer_asset_pending;
    ImVec2 designer_asset_selected_card={};
    std::vector<world_designer_assets::Record> designer_asset_records;
    std::vector<unsigned> designer_asset_filtered;
    world_designer_assets::RegistryIndex designer_asset_registry;
    bool designer_asset_refilter=true;
    int designer_asset_arm_frame=-1;
    // Completed object edits use the normal validated render-thread Apply
    // fence. Never rebuild while the mouse is dragging, and never save files.
    bool designer_object_preview=true;
    void designer_preview_object_edit(){
        if(designer_object_preview&&production_can_save()&&!production_playing()&&!production_freeze())apply=true;
    }

    void designer_asset_cancel(){designer_asset_pending.clear();designer_asset_arm_frame=-1;}
    bool designer_asset_placing()const{
        if(!designer_asset_pending.empty())return true;
        const auto *payload=ImGui::GetDragDropPayload();
        return payload&&payload->IsDataType(world_designer_assets::payload_type);
    }
    int designer_asset_index(std::string_view name){
        if(!designer_asset_registry.current(world_scenery_alias_revision(),world_scenery_alias_count())){
            // The view may refer to a cached alias or pending selection that a
            // registry replacement invalidates. Copy only on that boundary.
            const std::string stable(name);designer_asset_refresh();
            return designer_asset_registry.find(designer_asset_records,stable);
        }
        return designer_asset_registry.find(designer_asset_records,name);
    }
    void designer_asset_remember(std::string_view name){
        if(!designer_asset_recent.empty()&&designer_asset_recent.front()==name)return;
        world_designer_assets::remember(designer_asset_recent,name);designer_preferences_changed();
    }
    void designer_asset_select(unsigned index){
        const auto info=world_scenery_alias(index);if(!info.name)return;
        designer_asset_selected=info.name;alias=int(index);designer_asset_remember(info.name);
    }
    bool designer_place_asset(unsigned index){
        if(!production_can_save()||production_playing()||production_freeze()){message="Stop Play before placing scenery.";return false;}
        const auto info=world_scenery_alias(index);float placed_yaw=0,placed_scale=1;
        if(!target||!world_designer_assets::placement_parameters(info,x,z,yaw,scale,placed_yaw,placed_scale)){
            message="Point at valid ground to place the selected asset.";return false;
        }
        if(draft.placements.size()>=world_studio::placement_limit){message="The 1024 added-model limit is reached. Remove a draft placement before adding more.";return false;}
        checkpoint();world_studio::Placement placement;placement.alias=info.name;
        placement.x=x;placement.z=z;placement.yaw=placed_yaw;placement.scale=placed_scale;
        // Same zero burial/elevation/offset defaults as the Objects tool.
        draft.placements.push_back(std::move(placement));changed(true);
        choose(8192+unsigned(draft.placements.size()-1),false,false);
        designer_asset_select(index);designer_asset_cancel();requested_tab=2;
        designer_preview_object_edit();
        message=designer_object_preview?"Placing asset in the scene. Save keeps your changes.":"Asset placed in the draft. Apply updates visible geometry and collision.";return true;
    }
    void designer_asset_refresh(){
        if(!designer_asset_registry.refresh(designer_asset_records,world_scenery_alias_revision(),world_scenery_alias_count(),world_scenery_alias))return;
        designer_asset_refilter=true;
        const auto selected=designer_asset_registry.find(designer_asset_records,designer_asset_selected);
        if(selected<0)designer_asset_selected.clear();else alias=selected;
        if(designer_asset_registry.find(designer_asset_records,designer_asset_pending)<0)designer_asset_cancel();
    }
    void designer_asset_icon(ImDrawList *draw,ImVec2 origin,float size,world_designer_assets::Category category,ImU32 color){
        using C=world_designer_assets::Category;
        const float x=origin.x,y=origin.y,s=size;
        if(category==C::Buildings){
            draw->AddTriangle({x+s*.1f,y+s*.44f},{x+s*.5f,y+s*.12f},{x+s*.9f,y+s*.44f},color,2);
            draw->AddRect({x+s*.22f,y+s*.44f},{x+s*.78f,y+s*.89f},color,1,0,2);
            draw->AddRect({x+s*.43f,y+s*.65f},{x+s*.59f,y+s*.89f},color,0,0,1.5f);
        }else if(category==C::Nature){
            draw->AddLine({x+s*.5f,y+s*.44f},{x+s*.5f,y+s*.91f},color,2);
            draw->AddTriangle({x+s*.5f,y+s*.1f},{x+s*.13f,y+s*.67f},{x+s*.87f,y+s*.67f},color,2);
        }else if(category==C::Characters){
            draw->AddCircle({x+s*.5f,y+s*.23f},s*.13f,color,14,2);
            draw->AddLine({x+s*.5f,y+s*.39f},{x+s*.5f,y+s*.68f},color,2);
            draw->AddLine({x+s*.23f,y+s*.5f},{x+s*.77f,y+s*.5f},color,2);
            draw->AddLine({x+s*.5f,y+s*.68f},{x+s*.27f,y+s*.92f},color,2);
            draw->AddLine({x+s*.5f,y+s*.68f},{x+s*.73f,y+s*.92f},color,2);
        }else{
            draw->AddRect({x+s*.18f,y+s*.22f},{x+s*.82f,y+s*.84f},color,2,0,2);
            draw->AddLine({x+s*.18f,y+s*.22f},{x+s*.82f,y+s*.84f},color,1.5f);
            draw->AddLine({x+s*.82f,y+s*.22f},{x+s*.18f,y+s*.84f},color,1.5f);
        }
    }
    void designer_asset_tooltip(const world_designer_assets::Record &asset){
        ImGui::BeginTooltip();ImGui::TextUnformatted(asset.label.c_str());
        ImGui::TextDisabled("Alias: %s",asset.alias.c_str());ImGui::TextDisabled("Source stem: %s",asset.stem.c_str());
        ImGui::Text("%s | %u LOD%s | collision: %s",world_designer_assets::category_name(asset.category),asset.lods,asset.lods==1?"":"s",world_designer_assets::collision_name(asset.collision));
        if(asset.distance>0)ImGui::Text("Configured draw distance: %.0f m",asset.distance);
        else ImGui::TextUnformatted("Configured draw distance: unavailable");
        if(asset.lods>1)ImGui::Text("LOD split: %.0f m",asset.split);
        ImGui::TextUnformatted("Icon denotes asset type; it is not a model preview.");
        if(asset.category==world_designer_assets::Category::Characters)ImGui::TextUnformatted("Placed as scenery. Use NPC / Nav for animated actors.");
        ImGui::TextUnformatted("Drag into the viewport to place. Double-click to inspect.");ImGui::EndTooltip();
    }
    void designer_asset_card(const world_designer_assets::Record &asset,float width,bool grid){
        ImGui::PushID(int(asset.index));const auto top=ImGui::GetCursorScreenPos();
        const float height=grid?86.f:29.f;const bool selected=designer_asset_selected==asset.alias;
        if(selected)designer_asset_selected_card={top.x+width*.5f,top.y+height*.5f};
        ImGui::InvisibleButton("asset",{width,height});const bool hovered=ImGui::IsItemHovered();
        if(ImGui::IsItemClicked())designer_asset_select(asset.index);
        if(hovered&&ImGui::IsMouseDoubleClicked(0)){inspect_asset(asset.alias);requested_tab=9;}
        if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)){
            designer_asset_cancel();
            world_designer_assets::AliasPayload payload;
            if(world_designer_assets::encode_payload(asset.alias,payload))ImGui::SetDragDropPayload(world_designer_assets::payload_type,&payload,sizeof payload);
            ImGui::TextUnformatted(asset.label.c_str());ImGui::TextDisabled(designer_object_preview?"Drop on ground to place":"Drop on ground to stage a placement");ImGui::EndDragDropSource();
        }
        auto *draw=ImGui::GetWindowDrawList();
        const ImU32 border=selected?IM_COL32(214,173,100,255):(hovered?IM_COL32(123,143,156,255):IM_COL32(65,77,88,255));
        draw->AddRectFilled(top,{top.x+width,top.y+height},selected?IM_COL32(48,58,66,255):(hovered?IM_COL32(38,47,55,255):IM_COL32(28,35,43,255)),4);
        draw->AddRect(top,{top.x+width,top.y+height},border,4,0,selected?2.f:1.f);
        const ImU32 icon=asset.category==world_designer_assets::Category::Nature?IM_COL32(131,186,133,255):IM_COL32(174,185,196,255);
        designer_asset_icon(draw,{top.x+6,top.y+(grid?8.f:2.f)},grid?34.f:24.f,asset.category,icon);
        const ImVec4 clip{top.x+2,top.y+2,top.x+width-5,top.y+height-4};
        const float text_x=top.x+(grid?46.f:35.f);
        draw->AddText(ImGui::GetFont(),ImGui::GetFontSize(),{text_x,top.y+6},IM_COL32(225,229,231,255),asset.label.c_str(),nullptr,grid?width-55.f:0,&clip);
        char metadata[96];std::snprintf(metadata,sizeof metadata,"%u LOD%s  |  %.0f m  |  %s",asset.lods,asset.lods==1?"":"s",asset.distance,world_designer_assets::collision_name(asset.collision));
        if(grid)draw->AddText(ImGui::GetFont(),ImGui::GetFontSize()*.82f,{top.x+9,top.y+height-22},IM_COL32(154,170,182,255),metadata,nullptr,0,&clip);
        else if(width>440)draw->AddText(ImGui::GetFont(),ImGui::GetFontSize()*.82f,{top.x+width-245,top.y+8},IM_COL32(154,170,182,255),metadata,nullptr,0,&clip);
        if(world_designer_assets::contains(designer_asset_favorites,asset.alias))draw->AddCircleFilled({top.x+width-10,top.y+10},3,IM_COL32(229,186,107,255));
        if(hovered&&!ImGui::IsMouseDragging(0))designer_asset_tooltip(asset);
        ImGui::PopID();
    }
    void designer_assets_panel(){
        using namespace world_designer_assets;designer_asset_refresh();
        const float available=ImGui::GetContentRegionAvail().x;
        ImGui::SetNextItemWidth(std::clamp(available*.25f,130.f,310.f));
        if(ImGui::InputTextWithHint("##asset-search","Search assets (all words)",designer_asset_search,sizeof designer_asset_search))designer_asset_refilter=true;
        ImGui::SameLine();ImGui::SetNextItemWidth(110);
        if(ImGui::Combo("##asset-category",&designer_asset_category,"All\0Buildings\0Nature\0Props\0Characters\0")){designer_asset_refilter=true;designer_preferences_changed();}
        ImGui::SameLine();if(ImGui::Button(designer_asset_grid?"List view":"Grid view")){designer_asset_grid=!designer_asset_grid;designer_preferences_changed();}
        if(available>580)ImGui::SameLine();
        if(ImGui::Checkbox("Favorites",&designer_asset_favorites_only)){designer_asset_refilter=true;designer_preferences_changed();}
        if(available>650){ImGui::SameLine();ImGui::SetNextItemWidth(145);
            if(ImGui::BeginCombo("##asset-recent","Recently used")){
                bool any_recent=false;for(const auto &name:designer_asset_recent){const auto index=designer_asset_index(name);if(index<0)continue;any_recent=true;
                    // Selecting mutates MRU, so close this iteration immediately.
                    if(ImGui::Selectable(name.c_str(),designer_asset_selected==name)){designer_asset_select(unsigned(index));break;}
                }if(!any_recent)ImGui::TextDisabled("Select an asset to begin");ImGui::EndCombo();
            }
        }
        designer_asset_category=std::clamp(designer_asset_category,0,4);
        if(designer_asset_refilter){
            designer_asset_filtered.clear();designer_asset_filtered.reserve(designer_asset_records.size());
            for(unsigned i=0;i<designer_asset_records.size();++i){const auto &asset=designer_asset_records[i];
                if(designer_asset_category&&asset.category!=Category(designer_asset_category))continue;
                if(designer_asset_favorites_only&&!contains(designer_asset_favorites,asset.alias))continue;
                if(matches(asset,designer_asset_search))designer_asset_filtered.push_back(i);
            }designer_asset_refilter=false;
        }
        const auto chosen=designer_asset_index(designer_asset_selected);const bool locked=!production_can_save()||production_playing()||production_freeze();
        ImGui::TextDisabled("%zu / %zu assets | Type icons",designer_asset_filtered.size(),designer_asset_records.size());
        if(available>480)ImGui::SameLine();
        ImGui::BeginDisabled(chosen<0);
        if(ImGui::SmallButton("Inspect")){inspect_asset(designer_asset_selected);requested_tab=9;}
        ImGui::SameLine();ImGui::BeginDisabled(locked);
        if(ImGui::SmallButton("Place")){
            if(target)designer_place_asset(unsigned(chosen));
            else{designer_asset_pending=designer_asset_selected;designer_asset_arm_frame=ImGui::GetFrameCount();message="Click ground in the viewport to place. Escape cancels.";}
        }ImGui::EndDisabled();
        ImGui::SameLine();const bool favorite=contains(designer_asset_favorites,designer_asset_selected);
        if(ImGui::SmallButton(favorite?"Unfavorite":"Favorite")){
            if(toggle_favorite(designer_asset_favorites,designer_asset_selected)){designer_asset_refilter=true;designer_preferences_changed();}
            else message="The 128 favorite-asset limit is reached.";
        }ImGui::EndDisabled();
        if(available>790){ImGui::SameLine();ImGui::TextDisabled("Drag to ground | Double-click to inspect");}
        if(designer_asset_category==int(Category::Characters)){
            ImGui::TextDisabled("Scenery registry only. Animated characters:");ImGui::SameLine();if(ImGui::SmallButton("NPC / Nav"))requested_tab=10;
        }
        ImGui::Separator();
        if(ImGui::BeginChild("Asset cards",{0,0},false,ImGuiWindowFlags_AlwaysVerticalScrollbar)){
            if(designer_asset_filtered.empty()){
                ImGui::TextDisabled(designer_asset_favorites_only?"No favorite assets match these filters.":"No registered assets match these filters.");
                if(*designer_asset_search&&ImGui::SmallButton("Clear search")){designer_asset_search[0]=0;designer_asset_refilter=true;}
            }else{
                const float area=std::max(1.f,ImGui::GetContentRegionAvail().x),gap=8.f;
                const unsigned columns=designer_asset_grid?std::max(1,int((area+gap)/190.f)):1;
                const float width=std::max(1.f,(area-gap*float(columns-1))/float(columns));
                const float pitch=(designer_asset_grid?86.f:29.f)+ImGui::GetStyle().ItemSpacing.y;
                const int rows=int((designer_asset_filtered.size()+columns-1)/columns);ImGuiListClipper clipper;clipper.Begin(rows,pitch);
                while(clipper.Step())for(int row=clipper.DisplayStart;row<clipper.DisplayEnd;++row){
                    for(unsigned column=0;column<columns;++column){const auto at=std::size_t(row)*columns+column;if(at>=designer_asset_filtered.size())break;
                        if(column)ImGui::SameLine(0,gap);
                        designer_asset_card(designer_asset_records[designer_asset_filtered[at]],width,designer_asset_grid);
                    }
                }
            }
        }ImGui::EndChild();
    }
    void designer_asset_ghost(const Camera &cam,std::string_view name){
        const auto index=designer_asset_index(name);if(index<0||!target)return;
        const auto info=world_scenery_alias(unsigned(index));float object_yaw=0,object_scale=1;
        if(!world_designer_assets::placement_parameters(info,x,z,yaw,scale,object_yaw,object_scale))return;
        float lo[3]={-.5f,0,-.5f},hi[3]={.5f,1,.5f};
        const bool exact=scene_ptr&&world_scenery_alias_bounds(scene_ptr,info.name,lo,hi);
        const auto display=ImGui::GetIO().DisplaySize;const auto viewport=designer_viewport(display.x,display.y);
        auto *draw=ImGui::GetForegroundDrawList();draw->PushClipRect({viewport.x,viewport.y},{viewport.x+viewport.w,viewport.y+viewport.h},true);
        const double angle=object_yaw*.017453292519943295,c=std::cos(angle),s=std::sin(angle);
        ImVec2 points[8];bool visible[8]={};
        // Ground-supported bounds preview; Apply performs authoritative grounding.
        for(unsigned k=0;k<8;++k){const double a=(k&1?hi[0]:lo[0])*object_scale,b=(k&4?hi[2]:lo[2])*object_scale;
            visible[k]=project_point(cam,{x+c*a+s*b,y+((k&2?hi[1]:lo[1])-lo[1])*object_scale,z-s*a+c*b},points[k]);
        }
        for(unsigned k=0;k<8;++k)for(unsigned axis:{1u,2u,4u})if(!(k&axis)&&visible[k]&&visible[k|axis])draw->AddLine(points[k],points[k|axis],IM_COL32(121,230,194,235),2);
        ImVec2 root;if(project_point(cam,{x,y,z},root)){draw->AddCircle(root,6,IM_COL32(121,230,194,255),20,2);draw->AddText({root.x+12,root.y+5},IM_COL32(215,246,234,255),designer_object_preview?"Release to place":(exact?"Placement bounds | Apply to render":"Placement marker | bounds unavailable"));}
        draw->PopClipRect();
    }
    void designer_asset_drop(const Camera &cam){
        if(!open||production_playing()||production_freeze()||!production_can_save()){designer_asset_cancel();return;}
        auto &io=ImGui::GetIO();const auto viewport=designer_viewport(io.DisplaySize.x,io.DisplaySize.y);
        if(io.AppFocusLost){designer_asset_cancel();return;}
        if(ImGui::IsKeyPressed(ImGuiKey_Escape)){designer_asset_cancel();return;}
        if(designer_blocks_viewport()||io.WantTextInput)return;
        const bool inside=viewport.contains(io.MousePos.x,io.MousePos.y);
        // Floating tools can overlap the scene rectangle. Drop/click placement
        // belongs to exposed ground, not a graphics panel or other UI window.
        if(ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow|ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))return;
        const auto *payload=ImGui::GetDragDropPayload();std::string_view dragged;
        const bool asset_payload=payload&&payload->IsDataType(world_designer_assets::payload_type)&&
            world_designer_assets::decode_payload(payload->Data,std::size_t(payload->DataSize),dragged);
        if(asset_payload){
            if(!inside)return;
            // Reset a previous ground hit: a sky ray must not place at stale ground.
            target=false;pick(cam,io.MousePos.x,io.MousePos.y);
            if(target)designer_asset_ghost(cam,dragged);
            if(ImGui::IsMouseReleased(0)&&target){const int index=designer_asset_index(dragged);if(index>=0)designer_place_asset(unsigned(index));}
            return;
        }
        if(designer_asset_pending.empty())return;
        if(designer_asset_index(designer_asset_pending)<0){designer_asset_cancel();message="The selected asset is no longer registered.";return;}
        if(!inside)return;
        target=false;pick(cam,io.MousePos.x,io.MousePos.y);if(target)designer_asset_ghost(cam,designer_asset_pending);
        if(ImGui::IsMouseClicked(0)&&ImGui::GetFrameCount()>designer_asset_arm_frame&&target){const auto index=designer_asset_index(designer_asset_pending);if(index>=0)designer_place_asset(unsigned(index));}
    }
