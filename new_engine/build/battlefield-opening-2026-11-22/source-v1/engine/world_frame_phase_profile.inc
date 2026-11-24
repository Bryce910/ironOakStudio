// Opt-in measurement only. The boundary follows captures AND presentation,
// so readbacks cannot silently land in the next frame's gameplay category.
struct FramePhaseProfile {
    FILE *file=nullptr;
    std::array<char,262144> buffer={};
    Uint64 boundary=0;
    bool previous_controls=false,previous_loading=true,previous_modal=true;
    explicit FramePhaseProfile(const char *path) {
        const char *enabled=std::getenv("VALWOOD_PROFILE_PHASES");
        if(!path||!*path||!enabled||std::strcmp(enabled,"1"))return;
        file=std::fopen((std::string(path)+".phases.jsonl").c_str(),"w");
        if(file)std::setvbuf(file,buffer.data(),_IOFBF,buffer.size());
        boundary=SDL_GetPerformanceCounter();
    }
    ~FramePhaseProfile(){if(file)std::fclose(file);}
    void record(uint64_t frame,double work_ms,double capture_hooks_ms,bool controls,
                bool loading,bool modal,bool cinematic,bool editor,bool inventory,
                bool paused,double x,double z,float present_ms) {
        if(!file)return;
        const Uint64 now=SDL_GetPerformanceCounter();
        const double interval_ms=1000.*double(now-boundary)/SDL_GetPerformanceFrequency();
        // Conservatively exclude the whole interval containing expensive QA
        // capture hooks; retain its full duration in the complete distribution.
        const bool normal=controls&&previous_controls&&!loading&&!previous_loading&&
                          !modal&&!previous_modal&&capture_hooks_ms<1.;
        std::fprintf(file,"{\"frame\":%llu,\"interval_ms\":%.6f,\"work_ms\":%.6f,\"capture_hooks_ms\":%.6f,\"normal_playable\":%s,\"controls\":%s,\"loading\":%s,\"modal\":%s,\"cinematic\":%s,\"editor\":%s,\"inventory\":%s,\"paused\":%s,\"x\":%.5f,\"z\":%.5f,\"present_ms\":%.6f}\n",
            (unsigned long long)frame,interval_ms,work_ms,capture_hooks_ms,
            normal?"true":"false",controls?"true":"false",loading?"true":"false",
            modal?"true":"false",cinematic?"true":"false",editor?"true":"false",
            inventory?"true":"false",paused?"true":"false",x,z,double(present_ms));
        boundary=SDL_GetPerformanceCounter();
        previous_controls=controls;previous_loading=loading;previous_modal=modal;
    }
} frame_phase_profile(launch->profile);
