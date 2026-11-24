struct WorldSurfaceRivers {
    struct Range {int first=0,count=0;float level=0;std::array<double,4> bounds={};float ymin=0,ymax=0;};
    GLuint vao=0,vbo=0;std::vector<Range> ranges;std::vector<world_surface::RiverVertex> support_vertices;std::uint64_t bytes=0;
    void release(){if(vbo)glDeleteBuffers(1,&vbo);if(vao)glDeleteVertexArrays(1,&vao);vao=vbo=0;ranges.clear();support_vertices.clear();bytes=0;}
    static bool nearby(const Range &r,double x,double z,double distance=220){double dx=x-std::clamp(x,r.bounds[0],r.bounds[2]),dz=z-std::clamp(z,r.bounds[1],r.bounds[3]);return dx*dx+dz*dz<distance*distance;}
    bool visible(double x,double z,double distance=220)const{for(const auto &r:ranges)if(nearby(r,x,z,distance))return true;return false;}
    bool prepare(const world_surface::Data &data,std::string &error){
        std::vector<world_surface::BakedRibbon> ribbons;
        for(const auto &source:data.ribbons)if(source.kind==world_surface::RibbonKind::River){world_surface::BakedRibbon ribbon;if(!world_surface::bake_ribbon(source,ribbon,error))return false;ribbons.push_back(std::move(ribbon));}
        world_surface::RiverNetwork network;world_surface::RiverMesh mesh;
        if(!network.prepare(ribbons,error)||!world_surface::bake_river_mesh(network,mesh,error))return false;
        auto &vertices=mesh.vertices;WorldSurfaceRivers next;
        for(const auto &r:mesh.ranges){Range range{r.first,r.count,r.level,r.bounds};range.ymin=range.ymax=vertices[size_t(r.first)].y;for(int k=r.first;k<r.first+r.count;k++){range.ymin=std::min(range.ymin,vertices[size_t(k)].y);range.ymax=std::max(range.ymax,vertices[size_t(k)].y);}next.ranges.push_back(range);}
        if(!vertices.empty()){
            GLint old_vao=0,old_vbo=0;glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&old_vao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&old_vbo);
            glGenVertexArrays(1,&next.vao);glGenBuffers(1,&next.vbo);glBindVertexArray(next.vao);glBindBuffer(GL_ARRAY_BUFFER,next.vbo);
            glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(vertices.size()*sizeof(world_surface::RiverVertex)),vertices.data(),GL_STATIC_DRAW);glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(world_surface::RiverVertex),nullptr);glEnableVertexAttribArray(1);glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,sizeof(world_surface::RiverVertex),reinterpret_cast<void *>(3*sizeof(float)));
            glBindVertexArray(GLuint(old_vao));glBindBuffer(GL_ARRAY_BUFFER,GLuint(old_vbo));
            if(glGetError()!=GL_NO_ERROR){next.release();error="River mesh upload failed; previous water retained";return false;}
            next.bytes=vertices.size()*sizeof(world_surface::RiverVertex);
        }
        next.support_vertices=std::move(vertices);
        release();*this=std::move(next);return true;
    }
};
