struct WorldSurfaceRivers {
    struct Range {int first=0,count=0;float level=0;std::array<double,4> bounds={};};
    GLuint vao=0,vbo=0;std::vector<Range> ranges;std::uint64_t bytes=0;
    void release(){if(vbo)glDeleteBuffers(1,&vbo);if(vao)glDeleteVertexArrays(1,&vao);vao=vbo=0;ranges.clear();bytes=0;}
    static bool nearby(const Range &r,double x,double z){double dx=x-std::clamp(x,r.bounds[0],r.bounds[2]),dz=z-std::clamp(z,r.bounds[1],r.bounds[3]);return dx*dx+dz*dz<220.*220.;}
    bool visible(double x,double z)const{for(const auto &r:ranges)if(nearby(r,x,z))return true;return false;}
    bool prepare(const world_surface::Data &data,std::string &error){
        std::vector<float> vertices;WorldSurfaceRivers next;
        auto vertex=[&](world_surface::Point p){vertices.push_back(float(p.x));vertices.push_back(float(p.y));vertices.push_back(float(p.z));};
        for(const auto &source:data.ribbons)if(source.kind==world_surface::RibbonKind::River){
            world_surface::BakedRibbon r;if(!world_surface::bake_ribbon(source,r,error))return false;
            Range range;range.first=int(vertices.size()/3);range.bounds=r.bounds;range.level=float(r.samples.front().position.y);
            auto side=[&](const world_surface::RibbonSample &s,int sign){return world_surface::Point{s.position.x+s.right[0]*source.width*.5*sign,s.position.y,s.position.z+s.right[1]*source.width*.5*sign};};
            for(unsigned i=1;i<r.samples.size();i++){auto a=side(r.samples[i-1],-1),b=side(r.samples[i-1],1),c=side(r.samples[i],-1),d=side(r.samples[i],1);vertex(a);vertex(c);vertex(b);vertex(b);vertex(c);vertex(d);}
            // The CPU's nearest-segment channel has rounded end caps.
            for(auto s:{r.samples.front(),r.samples.back()})for(unsigned i=0;i<32;i++){
                double a=i*6.283185307/32,b=(i+1)*6.283185307/32;vertex(s.position);vertex({s.position.x+std::cos(b)*source.width*.5,s.position.y,s.position.z+std::sin(b)*source.width*.5});vertex({s.position.x+std::cos(a)*source.width*.5,s.position.y,s.position.z+std::sin(a)*source.width*.5});
            }
            range.count=int(vertices.size()/3)-range.first;next.ranges.push_back(range);
        }
        if(!vertices.empty()){
            GLint old_vao=0,old_vbo=0;glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&old_vao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&old_vbo);
            glGenVertexArrays(1,&next.vao);glGenBuffers(1,&next.vbo);glBindVertexArray(next.vao);glBindBuffer(GL_ARRAY_BUFFER,next.vbo);
            glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(vertices.size()*sizeof(float)),vertices.data(),GL_STATIC_DRAW);glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),nullptr);
            glBindVertexArray(GLuint(old_vao));glBindBuffer(GL_ARRAY_BUFFER,GLuint(old_vbo));
            if(glGetError()!=GL_NO_ERROR){next.release();error="River mesh upload failed; previous water retained";return false;}
            next.bytes=vertices.size()*sizeof(float);
        }
        release();*this=std::move(next);return true;
    }
};
