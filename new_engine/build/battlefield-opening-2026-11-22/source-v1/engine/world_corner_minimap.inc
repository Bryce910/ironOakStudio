// Bounded corner-map drawing; included by Viewer and the CPU drawlist review.
    struct MinimapStats {
        uint64_t visible_frames=0,hidden_frames=0,cache_builds=0,cache_failures=0;
        unsigned primitives=0,peak_primitives=0,dropped=0,npc_dots=0,poi_labels=0;
        double player_x=0,player_y=0,player_z=0,yaw=0;
        float center_x=0,center_y=0,half_size=0,heading_x=0,heading_y=0,bottom=0;
    };
    void draw_corner_minimap(const world_minimap::Config &config,const world_minimap::Geometry &geometry,
        double x,double feet,double z,double yaw,float width,float height,
        const std::array<world_minimap::Marker,32> &npcs,unsigned npc_count,MinimapStats &stats) {
        namespace mm=world_minimap;
        ImDrawList *draw=ImGui::GetForegroundDrawList();
        const float scale=std::clamp(std::min(width/960.f,height/650.f),.78f,1.f);
        const ImVec2 corner(width-16-228*scale,52),map0(corner.x+12*scale,corner.y+28*scale);
        const float size=204*scale,half=size*.5f;
        const ImVec2 map1(map0.x+size,map0.y+size),center(map0.x+half,map0.y+half);
        const ImVec2 end(corner.x+228*scale,corner.y+258*scale);
        mm::Budget budget;
        const auto stroke=[&](ImVec2 a,ImVec2 b,ImU32 color,float thickness){if(budget.take(1))draw->AddLine(a,b,color,thickness);};
        const auto text=[&](ImVec2 at,ImU32 color,const char *label,float font){if(budget.take(1))draw->AddText(ImGui::GetFont(),font*scale,at,color,label);};
        if(budget.take(1))draw->AddRectFilled(corner,end,IM_COL32(13,20,17,231),9*scale);
        if(budget.take(1))draw->AddRect(corner,end,IM_COL32(151,132,86,220),9*scale,0,1.2f*scale);
        if(budget.take(1))draw->AddRectFilled(map0,map1,IM_COL32(35,48,36,246),3*scale);
        text({corner.x+12*scale,corner.y+7*scale},IM_COL32(230,222,196,255),"SELDONNE",13.5f);
        text({corner.x+200*scale,corner.y+7*scale},IM_COL32(207,217,203,255),"N",13.5f);
        stroke({corner.x+194*scale,corner.y+18*scale},{corner.x+194*scale,corner.y+8*scale},IM_COL32(204,214,201,255),1.2f*scale);
        stroke({corner.x+191*scale,corner.y+11*scale},{corner.x+194*scale,corner.y+8*scale},IM_COL32(204,214,201,255),1.2f*scale);
        stroke({corner.x+197*scale,corner.y+11*scale},{corner.x+194*scale,corner.y+8*scale},IM_COL32(204,214,201,255),1.2f*scale);
        draw->PushClipRect(map0,map1,true);
        stroke({center.x,map0.y},{center.x,map1.y},IM_COL32(135,151,126,22),scale);
        stroke({map0.x,center.y},{map1.x,center.y},IM_COL32(135,151,126,22),scale);
        const auto screen=[&](mm::Point p){return ImVec2(center.x+float(p.x)*half,center.y+float(p.z)*half);};
        const mm::Point player{x,z};
        // All paths got a fair cache quota; clipping occurs before drawing.
        // Reserve the last128 primitives for people, landmarks and the player.
        for(unsigned i=0;i<geometry.count;++i){
            const auto &line=geometry.lines[i];mm::Point a,b;
            if(!mm::project(line.a,player,config.radius,a)||!mm::project(line.b,player,config.radius,b)||!mm::clip(a,b,line.width/(2*config.radius)))continue;
            if(budget.used+3>mm::primitive_limit-128){++budget.dropped;continue;}
            const float thickness=std::max(line.kind==mm::LineKind::River?1.7f:1.5f,float(line.width/config.radius*half));
            const auto sa=screen(a),sb=screen(b);
            if(line.kind==mm::LineKind::River){stroke(sa,sb,IM_COL32(15,40,48,255),thickness+2*scale);stroke(sa,sb,IM_COL32(71,127,149,245),thickness);}
            else if(line.kind==mm::LineKind::Road){stroke(sa,sb,IM_COL32(42,37,27,255),thickness+2*scale);stroke(sa,sb,IM_COL32(175,153,104,245),thickness);}
            else {stroke(sa,sb,IM_COL32(33,27,19,255),std::max(thickness,3.f*scale)+2*scale);stroke(sa,sb,IM_COL32(226,202,150,255),std::max(thickness,3.f*scale));}
        }
        stats.npc_dots=0;
        for(unsigned i=0;i<std::min<unsigned>(npc_count,npcs.size());++i){mm::Point q;if(!mm::project(npcs[i].point,player,config.radius,q)||!mm::inside(q,.96))continue;
            const auto at=screen(q);if(!budget.take(2))continue;
            const ImU32 halo=IM_COL32(7,14,19,255);
            // A dark separation remains visible on both pale roads and green
            // terrain. Shapes distinguish traffic without adding map labels.
            if(npcs[i].kind==mm::MarkerKind::Rider){
                const auto diamond=[&](float r,ImU32 color){draw->AddQuadFilled({at.x,at.y-r*scale},{at.x+r*scale,at.y},{at.x,at.y+r*scale},{at.x-r*scale,at.y},color);};
                diamond(4.2f,halo);diamond(2.9f,IM_COL32(139,205,238,255));
            }else if(npcs[i].kind==mm::MarkerKind::Cart){
                draw->AddRectFilled({at.x-4.3f*scale,at.y-3.3f*scale},{at.x+4.3f*scale,at.y+3.3f*scale},halo,scale);
                draw->AddRectFilled({at.x-3.f*scale,at.y-2.f*scale},{at.x+3.f*scale,at.y+2.f*scale},IM_COL32(239,224,190,255),.5f*scale);
            }else{
                draw->AddCircleFilled(at,3.5f*scale,halo,10);
                draw->AddCircleFilled(at,2.1f*scale,IM_COL32(174,226,214,255),10);
            }
            ++stats.npc_dots;
        }
        std::array<std::array<float,4>,4> labels{};unsigned label_count=0;
        labels[label_count++]={center.x-13*scale,center.y-13*scale,center.x+13*scale,center.y+13*scale};
        stats.poi_labels=0;bool far_town=false;
        std::array<unsigned,mm::landmark_limit> order{};for(unsigned i=0;i<config.count;++i)order[i]=i;
        std::sort(order.begin(),order.begin()+config.count,[&](unsigned a,unsigned b){const auto &aa=config.landmarks[a].point,&bb=config.landmarks[b].point;return std::hypot(aa.x-x,aa.z-z)<std::hypot(bb.x-x,bb.z-z);});
        for(unsigned k=0;k<config.count;++k){const auto &landmark=config.landmarks[order[k]];mm::Point q;if(!mm::project(landmark.point,player,config.radius,q))continue;
            const bool near=mm::inside(q,.86);const double distance=std::hypot(landmark.point.x-x,landmark.point.z-z);
            if(!near){if(landmark.kind!=mm::Kind::Town||far_town||distance>config.radius*3.5)continue;far_town=true;}
            q=mm::clamp_marker(q);const auto at=screen(q);
            if(budget.take(1))draw->AddCircleFilled(at,4.5f*scale,IM_COL32(24,29,23,240),8);
            if(budget.take(1))draw->AddCircleFilled(at,2.8f*scale,near?IM_COL32(199,204,162,255):IM_COL32(237,198,112,255),4);
            if(stats.poi_labels>=3)continue;
            char label[64];if(near)std::snprintf(label,sizeof label,"%s",landmark.label.c_str());else std::snprintf(label,sizeof label,"%s %dm",landmark.label.c_str(),int(std::round(distance/5)*5));
            const float font=10.5f*scale;const auto extent=ImGui::GetFont()->CalcTextSizeA(font,1000,0,label);
            float tx=std::clamp(at.x+7*scale,map0.x+3*scale,map1.x-extent.x-3*scale),ty=std::clamp(at.y-16*scale,map0.y+3*scale,map1.y-extent.y-3*scale);
            auto overlaps=[&](){for(unsigned j=0;j<label_count;++j){const auto &r=labels[j];if(tx<r[2]+3*scale&&tx+extent.x>r[0]-3*scale&&ty<r[3]+3*scale&&ty+extent.y>r[1]-3*scale)return true;}return false;};
            if(overlaps())ty=std::clamp(ty-19*scale,map0.y+3*scale,map1.y-extent.y-3*scale);
            if(overlaps())continue;
            if(budget.take(1))draw->AddRectFilled({tx-2*scale,ty-scale},{tx+extent.x+2*scale,ty+extent.y+scale},IM_COL32(19,26,21,220),2*scale);
            text({tx,ty},IM_COL32(224,225,201,255),label,10.5f);labels[label_count++]={tx,ty,tx+extent.x,ty+extent.y};++stats.poi_labels;
        }
        const auto facing=mm::heading(yaw);const ImVec2 forward(float(facing.x),float(facing.z)),right(-forward.y,forward.x);
        if(budget.take(1))draw->AddTriangleFilled({center.x+forward.x*11*scale,center.y+forward.y*11*scale},{center.x-forward.x*7*scale+right.x*6*scale,center.y-forward.y*7*scale+right.y*6*scale},{center.x-forward.x*7*scale-right.x*6*scale,center.y-forward.y*7*scale-right.y*6*scale},IM_COL32(10,17,13,255));
        if(budget.take(1))draw->AddTriangleFilled({center.x+forward.x*9*scale,center.y+forward.y*9*scale},{center.x-forward.x*5*scale+right.x*4*scale,center.y-forward.y*5*scale+right.y*4*scale},{center.x-forward.x*5*scale-right.x*4*scale,center.y-forward.y*5*scale-right.y*4*scale},IM_COL32(250,237,194,255));
        draw->PopClipRect();
        if(budget.take(1))draw->AddRect(map0,map1,IM_COL32(121,134,104,160),3*scale,0,scale);
        char footer[64];std::snprintf(footer,sizeof footer,"%.0fm radius  |  TAB: atlas",config.radius);
        text({corner.x+12*scale,corner.y+239*scale},IM_COL32(183,191,169,255),footer,11.f);
        stats.primitives=budget.used;stats.peak_primitives=std::max(stats.peak_primitives,budget.used);stats.dropped+=budget.dropped;
        stats.player_x=x;stats.player_y=feet;stats.player_z=z;stats.yaw=yaw;stats.center_x=center.x;stats.center_y=center.y;stats.half_size=half;
        stats.heading_x=center.x+forward.x*9*scale;stats.heading_y=center.y+forward.y*9*scale;stats.bottom=end.y;
    }

