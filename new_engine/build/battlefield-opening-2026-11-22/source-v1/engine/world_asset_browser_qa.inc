// Opt-in, bounded root-run integration rehearsal. Include after Studio.
// Production paths are exercised directly except for the explicitly tagged
// ImGui card click. This is not a physical mouse/controller acceptance test.
struct AssetBrowserQA {
    enum class Result {Inactive,Running,Passed,Failed};
    unsigned phase=0,checks=0,probe=0,click=0;
    std::uint64_t since=0,started=0,click_since=0;
    bool initialized=false,passed=false,failed=false,reopen=false;
    unsigned first_gl_error=0;
    std::size_t baseline=0,history=0,ghost_count=0;
    std::uint64_t rendered_before=0,ghosts_before=0;
    std::string directory,failure,selected_alias,ghost_alias;
    std::vector<std::string> assertions,preview_aliases,preview_stems,preview_captures;
    double placed_x=0,placed_z=0;float placed_yaw=0,placed_scale=1;
    ImVec2 mouse{};

    bool check(bool value,const char *name){
        ++checks;if(value){if(std::find(assertions.begin(),assertions.end(),name)==assertions.end())assertions.emplace_back(name);return true;}
        failed=true;failure=name;std::fprintf(stderr,"[Asset browser QA] FAIL phase%u: %s\n",phase,name);std::fflush(stderr);return false;
    }
    void report(){
        if(directory.empty())return;
        const auto path=std::filesystem::path(directory)/(reopen?"asset-browser-reopen.json":"asset-browser-workflow.json");
        std::ofstream f(path);if(!f)return;
        f<<"{\n\"schema\":\"valwood.asset-browser-runtime-qa.v1\",\n\"passed\":"<<(passed?"true":"false")<<",\n\"failed\":"<<(failed?"true":"false")
         <<",\n\"phase\":"<<phase<<",\n\"checks\":"<<checks<<",\n\"reopen\":"<<(reopen?"true":"false")<<",\n\"first_gl_error\":"<<first_gl_error
         <<",\n\"failure\":"<<world_json::quote(failure)<<",\n\"interaction_scope\":\"Tagged real ImGui card click; production methods for filters, favorite/recent, Place arm, fresh picker commit, Undo/Redo and Save. Actual model GL thumbnails are read back.\",\n\"assertions\":[";
        for(std::size_t i=0;i<assertions.size();++i){if(i)f<<',';f<<world_json::quote(assertions[i]);}f<<"],\n\"previews\":[";
        for(std::size_t i=0;i<preview_aliases.size();++i){if(i)f<<',';f<<"{\"alias\":"<<world_json::quote(preview_aliases[i])<<",\"preview_stem\":"<<world_json::quote(preview_stems[i])<<",\"capture\":"<<world_json::quote(preview_captures[i])<<'}';}
        f<<"],\n\"selected_alias\":"<<world_json::quote(selected_alias)<<",\n\"ghost_alias\":"<<world_json::quote(ghost_alias)<<",\n\"baseline_placements\":"<<baseline<<",\n\"placed_x\":"<<std::setprecision(17)<<placed_x<<",\n\"placed_z\":"<<placed_z<<"\n}\n";
    }
    bool guard(Studio &studio){
        std::error_code error;const auto root=std::filesystem::weakly_canonical(studio.project_directory,error);
        if(error)return check(false,"resolve disposable QA project");
        const auto name=root.filename().string();const auto generic=root.generic_string();
        const bool private_root=name.rfind("qa-project-asset-browser-",0)==0&&
            (generic.find("/build/asset-browser-")!=std::string::npos||generic.rfind("/tmp/",0)==0||generic.find(":/tmp/")!=std::string::npos);
        if(!check(private_root,"full disposable asset-browser project only"))return false;
        const auto scene=std::filesystem::weakly_canonical(studio.path,error);
        if(!check(!error&&scene.parent_path()==root&&!std::getenv("VALWOOD_QA_STUDIO_DOCUMENT"),"private complete project owns scene and saves"))return false;
        std::ifstream marker(root/".valwood-asset-browser-qa");std::string token;std::getline(marker,token);
        if(!check(token=="VALWOOD_ASSET_BROWSER_QA_V1","explicit disposable-project marker"))return false;
        directory=root.string();return true;
    }
    void reset_filters(Studio &s){s.designer_asset_search[0]=0;s.designer_asset_category=0;s.designer_asset_collection.clear();s.designer_asset_favorites_only=s.designer_asset_recent_only=false;s.designer_asset_refilter=true;}
    void show(Studio &s,const char *alias){
        reset_filters(s);std::snprintf(s.designer_asset_search,sizeof s.designer_asset_search,"%s",alias);
        const int index=s.designer_asset_index(alias);if(check(index>=0,"required source alias exists"))s.designer_asset_select(unsigned(index));
    }
    bool has(const Studio &s,const char *name)const{for(auto i:s.designer_asset_filtered)if(s.designer_asset_records[i].alias==name)return true;return false;}
    bool choose_resident_ghost(Studio &s){
        // A thumbnail may exist for a library alias that has no group in this
        // scene. Authoritative grounding requires an actual first-LOD group;
        // use its verified bounds instead of assuming a familiar library prop
        // or tree is part of this particular city's imported scene.
        s.designer_asset_refresh();float best_score=std::numeric_limits<float>::max();ghost_alias.clear();
        for(const auto &asset:s.designer_asset_records){
            if(asset.category!=world_designer_assets::Category::Props)continue;
            const auto info=world_scenery_alias(asset.index);if(!info.name||info.tree||info.vegetation)continue;
            float lo[3],hi[3];if(!world_scenery_alias_bounds(s.scene_ptr,info.name,lo,hi))continue;
            float extent=0;bool valid=true;
            for(unsigned i=0;i<3;++i){const float side=hi[i]-lo[i];valid=valid&&std::isfinite(lo[i])&&std::isfinite(hi[i])&&side>=0;extent=std::max(extent,side);}
            if(!valid||extent<.05f||extent>6.f)continue;
            float score=extent;
            if(asset.alias.find("bucket")!=std::string::npos||asset.alias.find("barrel")!=std::string::npos)score-=100.f;
            else if(asset.alias.find("crate")!=std::string::npos)score-=90.f;
            else if(asset.alias.find("wheel")!=std::string::npos)score-=80.f;
            if(score<best_score||(score==best_score&&asset.alias<ghost_alias)){best_score=score;ghost_alias=asset.alias;}
        }
        if(!check(!ghost_alias.empty(),"actual scene contains a bounded resident prop for grounded preview"))return false;
        std::fprintf(stderr,"[Asset browser QA] Grounded preview source: %s\n",ghost_alias.c_str());std::fflush(stderr);return true;
    }
    void hover(SDL_Window *window,ImVec2 p){
        mouse=p;SDL_WarpMouseInWindow(window,int(p.x),int(p.y));SDL_PumpEvents();
        SDL_Event e{};e.type=SDL_MOUSEMOTION;e.motion.windowID=SDL_GetWindowID(window);e.motion.which=0x56414c54u;e.motion.x=int(p.x);e.motion.y=int(p.y);SDL_PushEvent(&e);ImGui::GetIO().AddMousePosEvent(p.x,p.y);
    }
    void button(SDL_Window *window,bool down){
        hover(window,mouse);SDL_Event e{};e.type=down?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;e.button.windowID=SDL_GetWindowID(window);e.button.which=0x56414c54u;e.button.button=SDL_BUTTON_LEFT;e.button.state=down?SDL_PRESSED:SDL_RELEASED;e.button.x=int(mouse.x);e.button.y=int(mouse.y);
        check(SDL_PushEvent(&e)==1,"tagged real card mouse event");ImGui::GetIO().AddMouseButtonEvent(0,down);
    }
    // Read the real GPU tile, not a recreated illustration or source texture.
    bool capture_tile(unsigned texture,const char *alias){
        GLint active=0,binding=0,pack_buffer=0;GLint pack[8]{};
        constexpr GLenum names[8]={GL_PACK_ALIGNMENT,GL_PACK_ROW_LENGTH,GL_PACK_SKIP_PIXELS,GL_PACK_SKIP_ROWS,GL_PACK_IMAGE_HEIGHT,GL_PACK_SKIP_IMAGES,GL_PACK_SWAP_BYTES,GL_PACK_LSB_FIRST};
        glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding);glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&pack_buffer);
        for(unsigned i=0;i<8;++i)glGetIntegerv(names[i],&pack[i]);
        glBindTexture(GL_TEXTURE_2D,texture);GLint width=0,height=0;glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&width);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&height);
        bool ok=width==int(world_asset_thumbnail::tile_side)&&height==int(world_asset_thumbnail::tile_side);
        std::vector<unsigned char> pixels;
        if(ok){pixels.resize(std::size_t(width)*height*4);glBindBuffer(GL_PIXEL_PACK_BUFFER,0);for(unsigned i=0;i<8;++i)glPixelStorei(names[i],i==0?1:0);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());}
        for(unsigned i=0;i<8;++i)glPixelStorei(names[i],pack[i]);glBindBuffer(GL_PIXEL_PACK_BUFFER,GLuint(pack_buffer));glBindTexture(GL_TEXTURE_2D,GLuint(binding));glActiveTexture(GLenum(active));
        if(!check(ok,"real GL thumbnail has bounded tile dimensions"))return false;
        std::vector<unsigned> colors;colors.reserve(pixels.size()/4);for(std::size_t i=0;i<pixels.size();i+=4)colors.push_back(unsigned(pixels[i])|(unsigned(pixels[i+1])<<8)|(unsigned(pixels[i+2])<<16));
        std::sort(colors.begin(),colors.end());const auto distinct=std::unique(colors.begin(),colors.end())-colors.begin();
        if(!check(distinct>8,"real GL tile contains rendered source detail"))return false;
        const std::size_t stride=std::size_t(width)*4;for(int row=0;row<height/2;++row)std::swap_ranges(pixels.data()+row*stride,pixels.data()+(row+1)*stride,pixels.data()+(height-1-row)*stride);
        const auto path=std::filesystem::path(directory)/(std::string("asset-browser-preview-")+alias+".bmp");
        SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels.data(),width,height,32,int(stride),SDL_PIXELFORMAT_RGBA32);const bool saved=surface&&SDL_SaveBMP(surface,path.string().c_str())==0;if(surface)SDL_FreeSurface(surface);
        if(check(saved,"capture actual source-model GPU tile"))preview_captures.push_back(path.string());return saved;
    }
    Result tick(Studio &s,const Camera &cam,std::uint64_t frame,bool bounded_editor,bool ready){
        if(!std::getenv("VALWOOD_QA_ASSET_BROWSER")||!bounded_editor)return Result::Inactive;
        if(passed)return Result::Passed;if(failed)return Result::Failed;
        if(!initialized){initialized=true;started=since=frame;reopen=std::getenv("VALWOOD_QA_ASSET_BROWSER_REOPEN")!=nullptr;if(!guard(s)){report();return Result::Failed;}ImGui::GetIO().IniFilename=nullptr;}
        const auto error=glGetError();if(error){first_gl_error=error;check(false,"thumbnail/browser frame has no GL errors");report();return Result::Failed;}
        const auto stats=s.designer_asset_thumbnails.stats();
        if(!check(stats.tiles<=world_asset_thumbnail::tile_limit&&stats.bytes<=std::size_t(world_asset_thumbnail::tile_limit)*world_asset_thumbnail::tile_side*world_asset_thumbnail::tile_side*4,"thumbnail retained tile budget")){report();return Result::Failed;}
        if(frame-started>12000||frame-since>2400){check(false,"bounded browser workflow timeout");report();return Result::Failed;}
        if(!ready||frame-since<3)return Result::Running;
        SDL_Window *window=SDL_GL_GetCurrentWindow();if(!check(window!=nullptr,"owned live SDL editor window")){report();return Result::Failed;}
        if(click){
            if(click==1&&frame-click_since>=3){button(window,true);click=2;click_since=frame;}
            else if(click==2&&frame-click_since>=3){button(window,false);click=3;click_since=frame;}
            else if(click==3&&frame-click_since>=5){click=0;since=frame;}
            return Result::Running;
        }
        switch(phase){
        case 0:{
            s.open=true;s.designer_immersive=false;s.designer_content=true;s.designer_layout.bottom=300;s.designer_asset_grid=true;s.designer_object_preview=true;s.designer_tools=false;s.tab=s.requested_tab=2;
            SDL_SetRelativeMouseMode(SDL_FALSE);baseline=s.draft.placements.size();history=s.history.back.size();rendered_before=stats.rendered;
            if(reopen){std::ifstream expected(std::filesystem::path(directory)/".asset-browser-expected");std::size_t expected_count=0;expected>>selected_alias>>placed_x>>placed_z>>placed_yaw>>placed_scale>>expected_count;
                if(!check(bool(expected)&&s.live.placements.size()==expected_count,"fresh process loads saved placement count"))break;
                const auto &p=s.live.placements.back();if(!check(p.alias==selected_alias&&std::abs(p.x-placed_x)<1e-6&&std::abs(p.z-placed_z)<1e-6&&std::abs(p.yaw-placed_yaw)<1e-6&&std::abs(p.scale-placed_scale)<1e-6,"fresh process preserves exact alias and pose"))break;
                if(!check(world_designer_assets::contains(s.designer_asset_favorites,"pine")&&world_designer_assets::contains(s.designer_asset_favorites,"purchased_house_a"),"fresh process restores original favorite asset IDs"))break;
                baseline=expected_count-1;
            }
            show(s,"pine");phase=1;since=frame;break;
        }
        case 1:{
            constexpr const char *names[]={"pine","purchased_house_a","fern0","purchased_barrel","starting_bucket"};const auto name=names[probe];
            const int index=s.designer_asset_index(name);if(!check(index>=0,"preview probe alias remains registered"))break;
            const auto &asset=s.designer_asset_records[unsigned(index)];const auto image=s.designer_asset_thumbnails.request(asset.preview_stem,world_scenery_alias_revision(),true);
            if(image.status==AssetThumbnailCache::Status::Unavailable){check(false,(std::string("required preview available: ")+name+" | "+(image.message?image.message:"")).c_str());break;}
            if(image.status!=AssetThumbnailCache::Status::Ready||!image.texture)break;
            if(!capture_tile(image.texture,name))break;preview_aliases.emplace_back(name);preview_stems.push_back(asset.preview_stem);
            if(++probe<5)show(s,names[probe]);else{selected_alias="starting_bucket";phase=2;}
            since=frame;break;
        }
        case 2:{
            if(!check(stats.rendered>=rendered_before+5,"five actual models rendered by private thumbnail shader"))break;
            const auto at=s.designer_asset_selected_card;const auto size=ImGui::GetIO().DisplaySize;
            if(!check(at.x>0&&at.y>0&&at.x<size.x&&at.y<size.y,"selected real card is visible for input"))break;
            s.designer_asset_selected.clear();hover(window,at);click=1;click_since=frame;phase=3;since=frame;break;
        }
        case 3:
            if(!check(s.designer_asset_selected=="starting_bucket","tagged real ImGui card click selects stable asset ID"))break;
            reset_filters(s);s.designer_asset_category=int(world_designer_assets::Category::Nature);std::snprintf(s.designer_asset_search,sizeof s.designer_asset_search,"pine");phase=4;since=frame;break;
        case 4:
            if(!check(has(s,"pine")&&!has(s,"purchased_house_a"),"rendered category and multi-field search filter"))break;
            reset_filters(s);s.designer_asset_favorites.clear();check(world_designer_assets::toggle_favorite(s.designer_asset_favorites,"pine"),"favorite adds original alias");check(world_designer_assets::toggle_favorite(s.designer_asset_favorites,"purchased_house_a"),"second favorite adds original alias");s.designer_asset_favorites_only=true;phase=5;since=frame;break;
        case 5:
            if(!check(s.designer_asset_filtered.size()==2&&has(s,"pine")&&has(s,"purchased_house_a"),"rendered Favorites filter shows only original favorite aliases"))break;
            reset_filters(s);s.designer_asset_recent.clear();s.designer_asset_select(unsigned(s.designer_asset_index("purchased_house_a")));s.designer_asset_select(unsigned(s.designer_asset_index("pine")));s.designer_asset_recent_only=true;phase=6;since=frame;break;
        case 6:
            if(!check(s.designer_asset_filtered.size()==2&&s.designer_asset_records[s.designer_asset_filtered.front()].alias=="pine","rendered Recent filter preserves MRU ordering"))break;
            if(!choose_resident_ghost(s))break;show(s,ghost_alias.c_str());phase=70;since=frame;break;
        case 70:{
            s.target=false;const auto size=ImGui::GetIO().DisplaySize;const auto viewport=s.designer_viewport(size.x,size.y);ImVec2 point{};
            for(float row:{.8f,.65f,.5f})for(float column:{.5f,.35f,.65f})if(!s.target){point={viewport.x+viewport.w*column,viewport.y+viewport.h*row};s.pick(cam,point.x,point.y);}
            if(!check(s.target,"fresh actual terrain ray supports verified resident prop preview"))break;
            ghost_count=s.draft.placements.size();ghosts_before=s.designer_asset_ghost_draws;s.designer_asset_arm();hover(window,point);phase=75;since=frame;break;
        }
        case 75:
            if(s.designer_asset_ghost_draws<=ghosts_before)break;
            if(!check(s.draft.placements.size()==ghost_count,"actual depth-tested model ghost renders without placing"))break;
            s.designer_asset_cancel();show(s,"starting_bucket");phase=reopen?14:7;since=frame;break;
        case 7:{
            const auto count=s.draft.placements.size();s.target=true;s.x=cam.x;s.z=cam.z;s.designer_asset_arm();
            if(!check(s.designer_asset_pending=="starting_bucket"&&s.draft.placements.size()==count,"Place arms without committing stale ground"))break;
            s.designer_asset_cancel();if(!check(s.designer_asset_pending.empty()&&s.draft.placements.size()==count,"cancelling placement preserves scene and history"))break;
            s.target=false;const auto size=ImGui::GetIO().DisplaySize;const auto viewport=s.designer_viewport(size.x,size.y);
            for(float row:{.8f,.65f,.5f})for(float column:{.5f,.35f,.65f})if(!s.target)s.pick(cam,viewport.x+viewport.w*column,viewport.y+viewport.h*row);
            if(!check(s.target,"fresh exposed viewport ray finds valid actual ground"))break;
            s.designer_asset_arm();const int index=s.designer_asset_index("starting_bucket");
            if(!check(index>=0&&s.designer_place_asset(unsigned(index)),"production placement commits selected source alias at fresh picked ground"))break;
            if(!check(s.draft.placements.size()==baseline+1&&s.history.back.size()==history+1,"one placement adds one Undo transaction"))break;
            const auto &p=s.draft.placements.back();placed_x=p.x;placed_z=p.z;placed_yaw=p.yaw;placed_scale=p.scale;phase=8;since=frame;break;
        }
        case 8:
            if(s.dirty||s.apply||s.live.placements.size()!=baseline+1)break;
            if(!check(s.live.placements.back().alias=="starting_bucket","placement reaches real committed scenery"))break;
            s.restore_history(false);if(!check(s.draft.placements.size()==baseline,"production Undo removes placement"))break;phase=9;since=frame;break;
        case 9:
            if(s.dirty||s.apply||s.live.placements.size()!=baseline)break;
            s.restore_history(true);if(!check(s.draft.placements.size()==baseline+1,"production Redo restores exact source placement"))break;phase=10;since=frame;break;
        case 10:{
            if(s.dirty||s.apply||s.live.placements.size()!=baseline+1)break;
            if(!check(s.save(),"real Apply then Save persists private scene"))break;
            world_studio::Document disk;std::string error,a,b;
            if(!check(world_studio::load(s.path.c_str(),disk,error)&&world_studio::encode(disk,a,error)&&world_studio::encode(s.live,b,error)&&a==b,"saved scene codec reload equals complete live document"))break;
            std::ofstream expected(std::filesystem::path(directory)/".asset-browser-expected");expected<<"starting_bucket "<<std::setprecision(17)<<placed_x<<' '<<placed_z<<' '<<placed_yaw<<' '<<placed_scale<<' '<<s.live.placements.size()<<'\n';expected.close();
            if(!check(bool(expected),"record fresh-process reopen expectation"))break;phase=14;since=frame;break;
        }
        case 14:
            SDL_SetWindowSize(window,960,640);s.designer_layout.left=220;s.designer_layout.right=280;s.designer_layout.bottom=112;show(s,"starting_bucket");phase=15;since=frame;break;
        case 15:{
            const auto size=ImGui::GetIO().DisplaySize;if(size.x>970||size.y>650)break;
            const auto viewport=s.designer_viewport(size.x,size.y);if(!check(viewport.w>=320&&viewport.h>=240,"narrow editor retains usable viewport"))break;
            if(!check(has(s,"starting_bucket")&&s.designer_asset_selected=="starting_bucket","minimum-height browser preserves source filter and selection"))break;
            s.designer_content=false;s.designer_asset_cancel();s.designer_asset_thumbnails.invalidate_all();phase=16;since=frame;break;
        }
        case 16:
            if(frame-since<8)break;
            if(!check(stats.tiles==0&&stats.bytes==0,"deferred Refresh retires all unqueued thumbnail images"))break;
            s.designer_asset_thumbnails.shutdown();{
                const auto clean=s.designer_asset_thumbnails.stats();if(!check(clean.tiles==0&&clean.bytes==0&&clean.pending==0,"explicit cache cleanup releases tile and queue ownership"))break;
            }
            {const auto cleanup_error=glGetError();if(cleanup_error){first_gl_error=cleanup_error;check(false,"cache cleanup has no GL errors");break;}}
            passed=true;phase=100;report();std::fprintf(stderr,"[Asset browser QA] PASS %u checks; real GL tiles, tagged card click, direct production filter/place/Undo/save methods%s\n",checks,reopen?", fresh process reload":"");std::fflush(stderr);return Result::Passed;
        default:break;
        }
        if(failed){report();return Result::Failed;}return Result::Running;
    }
};
