// Included after the private model/pose structs. Lifetime is explicitly owned
// by the combat system; pose batches only borrow their loading-time attachment.
struct WorldModelVertexStream {
    GLuint buffer=0;
    std::vector<std::size_t> counts,offsets;
    std::size_t vertices=0;
};
extern "C" size_t world_model_pose_mesh_count(const WorldModelAsset *a){return a&&a->pose_asset?a->meshes.size():0;}
extern "C" size_t world_model_pose_mesh_vertices(const WorldModelAsset *a,size_t mesh){return a&&a->pose_asset&&mesh<a->meshes.size()?a->meshes[mesh].pose_keys.size():0;}
extern "C" void world_model_vertex_stream_free(WorldModelVertexStream *s){if(s){glDeleteBuffers(1,&s->buffer);delete s;}}
extern "C" WorldModelVertexStream *world_model_vertex_stream_create(const WorldModelAsset *a){
    static_assert(sizeof(WorldModelVertexOverride)==48,"A deformation stream contains three vec4s");
    if(!a||!a->pose_asset||a->meshes.empty())return nullptr;
    auto *s=new(std::nothrow)WorldModelVertexStream;if(!s)return nullptr;
    try{
        s->counts.reserve(a->meshes.size());s->offsets.reserve(a->meshes.size());
        for(const auto &mesh:a->meshes){s->offsets.push_back(s->vertices);s->counts.push_back(mesh.pose_keys.size());s->vertices+=mesh.pose_keys.size();}
        if(!s->vertices||s->vertices>100000)throw "Player deformation vertex budget";
        std::vector<WorldModelVertexOverride> zero(s->vertices);
        GLint previous=0;glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&previous);
        glGenBuffers(1,&s->buffer);glBindBuffer(GL_ARRAY_BUFFER,s->buffer);
        glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(zero.size()*sizeof(zero[0])),zero.data(),GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER,GLuint(previous));
        if(!s->buffer||glGetError()!=GL_NO_ERROR)throw "Player deformation allocation failed";
        return s;
    }catch(...){world_model_vertex_stream_free(s);return nullptr;}
}
extern "C" bool world_model_vertex_stream_upload(WorldModelVertexStream *s,const WorldModelVertexOverride *v,size_t count){
    if(!s||!v||count!=s->vertices)return false;
    for(size_t i=0;i<count;++i){const auto &p=v[i];if(!finite3(p.position)||!finite3(p.normal)||!finite3(p.tangent)||!std::isfinite(p.weight)||p.weight<0||p.weight>1)return false;}
    glBindBuffer(GL_ARRAY_BUFFER,s->buffer);
    glBufferSubData(GL_ARRAY_BUFFER,0,GLsizeiptr(count*sizeof(*v)),v);
    glBindBuffer(GL_ARRAY_BUFFER,0);return glGetError()==GL_NO_ERROR;
}
extern "C" bool world_model_pose_blend_attach_vertex_stream(WorldModelPoseBlend *b,WorldModelVertexStream *s){
    if(!b||!s||!b->a||b->full_transform||b->vaos.size()!=s->counts.size())return false;
    for(size_t k=0;k<s->counts.size();++k)if(b->a->meshes[k].pose_keys.size()!=s->counts[k]||b->b->meshes[k].pose_keys.size()!=s->counts[k])return false;
    GLint vao=0,buffer=0;glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&buffer);
    for(size_t k=0;k<b->vaos.size();++k){
        glBindVertexArray(b->vaos[k]);glBindBuffer(GL_ARRAY_BUFFER,s->buffer);
        const unsigned locations[3]={6,14,15};
        for(unsigned i=0;i<3;++i){glEnableVertexAttribArray(locations[i]);glVertexAttribPointer(locations[i],4,GL_FLOAT,GL_FALSE,sizeof(WorldModelVertexOverride),(void*)(s->offsets[k]*sizeof(WorldModelVertexOverride)+i*4*sizeof(float)));glVertexAttribDivisor(locations[i],0);}
    }
    glBindVertexArray(GLuint(vao));glBindBuffer(GL_ARRAY_BUFFER,GLuint(buffer));
    if(glGetError()!=GL_NO_ERROR)return false;b->vertex_stream=s;return true;
}
// Compact NPCs reuse the ordinary A/B pose shader, including rigid weapons.
// Only two current endpoints are resident per actor; UV/index/materials remain
// in the shared base asset. No shader variant or per-frame skin reduction.
extern "C" bool world_model_pose_blend_attach_pose_streams(WorldModelPoseBlend *b,WorldModelVertexStream *a,WorldModelVertexStream *z){
 if(!b||!a||!z||!b->a||!b->b||b->full_transform||b->vaos.size()!=a->counts.size()||a->counts!=z->counts)return false;
 for(size_t m=0;m<a->counts.size();++m)if(b->a->meshes[m].pose_keys.size()!=a->counts[m]||b->b->meshes[m].pose_keys.size()!=a->counts[m])return false;
 GLint vao=0,buffer=0;glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&buffer);
 for(size_t m=0;m<b->vaos.size();++m){glBindVertexArray(b->vaos[m]);for(unsigned endpoint=0;endpoint<2;++endpoint){auto *s=endpoint?z:a;glBindBuffer(GL_ARRAY_BUFFER,s->buffer);
  const unsigned locations[2][3]={{0,1,3},{8,9,10}};const size_t offsets[3]={offsetof(WorldModelVertexOverride,position),offsetof(WorldModelVertexOverride,normal),offsetof(WorldModelVertexOverride,tangent)};
  for(unsigned i=0;i<3;++i){const unsigned loc=locations[endpoint][i];glEnableVertexAttribArray(loc);glVertexAttribPointer(loc,i==2?4:3,GL_FLOAT,GL_FALSE,sizeof(WorldModelVertexOverride),(void*)(s->offsets[m]*sizeof(WorldModelVertexOverride)+offsets[i]));glVertexAttribDivisor(loc,0);}
 }}glBindVertexArray(GLuint(vao));glBindBuffer(GL_ARRAY_BUFFER,GLuint(buffer));if(glGetError()!=GL_NO_ERROR)return false;b->pose_streams_attached=true;return true;
}
extern "C" bool world_model_pose_blend_upload_pose_pair(WorldModelPoseBlend *b,WorldModelVertexStream *a,WorldModelVertexStream *z,WorldModelVertexOverride *x,WorldModelVertexOverride *y,size_t vertices,const WorldModelRigidWeaponPose *rigid,size_t mesh_count){
 if(!b||!a||!z||!x||!y||!rigid||mesh_count!=b->a->meshes.size()||a->vertices!=vertices||z->vertices!=vertices)return false;
 if(b->rigid_weapons.size()!=mesh_count){try{b->rigid_weapons.resize(mesh_count);}catch(...){return false;}}
 size_t offset=0;for(size_t m=0;m<mesh_count;++m){const auto &mesh=b->a->meshes[m];const auto &r=rigid[m];auto &part=b->rigid_weapons[m];part={};
  if(r.enabled){if(!finite3(r.pivot_a)||!finite3(r.pivot_b))return false;float norm=0;for(float q:r.rotation){if(!std::isfinite(q))return false;norm+=q*q;}if(std::abs(norm-1)>.001f)return false;part.count=1;part.parts[0]={r,0,unsigned(mesh.pose_indices.size())};}
  for(size_t i=0;i<mesh.pose_keys.size();++i){x[offset+i].reserved1=y[offset+i].reserved1=mesh.pose_keys[i].handedness;}offset+=mesh.pose_keys.size();
 }return offset==vertices&&world_model_vertex_stream_upload(a,x,vertices)&&world_model_vertex_stream_upload(z,y,vertices);
}
extern "C" bool world_model_pose_blend_load_index_subset(WorldModelPoseBlend *b,const char *path){
 if(!b||!b->a||!b->b||!path||b->full_transform||!b->subset_buffers.empty())return false;FILE*f=std::fopen(path,"rb");if(!f)return false;
 auto word=[&](unsigned &v){unsigned char x[4];if(std::fread(x,1,4,f)!=4)return false;v=unsigned(x[0])|(unsigned(x[1])<<8)|(unsigned(x[2])<<16)|(unsigned(x[3])<<24);return true;};
 char magic[8];unsigned count=0;bool ok=std::fread(magic,1,8,f)==8&&!std::memcmp(magic,"VWFPIDX1",8)&&word(count)&&count==b->a->meshes.size()&&count==b->vaos.size();
 std::vector<std::vector<unsigned>> selected(ok?count:0);size_t total=0;
 for(unsigned m=0;ok&&m<count;++m){const auto &mesh=b->a->meshes[m];unsigned n=0;ok=word(n)&&n%3==0&&n<=mesh.pose_indices.size()&&(!mesh.held_weapon||n==0);if(!ok)break;selected[m].resize(n);for(auto &i:selected[m])ok=ok&&word(i)&&i<mesh.pose_keys.size();if(!ok)break;
  std::set<std::array<unsigned,3>> original,seen;for(size_t i=0;i<mesh.pose_indices.size();i+=3)original.insert({mesh.pose_indices[i],mesh.pose_indices[i+1],mesh.pose_indices[i+2]});
  for(size_t i=0;ok&&i<n;i+=3){const std::array<unsigned,3> tri={selected[m][i],selected[m][i+1],selected[m][i+2]};ok=original.count(tri)&&seen.insert(tri).second;}total+=n;
 }
 ok=ok&&std::fgetc(f)==EOF&&total>=12;std::fclose(f);if(!ok)return false;
 GLint vao=0,buffer=0;glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&buffer);std::vector<GLuint> buffers(count);glGenBuffers(GLsizei(count),buffers.data());
 for(unsigned m=0;m<count;++m){glBindVertexArray(b->vaos[m]);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,buffers[m]);glBufferData(GL_ELEMENT_ARRAY_BUFFER,GLsizeiptr(selected[m].size()*4),selected[m].empty()?nullptr:selected[m].data(),GL_STATIC_DRAW);}
 glBindVertexArray(GLuint(vao));glBindBuffer(GL_ARRAY_BUFFER,GLuint(buffer));if(glGetError()!=GL_NO_ERROR){glDeleteBuffers(GLsizei(count),buffers.data());return false;}
 b->subset_buffers=std::move(buffers);b->subset_counts.resize(count);for(unsigned m=0;m<count;++m)b->subset_counts[m]=GLsizei(selected[m].size());return true;
}
