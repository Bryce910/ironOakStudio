typedef enum{ MPAGE_MAIN, MPAGE_LOAD, MPAGE_OPTIONS, MPAGE_CHARSELECT, MPAGE_CREDITS }MenuPage;

static GLuint g_menu_bg_tex = 0;
static bool   g_menu_bg_tried = false;
#define MENU_ANIM_MAX_FRAMES 128
static GLuint g_menu_anim_tex[MENU_ANIM_MAX_FRAMES] = {0};
static int    g_menu_anim_count = 0;
static bool   g_menu_anim_tried = false;
static float  g_menu_anim_fps = 12.0f;
static GLuint g_studio_logo_tex = 0;
static GLuint g_studio_logo_prog = 0;
static GLuint g_studio_logo_vao = 0;
static GLuint g_studio_logo_vbo = 0;
static int    g_studio_logo_w = 0;
static int    g_studio_logo_h = 0;
static bool   g_studio_logo_tried = false;

static void studio_logo_load(void){
    if(g_studio_logo_tried) return;
    g_studio_logo_tried=true;
    {
        int w=0,h=0,ch=0;
        uint8_t *px=asset_stbi_load("assets/iron_oak_games_logo.png",&w,&h,&ch,4);
        if(!px){
            printf("[Menu] Failed to load assets/iron_oak_games_logo.png\n");
            return;
        }
        glGenTextures(1,&g_studio_logo_tex);
        glBindTexture(GL_TEXTURE_2D,g_studio_logo_tex);
        glTexImage2D(GL_TEXTURE_2D,0,GL_SRGB8_ALPHA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,px);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        stbi_image_free(px);
        g_studio_logo_w=w;
        g_studio_logo_h=h;
        printf("[Menu] Loaded studio logo %dx%d\n",w,h);
    }
    g_studio_logo_prog=pfx_make_shader(
        "#version 330 core\n"
        "layout(location=0)in vec2 aPos;\n"
        "layout(location=1)in vec2 aUV;\n"
        "out vec2 vUV;\n"
        "void main(){gl_Position=vec4(aPos,0.0,1.0);vUV=aUV;}\n",
        "#version 330 core\n"
        "in vec2 vUV;\n"
        "uniform sampler2D uTex;\n"
        "uniform float uAlpha;\n"
        "out vec4 F;\n"
        "void main(){vec4 c=texture(uTex,vUV);F=vec4(c.rgb,c.a*uAlpha);}\n");
    glGenVertexArrays(1,&g_studio_logo_vao);
    glGenBuffers(1,&g_studio_logo_vbo);
    glBindVertexArray(g_studio_logo_vao);
    glBindBuffer(GL_ARRAY_BUFFER,g_studio_logo_vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(float)*24,NULL,GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(float)*4,(void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(float)*4,(void*)(sizeof(float)*2));
    glBindBuffer(GL_ARRAY_BUFFER,0);
    glBindVertexArray(0);
}

static void studio_logo_draw(float W,float H,float alpha){
    studio_logo_load();
    if(!g_studio_logo_tex||!g_studio_logo_prog||g_studio_logo_w<=0||g_studio_logo_h<=0) return;
    {
        float max_w=W*1.12f;
        float max_h=H*0.68f;
        float scale=fminf(max_w/(float)g_studio_logo_w,max_h/(float)g_studio_logo_h);
        float rw=(float)g_studio_logo_w*scale;
        float rh=(float)g_studio_logo_h*scale;
        float x=(W-rw)*0.5f;
        float y=(H-rh)*0.5f;
        float l=(x/W)*2.0f-1.0f;
        float r=((x+rw)/W)*2.0f-1.0f;
        float t=1.0f-(y/H)*2.0f;
        float b=1.0f-((y+rh)/H)*2.0f;
        float vtx[24]={
            l,b, 0.0f,1.0f,
            r,b, 1.0f,1.0f,
            r,t, 1.0f,0.0f,
            l,b, 0.0f,1.0f,
            r,t, 1.0f,0.0f,
            l,t, 0.0f,0.0f
        };
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(g_studio_logo_prog);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D,g_studio_logo_tex);
        glUniform1i(glGetUniformLocation(g_studio_logo_prog,"uTex"),0);
        glUniform1f(glGetUniformLocation(g_studio_logo_prog,"uAlpha"),clampf(alpha,0.0f,1.0f));
        glBindVertexArray(g_studio_logo_vao);
        glBindBuffer(GL_ARRAY_BUFFER,g_studio_logo_vbo);
        glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(vtx),vtx);
        glDrawArrays(GL_TRIANGLES,0,6);
        glBindBuffer(GL_ARRAY_BUFFER,0);
        glBindVertexArray(0);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    }
}

static void menu_bg_load(void){
    if(g_menu_bg_tried) return;
    g_menu_bg_tried=true;
    int w,h,ch;
    uint8_t *px=asset_stbi_load("assets/menu_bg.jpg",&w,&h,&ch,3);
    if(!px) px=asset_stbi_load("assets/menu_bg.png",&w,&h,&ch,3);
    if(!px){ printf("[Menu] No assets/menu_bg.jpg found - using gradient\n"); return; }
    glGenTextures(1,&g_menu_bg_tex);
    glBindTexture(GL_TEXTURE_2D,g_menu_bg_tex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB8,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,px);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    stbi_image_free(px);
    printf("[Menu] Loaded menu background %dx%d\n",w,h);
}

static GLuint menu_tex_from_file(const char *path){
    int w=0,h=0,ch=0;
    uint8_t *px;
    GLuint tex=0;
    if(!path||!path[0]) return 0;
    px=asset_stbi_load(path,&w,&h,&ch,3);
    if(!px) return 0;
    glGenTextures(1,&tex);
    glBindTexture(GL_TEXTURE_2D,tex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB8,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,px);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    stbi_image_free(px);
    return tex;
}

static bool menu_anim_try_pattern(const char *fmt,int start_index){
    int count=0;
    for(int i=0;i<MENU_ANIM_MAX_FRAMES;i++){
        char path[256];
        GLuint tex;
        snprintf(path,sizeof(path),fmt,i+start_index);
        tex=menu_tex_from_file(path);
        if(!tex){
            if(i==0) return false;
            break;
        }
        g_menu_anim_tex[count++]=tex;
    }
    if(count>0){
        g_menu_anim_count=count;
        printf("[Menu] Loaded animated menu loop: %d frames via %s\n",count,fmt);
        return true;
    }
    return false;
}

static void menu_anim_load(void){
    if(g_menu_anim_tried) return;
    g_menu_anim_tried=true;
    {
        static const char *patterns[]={
            "assets/menu_anim/frame_%03d.png",
            "assets/menu_anim/frame_%02d.png",
            "assets/menu_anim/menu_%03d.png",
            "assets/menu_anim/menu_%02d.png",
            "assets/menu_loop/frame_%03d.png",
            "assets/menu_loop/frame_%02d.png",
            NULL
        };
        for(int i=0;patterns[i];i++){
            if(menu_anim_try_pattern(patterns[i],0)) return;
            if(menu_anim_try_pattern(patterns[i],1)) return;
        }
    }
    printf("[Menu] No animated menu loop found, using static background fallback\n");
}

static bool menu_anim_draw(void){
    menu_anim_load();
    if(g_menu_anim_count<=0||!g_splash_ready) return false;
    {
        int frame=(int)(((float)SDL_GetTicks()*0.001f)*g_menu_anim_fps)%g_menu_anim_count;
        glDisable(GL_DEPTH_TEST);
        glUseProgram(g_splash.prog);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D,g_menu_anim_tex[frame]);
        glUniform1i(glGetUniformLocation(g_splash.prog,"uTex"),0);
        glUniform1f(glGetUniformLocation(g_splash.prog,"uFade"),1.0f);
        glBindVertexArray(g_splash.vao);
        glDrawArrays(GL_TRIANGLES,0,6);
        glBindVertexArray(0);
        glEnable(GL_DEPTH_TEST);
    }
    return true;
}

static void menu_bg(HUD *h,float W,float H,bool show_branding){
    menu_bg_load();
    if(g_menu_bg_tex){
        if(g_splash_ready){
            glDisable(GL_DEPTH_TEST);
            glUseProgram(g_splash.prog);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D,g_menu_bg_tex);
            glUniform1i(glGetUniformLocation(g_splash.prog,"uTex"),0);
            glUniform1f(glGetUniformLocation(g_splash.prog,"uFade"),1.0f);
            glBindVertexArray(g_splash.vao);
            glDrawArrays(GL_TRIANGLES,0,6);
            glBindVertexArray(0);
            glEnable(GL_DEPTH_TEST);
        }
        hud_rect(h,0,0,W,H*0.30f, 0.0f,0.0f,0.0f,0.55f);
        hud_rect(h,0,H*0.70f,W,H*0.30f, 0.0f,0.0f,0.0f,0.50f);
    } else {
        hud_rect(h,0,0,W,H,          0.04f,0.03f,0.06f,1.0f);
        hud_rect(h,0,H*0.5f,W,H*0.5f,0.07f,0.04f,0.02f,0.7f);
    }
    hud_rect(h,0,H-6,W,6, 0.7f,0.5f,0.1f,1.0f);
    hud_rect(h,0,0,W,6,   0.7f,0.5f,0.1f,1.0f);
    if(show_branding){
        float tsc=5.0f;
        const char *title="LORDS OF MASHITO";
        float tw=menu_text_width(tsc,title);
        menu_text(h,(W-tw)*0.5f+3,H*0.08f+3,tsc,title, 0.0f,0.0f,0.0f,0.7f);
        menu_text(h,(W-tw)*0.5f,H*0.08f,tsc,title, 1.0f,0.80f,0.2f,1.0f);
        const char *sub="An Epic RPG Adventure";
        float ssc=1.6f,stw=menu_text_width(ssc,sub);
        menu_text(h,(W-stw)*0.5f,H*0.19f,ssc,sub, 0.9f,0.8f,0.55f,0.9f);
    }
}

static void menu_bg_plain(HUD *h,float W,float H){
    menu_bg_load();
    if(g_menu_bg_tex){
        if(g_splash_ready){
            glDisable(GL_DEPTH_TEST);
            glUseProgram(g_splash.prog);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D,g_menu_bg_tex);
            glUniform1i(glGetUniformLocation(g_splash.prog,"uTex"),0);
            glUniform1f(glGetUniformLocation(g_splash.prog,"uFade"),1.0f);
            glBindVertexArray(g_splash.vao);
            glDrawArrays(GL_TRIANGLES,0,6);
            glBindVertexArray(0);
            glEnable(GL_DEPTH_TEST);
        }
    } else {
        hud_rect(h,0,0,W,H,          0.04f,0.03f,0.06f,1.0f);
        hud_rect(h,0,H*0.5f,W,H*0.5f,0.07f,0.04f,0.02f,0.7f);
    }
    hud_rect(h,0,H-6,W,6, 0.7f,0.5f,0.1f,1.0f);
    hud_rect(h,0,0,W,6,   0.7f,0.5f,0.1f,1.0f);
}

static void menu_main_overlay(HUD *h,float W,float H){
    hud_rect(h,0,0,W,H*0.18f, 0.0f,0.0f,0.0f,0.16f);
    hud_rect(h,0,H*0.72f,W,H*0.28f, 0.0f,0.0f,0.0f,0.20f);
    hud_rect(h,0,H-6,W,6, 0.7f,0.5f,0.1f,1.0f);
    hud_rect(h,0,0,W,6,   0.7f,0.5f,0.1f,1.0f);
}

static void studio_splash_draw(HUD *h,float W,float H,float alpha){
    float a=clampf(alpha,0.0f,1.0f);

    hud_rect(h,0,0,W,H, 0.0f,0.0f,0.0f,1.0f);
    hud_flush(h);
    h->vcount=0;
    studio_logo_draw(W,H,a);
    if(!g_studio_logo_tex){
        const char *studio="IRON OAK GAMES";
        const char *tag="A small studio forging dark worlds";
        const char *hint="PRESS ANY KEY";
        float title_sc=3.6f;
        float tag_sc=1.0f;
        float hint_sc=0.95f;
        float tw=menu_text_width(title_sc,studio);
        float gw=menu_text_width(tag_sc,tag);
        float hw=menu_text_width(hint_sc,hint);
        float cx=W*0.5f;
        float cy=H*0.47f;

        hud_rect(h,cx-94.0f,cy-122.0f,188.0f,2.0f, 0.92f,0.92f,0.92f,0.70f*a);
        hud_rect(h,cx-58.0f,cy-112.0f,116.0f,116.0f, 0.0f,0.0f,0.0f,0.0f);
        hud_rect(h,cx-30.0f,cy-92.0f,60.0f,60.0f, 0.96f,0.96f,0.96f,0.10f*a);
        hud_rect(h,cx-8.0f,cy-102.0f,16.0f,72.0f, 0.92f,0.92f,0.92f,0.88f*a);
        hud_rect(h,cx-34.0f,cy-66.0f,68.0f,12.0f, 0.92f,0.92f,0.92f,0.88f*a);
        hud_rect(h,cx-44.0f,cy-42.0f,88.0f,14.0f, 0.92f,0.92f,0.92f,0.88f*a);
        hud_rect(h,cx-16.0f,cy-118.0f,32.0f,14.0f, 0.92f,0.92f,0.92f,0.88f*a);
        hud_rect(h,cx-74.0f,cy-10.0f,148.0f,2.0f, 0.92f,0.92f,0.92f,0.40f*a);

        menu_text(h,cx-tw*0.5f+2.0f,cy+18.0f+2.0f,title_sc,studio, 0.0f,0.0f,0.0f,0.55f*a);
        menu_text(h,cx-tw*0.5f,cy+18.0f,title_sc,studio, 0.96f,0.96f,0.96f,1.0f*a);
        menu_text(h,cx-gw*0.5f,cy+54.0f,tag_sc,tag, 0.70f,0.70f,0.70f,0.78f*a);
        menu_text(h,cx-hw*0.5f,cy+108.0f,hint_sc,hint, 0.62f,0.62f,0.62f,0.72f*a);
    }
}

static int menu_slider(HUD *h,float x,float y,float w,const char *label,
                        int val,int mx,int my,bool clicked,bool held,
                        int slider_id,int *active_slider,int *drag_val){
    float lsc=1.4f,ltw=lsc*8.0f*(float)strlen(label);
    hud_str(h,x,y,lsc,label, 0.9f,0.85f,0.6f,1.0f);
    float tx=x+ltw+12,tw2=w-ltw-12,th=18;
    hud_rect(h,tx,y+4,tw2,th-8, 0.12f,0.10f,0.07f,1.0f);
    {
        float frac=val/100.0f;
        hud_rect(h,tx,y+4,tw2*frac,th-8, 0.7f,0.5f,0.1f,0.9f);
        {
            float tx2=tx+tw2*frac-6;
            bool over=(mx>=tx&&mx<=tx+tw2&&my>=y&&my<=y+th);
            if(clicked&&over&&active_slider) *active_slider=slider_id;
            hud_rect(h,tx2,y,12,th, over?1.0f:0.85f,over?0.9f:0.75f,0.3f,1.0f);
            if(held&&active_slider&&*active_slider==slider_id&&mx>=tx&&mx<=tx+tw2)
                val=(int)(((float)(mx-tx)/tw2)*100.0f);
        }
    }
    val=val<0?0:val>100?100:val;
    if(drag_val)*drag_val=val;
    {
        char vbuf[8];
        snprintf(vbuf,sizeof(vbuf),"%d",val);
        hud_str(h,tx+tw2+8,y,lsc,vbuf, 0.8f,0.8f,0.8f,1.0f);
    }
    return val;
}

static void page_main(HUD *h,float W,float H,int mx,int my,
                       bool *start,bool *load,bool *opts,bool *quit){
    if(!menu_anim_draw()){
        menu_bg(h,W,H,false);
    }
    menu_main_overlay(h,W,H);
    {
        const char *items[]={"START GAME","LOAD GAME","OPTIONS","QUIT"};
        bool *outs[]={start,load,opts,quit};
        float scale=2.25f;
        float line_h=42.0f;
        float right_pad=56.0f;
        float bottom_pad=56.0f;
        float max_w=0.0f;
        float total_h=line_h*(float)(sizeof(items)/sizeof(items[0]));
        float panel_pad_x=28.0f;
        float panel_pad_y=20.0f;
        for(size_t i=0;i<sizeof(items)/sizeof(items[0]);i++){
            float tw=menu_text_width(scale,items[i]);
            if(tw>max_w) max_w=tw;
        }
        {
            float x=W-right_pad-max_w-panel_pad_x;
            float y=H-bottom_pad-total_h-panel_pad_y;
            float w=max_w+panel_pad_x*1.55f;
            float ph=total_h+panel_pad_y*1.75f;
            hud_rect(h,x+10.0f,y+10.0f,w,ph, 0.0f,0.0f,0.0f,0.14f);
            hud_rect(h,x,y,w,ph, 0.01f,0.01f,0.01f,0.34f);
            hud_rect(h,x+12.0f,y,w-24.0f,1.0f, 0.78f,0.78f,0.78f,0.14f);
            hud_rect(h,x+12.0f,y+ph-1.0f,w-24.0f,1.0f, 0.78f,0.78f,0.78f,0.10f);
            hud_rect(h,x,y+12.0f,1.0f,ph-24.0f, 0.78f,0.78f,0.78f,0.08f);
            hud_rect(h,x+w-1.0f,y+12.0f,1.0f,ph-24.0f, 0.78f,0.78f,0.78f,0.08f);
            hud_rect(h,x,y,12.0f,12.0f, 0.0f,0.0f,0.0f,0.18f);
            hud_rect(h,x+w-12.0f,y,12.0f,12.0f, 0.0f,0.0f,0.0f,0.18f);
            hud_rect(h,x,y+ph-12.0f,12.0f,12.0f, 0.0f,0.0f,0.0f,0.18f);
            hud_rect(h,x+w-12.0f,y+ph-12.0f,12.0f,12.0f, 0.0f,0.0f,0.0f,0.18f);
        }
        for(size_t i=0;i<sizeof(items)/sizeof(items[0]);i++){
            const char *label=items[i];
            float x=W-right_pad-max_w;
            float y=H-bottom_pad-total_h+(float)i*line_h;
            bool over=(mx>=(int)(x-18.0f)&&mx<=(int)(W-right_pad+6.0f)&&
                       my>=(int)(y-6.0f)&&my<=(int)(y+28.0f));
            if(outs[i]) *outs[i]=over;
            if(over){
                menu_text(h,x-18.0f,y,scale,">",1.0f,0.88f,0.34f,1.0f);
            }
            menu_text(h,x+2.0f,y+2.0f,scale,label,0.0f,0.0f,0.0f,0.42f);
            menu_text(h,x,y,scale,label,
                      over?1.0f:0.82f,
                      over?0.88f:0.78f,
                      over?0.34f:0.70f,
                      over?1.0f:0.86f);
            if(i+1<sizeof(items)/sizeof(items[0])){
                hud_rect(h,x,y+30.0f,max_w,1.0f,0.72f,0.58f,0.20f,over?0.40f:0.18f);
            }
        }
    }
    hud_str(h,14,H-20,1.1f,"WASD Move  |  LClick/F Attack  |  1-5 Abilities",
            0.50f,0.50f,0.50f,0.62f);
    hud_flush(h);
    menu_text_flush(h);
}

static void page_load(HUD *h,float W,float H,int mx,int my,
                       int *chosen_slot,bool *back){
    menu_bg(h,W,H,false);
    {
        const char *hdr="SELECT SAVE SLOT";
        float hsc=3.4f,htw=menu_text_width(hsc,hdr);
        menu_text(h,(W-htw)*0.5f+2.0f,H*0.08f+2.0f,hsc,hdr, 0.0f,0.0f,0.0f,0.55f);
        menu_text(h,(W-htw)*0.5f,H*0.08f,hsc,hdr, 1.0f,0.85f,0.3f,1.0f);
        hud_rect(h,W*0.15f,H*0.17f+12,W*0.7f,2, 0.6f,0.45f,0.1f,0.6f);
        {
            float pw=(W*0.74f)/3.0f-14,ph=150,gap=18;
            float total_w=pw*3+gap*2,sx=(W-total_w)*0.5f,sy=H*0.31f;
            for(int i=0;i<3;i++){
                bool sc=false;
                slot_panel(h,sx+(pw+gap)*i,sy,pw,ph,i,mx,my,&sc);
                if(sc&&chosen_slot)*chosen_slot=i;
            }
        }
        {
            float bbw=160,bbh=40,bbx=W*0.5f-bbw*0.5f;
            menu_btn(h,bbx,H*0.80f,bbw,bbh,"BACK",2.0f,mx,my,true,back);
        }
    }
    hud_flush(h);
    menu_text_flush(h);
}

static bool menu_check(HUD *h,float x,float y,const char *label,bool val,int mx,int my){
    float s=22;
    bool over=(mx>=(int)x&&mx<=(int)(x+s)&&my>=(int)y&&my<=(int)(y+s));
    hud_rect(h,x,y,s,s, 0.12f,0.10f,0.07f,1.0f);
    if(val) hud_rect(h,x+3,y+3,s-6,s-6, 0.7f,0.5f,0.1f,1.0f);
    if(over) hud_rect(h,x-2,y-2,s+4,s+4, 0.8f,0.65f,0.1f,0.4f);
    hud_str(h,x+s+10,y+3,1.4f,label, 0.9f,0.85f,0.6f,1.0f);
    return over;
}

static int menu_difficulty(HUD *h,float x,float y,float bw,int cur,int mx,int my,bool clicked){
    const char *lbl[3]={"EASY","NORMAL","HARD"};
    float gap=10;
    int result=cur;
    for(int i=0;i<3;i++){
        float bx=x+(bw+gap)*(float)i;
        bool sel=(cur==i),over=(mx>=(int)bx&&mx<=(int)(bx+bw)&&my>=(int)y&&my<=(int)(y+28));
        if(sel) hud_rect(h,bx-2,y-2,bw+4,32, 0.7f,0.5f,0.1f,1.0f);
        hud_rect(h,bx,y,bw,28, sel?0.55f:(over?0.25f:0.10f),sel?0.35f:(over?0.18f:0.08f),sel?0.05f:0.03f,0.95f);
        {
            float tw=1.4f*8.0f*(float)strlen(lbl[i]);
            hud_str(h,bx+(bw-tw)*0.5f,y+6,1.4f,lbl[i], sel?1.0f:0.8f,sel?0.9f:0.75f,sel?0.3f:0.55f,1.0f);
        }
        if(clicked&&over) result=i;
    }
    return result;
}

static void menu_bind_button(HUD *h,float x,float y,float w,GameBindId bind,int mx,int my,bool clicked){
    float hgt=24.0f;
    bool over=(mx>=(int)x&&mx<=(int)(x+w)&&my>=(int)y&&my<=(int)(y+hgt));
    bool active=(g_options_capture_bind==bind);
    const char *label=options_bind_label(bind);
    const char *value=active?"Press key...":options_scancode_name((SDL_Scancode)g_options.keybinds[bind]);
    float valw=1.05f*8.0f*(float)strlen(value);
    hud_rect(h,x,y,w,hgt,
             active?0.42f:(over?0.20f:0.10f),
             active?0.17f:(over?0.14f:0.08f),
             active?0.07f:0.04f,0.95f);
    if(active||over) hud_rect(h,x,y,w,1,0.85f,0.68f,0.20f,0.9f);
    hud_str(h,x+8,y+5,1.05f,label,0.90f,0.84f,0.64f,1.0f);
    hud_str(h,x+w-valw-10,y+5,1.05f,value,active?1.0f:0.78f,active?0.92f:0.74f,active?0.32f:0.50f,1.0f);
    if(clicked&&over) options_begin_bind_capture(bind);
}

static void menu_pad_bind_button(HUD *h,float x,float y,float w,PadBindId bind,int mx,int my,bool clicked){
    float hgt=26.0f;
    bool over=(mx>=(int)x&&mx<=(int)(x+w)&&my>=(int)y&&my<=(int)(y+hgt));
    bool active=(g_options_capture_pad_bind==(int)bind);
    const char *label=options_pad_bind_label(bind);
    const char *value=active?"Press button...":options_pad_input_name((PadInputId)g_options.pad_binds[bind]);
    float valw=1.05f*8.0f*(float)strlen(value);
    hud_rect(h,x,y,w,hgt,
             active?0.42f:(over?0.20f:0.10f),
             active?0.17f:(over?0.14f:0.08f),
             active?0.07f:0.04f,0.95f);
    if(active||over) hud_rect(h,x,y,w,1,0.85f,0.68f,0.20f,0.9f);
    hud_str(h,x+8,y+6,1.05f,label,0.90f,0.84f,0.64f,1.0f);
    hud_str(h,x+w-valw-10,y+6,1.05f,value,active?1.0f:0.78f,active?0.92f:0.74f,active?0.32f:0.50f,1.0f);
    if(clicked&&over) options_begin_pad_bind_capture(bind);
}

static int g_options_menu_tab=0;

static void menu_settings_tab(HUD *h,float x,float y,float w,float hgt,const char *label,int tab_id,int mx,int my,bool clicked){
    bool selected=(g_options_menu_tab==tab_id);
    bool over=(mx>=(int)x&&mx<=(int)(x+w)&&my>=(int)y&&my<=(int)(y+hgt));
    hud_rect(h,x,y,w,hgt,
             selected?0.42f:(over?0.20f:0.10f),
             selected?0.18f:(over?0.15f:0.08f),
             selected?0.06f:0.03f,0.96f);
    if(selected) hud_rect(h,x,y,4,hgt,0.95f,0.78f,0.22f,1.0f);
    {
        float sc=1.25f;
        float tw=menu_text_width(sc,label);
        float tx=x+(w-tw)*0.5f;
        menu_text(h,tx+1.0f,y+11.0f,sc,label,0.0f,0.0f,0.0f,0.34f);
        menu_text(h,tx,y+10.0f,sc,label,
                  selected?1.0f:0.86f,
                  selected?0.90f:0.80f,
                  selected?0.30f:0.62f,1.0f);
    }
    if(clicked&&over){
        g_options_menu_tab=tab_id;
        g_options_capture_bind=-1;
        g_options_capture_pad_bind=-1;
    }
}

static void page_options(HUD *h,float W,float H,int mx,int my,bool clicked,
                          Options *opt,bool held,int *active_slider,
                          bool *back,bool *credits){
    menu_bg(h,W,H,false);
    {
        const char *hdr="SETTINGS";
        float hsc=3.8f,htw=menu_text_width(hsc,hdr);
        menu_text(h,(W-htw)*0.5f+2.0f,H*0.08f+2.0f,hsc,hdr, 0.0f,0.0f,0.0f,0.55f);
        menu_text(h,(W-htw)*0.5f,H*0.08f,hsc,hdr, 1.0f,0.85f,0.3f,1.0f);
        hud_rect(h,W*0.12f,H*0.17f+18,W*0.76f,2, 0.6f,0.45f,0.1f,0.6f);
    }

    {
        float nav_x=W*0.10f;
        float nav_y=H*0.26f;
        float nav_w=170.0f;
        float nav_h=230.0f;
        float item_h=44.0f;
        float panel_x=nav_x+nav_w+32.0f;
        float panel_y=nav_y;
        float panel_w=W*0.74f-nav_w;
        float panel_h=H*0.56f;
        float sg=42.0f;

        hud_rect(h,nav_x,nav_y,nav_w,nav_h,0.05f,0.04f,0.02f,0.88f);
        hud_rect(h,panel_x,panel_y,panel_w,panel_h,0.05f,0.04f,0.02f,0.88f);
        hud_rect(h,panel_x,panel_y,panel_w,3.0f,0.82f,0.64f,0.16f,0.92f);

        menu_settings_tab(h,nav_x+10,nav_y+12,nav_w-20,item_h,"GRAPHICS",0,mx,my,clicked);
        menu_settings_tab(h,nav_x+10,nav_y+66,nav_w-20,item_h,"KEYBINDS",1,mx,my,clicked);
        menu_settings_tab(h,nav_x+10,nav_y+120,nav_w-20,item_h,"CONTROLLER",2,mx,my,clicked);
        menu_settings_tab(h,nav_x+10,nav_y+174,nav_w-20,item_h,"AUDIO",3,mx,my,clicked);

        if(g_options_menu_tab==0){
            float lx=panel_x+24.0f, rx=panel_x+panel_w*0.52f;
            float y=panel_y+36.0f;
            menu_text(h,lx,y-20,1.22f,"DISPLAY",0.92f,0.80f,0.34f,0.96f);
            if(menu_check(h,lx,y,"Fullscreen",opt->fullscreen,mx,my)&&clicked) opt->fullscreen=!opt->fullscreen;
            if(menu_check(h,lx,y+34,"VSync",opt->vsync,mx,my)&&clicked) opt->vsync=!opt->vsync;
            if(menu_check(h,lx,y+68,"Show FPS",opt->show_fps,mx,my)&&clicked) opt->show_fps=!opt->show_fps;
            if(menu_check(h,lx,y+102,"Bloom",opt->bloom,mx,my)&&clicked) opt->bloom=!opt->bloom;
            menu_text(h,lx,y+152,1.22f,"MOUSE LOOK",0.92f,0.80f,0.34f,0.96f);
            menu_slider(h,lx,y+180,panel_w*0.34f,"Mouse",opt->mouse_sens,mx,my,clicked,held,5,active_slider,&opt->mouse_sens);
            if(menu_check(h,lx,y+180+sg+2,"Invert Mouse Y",opt->invert_mouse_y,mx,my)&&clicked) opt->invert_mouse_y=!opt->invert_mouse_y;

            menu_text(h,rx,y-20,1.22f,"GAMEPLAY",0.92f,0.80f,0.34f,0.96f);
            hud_str(h,rx,y+4,1.35f,"Difficulty:",0.85f,0.80f,0.55f,1.0f);
            opt->difficulty=menu_difficulty(h,rx,y+30,86,opt->difficulty,mx,my,clicked);

            menu_text(h,rx,y+90,1.22f,"RESOLUTION",0.92f,0.80f,0.34f,0.96f);
            {
                static const int RW[]={1280,1600,1920,2560};
                static const int RH[]={720,900,1080,1440};
                static const char *RL[]={"1280 x 720","1600 x 900","1920 x 1080","2560 x 1440"};
                for(int ri=0;ri<4;ri++){
                    float ry2=y+116+(float)ri*30.0f;
                    bool sel=(opt->res_w==RW[ri]&&opt->res_h==RH[ri]);
                    bool over=(mx>=(int)rx&&mx<=(int)(rx+220)&&my>=(int)ry2&&my<=(int)(ry2+26));
                    if(sel) hud_rect(h,rx-2,ry2-1,224,28,0.55f,0.35f,0.05f,0.9f);
                    else if(over) hud_rect(h,rx-2,ry2-1,224,28,0.18f,0.14f,0.05f,0.9f);
                    hud_str(h,rx+10,ry2+5,1.3f,RL[ri],sel?1.0f:0.75f,sel?0.85f:0.7f,sel?0.25f:0.45f,1.0f);
                    if(clicked&&over){opt->res_w=RW[ri];opt->res_h=RH[ri];}
                }
            }
        }else if(g_options_menu_tab==1){
            float lx=panel_x+24.0f;
            float col_w=panel_w*0.43f;
            float rx=lx+col_w+24.0f;
            float by=panel_y+34.0f;
            float row_gap=32.0f;
            int left_ids[]={BIND_MOVE_FORWARD,BIND_MOVE_BACKWARD,BIND_MOVE_LEFT,BIND_MOVE_RIGHT,BIND_JUMP,BIND_SPRINT,BIND_DODGE};
            int right_ids[]={BIND_INTERACT,BIND_BLOCK,BIND_INVENTORY,BIND_MAP,BIND_SKILLTREE,BIND_APPEARANCE,BIND_POTION};
            menu_text(h,lx,by-20,1.22f,"GAMEPLAY KEYBINDS",0.92f,0.80f,0.34f,0.96f);
            for(int i=0;i<7;i++){
                float ry=by+(float)i*row_gap;
                menu_bind_button(h,lx,ry,col_w,left_ids[i],mx,my,clicked);
                menu_bind_button(h,rx,ry,col_w,right_ids[i],mx,my,clicked);
            }
            hud_str(h,lx,by+7.0f*row_gap+8,1.05f,
                    options_is_binding_capture_active()?"Press a key, Esc cancels, Backspace clears":"Click a bind to reassign it",
                    0.62f,0.60f,0.48f,0.82f);
        }else if(g_options_menu_tab==2){
            float lx=panel_x+24.0f;
            float y=panel_y+36.0f;
            float col_w=panel_w*0.40f;
            float rx=lx+col_w+26.0f;
            menu_text(h,lx,y-20,1.22f,"CONTROLLER",0.92f,0.80f,0.34f,0.96f);
            menu_slider(h,lx,y,panel_w*0.56f,"Pad Look",opt->pad_look_sens,mx,my,clicked,held,3,active_slider,&opt->pad_look_sens);
            menu_slider(h,lx,y+sg,panel_w*0.56f,"Deadzone",opt->pad_deadzone,mx,my,clicked,held,4,active_slider,&opt->pad_deadzone);
            if(menu_check(h,lx,y+sg*2+4,"Invert Pad Y",opt->invert_pad_y,mx,my)&&clicked) opt->invert_pad_y=!opt->invert_pad_y;
            menu_text(h,rx,y-20,1.22f,"BUTTON BINDS",0.92f,0.80f,0.34f,0.96f);
            menu_pad_bind_button(h,rx,y,col_w,PAD_BIND_JUMP,mx,my,clicked);
            menu_pad_bind_button(h,rx,y+32,col_w,PAD_BIND_ATTACK,mx,my,clicked);
            menu_pad_bind_button(h,rx,y+64,col_w,PAD_BIND_HEAVY_ATTACK,mx,my,clicked);
            menu_pad_bind_button(h,rx,y+96,col_w,PAD_BIND_DODGE,mx,my,clicked);
            menu_pad_bind_button(h,rx,y+128,col_w,PAD_BIND_INTERACT,mx,my,clicked);
            menu_pad_bind_button(h,rx,y+160,col_w,PAD_BIND_BLOCK,mx,my,clicked);
            menu_pad_bind_button(h,rx,y+192,col_w,PAD_BIND_PAUSE,mx,my,clicked);
            menu_pad_bind_button(h,rx,y+224,col_w,PAD_BIND_INVENTORY,mx,my,clicked);
            hud_str(h,lx,y+sg*3+24,1.05f,
                    options_is_pad_binding_capture_active()?"Press a controller button to assign it":"Movement and camera sticks stay fixed; these are gameplay button binds.",
                    0.62f,0.60f,0.48f,0.82f);
        }else{
            float lx=panel_x+24.0f;
            float y=panel_y+36.0f;
            menu_text(h,lx,y-20,1.22f,"AUDIO",0.92f,0.80f,0.34f,0.96f);
            menu_slider(h,lx,y,panel_w*0.56f,"Master",opt->master_vol,mx,my,clicked,held,0,active_slider,&opt->master_vol);
            menu_slider(h,lx,y+sg,panel_w*0.56f,"Music",opt->music_vol,mx,my,clicked,held,1,active_slider,&opt->music_vol);
            menu_slider(h,lx,y+sg*2,panel_w*0.56f,"SFX",opt->sfx_vol,mx,my,clicked,held,2,active_slider,&opt->sfx_vol);
            hud_str(h,lx,y+sg*3+24,1.05f,"These sliders apply live and persist to config.ini when you leave the menu.",0.62f,0.60f,0.48f,0.82f);
        }
    }

    {
        float bby=H*0.87f,bbh=42,bbw2=155,gap2=20;
        float bbx0=W*0.5f-(bbw2*2+gap2)*0.5f;
        menu_btn(h,bbx0,bby,bbw2,bbh,"CREDITS",1.8f,mx,my,true,credits);
        menu_btn(h,bbx0+bbw2+gap2,bby,bbw2,bbh,"BACK",1.8f,mx,my,true,back);
    }
    hud_flush(h);
    menu_text_flush(h);
}

static void page_credits(HUD *h,float W,float H,int mx,int my,bool *back){
    menu_bg(h,W,H,false);
    {
        const char *hdr="CREDITS";
        float hsc=3.6f,htw=menu_text_width(hsc,hdr);
        menu_text(h,(W-htw)*0.5f+2.0f,H*0.08f+2.0f,hsc,hdr, 0.0f,0.0f,0.0f,0.55f);
        menu_text(h,(W-htw)*0.5f,H*0.08f,hsc,hdr, 1.0f,0.85f,0.3f,1.0f);
        hud_rect(h,W*0.15f,H*0.17f+12,W*0.7f,2, 0.6f,0.45f,0.1f,0.6f);
    }
    {
        static const char *entries[][2]={
            {"Game Design & Programming","Bryce"},
            {"Engine","C99 / SDL2 / OpenGL 3.3"},
            {"3D Animation","ufbx  (MIT License)"},
            {"Image Loading","stb_image  (Public Domain)"},
            {"Audio","SDL2_mixer  (zlib License)"},
            {"Special Thanks","The open-source community"},
            {NULL,NULL}
        };
        float ey=H*0.22f, row=34;
        float lx=W*0.18f, rx=W*0.52f;
        float panel_x=W*0.14f, panel_y=H*0.18f;
        float panel_w=W*0.72f, panel_h=H*0.58f;
        hud_rect(h,panel_x+10.0f,panel_y+10.0f,panel_w,panel_h, 0.0f,0.0f,0.0f,0.18f);
        hud_rect(h,panel_x,panel_y,panel_w,panel_h, 0.01f,0.01f,0.01f,0.70f);
        hud_rect(h,panel_x+14.0f,panel_y,panel_w-28.0f,2.0f, 0.82f,0.64f,0.18f,0.45f);
        hud_rect(h,panel_x+14.0f,panel_y+panel_h-2.0f,panel_w-28.0f,2.0f, 0.82f,0.64f,0.18f,0.20f);
        menu_text(h,lx,ey-2.0f,1.08f,"ROLE",  0.96f,0.82f,0.36f,0.96f);
        menu_text(h,rx,ey-2.0f,1.08f,"NAME",  0.96f,0.82f,0.36f,0.96f);
        hud_rect(h,lx,ey+20,W*0.62f,1, 0.70f,0.56f,0.16f,0.45f);
        ey+=28;
        for(int i=0;entries[i][0];i++){
            bool alt=(i&1);
            if(alt) hud_rect(h,lx-12,ey-2,W*0.64f,row, 0.0f,0.0f,0.0f,0.28f);
            menu_text(h,lx,ey+4,1.02f,entries[i][0], 0.92f,0.90f,0.82f,1.0f);
            menu_text(h,rx,ey+4,1.02f,entries[i][1], 1.00f,0.88f,0.52f,1.0f);
            ey+=row;
        }
        hud_rect(h,lx-12,ey+12,W*0.64f,48.0f, 0.0f,0.0f,0.0f,0.24f);
        hud_str(h,lx,ey+20,1.15f,"SDL2: https://libsdl.org  |  ufbx: https://github.com/ufbx/ufbx",
                0.78f,0.78f,0.74f,0.88f);
        hud_str(h,lx,ey+40,1.15f,"stb_image: https://github.com/nothings/stb",
                0.78f,0.78f,0.74f,0.88f);
    }
    {
        float csc=1.3f;
        const char *copy="Copyright 2025  All Rights Reserved";
        float ctw=csc*8.0f*(float)strlen(copy);
        hud_str(h,(W-ctw)*0.5f,H*0.88f,csc,copy, 0.55f,0.50f,0.35f,0.8f);
        {
            float bbw=160,bbh=40,bbx=W*0.5f-80;
            menu_btn(h,bbx,H*0.92f,bbw,bbh,"BACK",2.0f,mx,my,true,back);
        }
    }
    hud_flush(h);
    menu_text_flush(h);
}

#define CHARSEL_MAX 4

typedef struct {
    const char *name;
    const char *class_name;
    const char *lore;
    const char *fbx_path;
    float r,g,b;
    int str,agi,mag,end;
    GLuint portrait_tex;
    bool portrait_tried;
} CharDef;

static CharDef g_chardefs[CHARSEL_MAX]={
    {"Aldric","Warrior","A seasoned knight who survived the siege of Valdoa.","models/main_guy.glb",0.55f,0.25f,0.10f,9,5,2,8,0,false},
    {"Lyra","Rogue","She moves like shadow. Nobody sees her coming.","models/character_rogue.glb",0.15f,0.35f,0.20f,5,9,4,6,0,false},
    {"Soren","Mage","Exiled from the Arcane College. Power without limits.","models/character_mage.glb",0.10f,0.15f,0.45f,3,5,10,4,0,false},
    {"Brann","Ranger","Mountain born. Arrow never misses at distance.","models/character_ranger.glb",0.30f,0.28f,0.12f,6,8,3,7,0,false},
};

static int g_charsel_count=4;
static CharModel g_charsel_preview_model;
static int g_charsel_preview_idx=-1;
static bool g_charsel_preview_skin_ready=false;
static bool g_charsel_preview_light_ready=false;
static SkinProg g_charsel_preview_skin;
static LightingSystem g_charsel_preview_ls;
static float g_charsel_preview_yaw=0.12f;

static GLuint charsel_portrait(int idx){
    if(idx<0||idx>=CHARSEL_MAX)return 0;
    CharDef *c=&g_chardefs[idx];
    if(!c->portrait_tried){
        char path[64];
        snprintf(path,sizeof(path),"portrait_%d.png",idx);
        c->portrait_tex=terrain_tex_load(path,0,0,0);
        c->portrait_tried=true;
    }
    return c->portrait_tex;
}

static void charsel_preview_ensure(int idx){
    if(idx<0||idx>=g_charsel_count) return;
    if(idx==g_charsel_preview_idx&&scene_has_data(&g_charsel_preview_model.base)) return;
    if(g_charsel_preview_idx>=0) charmodel_free(&g_charsel_preview_model);
    charmodel_init(&g_charsel_preview_model,g_chardefs[idx].fbx_path);
    g_charsel_preview_idx=idx;
}

static void charsel_preview_draw(float W,float H,int idx){
    if(idx<0||idx>=g_charsel_count) return;
    if(!g_charsel_preview_skin_ready){
        skinprog_init(&g_charsel_preview_skin);
        g_charsel_preview_skin_ready=true;
    }
    if(!g_charsel_preview_light_ready){
        lighting_init(&g_charsel_preview_ls);
        g_charsel_preview_light_ready=true;
    }
    charsel_preview_ensure(idx);
    if(!scene_has_data(&g_charsel_preview_model.base)) return;
    {
        CharModel *cm=&g_charsel_preview_model;
        float aspect=(H>1.0f)?(W/H):1.0f;
        Vec3 cam_pos={0.0f,1.65f,5.80f};
        Vec3 cam_tgt={0.0f,1.55f,0.0f};
        Mat4 proj=mat4_persp(24.0f,aspect,0.1f,40.0f);
        Mat4 view=mat4_lookat(cam_pos,cam_tgt,(Vec3){0,1,0});
        Mat4 wm=entity_mat((Vec3){0.0f,-0.14f,0.0f},g_charsel_preview_yaw,cm->base.ground_offset,1.15f*cm->base.model_scale,
                           cm->base.cx,cm->base.cy,cm->base.cz);
        float t=(float)SDL_GetTicks()*0.001f;
        lighting_update(&g_charsel_preview_ls,DAY_DURATION*0.28f);
        g_charsel_preview_ls.sun_dir=vec3_norm((Vec3){-0.22f,-0.92f,-0.34f});
        g_charsel_preview_ls.sun_color=(Vec3){1.30f,1.24f,1.18f};
        g_charsel_preview_ls.ambient=(Vec3){0.50f,0.50f,0.56f};
        g_charsel_preview_ls.num_points=1;
        g_charsel_preview_ls.points[0]=(PointLight){
            .pos={1.6f,2.8f,2.2f},
            .color={1.0f,0.92f,0.82f},
            .radius=10.0f,
            .intensity=2.8f,
            .flicker_phase=0.0f,
            .flicker_speed=0.0f,
            .flicker=false,
            .active=true
        };
        glEnable(GL_DEPTH_TEST);
        glClear(GL_DEPTH_BUFFER_BIT);
        draw_charmodel(&g_charsel_preview_skin,cm,wm,ANIM_IDLE,(double)t*0.55,0.0f,proj,view,cam_pos,&g_charsel_preview_ls);
    }
}

static void charsel_stat(HUD *h,float x,float y,float w,const char *label,int val){
    float sc=1.15f;
    hud_str(h,x,y,sc,label, 0.85f,0.80f,0.55f,1.0f);
    {
        float bx=x+48,bw=w-50,bh=9;
        hud_rect(h,bx,y+2,bw,bh, 0.10f,0.08f,0.05f,1.0f);
        hud_rect(h,bx,y+2,bw*(val/10.0f),bh, 0.75f,0.55f,0.10f,0.95f);
    }
}

static void page_charselect(HUD *h,float W,float H,int mx,int my,bool clicked,
                             int *hovered,int *selected,bool *back){
    if(*hovered<0||*hovered>=g_charsel_count) *hovered=0;
    menu_bg_plain(h,W,H);
    charsel_preview_draw(W,H,*hovered);
    {
        CharDef *cd=&g_chardefs[*hovered];
        float left_x=28.0f,left_y=22.0f,left_w=W*0.24f,left_h=H-44.0f;
        float right_w=W*0.23f,right_x=W-right_w-28.0f,right_y=22.0f,right_h=H-44.0f;
        float center_x=left_x+left_w+18.0f,center_w=right_x-center_x-18.0f;
        float hdr_sc=3.2f;
        float label_sc=1.15f;
        float stat_y=left_y+188.0f;
        const char *hdr="CHOOSE YOUR HERO";
        float htw=menu_text_width(hdr_sc,hdr);
        menu_text(h,(W-htw)*0.5f+2.0f,26.0f+2.0f,hdr_sc,hdr, 0.0f,0.0f,0.0f,0.55f);
        menu_text(h,(W-htw)*0.5f,26.0f,hdr_sc,hdr, 1.0f,0.85f,0.3f,1.0f);
        hud_rect(h,left_x,left_y,left_w,left_h, 0.03f,0.02f,0.015f,0.84f);
        hud_rect(h,right_x,right_y,right_w,right_h, 0.03f,0.02f,0.015f,0.84f);
        hud_rect(h,center_x+center_w*0.18f,H*0.82f,center_w*0.64f,2.0f, 0.78f,0.58f,0.14f,0.35f);
        hud_rect(h,left_x,left_y,left_w,2.0f, 0.72f,0.54f,0.16f,0.85f);
        hud_rect(h,right_x,right_y,right_w,2.0f, 0.72f,0.54f,0.16f,0.85f);

        menu_text(h,left_x+20.0f,left_y+18.0f,2.2f,cd->name, 1.0f,0.88f,0.52f,1.0f);
        hud_str(h,left_x+20.0f,left_y+58.0f,1.2f,"Origin", 0.66f,0.58f,0.34f,0.88f);
        hud_str(h,left_x+20.0f,left_y+78.0f,1.5f,"Wandering Adventurer", 0.90f,0.86f,0.74f,1.0f);
        hud_str(h,left_x+20.0f,left_y+112.0f,1.2f,"Class", 0.66f,0.58f,0.34f,0.88f);
        menu_text(h,left_x+20.0f,left_y+130.0f,1.65f,cd->class_name, cd->r*1.4f+0.2f,cd->g*1.4f+0.2f,cd->b*1.4f+0.2f,1.0f);
        hud_rect(h,left_x+20.0f,left_y+165.0f,left_w-40.0f,1.0f, 0.55f,0.42f,0.12f,0.45f);

        hud_str(h,left_x+20.0f,stat_y-20.0f,1.2f,"ATTRIBUTES", 0.72f,0.62f,0.30f,0.90f);
        charsel_stat(h,left_x+20.0f,stat_y,left_w-40.0f,"STR",cd->str); stat_y+=22.0f;
        charsel_stat(h,left_x+20.0f,stat_y,left_w-40.0f,"AGI",cd->agi); stat_y+=22.0f;
        charsel_stat(h,left_x+20.0f,stat_y,left_w-40.0f,"MAG",cd->mag); stat_y+=22.0f;
        charsel_stat(h,left_x+20.0f,stat_y,left_w-40.0f,"END",cd->end); stat_y+=38.0f;
        hud_str(h,left_x+20.0f,stat_y,1.2f,"BACKGROUND", 0.72f,0.62f,0.30f,0.90f);
        {
            float ly=stat_y+22.0f;
            const char *l=cd->lore;
            int len=(int)strlen(l),lw=28;
            for(int c2=0;c2<len;c2+=lw){
                char tmp[48]={0};
                int take=len-c2>lw?lw:len-c2;
                strncpy(tmp,l+c2,(size_t)take);
                hud_str(h,left_x+20.0f,ly,1.08f,tmp, 0.84f,0.81f,0.70f,0.92f);
                ly+=15.0f;
            }
        }

        menu_text(h,right_x+20.0f,right_y+18.0f,1.9f,"Select Class", 0.98f,0.90f,0.68f,1.0f);
        hud_str(h,right_x+20.0f,right_y+50.0f,1.05f,"Preview the class on the stage", 0.66f,0.63f,0.54f,0.84f);
        {
            float bw=(right_w-52.0f)*0.5f;
            float bh=42.0f;
            float by=right_y+88.0f;
            for(int i=0;i<g_charsel_count;i++){
                float bx=right_x+20.0f+(float)(i%2)*(bw+12.0f);
                float yy=by+(float)(i/2)*(bh+12.0f);
                bool over=(mx>=bx&&mx<=bx+bw&&my>=yy&&my<=yy+bh);
                bool active=(i==*hovered);
                menu_btn(h,bx,yy,bw,bh,g_chardefs[i].class_name,1.55f,mx,my,true,NULL);
                if(active){
                    hud_rect(h,bx-2.0f,yy-2.0f,bw+4.0f,2.0f, 0.96f,0.76f,0.22f,1.0f);
                    hud_rect(h,bx-2.0f,yy+bh,bw+4.0f,2.0f, 0.96f,0.76f,0.22f,1.0f);
                }
                if(over&&clicked) *hovered=i;
            }
        }
        {
            float info_y=right_y+right_h-208.0f;
            hud_rect(h,right_x+20.0f,info_y,right_w-40.0f,120.0f, 0.07f,0.05f,0.03f,0.74f);
            menu_text(h,right_x+36.0f,info_y+16.0f,1.7f,cd->class_name, 1.0f,0.86f,0.46f,1.0f);
            hud_str(h,right_x+36.0f,info_y+48.0f,1.05f,"Idle Preview", 0.60f,0.56f,0.48f,0.86f);
            hud_str(h,right_x+36.0f,info_y+72.0f,label_sc,"Primary role and silhouette", 0.84f,0.80f,0.70f,0.92f);
            hud_str(h,right_x+36.0f,info_y+90.0f,label_sc,"are emphasized in the stage view.", 0.84f,0.80f,0.70f,0.92f);
        }
        {
            float action_y=H-74.0f;
            float back_x=center_x+center_w*0.08f;
            float pick_x=center_x+center_w*0.52f;
            float btn_w=center_w*0.28f;
            float btn_h=46.0f;
            bool pick_over=(mx>=pick_x&&mx<=pick_x+btn_w&&my>=action_y&&my<=action_y+btn_h);
            menu_btn(h,back_x,action_y,btn_w,btn_h,"BACK",1.9f,mx,my,true,back);
            menu_btn(h,pick_x,action_y,btn_w,btn_h,"SELECT HERO",1.9f,mx,my,true,NULL);
            if(pick_over&&clicked) *selected=*hovered;
        }
    }
    hud_flush(h);
    menu_text_flush(h);
}
