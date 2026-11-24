#include "engine/world_designer_viewport.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

using namespace world_designer_viewport;
static bool close(float a,float b,float epsilon=.001f) {return std::abs(a-b)<=epsilon;}
static void equal(Rect r,float x,float y,float w,float h) {
    assert(close(r.x,x)&&close(r.y,y)&&close(r.w,w)&&close(r.h,h));
}
static void equal(DrawableRect r,int x,int y,int w,int h) {
    assert(r.x==x&&r.y==y&&r.w==w&&r.h==h);
}
static void shortcuts() {
    struct Binding {Key key;Command command;bool ctrl=false,shift=false;};
    const Binding bindings[]={
        {Key::Q,Command::Select},{Key::W,Command::Move},{Key::E,Command::Yaw},
        {Key::R,Command::Scale},{Key::F,Command::Frame},
        {Key::End,Command::SnapTerrain},
        {Key::Delete,Command::Delete},{Key::Backspace,Command::Delete},
        {Key::P,Command::Palette,true},{Key::D,Command::Duplicate,true},
        {Key::C,Command::Copy,true},{Key::V,Command::Paste,true},
        {Key::S,Command::Save,true},{Key::S,Command::ApplySave,true,true},
        {Key::Z,Command::Undo,true},{Key::Z,Command::Redo,true,true},
        {Key::Y,Command::Redo,true}
    };
    for(const Binding &binding:bindings) {
        ShortcutContext context;context.ctrl=binding.ctrl;context.shift=binding.shift;
        Command command=Command::Frame;
        assert(shortcut(binding.key,context,command)&&command==binding.command);
        for(bool ShortcutContext::*guard:{&ShortcutContext::text_input,&ShortcutContext::active_widget,
                &ShortcutContext::blocked,&ShortcutContext::navigating,&ShortcutContext::alt,
                &ShortcutContext::super}) {
            ShortcutContext guarded=context;guarded.*guard=true;command=Command::Scale;
            assert(!shortcut(binding.key,guarded,command)&&command==Command::Scale);
        }
    }
    // RMB navigation's W/E/Q movement must never switch manipulation tools.
    ShortcutContext flying;flying.navigating=true;
    for(const Key key:{Key::W,Key::E,Key::Q}) {
        Command command=Command::Frame;
        assert(!shortcut(key,flying,command)&&command==Command::Frame);
    }
    for(const Key key:{Key::None,Key::Escape,Key::P,Key::D,Key::C,Key::V,Key::S,Key::Z,Key::Y}) {
        Command command=Command::Frame;
        assert(!shortcut(key,{},command)&&command==Command::Frame);
    }
    for(const Key key:{Key::Q,Key::W,Key::E,Key::R,Key::F,Key::End,Key::Delete,Key::Backspace,Key::Escape}) {
        ShortcutContext modified;modified.ctrl=true;Command command=Command::Frame;
        assert(!shortcut(key,modified,command)&&command==Command::Frame);
        modified.ctrl=false;modified.shift=true;
        assert(!shortcut(key,modified,command)&&command==Command::Frame);
    }
    for(const Key key:{Key::P,Key::D,Key::C,Key::V,Key::Y}) {
        ShortcutContext modified;modified.ctrl=modified.shift=true;Command command=Command::Frame;
        assert(!shortcut(key,modified,command)&&command==Command::Frame);
    }
}
static void cameras() {
    struct Camera {
        std::array<double,3> position{};
        float yaw=0,pitch=0,fov=0,vertical_velocity=0;
        bool fly=false;
        int mode=0;
        std::array<int,3> extra{};
        bool operator==(const Camera &b) const {
            return position==b.position&&yaw==b.yaw&&pitch==b.pitch&&fov==b.fov&&
                vertical_velocity==b.vertical_velocity&&fly==b.fly&&mode==b.mode&&extra==b.extra;
        }
    };
    const Camera playerA{{100,20,-300},.5f,-.2f,70,3,false,2,{11,12,13}};
    const Camera editorB{{900,120,600},1.5f,-.6f,90,0,true,5,{21,22,23}};
    const Camera playerC{{110,22,-320},.8f,-.1f,65,-2,false,3,{31,32,33}};
    const Camera editorD{{-100,200,500},2.5f,.2f,80,0,true,6,{41,42,43}};
    Camera current=playerA;CameraOwnership<Camera> ownership;
    assert(!ownership.sync(false,current)&&current==playerA);
    assert(!ownership.active&&!ownership.initialized&&ownership.editor==Camera{}&&ownership.player==Camera{});
    assert(ownership.sync(true,current)&&current==playerA&&ownership.player==playerA&&ownership.editor==playerA);
    assert(ownership.active&&ownership.initialized);
    current=editorB;
    assert(!ownership.sync(true,current)&&current==editorB&&ownership.player==playerA&&ownership.editor==playerA);
    assert(ownership.sync(false,current)&&current==playerA&&ownership.editor==editorB&&!ownership.active);
    current=playerC;
    assert(!ownership.sync(false,current)&&current==playerC&&ownership.player==playerA&&ownership.editor==editorB);
    assert(ownership.sync(true,current)&&current==editorB&&ownership.player==playerC&&ownership.active);
    current=editorD;
    assert(!ownership.sync(true,current)&&current==editorD&&ownership.player==playerC&&ownership.editor==editorB);
    assert(ownership.sync(false,current)&&current==playerC&&ownership.editor==editorD&&!ownership.active);
}
int main() {
    shortcuts();cameras();
    const Layout layout;
    equal(compute(1920,1080,layout,false),0,0,1920,1080);
    equal(compute(1920,1080,layout,true),260,72,1320,760);
    equal(compute(1280,720,layout,true),260,72,680,400);
    equal(compute(320,240,layout,true),0,0,320,240);
    equal(compute(319,239,layout,true),0,0,319,239);
    equal(compute(1,1,layout,true),0,0,1,1);
    const Rect small=compute(640,480,layout,true);
    assert(close(small.w,320)&&close(small.h,240));
    assert(close(small.x/(640-small.x-small.w),layout.left/layout.right));
    assert(close(small.y/(480-small.y-small.h),layout.top/(layout.bottom+layout.status)));
    for(unsigned i=1;i<=2000;++i) {
        const float w=float((i*7919u)%4096u)+.25f,h=float((i*3571u)%2160u)+.5f;
        const Rect r=compute(w,h,layout,true);
        assert(valid(r)&&r.x>=0&&r.y>=0);
        assert(r.w>=std::min(w,320.0f)&&r.h>=std::min(h,240.0f));
        assert(r.x+r.w<=w+.001f&&r.y+r.h<=h+.001f);
    }
    const float nan=std::numeric_limits<float>::quiet_NaN();
    const float inf=std::numeric_limits<float>::infinity();
    for(const float bad:{0.0f,-1.0f,nan,inf,-inf}) {
        equal(compute(bad,720,layout,true),0,0,0,0);
        equal(compute(1280,bad,layout,false),0,0,0,0);
    }
    Layout malformed;malformed.top=nan;malformed.left=-20;malformed.right=inf;
    malformed.bottom=-inf;malformed.status=-1;
    equal(compute(1280,720,malformed,true),0,0,1280,720);
    Layout huge;huge.left=huge.right=huge.top=huge.bottom=huge.status=std::numeric_limits<float>::max();
    const Rect bounded=compute(1280,720,huge,true);
    assert(valid(bounded)&&close(bounded.w,320)&&close(bounded.h,240));

    const Rect view=compute(1280,720,layout,true);
    assert(view.contains(view.x,view.y));
    assert(view.contains(view.x+view.w-.01f,view.y+view.h-.01f));
    assert(!view.contains(view.x+view.w,view.y)&&!view.contains(view.x,view.y+view.h));
    assert(!view.contains(view.x-.01f,view.y)&&!view.contains(nan,view.y));
    assert((!Rect{}.contains(0,0)&&!Rect{0,0,inf,10}.contains(0,0)));
    float nx=7,ny=7;
    assert(ndc(view,view.x,view.y,nx,ny)&&nx==-1&&ny==1);
    assert(ndc(view,view.x+view.w,view.y+view.h,nx,ny)&&nx==1&&ny==-1);
    assert(ndc(view,view.x+view.w*.5f,view.y+view.h*.5f,nx,ny)&&nx==0&&ny==0);
    assert(ndc(view,view.x-view.w*.5f,view.y,nx,ny)&&nx==-2&&ny==1);
    assert(!ndc(view,nan,0,nx,ny)&&nx==0&&ny==0);
    assert(!ndc({},0,0,nx,ny)&&nx==0&&ny==0);

    // Projection/input round trips at offsets and several aspect ratios ensure
    // a side panel or resize cannot silently change the picking ray's origin.
    for(const Rect r:{view,Rect{13,41,400,800},Rect{101,73,1920,1080}}) {
        for(const float depth:{.1f,2.0f,100.0f}) {
            for(const float x:{-1.25f,-1.0f,-.2f,0.0f,.7f,1.0f,1.25f}) {
                for(const float y:{-1.0f,0.0f,.4f,1.0f}) {
                    const float tanHalf=.65f,right=x*depth*tanHalf*aspect(r),up=y*depth*tanHalf;
                    float sx=0,sy=0;
                    assert(project(r,depth,right,up,tanHalf,sx,sy));
                    assert(ndc(r,sx,sy,nx,ny)&&close(nx,x,.00001f)&&close(ny,y,.00001f));
                }
            }
        }
    }
    float sx=8,sy=8;
    for(const float bad:{0.0f,-1.0f,nan,inf}) {
        assert(!project(view,bad,0,0,.5f,sx,sy)&&sx==0&&sy==0);
        assert(!project(view,1,0,0,bad,sx,sy)&&sx==0&&sy==0);
    }
    assert(!project(view,1,nan,0,.5f,sx,sy)&&sx==0&&sy==0);
    assert(!project(view,1,0,inf,.5f,sx,sy)&&sx==0&&sy==0);
    assert(!project({},1,0,0,.5f,sx,sy)&&sx==0&&sy==0);
    assert(aspect({})==1&&std::isfinite(aspect({0,0,std::numeric_limits<float>::max(),
        std::numeric_limits<float>::min()})));

    equal(drawable_rect(view,1280,720,1280,720),260,248,680,400);
    equal(drawable_rect(view,1280,720,2560,1440),520,496,1360,800);
    equal(drawable_rect({0,0,1280,720},1280,720,1920,1080),0,0,1920,1080);
    equal(drawable_rect({-20,-10,1320,740},1280,720,1920,1080),0,0,1920,1080);
    equal(drawable_rect({2000,800,10,10},1280,720,1920,1080),1920,0,0,0);
    equal(drawable_rect(view,0,720,1920,1080),0,0,0,0);
    equal(drawable_rect(view,1280,inf,1920,1080),0,0,0,0);
    equal(drawable_rect(view,1280,720,-1,1080),0,0,0,0);
    equal(drawable_rect({},1280,720,1920,1080),0,0,0,0);
    for(const int drawableW:{640,1023,1537,2560}) {
        // Adjacent logical regions share exactly the same rounded physical edge.
        const auto a=drawable_rect({0,0,260,720},1280,720,drawableW,1081);
        const auto b=drawable_rect(view,1280,720,drawableW,1081);
        const auto c=drawable_rect({940,0,340,720},1280,720,drawableW,1081);
        assert(a.x+a.w==b.x&&b.x+b.w==c.x&&c.x+c.w==drawableW);
        const auto above=drawable_rect({260,0,680,72},1280,720,drawableW,1081);
        const auto below=drawable_rect({260,472,680,248},1280,720,drawableW,1081);
        assert(below.y+below.h==b.y&&b.y+b.h==above.y&&above.y+above.h==1081);
    }
    std::puts("PASS designer viewport: adaptive layout, projection/DPI edges, guarded shortcuts and complete camera ownership");
}
