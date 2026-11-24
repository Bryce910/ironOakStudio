// Controller-transition review, using the candidate's real animation adapter,
// importer and GPU skinning. Scripted travel/combat inputs are deliberately
// labelled as a controller preview, not an end-to-end gameplay rehearsal.
#define main valwood_asset_preview_main
#include "preview_hero_skeletal.cpp"
#undef main
#include "engine/world_skeletal_hero.h"

namespace {
struct ControllerFrame {
    double vx=0,vz=0;
    float yaw=0;
    const char *label="Idle";
    std::uint64_t reaction_serial=0;
    unsigned reaction_direction=2;
};
double ease(double x) {x=std::clamp(x,0.,1.);return x*x*(3-2*x);}
ControllerFrame script(const std::string &scenario,double t,WorldTrainingCombat &fight) {
    ControllerFrame frame;
    fight.player_phase=TrainingPlayerPhase::Ready;
    fight.guard=false;
    fight.player_time=fight.player_duration=0;
    if(scenario=="locomotion") {
        if(t<1)frame.label="Idle";
        else if(t<3){frame.vz=1.65*ease((t-1)/.3);frame.label="Start / walk forward";}
        else if(t<4){frame.vz=1.65*(1-ease((t-3)/.3));frame.label="Decelerate / settle";}
        else if(t<6){frame.vx=-1.65*ease((t-4)/.25);frame.label="Strafe / change direction";}
        else if(t<7){frame.label="Blocked / no travel";}
        else if(t<10){frame.vz=5*ease((t-7)/.5);frame.label="Accelerate / full two-cycle run";}
        else if(t<11){frame.vz=5*(1-ease((t-10)/.35));frame.label="Stop running";}
        else frame.label="Idle / settled finish";
    } else if(scenario=="combat"||scenario=="combat-moving"||scenario=="combat-running") {
        if(scenario=="combat-moving"||scenario=="combat-running")frame.vz=(scenario=="combat-running"?5.:1.65)*ease(t/.3);
        int attack=-1;double age=0;
        for(int n=0;n<5;++n)if(t>=1+n*1.5&&t<1+n*1.5+.75){attack=n;age=t-(1+n*1.5);}
        if(t>=9.1&&t<10.25){const int n=std::min(2,int((t-9.1)/.31));attack=5+n;age=t-(9.1+n*.31);}
        if(attack>=0) {
            const bool combo=attack>=5;
            const double wind=attack%5==2?.36:combo?.14:.18,strike=.09,recovery=combo?.08:.24;
            fight.attack_direction=TrainingDirection(attack%5);
            fight.attack_start_serial=std::uint64_t(attack+1);
            if(age<wind){fight.player_phase=TrainingPlayerPhase::Windup;fight.player_time=age;fight.player_duration=wind;}
            else if(age<wind+strike){fight.player_phase=TrainingPlayerPhase::Strike;fight.player_time=age-wind;fight.player_duration=strike;}
            else if(age<wind+strike+recovery){fight.player_phase=TrainingPlayerPhase::Recovery;fight.player_time=age-wind-strike;fight.player_duration=recovery;}
            constexpr const char *names[]={"Left cut","Right cut","High cut","Low cut","Thrust"};
            frame.label=combo?"Three attack combo / interruption continuity":names[attack%5];
        }
    } else if(scenario=="run-entries") {
        const int n=std::min(4,int(t/3.));const double age=t-n*3.;
        frame.vz=age<1.3?5*ease(age/.25):0;
        fight.selected=TrainingDirection(n);
        fight.guard=age>=1&&age<1.65;
        frame.label=age<1?"Run / captured off-hand release":age<1.65?"Run to directional guard / re-grip":"Guard to attack / recover";
        const double wind=n==2?.36:.18;
        if(age>=1.65&&age<1.65+wind+.09+.24){const double attack=age-1.65;
            fight.attack_direction=TrainingDirection(n);fight.attack_start_serial=std::uint64_t(n+1);
            if(attack<wind){fight.player_phase=TrainingPlayerPhase::Windup;fight.player_time=attack;fight.player_duration=wind;}
            else if(attack<wind+.09){fight.player_phase=TrainingPlayerPhase::Strike;fight.player_time=attack-wind;fight.player_duration=.09;}
            else {fight.player_phase=TrainingPlayerPhase::Recovery;fight.player_time=attack-wind-.09;fight.player_duration=.24;}
        }
    } else if(scenario=="guard-switch") {
        const unsigned n=unsigned(t/1.1)%5;
        fight.selected=TrainingDirection(n);fight.guard=t>=.25;
        constexpr const char *names[]={"Hold LT / switch LEFT","Hold LT / switch RIGHT","Hold LT / switch HIGH","Hold LT / switch LOW","Hold LT / switch THRUST"};
        frame.label=names[n];
        if(t>=5.5)frame.vx=-.8;
    } else if(scenario=="steering") {
        frame.yaw=float(.8*std::sin(t*.75));
        const double speed=1.65*ease(t/.3);
        frame.vx=std::sin(frame.yaw)*speed;frame.vz=std::cos(frame.yaw)*speed;
        frame.label="Walk / continuous steering";
    } else if(scenario=="guard") {
        const int n=std::min(4,int(t/1.8));const double age=t-n*1.8;
        fight.selected=TrainingDirection(n);
        fight.guard=age>=.25&&age<1.4;
        if(t>4&&t<7)frame.vx=-.8;
        constexpr const char *names[]={"Left guard","Right guard","High guard","Low guard","Thrust guard"};
        frame.label=names[n];
    } else if(scenario=="reaction") {
        if(t>=.75) {
            const int n=std::min(4,int((t-.75)/1.3));
            frame.reaction_serial=std::uint64_t(n+1);frame.reaction_direction=unsigned(n);
            constexpr const char *names[]={"Left hit reaction","Right hit reaction","High hit reaction","Low hit reaction","Thrust hit reaction"};
            frame.label=names[n];
        }
        if(t>=3)frame.vz=.7;
    } else throw std::runtime_error("Unknown controller scenario");
    return frame;
}

int controller_preview(const Options &o,const char *exe,const std::string &scenario) {
    WorldSkeletalHero hero;std::string error;
    if(!hero.load_cpu(o.asset.c_str(),error))throw std::runtime_error("Controller import: "+error);
    const auto &asset=hero.asset();
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)!=0)throw std::runtime_error(SDL_GetError());
    Resources r;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
    r.window=SDL_CreateWindow("Valwood controller transitions | scripted preview",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,o.width,o.height,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    if(!r.window)throw std::runtime_error(SDL_GetError());
    r.context=SDL_GL_CreateContext(r.window);if(!r.context)throw std::runtime_error(SDL_GetError());
    SDL_GL_SetSwapInterval(0);
    const auto directory=shaders(o,exe);
    r.hero=program(read(directory/"world_skeletal.vs.glsl"),read(directory/"hero_preview.fs.glsl"));
    r.ground=program(ground_vertex,ground_fragment);
    r.renderer=ar::create(asset,1,error,256ull*1024*1024);if(!r.renderer)throw std::runtime_error(error);
    if(!ar::warm(r.renderer,r.hero,error))throw std::runtime_error(error);
    const float ground_vertices[]={-80,-.008f,-80,80,-.008f,-80,80,-.008f,80,-80,-.008f,-80,80,-.008f,80,-80,-.008f,80};
    glGenVertexArrays(1,&r.ground_vao);glGenBuffers(1,&r.ground_vbo);glBindVertexArray(r.ground_vao);glBindBuffer(GL_ARRAY_BUFFER,r.ground_vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof ground_vertices,ground_vertices,GL_STATIC_DRAW);glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),nullptr);glBindVertexArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);
    glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);glCullFace(GL_BACK);glDisable(GL_BLEND);glDisable(GL_FRAMEBUFFER_SRGB);
    const auto restore=ar::capture_bindings();
    if(!o.capture_directory.empty())std::filesystem::create_directories(o.capture_directory);
    std::ofstream trace;if(!o.capture_directory.empty()){
        trace.open(std::filesystem::path(o.capture_directory)/"controller-trace.csv");
        trace<<"frame,time,label,x,z,travel,gait_cycles,filtered_speed,run_weight,guard_weight,reaction_weight,blade_base_x,blade_base_y,blade_base_z,blade_tip_x,blade_tip_y,blade_tip_z";
        for(const char *foot:{"foot_l","ball_l","foot_r","ball_r"})for(const char *axis:{"x","y","z"})trace<<','<<foot<<'_'<<axis;
        trace<<'\n';
    }
    WorldTrainingCombat fight;WorldSkeletalHero::Input input;input.dt=1/o.fps;
    const auto started=std::chrono::steady_clock::now();
    for(int frame_index=0;frame_index<o.frames;++frame_index) {
        const double t=frame_index/o.fps;
        const auto frame=script(scenario,t,fight);
        input.dx=frame.vx*input.dt;input.dz=frame.vz*input.dt;input.x+=input.dx;input.z+=input.dz;input.yaw=frame.yaw;input.reset=frame_index==0;
        input.reaction_serial=frame.reaction_serial;input.reaction_direction=frame.reaction_direction;
        if(!hero.update_cpu(input,fight,error))throw std::runtime_error("Controller pose: "+error);
        ar::Instance instance{float(input.x),0,float(input.z),1,input.yaw};
        if(!ar::upload(r.renderer,&hero.pose(),&instance,1,restore,error))throw std::runtime_error("Controller upload: "+error);
        glViewport(0,0,o.width,o.height);glClearColor(.285f,.31f,.345f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        const wa::Vec3 target{float(input.x),.93f,float(input.z)};
        const float distance=4.3f*o.zoom*std::max(1.f,float(o.height)/o.width);
        const auto eye=add(target,{std::sin(o.yaw)*std::cos(o.pitch)*distance,std::sin(o.pitch)*distance,std::cos(o.yaw)*std::cos(o.pitch)*distance});
        const auto vp=product(perspective(float(o.width)/o.height),look(eye,target));
        glDisable(GL_CULL_FACE);glUseProgram(r.ground);uniform_matrix(r.ground,"uVP",vp);glBindVertexArray(r.ground_vao);glDrawArrays(GL_TRIANGLES,0,6);glBindVertexArray(0);glEnable(GL_CULL_FACE);
        glUseProgram(r.hero);uniform_matrix(r.hero,"uVP",vp);uniform3(r.hero,"uEye",eye);uniform3(r.hero,"uOffset",{});
        if(!ar::draw(r.renderer,r.hero,restore,error))throw std::runtime_error("Controller draw: "+error);
        gl_check("Controller preview frame");
        if(!o.capture_directory.empty()&&frame_index%o.capture_every==0){std::ostringstream name;name<<"frame-"<<std::setw(6)<<std::setfill('0')<<frame_index/o.capture_every<<".png";capture((std::filesystem::path(o.capture_directory)/name.str()).string(),o.width,o.height);}
        if(!o.capture.empty()&&frame_index==o.frames-1)capture(o.capture,o.width,o.height);
        double base[3],tip[3];if(!hero.blade(base,tip))throw std::runtime_error("Missing blade");
        if(trace){const auto &stats=hero.stats();trace<<std::setprecision(10)<<frame_index<<','<<t<<','<<frame.label<<','<<input.x<<','<<input.z<<','<<std::hypot(input.dx,input.dz)<<','<<stats.gait_cycles<<','<<stats.speed<<','<<stats.run<<','<<stats.guard<<','<<stats.reaction;for(double v:base)trace<<','<<v;for(double v:tip)trace<<','<<v;
            for(const char *foot:{"foot_l","ball_l","foot_r","ball_r"}) {
                const auto joint=std::find_if(asset.skeleton.joints.begin(),asset.skeleton.joints.end(),[&](const wa::Joint &j){return j.name==foot;});
                if(joint==asset.skeleton.joints.end())throw std::runtime_error(std::string("Missing ")+foot);
                const auto &m=hero.pose().global[std::size_t(joint-asset.skeleton.joints.begin())].m;
                trace<<','<<input.x+std::cos(input.yaw)*m[12]+std::sin(input.yaw)*m[14]<<','<<m[13]<<','<<input.z-std::sin(input.yaw)*m[12]+std::cos(input.yaw)*m[14];
            }trace<<'\n';}
        SDL_GL_SwapWindow(r.window);
    }
    const auto stats=ar::stats(r.renderer);
    std::cout<<"controller_scenario="<<scenario<<" frames="<<o.frames<<" uploads="<<stats.uploads<<" draws="<<stats.draws<<" failures="<<stats.failures
             <<" seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<" lighting=neutral scripted_controller_preview_not_gameplay\n";
    return stats.failures?1:0;
}
}

int main(int argc,char **argv) {
    try {
        std::string scenario="locomotion";std::vector<char *> args{argv[0]};
        for(int i=1;i<argc;++i){if(std::string(argv[i])=="--scenario"){if(++i>=argc)throw std::runtime_error("Missing --scenario");scenario=argv[i];}else args.push_back(argv[i]);}
        auto o=options(int(args.size()),args.data());
        if(o.help){std::cout<<"Usage: preview_knight_controller ASSET.glb --scenario locomotion|combat|combat-moving|combat-running|guard|guard-switch|run-entries|reaction|steering --frames N --fps 30 --capture-dir PATH [asset preview camera options]\nScripted animation-adapter transitions, not gameplay.\n";return 0;}
        if(!o.frames)throw std::runtime_error("A bounded --frames count is required");
        return controller_preview(o,argv[0],scenario);
    } catch(const std::exception &e){std::cerr<<"preview_knight_controller: "<<e.what()<<'\n';return 1;}
}
