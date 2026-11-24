#ifndef VALWOOD_WORLD_DESIGNER_VIEWPORT_H
#define VALWOOD_WORLD_DESIGNER_VIEWPORT_H

#include <algorithm>
#include <cmath>
#include <limits>

// All editor and input coordinates are logical pixels with a top-left origin.
// Only drawable_rect() converts to drawable pixels with a bottom-left origin.
namespace world_designer_viewport {
enum class Command {Select,Move,Yaw,Scale,Frame,Copy,Paste,Duplicate,Delete,Palette,Save,Undo,Redo,ApplySave,SnapTerrain};
enum class Key {None,Q,W,E,R,F,P,C,V,D,S,Z,Y,Delete,Backspace,Escape,End};
struct ShortcutContext {
    bool text_input=false,active_widget=false,blocked=false,navigating=false;
    bool ctrl=false,shift=false,alt=false,super=false;
};

// Resolve only editor shortcuts after the caller has established editor input
// ownership. Active text/widgets and camera navigation keep their own keys.
// On false the output remains untouched. Escape belongs to drag/cancel handling.
inline bool shortcut(Key key,const ShortcutContext &context,Command &command) {
    if(context.text_input||context.active_widget||context.blocked||context.navigating||
       context.alt||context.super)return false;
    if(context.ctrl) {
        switch(key) {
        case Key::S:command=context.shift?Command::ApplySave:Command::Save;return true;
        case Key::Z:command=context.shift?Command::Redo:Command::Undo;return true;
        case Key::P:if(!context.shift){command=Command::Palette;return true;}return false;
        case Key::D:if(!context.shift){command=Command::Duplicate;return true;}return false;
        case Key::C:if(!context.shift){command=Command::Copy;return true;}return false;
        case Key::V:if(!context.shift){command=Command::Paste;return true;}return false;
        case Key::Y:if(!context.shift){command=Command::Redo;return true;}return false;
        default:return false;
        }
    }
    if(context.shift)return false;
    switch(key) {
    case Key::Q:command=Command::Select;return true;
    case Key::W:command=Command::Move;return true;
    case Key::E:command=Command::Yaw;return true;
    case Key::R:command=Command::Scale;return true;
    case Key::F:command=Command::Frame;return true;
    case Key::End:command=Command::SnapTerrain;return true;
    case Key::Delete:case Key::Backspace:command=Command::Delete;return true;
    default:return false;
    }
}

// Camera is copied as a whole: this helper assumes no position, velocity, mode,
// or projection fields. Camera-mode adjustments belong to the entering caller.
template<class Camera> struct CameraOwnership {
    Camera editor{},player{};
    bool active=false,initialized=false;
    bool sync(bool editor_open,Camera &current) {
        if(editor_open==active)return false;
        if(editor_open) {
            player=current;
            if(initialized)current=editor;
            else {editor=current;initialized=true;}
            active=true;
        } else {
            editor=current;current=player;active=false;
        }
        return true;
    }
};

struct Rect {
    float x=0,y=0,w=0,h=0;
    // Half-open edges keep adjacent panels from also claiming viewport input.
    bool contains(float px,float py) const {
        return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(w)&&std::isfinite(h)&&
            std::isfinite(px)&&std::isfinite(py)&&w>0&&h>0&&px>=x&&py>=y&&
            double(px)<double(x)+w&&double(py)<double(y)+h;
    }
};
struct Layout {float top=72,left=260,right=340,bottom=224,status=24;};
struct DrawableRect {int x=0,y=0,w=0,h=0;};

inline bool valid(const Rect &r) {
    return std::isfinite(r.x)&&std::isfinite(r.y)&&std::isfinite(r.w)&&
        std::isfinite(r.h)&&r.w>0&&r.h>0;
}
inline double panel_size(float value) {
    return std::isfinite(value)&&value>0?double(value):0.0;
}

// Shrink opposing panels proportionally before sacrificing the minimum usable
// viewport. On physically smaller windows the viewport uses the available size.
inline Rect compute(float displayW,float displayH,const Layout &layout,bool open) {
    if(!std::isfinite(displayW)||!std::isfinite(displayH)||displayW<=0||displayH<=0)
        return {};
    if(!open)return {0,0,displayW,displayH};
    const double width=displayW,height=displayH;
    const double minimumW=std::min(320.0,width),minimumH=std::min(240.0,height);
    double left=panel_size(layout.left),right=panel_size(layout.right);
    double top=panel_size(layout.top),bottom=panel_size(layout.bottom)+panel_size(layout.status);
    const double horizontal=left+right,vertical=top+bottom;
    if(horizontal>width-minimumW) {
        const double scale=(width-minimumW)/horizontal;
        left*=scale;right*=scale;
    }
    if(vertical>height-minimumH) {
        const double scale=(height-minimumH)/vertical;
        top*=scale;bottom*=scale;
    }
    return {float(left),float(top),float(std::max(minimumW,width-left-right)),
        float(std::max(minimumH,height-top-bottom))};
}

inline float aspect(const Rect &r) {
    if(!valid(r))return 1.0f;
    return float(std::max(double(std::numeric_limits<float>::min()),
        std::min(double(std::numeric_limits<float>::max()),double(r.w)/r.h)));
}

// Coordinates outside the rectangle deliberately produce NDC outside [-1,1],
// allowing an owned drag to continue beyond the viewport. Use contains() when
// deciding whether a new click belongs to the viewport.
inline bool ndc(const Rect &r,float screenX,float screenY,float &ndcX,float &ndcY) {
    ndcX=ndcY=0;
    if(!valid(r)||!std::isfinite(screenX)||!std::isfinite(screenY))return false;
    const double nx=2.0*(double(screenX)-r.x)/r.w-1.0;
    const double ny=1.0-2.0*(double(screenY)-r.y)/r.h;
    if(std::abs(nx)>std::numeric_limits<float>::max()||
       std::abs(ny)>std::numeric_limits<float>::max())return false;
    ndcX=float(nx);ndcY=float(ny);return true;
}

// depth/right/up are coordinates along the camera's forward/right/up axes.
// tanHalf is tan(vertical field of view / 2); no implicit horizontal-FOV change
// occurs when side panels open. Off-screen points remain projectable for clips.
inline bool project(const Rect &r,float depth,float right,float up,float tanHalf,
                    float &screenX,float &screenY) {
    screenX=screenY=0;
    if(!valid(r)||!std::isfinite(depth)||!std::isfinite(right)||!std::isfinite(up)||
       !std::isfinite(tanHalf)||depth<=0||tanHalf<=0)return false;
    const double vertical=double(depth)*tanHalf;
    const double nx=double(right)*r.h/(vertical*r.w),ny=double(up)/vertical;
    const double sx=double(r.x)+(nx+1.0)*r.w*.5;
    const double sy=double(r.y)+(1.0-ny)*r.h*.5;
    if(!std::isfinite(sx)||!std::isfinite(sy)||
       std::abs(sx)>std::numeric_limits<float>::max()||
       std::abs(sy)>std::numeric_limits<float>::max())return false;
    screenX=float(sx);screenY=float(sy);return true;
}

// Round shared edges instead of independently rounding width/height, so resize
// and non-integral DPI changes cannot introduce a one-pixel gap between regions.
// Clamp to the drawable: this is safe to pass directly to GL viewport/scissor.
inline DrawableRect drawable_rect(const Rect &r,float logicalW,float logicalH,
                                  int drawableW,int drawableH) {
    if(!valid(r)||!std::isfinite(logicalW)||!std::isfinite(logicalH)||
       logicalW<=0||logicalH<=0||drawableW<=0||drawableH<=0)return {};
    const double left=std::max(0.0,std::min(double(logicalW),double(r.x)));
    const double right=std::max(left,std::min(double(logicalW),double(r.x)+r.w));
    const double top=std::max(0.0,std::min(double(logicalH),double(r.y)));
    const double bottom=std::max(top,std::min(double(logicalH),double(r.y)+r.h));
    const double sx=double(drawableW)/logicalW,sy=double(drawableH)/logicalH;
    const int x0=int(std::floor(left*sx+.5)),x1=int(std::floor(right*sx+.5));
    const int y0=int(std::floor((double(logicalH)-bottom)*sy+.5));
    const int y1=int(std::floor((double(logicalH)-top)*sy+.5));
    return {x0,y0,x1-x0,y1-y0};
}
}
#endif
