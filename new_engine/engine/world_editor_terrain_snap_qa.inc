// Real End-key terrain snapping, against an isolated temporary document only.
        if(launch->frames&&!launch->benchmark&&studio.path.rfind("/tmp/",0)==0&&std::getenv("VALWOOD_QA_EDITOR_TERRAIN_SNAP")){
            ImGui::GetIO().IniFilename=nullptr;
            struct TerrainSnapQA {unsigned phase=0,checks=0,source=0,applies=0;std::uint64_t since=0;size_t history=0,colliders=0;world_studio::Pose original,raised,snapped;};static TerrainSnapQA q;
            auto check=[&](bool good,const char *label){++q.checks;if(!good){std::fprintf(stderr,"[Terrain snap QA] FAIL phase %u: %s | %s\n",q.phase,label,studio.message.c_str());std::fflush(stderr);launch->exit_code=1;running=false;}return good;};
            auto key=[&](SDL_Keycode code,bool ctrl=false){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.windowID=SDL_GetWindowID(window);e.key.state=SDL_PRESSED;e.key.keysym.sym=code;e.key.keysym.mod=ctrl?KMOD_CTRL:KMOD_NONE;SDL_PushEvent(&e);e.type=SDL_KEYUP;e.key.state=SDL_RELEASED;SDL_PushEvent(&e);q.since=frame;};
            const bool settled=!destination_loading&&terrain.view_ready(cam.x,cam.z,cam.fly)&&!studio.dirty&&!studio.designer_paste_pending&&!studio.designer_frame_pending;
            auto seated=[&](world_studio::Pose &p){
                if(!check(studio.pose(q.source,p),"inspect actual rendered model"))return false;
                auto shape=vw_transform::pose(p);shape.x=shape.y=shape.z=0;const auto matrix=vw_transform::matrix(shape);double low=1e30,high_ground=-1e30;
                for(unsigned corner=0;corner<8;++corner){const auto v=vw_transform::vector(matrix,{corner&1?p.maximum.x:p.minimum.x,corner&2?p.maximum.y:p.minimum.y,corner&4?p.maximum.z:p.minimum.z});low=std::min(low,v[1]);}
                for(unsigned face=0;face<2;++face)for(unsigned z=0;z<=4;++z)for(unsigned x=0;x<=4;++x){const auto v=vw_transform::vector(matrix,{p.minimum.x+(p.maximum.x-p.minimum.x)*x*.25,face?p.maximum.y:p.minimum.y,p.minimum.z+(p.maximum.z-p.minimum.z)*z*.25});high_ground=std::max(high_ground,double(world_atlas_height(p.root.x+v[0],p.root.z+v[2])));}
                std::fprintf(stderr,"[Terrain snap QA] visible bottom %.6f, terrain support %.6f\n",p.root.y+low,high_ground);
                return check(std::abs(p.root.y+low-high_ground)<.002,"actual mesh base rests on sampled terrain");
            };
            if(q.phase==0&&frame>40&&settled){
                studio.open=true;double nearest=1e30;bool found=false;
                for(unsigned i=0;i<studio.source_poses.size();++i)if(studio.source_poses[i].alias=="rock0"){world_studio::Pose p;if(studio.pose(i,p)){const double distance=std::hypot(p.root.x-cam.x,p.root.z-cam.z);if(distance<nearest){nearest=distance;q.source=i;q.original=p;found=true;}}}
                if(!check(found,"nearby boulder with real GLB geometry exists"))continue;
                check(studio.designer_select(world_designer::Ref::source_model(q.source)),"select boulder");
                world_designer::Command command;command.operation=world_designer::Operation::Transform;
                check(world_designer::world_pose(studio.draft,studio.designer_context(),studio.designer_selection.active,command.transform,studio.message),"read source transform");command.transform.y+=6;
                q.colliders=world_scenery_stats(scenery).source_colliders;q.applies=studio.applied+1;
                check(studio.designer_run(command),"raise test model by six metres");q.phase=1;
            }else if(q.phase==1&&studio.applied==q.applies&&settled){
                check(studio.pose(q.source,q.raised)&&std::abs(q.raised.root.y-q.original.root.y-6)<.002,"test model is visibly floating before snap");
                q.history=studio.history.back.size();q.applies=studio.applied+1;key(SDLK_END);q.phase=2;
            }else if(q.phase==2&&studio.applied==q.applies&&settled){
                seated(q.snapped);check(studio.history.back.size()==q.history+1,"one End press is one undo step");
                std::fprintf(stderr,"[Terrain snap QA] preserved transform deltas: X %.12g Z %.12g yaw %.12g scale %.12g\n",q.snapped.root.x-q.original.root.x,q.snapped.root.z-q.original.root.z,q.snapped.yaw-q.original.yaw,q.snapped.scale-q.original.scale);
                check(std::abs(q.snapped.root.x-q.original.root.x)<.00001&&std::abs(q.snapped.root.z-q.original.root.z)<.00001&&std::abs(q.snapped.yaw-q.original.yaw)<.00001&&std::abs(q.snapped.scale-q.original.scale)<.00001,"snap keeps XZ, yaw and scale within scene serialization precision");
                check(world_scenery_stats(scenery).source_colliders==q.colliders,"static collision retained");
                q.history=studio.history.back.size();q.applies=studio.applied;key(SDLK_END);q.phase=3;
            }else if(q.phase==3&&frame-q.since>8){
                check(studio.applied==q.applies&&studio.history.back.size()==q.history,"already snapped model does not reapply or add undo");
                q.applies=studio.applied+1;key(SDLK_z,true);q.phase=4;
            }else if(q.phase==4&&studio.applied==q.applies&&settled){
                world_studio::Pose p;check(studio.pose(q.source,p)&&std::abs(p.root.y-q.raised.root.y)<.002,"Ctrl+Z restores floating height");
                q.applies=studio.applied+1;key(SDLK_y,true);q.phase=5;
            }else if(q.phase==5&&studio.applied==q.applies&&settled){
                world_studio::Pose p;seated(p);check(std::abs(p.root.y-q.snapped.root.y)<.002,"Ctrl+Y restores terrain snap");
                world_designer::Command command;command.operation=world_designer::Operation::Transform;
                check(world_designer::world_pose(studio.draft,studio.designer_context(),studio.designer_selection.active,command.transform,studio.message),"read grounded model transform");
                command.transform.pitch=15;command.transform.roll=10;command.transform.sx=1.3;command.transform.sy=.7;command.transform.sz=1.1;command.transform.y+=4;
                q.applies=studio.applied+1;check(studio.designer_run(command),"raise tilted nonuniform model");q.phase=6;
            }else if(q.phase==6&&studio.applied==q.applies&&settled){
                q.applies=studio.applied+1;key(SDLK_END);q.phase=7;
            }else if(q.phase==7&&studio.applied==q.applies&&settled){
                world_studio::Pose p;seated(p);
                check(std::abs(p.pitch-15*.017453292519943295)<.0001&&std::abs(p.roll-10*.017453292519943295)<.0001&&std::abs(p.sx-1.3)<.0001&&std::abs(p.sy-.7)<.0001,"tilt and axis scales preserved after real terrain snap");
                check(studio.rejected==0,"all scene updates accepted");studio.save();studio.designer_paste_notice_until=Studio::production_now()+600;
                std::fprintf(stderr,"[Terrain snap QA] PASS %u checks; floating and tilted models, End, Undo, Redo and render/collision verified\n",q.checks);std::fflush(stderr);q.phase=8;
            }
            if(q.phase<8&&frame>900){check(false,"fixture completed before deadline");q.phase=8;}
        }
