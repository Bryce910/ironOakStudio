// Included inside Studio. Stable aliases identify cards and placements; model
// previews are lazy, bounded renders of the real imported asset.
    AssetThumbnailCache designer_asset_thumbnails;
    M4 designer_asset_ghost_vp{};
    V3 designer_asset_ghost_eye{};
    GLuint designer_asset_ghost_depth=0;
    float designer_asset_ghost_near=.4f;
    int designer_asset_ghost_rect[4]{};
    std::uint64_t designer_asset_ghost_draws=0;
    char designer_asset_search[128]={};
    int designer_asset_category=0,designer_asset_sort=0;
    bool designer_asset_grid=true,designer_asset_favorites_only=false,designer_asset_recent_only=false;
    std::vector<std::string> designer_asset_favorites,designer_asset_recent,designer_asset_collections;
    std::string designer_asset_collection,designer_asset_selected,designer_asset_pending;
    ImVec2 designer_asset_selected_card={};
    std::vector<world_designer_assets::Record> designer_asset_records;
    std::vector<unsigned> designer_asset_filtered;
    world_designer_assets::RegistryIndex designer_asset_registry;
    bool designer_asset_refilter=true;
    int designer_asset_arm_frame=-1;
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
            const std::string stable(name);designer_asset_refresh();
            return designer_asset_registry.find(designer_asset_records,stable);
        }
        return designer_asset_registry.find(designer_asset_records,name);
    }
    void designer_asset_remember(std::string_view name){
        if(!designer_asset_recent.empty()&&designer_asset_recent.front()==name)return;
        world_designer_assets::remember(designer_asset_recent,name);
        if(designer_asset_recent_only)designer_asset_refilter=true;
        designer_preferences_changed();
    }
    void designer_asset_select(unsigned index){
        const auto info=world_scenery_alias(index);if(!info.name)return;
        if(!designer_asset_pending.empty()&&designer_asset_pending!=info.name)designer_asset_cancel();
        designer_asset_selected=info.name;alias=int(index);designer_asset_remember(info.name);
    }
    bool designer_place_asset(unsigned index){
        if(!production_can_save()||production_playing()||production_freeze()){message="Stop Play before placing scenery.";return false;}
        const auto info=world_scenery_alias(index);float placed_yaw=0,placed_scale=1;
        if(info.name&&structure_alias_protected(info.name)){message="This source structure shares fixed collision/support. Use decorative props here; layout changes need the structural source workflow in Project tools.";++structure_guard_rejections;return false;}
        if(!target||!world_designer_assets::placement_parameters(info,x,z,yaw,scale,placed_yaw,placed_scale)){
            message="Point at valid ground to place the selected asset.";return false;
        }
        if(draft.placements.size()>=world_studio::placement_limit){message="The 1024 added-model limit is reached. Remove a draft placement before adding more.";return false;}
        checkpoint();world_studio::Placement placement;placement.alias=info.name;
        placement.x=x;placement.z=z;placement.yaw=placed_yaw;placement.scale=placed_scale;
        draft.placements.push_back(std::move(placement));changed(true);
        choose(WORLD_ADDED_KEY_BASE+unsigned(draft.placements.size()-1),false,false);
        designer_asset_select(index);designer_asset_cancel();requested_tab=2;
        designer_preview_object_edit();
        message=designer_object_preview?"Placing asset in the scene. Save keeps your changes.":"Asset placed in the draft. Apply updates visible geometry and collision.";return true;
    }
    void designer_asset_refresh(){
        if(!designer_asset_registry.refresh(designer_asset_records,world_scenery_alias_revision(),world_scenery_alias_count(),world_scenery_alias))return;
        designer_asset_refilter=true;designer_asset_collections.clear();
        for(const auto &record:designer_asset_records)if(!record.collection.empty())designer_asset_collections.push_back(record.collection);
        std::sort(designer_asset_collections.begin(),designer_asset_collections.end());
        designer_asset_collections.erase(std::unique(designer_asset_collections.begin(),designer_asset_collections.end()),designer_asset_collections.end());
        if(!designer_asset_collection.empty()&&std::find(designer_asset_collections.begin(),designer_asset_collections.end(),designer_asset_collection)==designer_asset_collections.end())designer_asset_collection.clear();
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
    // This row-wrap helper keeps filters usable in a narrow or floating browser.
    void designer_asset_inline(float width){
        const float right=ImGui::GetWindowPos().x+ImGui::GetWindowContentRegionMax().x;
        if(ImGui::GetItemRectMax().x+ImGui::GetStyle().ItemSpacing.x+width<=right)ImGui::SameLine();
    }
    void designer_asset_inspect(const std::string &name){designer_asset_cancel();inspect_asset(name);requested_tab=9;}
    void designer_asset_arm(){
        if(designer_asset_index(designer_asset_selected)<0)return;
        // A browser click cannot commit at a stale viewport hit. Always preview
        // the current cursor location, then commit on an exposed-ground click.
        designer_asset_pending=designer_asset_selected;designer_asset_arm_frame=ImGui::GetFrameCount();
        message="Move into the viewport and click ground to place. Escape cancels.";
    }
    void designer_asset_tooltip(const world_designer_assets::Record &asset){
        ImGui::BeginTooltip();ImGui::TextUnformatted(asset.label.c_str());
        ImGui::TextDisabled("%s / %s",asset.collection.c_str(),asset.subcategory.c_str());
        ImGui::Separator();ImGui::TextDisabled("Asset ID: %s",asset.alias.c_str());ImGui::TextDisabled("Source: %s",asset.stem.c_str());
        const auto preview=designer_asset_thumbnails.request(asset.preview_stem,world_scenery_alias_revision(),true);
        if(preview.status==AssetThumbnailCache::Status::Unavailable&&preview.message)ImGui::TextWrapped("%s",preview.message);
        ImGui::Text("%u detail level%s | collision: %s",asset.lods,asset.lods==1?"":"s",world_designer_assets::collision_name(asset.collision));
        float lo[3],hi[3];if(scene_ptr&&world_scenery_alias_bounds(scene_ptr,asset.alias.c_str(),lo,hi))ImGui::Text("Size: %.2f x %.2f x %.2f m",hi[0]-lo[0],hi[1]-lo[1],hi[2]-lo[2]);
        if(asset.category==world_designer_assets::Category::Characters)ImGui::TextUnformatted("Scenery model. Use NPC / Nav for walking characters.");
        ImGui::TextUnformatted("Select to preview | Drag to place | Double-click to inspect");ImGui::EndTooltip();
    }
    void designer_asset_card(const world_designer_assets::Record &asset,float width,bool grid,float height){
        ImGui::PushID(asset.alias.c_str());const auto top=ImGui::GetCursorScreenPos();
        const bool selected=designer_asset_selected==asset.alias;
        if(selected)designer_asset_selected_card={top.x+width*.5f,top.y+height*.5f};
        ImGui::InvisibleButton("asset",{width,height});const bool hovered=ImGui::IsItemHovered(),visible=ImGui::IsItemVisible();
        if(ImGui::IsItemClicked())designer_asset_select(asset.index);
        if(hovered&&ImGui::IsMouseDoubleClicked(0))designer_asset_inspect(asset.alias);
        if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)){
            designer_asset_cancel();world_designer_assets::AliasPayload payload;
            if(world_designer_assets::encode_payload(asset.alias,payload))ImGui::SetDragDropPayload(world_designer_assets::payload_type,&payload,sizeof payload);
            ImGui::TextUnformatted(asset.label.c_str());ImGui::TextDisabled(designer_object_preview?"Drop on ground to place":"Drop on ground to stage a placement");ImGui::EndDragDropSource();
        }
        if(!visible){ImGui::PopID();return;}
        auto *draw=ImGui::GetWindowDrawList();
        const ImU32 border=selected?IM_COL32(224,183,109,255):(hovered?IM_COL32(126,149,163,255):IM_COL32(61,74,86,255));
        draw->AddRectFilled(top,{top.x+width,top.y+height},selected?IM_COL32(46,59,67,255):(hovered?IM_COL32(36,48,57,255):IM_COL32(25,34,42,255)),4);
        draw->AddRect(top,{top.x+width,top.y+height},border,4,0,selected?2.f:1.f);
        const float preview_h=grid?std::max(24.f,height-57.f):height-8.f;
        const float preview_w=grid?width-10.f:preview_h;
        const ImVec2 preview_min{top.x+5,top.y+4},preview_max{preview_min.x+preview_w,preview_min.y+preview_h};
        draw->AddRectFilled(preview_min,preview_max,IM_COL32(18,25,31,255),3);
        const auto thumbnail=designer_asset_thumbnails.request(asset.preview_stem,world_scenery_alias_revision(),selected);
        const float side=std::min(preview_w,preview_h),px=preview_min.x+(preview_w-side)*.5f,py=preview_min.y+(preview_h-side)*.5f;
        if(thumbnail.texture)draw->AddImage((ImTextureID)(intptr_t)thumbnail.texture,{px,py},{px+side,py+side},{0,1},{1,0});
        else{
            const float icon_size=std::min(side*.58f,36.f);
            designer_asset_icon(draw,{px+(side-icon_size)*.5f,py+(side-icon_size)*.4f},icon_size,asset.category,IM_COL32(117,139,153,255));
            if(grid&&preview_h>=68.f&&thumbnail.message){
                const char *status=thumbnail.status==AssetThumbnailCache::Status::Unavailable?"Preview unavailable":thumbnail.message;
                const auto tw=ImGui::CalcTextSize(status);draw->PushClipRect(preview_min,preview_max,true);
                draw->AddText(ImGui::GetFont(),ImGui::GetFontSize()*.75f,{preview_min.x+std::max(3.f,(preview_w-tw.x*.75f)*.5f),preview_max.y-15},IM_COL32(142,159,171,255),status);draw->PopClipRect();
            }
        }
        const float text_x=grid?top.x+8:preview_max.x+9,text_y=grid?top.y+height-51:top.y+6;
        const ImVec4 label_clip{text_x,text_y,top.x+width-9,grid?top.y+height-21:top.y+25};
        draw->AddText(ImGui::GetFont(),ImGui::GetFontSize(),{text_x,text_y},IM_COL32(230,234,236,255),asset.label.c_str(),nullptr,grid?width-16.f:0,&label_clip);
        const ImVec4 meta_clip{text_x,top.y+height-19,top.x+width-8,top.y+height-2};
        const std::string metadata=grid?asset.subcategory:asset.collection+" / "+asset.subcategory;
        draw->AddText(ImGui::GetFont(),ImGui::GetFontSize()*.79f,{text_x,top.y+height-18},IM_COL32(153,172,184,255),metadata.c_str(),nullptr,0,&meta_clip);
        if(world_designer_assets::contains(designer_asset_favorites,asset.alias))draw->AddCircleFilled({top.x+width-11,top.y+11},4,IM_COL32(229,186,107,255));
        if(hovered&&!ImGui::IsMouseDragging(0))designer_asset_tooltip(asset);
        ImGui::PopID();
    }
    void designer_asset_actions(const world_designer_assets::Record *asset){
        const bool locked=!production_can_save()||production_playing()||production_freeze();
        ImGui::BeginDisabled(!asset);
        ImGui::BeginDisabled(locked);
        if(ImGui::Button(designer_asset_pending.empty()?"Place in scene":"Cancel placement")){
            if(designer_asset_pending.empty())designer_asset_arm();else designer_asset_cancel();
        }ImGui::EndDisabled();
        designer_asset_inline(60);if(ImGui::Button("Inspect")&&asset)designer_asset_inspect(asset->alias);
        designer_asset_inline(84);const bool favorite=asset&&world_designer_assets::contains(designer_asset_favorites,asset->alias);
        if(ImGui::Button(favorite?"Unfavorite":"Favorite")&&asset){
            if(world_designer_assets::toggle_favorite(designer_asset_favorites,asset->alias)){designer_asset_refilter=true;designer_preferences_changed();}
            else message="The 128 favorite-asset limit is reached.";
        }ImGui::EndDisabled();
    }
    void designer_asset_details(const world_designer_assets::Record *asset){
        if(!asset){ImGui::TextUnformatted("Select a model");ImGui::TextWrapped("Browse the library, then drag a model onto the ground or choose Place in scene.");return;}
        const auto at=ImGui::GetCursorScreenPos();
        const float side=std::min(136.f,ImGui::GetContentRegionAvail().x*.46f);
        ImGui::Dummy({side,side});auto *draw=ImGui::GetWindowDrawList();
        draw->AddRectFilled(at,{at.x+side,at.y+side},IM_COL32(18,25,31,255),4);
        const auto thumbnail=designer_asset_thumbnails.request(asset->preview_stem,world_scenery_alias_revision(),true);
        if(thumbnail.texture)draw->AddImage((ImTextureID)(intptr_t)thumbnail.texture,at,{at.x+side,at.y+side},{0,1},{1,0});
        else{designer_asset_icon(draw,{at.x+side*.27f,at.y+side*.18f},side*.46f,asset->category,IM_COL32(117,139,153,255));
            if(thumbnail.message){draw->PushClipRect(at,{at.x+side,at.y+side},true);draw->AddText(ImGui::GetFont(),ImGui::GetFontSize()*.75f,{at.x+5,at.y+side-18},IM_COL32(142,159,171,255),thumbnail.status==AssetThumbnailCache::Status::Unavailable?"Preview unavailable":thumbnail.message);draw->PopClipRect();}}
        ImGui::SameLine();ImGui::BeginGroup();
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+std::max(1.f,ImGui::GetContentRegionAvail().x));
        ImGui::TextWrapped("%s",asset->label.c_str());
        ImGui::TextDisabled("%s",asset->collection.c_str());
        ImGui::TextDisabled("%s",asset->subcategory.c_str());
        float lo[3],hi[3];if(scene_ptr&&world_scenery_alias_bounds(scene_ptr,asset->alias.c_str(),lo,hi))ImGui::TextDisabled("%.1f x %.1f x %.1f m",hi[0]-lo[0],hi[1]-lo[1],hi[2]-lo[2]);
        ImGui::PopTextWrapPos();ImGui::EndGroup();
        ImGui::TextDisabled("Source preview / neutral studio lighting");
        designer_asset_actions(asset);
        if(asset->category==world_designer_assets::Category::Characters){
            ImGui::TextDisabled("Scenery model");ImGui::SameLine();if(ImGui::SmallButton("Create walking NPC"))requested_tab=10;
        }
        if(!designer_asset_pending.empty())ImGui::TextWrapped("Click ground to place. Escape cancels.");
        else ImGui::TextDisabled("Drag to place | Double-click to inspect");
    }
    void designer_asset_reset_filters(){
        designer_asset_search[0]=0;designer_asset_category=0;designer_asset_collection.clear();
        designer_asset_favorites_only=designer_asset_recent_only=false;designer_asset_refilter=true;designer_preferences_changed();
    }
    void designer_asset_filters(bool popup){
        using namespace world_designer_assets;
        ImGui::SetNextItemWidth(popup?190.f:125.f);
        if(ImGui::Combo(popup?"Category##asset-category":"##asset-category",&designer_asset_category,"All categories\0Buildings\0Nature\0Props\0Characters\0")){designer_asset_refilter=true;designer_preferences_changed();}
        if(!popup)designer_asset_inline(180);ImGui::SetNextItemWidth(popup?190.f:180.f);
        if(ImGui::BeginCombo(popup?"Collection##asset-collection":"##asset-collection",designer_asset_collection.empty()?"All collections":designer_asset_collection.c_str())){
            if(ImGui::Selectable("All collections",designer_asset_collection.empty())){designer_asset_collection.clear();designer_asset_refilter=true;}
            for(const auto &collection:designer_asset_collections)if(ImGui::Selectable(collection.c_str(),designer_asset_collection==collection)){designer_asset_collection=collection;designer_asset_refilter=true;}
            ImGui::EndCombo();
        }
        if(!popup)designer_asset_inline(130);ImGui::SetNextItemWidth(popup?190.f:130.f);
        if(ImGui::Combo(popup?"Sort##asset-sort":"##asset-sort",&designer_asset_sort,"Name (A-Z)\0Category\0Asset ID\0"))designer_asset_refilter=true;
        if(!popup)designer_asset_inline(80);if(ImGui::Button(designer_asset_grid?"List view":"Grid view")){designer_asset_grid=!designer_asset_grid;designer_preferences_changed();}
        if(!popup)designer_asset_inline(90);if(ImGui::Checkbox("Favorites",&designer_asset_favorites_only)){designer_asset_refilter=true;designer_preferences_changed();}
        if(!popup)designer_asset_inline(80);if(ImGui::Checkbox("Recent",&designer_asset_recent_only))designer_asset_refilter=true;
        if(!popup)designer_asset_inline(110);if(ImGui::SmallButton("Refresh previews"))designer_asset_thumbnails.invalidate_all();
        if(popup&&ImGui::Button("Reset filters"))designer_asset_reset_filters();
    }
    void designer_assets_panel(){
        using namespace world_designer_assets;designer_asset_refresh();
        const float available=ImGui::GetContentRegionAvail().x;
        const bool compact=ImGui::GetContentRegionAvail().y<170.f||available<650.f;
        if(compact){
            // One toolbar row leaves a complete list row visible even at the
            // supported 112px browser minimum. Less-used controls live above
            // the browser in an ordinary scrolling popup.
            const float gap=ImGui::GetStyle().ItemSpacing.x;
            ImGui::SetNextItemWidth(std::max(60.f,available-64.f-64.f-96.f-gap*3));
            if(ImGui::InputTextWithHint("##asset-search","Search models",designer_asset_search,sizeof designer_asset_search))designer_asset_refilter=true;
            ImGui::SameLine();if(ImGui::Button("Filters",{64.f,0}))ImGui::OpenPopup("Asset filters");
            ImGui::SetNextWindowSizeConstraints({285.f,0},{440.f,std::max(200.f,ImGui::GetIO().DisplaySize.y*.8f)});
            if(ImGui::BeginPopup("Asset filters")){
                designer_asset_filters(true);
                const auto index=designer_asset_index(designer_asset_selected);
                if(index>=0){ImGui::Separator();ImGui::TextWrapped("%s",designer_asset_records[unsigned(index)].label.c_str());designer_asset_actions(&designer_asset_records[unsigned(index)]);}
                ImGui::EndPopup();
            }
            ImGui::SameLine();const bool locked=designer_asset_selected.empty()||!production_can_save()||production_playing()||production_freeze();
            ImGui::BeginDisabled(locked);
            if(ImGui::Button(designer_asset_pending.empty()?"Place":"Cancel",{64.f,0})){
                if(designer_asset_pending.empty())designer_asset_arm();else designer_asset_cancel();
            }ImGui::EndDisabled();
        }else{
            ImGui::SetNextItemWidth(std::max(100.f,std::min(420.f,available-90.f)));
            if(ImGui::InputTextWithHint("##asset-search","Search names, collections, tags",designer_asset_search,sizeof designer_asset_search))designer_asset_refilter=true;
            designer_asset_inline(80);if(ImGui::Button("Reset filters"))designer_asset_reset_filters();
            if(available>850){designer_asset_inline(340);ImGui::TextDisabled("Select to preview  |  Drag to place  |  Double-click to inspect");}
            designer_asset_filters(false);
        }
        designer_asset_category=std::clamp(designer_asset_category,0,4);designer_asset_sort=std::clamp(designer_asset_sort,0,2);
        if(designer_asset_refilter){
            designer_asset_filtered.clear();designer_asset_filtered.reserve(designer_asset_records.size());
            for(unsigned i=0;i<designer_asset_records.size();++i){const auto &asset=designer_asset_records[i];
                if(designer_asset_category&&asset.category!=Category(designer_asset_category))continue;
                if(!designer_asset_collection.empty()&&asset.collection!=designer_asset_collection)continue;
                if(designer_asset_favorites_only&&!contains(designer_asset_favorites,asset.alias))continue;
                if(designer_asset_recent_only&&!contains(designer_asset_recent,asset.alias))continue;
                if(matches(asset,designer_asset_search))designer_asset_filtered.push_back(i);
            }
            std::sort(designer_asset_filtered.begin(),designer_asset_filtered.end(),[&](unsigned a,unsigned b){
                if(designer_asset_recent_only){const auto ra=std::find(designer_asset_recent.begin(),designer_asset_recent.end(),designer_asset_records[a].alias),rb=std::find(designer_asset_recent.begin(),designer_asset_recent.end(),designer_asset_records[b].alias);if(ra!=rb)return ra<rb;}
                return less(designer_asset_records[a],designer_asset_records[b],Sort(designer_asset_sort));
            });designer_asset_refilter=false;
        }
        const auto chosen=designer_asset_index(designer_asset_selected);const Record *asset=chosen>=0?&designer_asset_records[unsigned(chosen)]:nullptr;
        if(compact){ImGui::SameLine();ImGui::TextDisabled("%zu / %zu",designer_asset_filtered.size(),designer_asset_records.size());}
        else ImGui::TextDisabled("%zu of %zu models",designer_asset_filtered.size(),designer_asset_records.size());
        if(!compact&&!designer_asset_pending.empty()){designer_asset_inline(220);ImGui::TextUnformatted("Click ground to place | Escape cancels");}
        const float body_height=std::max(1.f,ImGui::GetContentRegionAvail().y);
        const bool side_details=!compact&&available>=1000.f&&body_height>=136.f;
        const float detail_width=side_details?300.f:0.f;
        const float footer=compact||side_details?0.f:(body_height>=112.f?31.f:0.f);
        const float cards_height=std::max(1.f,body_height-footer);
        const bool grid=designer_asset_grid&&cards_height>=92.f;
        const float card_height=grid?std::clamp(cards_height-4.f,92.f,174.f):46.f;
        if(ImGui::BeginChild("Asset cards",{side_details?-detail_width-8.f:0.f,cards_height},false,ImGuiWindowFlags_AlwaysVerticalScrollbar)){
            if(designer_asset_filtered.empty()){
                ImGui::TextDisabled(designer_asset_recent_only?"No recent models match these filters.":(designer_asset_favorites_only?"No favorites match these filters.":"No models match these filters."));
                ImGui::TextDisabled("Try a shorter search or Reset filters.");
            }else{
                const float area=std::max(1.f,ImGui::GetContentRegionAvail().x),gap=8.f;
                const unsigned columns=grid?std::max(1,int((area+gap)/180.f)):1;
                const float width=std::max(1.f,(area-gap*float(columns-1))/float(columns));
                const float pitch=card_height+ImGui::GetStyle().ItemSpacing.y;
                const int rows=int((designer_asset_filtered.size()+columns-1)/columns);ImGuiListClipper clipper;clipper.Begin(rows,pitch);
                while(clipper.Step())for(int row=clipper.DisplayStart;row<clipper.DisplayEnd;++row){
                    for(unsigned column=0;column<columns;++column){const auto at=std::size_t(row)*columns+column;if(at>=designer_asset_filtered.size())break;
                        if(column)ImGui::SameLine(0,gap);
                        designer_asset_card(designer_asset_records[designer_asset_filtered[at]],width,grid,card_height);
                    }
                }
            }
        }ImGui::EndChild();
        if(side_details){ImGui::SameLine(0,8);if(ImGui::BeginChild("Selected asset",{0,cards_height},true))designer_asset_details(asset);ImGui::EndChild();}
        else if(footer>0)designer_asset_actions(asset);
    }
    void designer_asset_ghost(const Camera &cam,std::string_view name){
        const auto index=designer_asset_index(name);if(index<0||!target)return;
        const auto info=world_scenery_alias(unsigned(index));float object_yaw=0,object_scale=1;
        if(!world_designer_assets::placement_parameters(info,x,z,yaw,scale,object_yaw,object_scale))return;
        float lo[3]={-.5f,0,-.5f},hi[3]={.5f,1,.5f};
        const std::string preview_stem=world_designer_assets::preview_stem(info);
        designer_asset_thumbnails.touch_placement(preview_stem,world_scenery_alias_revision());
        bool exact=scene_ptr&&world_scenery_alias_bounds(scene_ptr,info.name,lo,hi);
        if(!exact)exact=designer_asset_thumbnails.placement_bounds(preview_stem,lo,hi);
        double pivot=y-lo[1]*object_scale;
        WorldSceneryPlacement placement;placement.alias=info.name;placement.x=x;placement.z=z;
        placement.yaw=float(object_yaw*.017453292519943295);placement.scale=object_scale;
        const bool grounded=scene_ptr&&world_scenery_grounded_pivot(scene_ptr,placement,production_ground,&draft,pivot);
        const auto display=ImGui::GetIO().DisplaySize;const auto viewport=designer_viewport(display.x,display.y);
        auto *draw=ImGui::GetForegroundDrawList();draw->PushClipRect({viewport.x,viewport.y},{viewport.x+viewport.w,viewport.y+viewport.h},true);
        const double angle=object_yaw*.017453292519943295,c=std::cos(angle),s=std::sin(angle);
        ImVec2 points[8];bool visible[8]={};
        // The same support policy as committed scenery keeps the preview's
        // pivot aligned on slopes and for assets with an off-center origin.
        for(unsigned k=0;k<8;++k){const double a=(k&1?hi[0]:lo[0])*object_scale,b=(k&4?hi[2]:lo[2])*object_scale;
            visible[k]=project_point(cam,{x+c*a+s*b,pivot+(k&2?hi[1]:lo[1])*object_scale,z-s*a+c*b},points[k]);
        }
        for(unsigned k=0;k<8;++k)for(unsigned axis:{1u,2u,4u})if(!(k&axis)&&visible[k]&&visible[k|axis])draw->AddLine(points[k],points[k|axis],IM_COL32(121,230,194,235),2);
        const bool rendered=grounded&&designer_asset_ghost_depth&&designer_asset_thumbnails.draw_placement(preview_stem,designer_asset_ghost_vp,designer_asset_ghost_eye,
            {float(x-cam.x),float(pivot-cam.y),float(z-cam.z)},placement.yaw,object_scale,
            designer_asset_ghost_rect[0],designer_asset_ghost_rect[1],designer_asset_ghost_rect[2],designer_asset_ghost_rect[3],true,designer_asset_ghost_depth,designer_asset_ghost_near,15000.f);
        designer_asset_ghost_draws+=rendered;
        const char *hint=!grounded?"Bounds preview | grounding confirmed on placement":(!designer_asset_pending.empty()?"Click to place | Escape cancels":(rendered?"Release to place":(exact?"Preparing placement preview":"Placement marker | bounds unavailable")));
        ImVec2 root;if(project_point(cam,{x,y,z},root)){draw->AddCircle(root,6,IM_COL32(121,230,194,255),20,2);draw->AddText({root.x+12,root.y+5},IM_COL32(215,246,234,255),hint);}
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
