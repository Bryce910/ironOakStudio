// Production HDR resolve: focus is opt-in, foreground stays sharp and the
// background filter cannot bleed an in-focus silhouette into distant scenery.
#define main original_post_main
#include "world_post_gl_test.cpp"
#undef main
#include "engine/world_milestone.h"

int main(){
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL"))SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
    using namespace world_milestone;
    Review review;review.view=View::Hero;review.zoom(1000);assert(review.distance==1.45f);review.zoom(-1000);assert(review.distance==3.2f);
    review.distance=1.95f;review.drag(100000,10000);assert(review.pitch==-.25f);
    auto pose=portrait(-1348,24,-10281,0,review);
    const double horizontal=std::hypot(pose.x+1348,pose.z+10281);
    assert(std::abs(horizontal-review.distance)<1e-6&&std::abs(pose.focus-std::hypot(horizontal,pose.y-25.48))<1e-5);
    // A ray through the lens centre intersects the intended upper-body target.
    assert(std::abs(std::sin(pose.yaw)*horizontal+pose.x+1348)<1e-6);
    assert(std::abs(-std::cos(pose.yaw)*horizontal+pose.z+10281)<1e-6);
    assert(std::abs(std::tan(pose.pitch)*horizontal+pose.y-25.48)<1e-6);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    auto *window=SDL_CreateWindow("Portrait resolve checks",0,0,side,side,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);
    auto context=SDL_GL_CreateContext(window);assert(context);std::printf("Portrait renderer: %s\n",glGetString(GL_RENDERER));
    {
        Fixture f;std::vector<Pixel> input(side*side);std::vector<float> depth(side*side);
        for(int y=0;y<side;y++)for(int x=0;x<side;x++){
            const bool near=x<side/2;const float v=x%2?.8f:.06f;
            input[y*side+x]=near?Pixel{v,.01f,.01f,1}:Pixel{0,v,0,1};
            const float z=near?2.f:25.f;depth[y*side+x]=(15000.f-.4f*15000.f/z)/(15000.f-.4f);
        }
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,f.scene);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,side,side,0,GL_RGBA,GL_FLOAT,input.data());
        glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,f.depth);glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,side,side,0,GL_RED,GL_FLOAT,depth.data());
        auto draw=[&](GLuint p,float focus){
            glUseProgram(p);
            glUniform1i(glGetUniformLocation(p,"uScene"),0);glUniform1i(glGetUniformLocation(p,"uDepth"),1);glUniform1i(glGetUniformLocation(p,"uVillageGrade"),2);glUniform1i(glGetUniformLocation(p,"uBloom"),3);
            glUniform1f(glGetUniformLocation(p,"uExposure"),1);glUniform1f(glGetUniformLocation(p,"uNear"),.4f);glUniform1f(glGetUniformLocation(p,"uFar"),15000);
            glUniform2f(glGetUniformLocation(p,"uResolution"),side,side);glUniform1i(glGetUniformLocation(p,"uTemporalActive"),1);glUniform1f(glGetUniformLocation(p,"uDetailSharpness"),.2f);glUniform1f(glGetUniformLocation(p,"uPortraitFocus"),focus);
            glDrawArrays(GL_TRIANGLES,0,3);std::vector<Pixel> result(side*side);glReadPixels(0,0,side,side,GL_RGBA,GL_FLOAT,result.data());return result;
        };
        GLuint v=shader(GL_VERTEX_SHADER,source("shaders/world_post.vs.glsl"));GLuint frag=shader(GL_FRAGMENT_SHADER,source("tests/reference/world_post_before_milestone.fs.glsl"));GLuint old=glCreateProgram();glAttachShader(old,v);glAttachShader(old,frag);glLinkProgram(old);GLint linked=0;glGetProgramiv(old,GL_LINK_STATUS,&linked);assert(linked);
        auto reference=draw(old,0),off=draw(f.shader_program,0),on=draw(f.shader_program,2);
        float delta=0,foreground=0,oldContrast=0,newContrast=0,backgroundRed=0;
        for(int y=0;y<side;y++)for(int x=side/2+1;x<side;x++)backgroundRed=std::max(backgroundRed,off[y*side+x][0]);
        for(int y=3;y<side-3;y++)for(int x=3;x<side-3;x++){
            const int i=y*side+x;for(int c=0;c<4;c++){assert(std::isfinite(on[i][c]));delta=std::max(delta,std::abs(reference[i][c]-off[i][c]));if(x<side/2-1)foreground=std::max(foreground,std::abs(on[i][c]-off[i][c]));}
            if(x>side/2+1){assert(on[i][0]<=backgroundRed+1e-5f);oldContrast+=std::abs(off[i][1]-off[i-1][1]);newContrast+=std::abs(on[i][1]-on[i-1][1]);}
        }
        assert(delta<1e-6&&foreground<1e-6&&newContrast<oldContrast*.5f);
        std::vector<float> after(depth.size());glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,f.depth);glGetTexImage(GL_TEXTURE_2D,0,GL_RED,GL_FLOAT,after.data());assert(depth==after);assert(glGetError()==GL_NO_ERROR);
        std::printf("PASS legacy max delta %.9f, sharp subject delta %.9f, background contrast %.4f -> %.4f, depth unchanged\n",delta,foreground,oldContrast,newContrast);
        glDeleteProgram(old);glDeleteShader(v);glDeleteShader(frag);
    }
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
