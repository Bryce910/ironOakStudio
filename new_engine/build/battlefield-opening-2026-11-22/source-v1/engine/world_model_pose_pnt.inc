namespace {
using PntPoint=world_rigid_weapon_pose::Point;
PntPoint pnt_position(const WorldModelVertexOverride &v){return {v.position[0],v.position[1],v.position[2]};}
PntPoint pnt_normal(const WorldModelVertexOverride &v){return {v.normal[0],v.normal[1],v.normal[2]};}
PntPoint pnt_tangent(const WorldModelVertexOverride &v){return {v.tangent[0],v.tangent[1],v.tangent[2]};}
bool pnt_basis(WorldModelVertexOverride &v,PntPoint n,PntPoint t){
    double normal[3]={n[0],n[1],n[2]},tangent[3]={t[0],t[1],t[2]},nn=0;
    for(auto x:normal)nn+=x*x;if(!(nn>1.e-14)||!std::isfinite(nn))return false;
    nn=std::sqrt(nn);for(auto &x:normal)x/=nn;double dot=0;
    for(unsigned k=0;k<3;++k)dot+=normal[k]*tangent[k];double tt=0;
    for(unsigned k=0;k<3;++k){tangent[k]-=normal[k]*dot;tt+=tangent[k]*tangent[k];}
    if(!(tt>1.e-14)){const unsigned axis=std::abs(normal[1])<.9?1:0;
        tangent[0]=axis==1?normal[2]:0;tangent[1]=axis==0?-normal[2]:0;tangent[2]=axis==1?-normal[0]:normal[1];tt=0;for(auto x:tangent)tt+=x*x;
    }
    if(!(tt>1.e-14)||!std::isfinite(tt))return false;tt=std::sqrt(tt);
    for(unsigned k=0;k<3;++k){v.normal[k]=float(normal[k]);v.tangent[k]=float(tangent[k]/tt);}return true;
}
bool pnt_valid(const WorldModelPoseBlend *b,size_t count){
    if(!b||!b->a||!b->b||b->a->meshes.size()!=b->b->meshes.size()||!count||count>100000)return false;
    size_t n=0;for(size_t m=0;m<b->a->meshes.size();++m){const auto &a=b->a->meshes[m],&z=b->b->meshes[m];
        if(a.cut_support_xyz.empty()||a.cut_support_xyz.size()!=z.cut_support_xyz.size()||a.cut_basis_nt.size()!=a.cut_support_xyz.size()||z.cut_basis_nt.size()!=a.cut_support_xyz.size())return false;
        n+=a.cut_support_xyz.size();
    }return n==count;
}
bool pnt_linear_vertex(const Mesh &a,const Mesh &b,size_t i,float w,WorldModelVertexOverride &v){
    v={};v.weight=1;const auto &x=a.cut_support_xyz[i],&y=b.cut_support_xyz[i];const auto &n=a.cut_basis_nt[i],&t=b.cut_basis_nt[i];
    if(w<=0){std::copy(x.begin(),x.end(),v.position);std::copy_n(n.begin(),3,v.normal);std::copy_n(n.begin()+3,3,v.tangent);return true;}
    if(w>=1){std::copy(y.begin(),y.end(),v.position);std::copy_n(t.begin(),3,v.normal);std::copy_n(t.begin()+3,3,v.tangent);return true;}
    PntPoint normal,tangent;for(unsigned k=0;k<3;++k){v.position[k]=x[k]+(y[k]-x[k])*w;normal[k]=n[k]+(t[k]-n[k])*w;tangent[k]=n[k+3]+(t[k+3]-n[k+3])*w;}
    if(world_rigid_weapon_pose::dot(normal,normal)<=1.e-12f)normal={n[0],n[1],n[2]};
    return pnt_basis(v,normal,tangent);
}
bool pnt_rigid_vertex(const Mesh &a,size_t i,const WorldModelRigidWeaponPose &pose,float w,WorldModelVertexOverride &v){
    float q[4]={pose.rotation[0]*w,pose.rotation[1]*w,pose.rotation[2]*w,1+(pose.rotation[3]-1)*w};float norm=0;for(auto f:q)norm+=f*f;norm=std::sqrt(norm);if(!(norm>1.e-7f))return false;for(auto &f:q)f/=norm;
    const auto point=world_rigid_weapon_pose::interpolate(a.cut_support_xyz[i],pose.pivot_a,pose.pivot_b,pose.rotation,w);
    v={};v.weight=1;std::copy(point.begin(),point.end(),v.position);const auto &basis=a.cut_basis_nt[i];
    return pnt_basis(v,world_rigid_weapon_pose::rotate({basis[0],basis[1],basis[2]},q),world_rigid_weapon_pose::rotate({basis[3],basis[4],basis[5]},q));
}
}
extern "C" bool world_model_pose_blend_sample_pnt(const WorldModelPoseBlend *b,float weight,WorldModelVertexOverride *out,size_t count){
    if(!out||!std::isfinite(weight)||weight<0||weight>1||!pnt_valid(b,count))return false;
    size_t offset=0;for(size_t m=0;m<b->a->meshes.size();++m){const auto &a=b->a->meshes[m],&z=b->b->meshes[m];
        for(size_t i=0;i<a.cut_support_xyz.size();++i)if(!pnt_linear_vertex(a,z,i,weight,out[offset+i]))return false;
        if(m<b->rigid_weapons.size())for(unsigned p=0;p<b->rigid_weapons[m].count;++p){const auto &part=b->rigid_weapons[m].parts[p];
            if(part.first_index>a.pose_indices.size()||part.index_count>a.pose_indices.size()-part.first_index)return false;
            const bool whole=part.first_index==0&&part.index_count==a.pose_indices.size();
            const size_t count=whole?a.cut_support_xyz.size():part.index_count;
            for(size_t at=0;at<count;++at){const auto id=whole?at:a.pose_indices[part.first_index+at];if(id>=a.cut_support_xyz.size()||!pnt_rigid_vertex(a,id,part.pose,weight,out[offset+id]))return false;}
        }offset+=a.cut_support_xyz.size();
    }return true;
}
extern "C" bool world_model_pose_blend_bridge_pnt(const WorldModelPoseBlend *b,float weight,const WorldModelVertexOverride *frozen,float alpha,WorldModelVertexOverride *out,size_t count){
    if(!frozen||!out||frozen==out||!std::isfinite(alpha)||alpha<0||alpha>1||!pnt_valid(b,count)||!std::isfinite(weight)||weight<0||weight>1)return false;
    for(size_t i=0;i<count;++i)if(!finite3(frozen[i].position)||!finite3(frozen[i].normal)||!finite3(frozen[i].tangent))return false;
    if(alpha==0){std::copy_n(frozen,count,out);return true;}
    if(!world_model_pose_blend_sample_pnt(b,weight,out,count))return false;
    if(alpha==1)return true;
    size_t offset=0;for(size_t m=0;m<b->a->meshes.size();++m){const auto &a=b->a->meshes[m];
        for(size_t i=0;i<a.cut_support_xyz.size();++i){auto &v=out[offset+i];const auto &f=frozen[offset+i];PntPoint n,t;
            for(unsigned k=0;k<3;++k){v.position[k]=f.position[k]+(v.position[k]-f.position[k])*alpha;n[k]=f.normal[k]+(v.normal[k]-f.normal[k])*alpha;t[k]=f.tangent[k]+(v.tangent[k]-f.tangent[k])*alpha;}
            if(world_rigid_weapon_pose::dot(n,n)<=1.e-12f)n=pnt_normal(f);
            if(!pnt_basis(v,n,t))return false;
        }
        if(m<b->rigid_weapons.size())for(unsigned p=0;p<b->rigid_weapons[m].count;++p){const auto &part=b->rigid_weapons[m].parts[p];
            if(!part.index_count||part.first_index>a.pose_indices.size()||part.index_count>a.pose_indices.size()-part.first_index)return false;
            const unsigned x=a.pose_indices[part.first_index];unsigned y=x,z=x;float far=0,area=0;
            for(size_t at=part.first_index;at<part.first_index+part.index_count;++at){const unsigned id=a.pose_indices[at];if(id>=a.cut_support_xyz.size())return false;const auto d=world_rigid_weapon_pose::sub(a.cut_support_xyz[id],a.cut_support_xyz[x]);const float n=world_rigid_weapon_pose::dot(d,d);if(n>far){far=n;y=id;}}
            const auto edge=world_rigid_weapon_pose::sub(a.cut_support_xyz[y],a.cut_support_xyz[x]);
            for(size_t at=part.first_index;at<part.first_index+part.index_count;++at){const unsigned id=a.pose_indices[at];const auto d=world_rigid_weapon_pose::cross(edge,world_rigid_weapon_pose::sub(a.cut_support_xyz[id],a.cut_support_xyz[x]));const float n=world_rigid_weapon_pose::dot(d,d);if(n>area){area=n;z=id;}}
            WorldModelVertexOverride tx,ty,tz;
            if(!pnt_rigid_vertex(a,x,part.pose,weight,tx)||!pnt_rigid_vertex(a,y,part.pose,weight,ty)||!pnt_rigid_vertex(a,z,part.pose,weight,tz))return false;
            const std::array<PntPoint,3> f={pnt_position(frozen[offset+x]),pnt_position(frozen[offset+y]),pnt_position(frozen[offset+z])},target={pnt_position(tx),pnt_position(ty),pnt_position(tz)},base={a.cut_support_xyz[x],a.cut_support_xyz[y],a.cut_support_xyz[z]};
            float rotation[4],base_to_frozen[4];if(!world_rigid_weapon_pose::rotation(f,target,rotation)||!world_rigid_weapon_pose::rotation(base,f,base_to_frozen))return false;
            const auto arm=world_rigid_weapon_pose::rotate({part.pose.pivot_a[0]-base[0][0],part.pose.pivot_a[1]-base[0][1],part.pose.pivot_a[2]-base[0][2]},base_to_frozen);
            float from[3],to[3];for(unsigned k=0;k<3;++k){from[k]=f[0][k]+arm[k];to[k]=part.pose.pivot_a[k]+(part.pose.pivot_b[k]-part.pose.pivot_a[k])*weight;}
            float q[4]={rotation[0]*alpha,rotation[1]*alpha,rotation[2]*alpha,1+(rotation[3]-1)*alpha};float norm=0;for(auto n:q)norm+=n*n;norm=std::sqrt(norm);if(!(norm>1.e-7f))return false;for(auto &n:q)n/=norm;
            const bool whole=part.first_index==0&&part.index_count==a.pose_indices.size();
            const size_t rigid_vertices=whole?a.cut_support_xyz.size():part.index_count;
            for(size_t at=0;at<rigid_vertices;++at){const unsigned id=whole?unsigned(at):a.pose_indices[part.first_index+at];const auto &fnt=frozen[offset+id];auto &v=out[offset+id];WorldModelVertexOverride target_pnt;
                if(!pnt_rigid_vertex(a,id,part.pose,weight,target_pnt))return false;
                const auto rigid=world_rigid_weapon_pose::interpolate(pnt_position(fnt),from,to,rotation,alpha),end=world_rigid_weapon_pose::interpolate(pnt_position(fnt),from,to,rotation,1);
                const auto n=world_rigid_weapon_pose::rotate(pnt_normal(fnt),q),ne=world_rigid_weapon_pose::rotate(pnt_normal(fnt),rotation),t=world_rigid_weapon_pose::rotate(pnt_tangent(fnt),q),te=world_rigid_weapon_pose::rotate(pnt_tangent(fnt),rotation);PntPoint normal,tangent;
                float error=0;for(unsigned k=0;k<3;++k){const float residual=target_pnt.position[k]-end[k];error+=residual*residual;v.position[k]=rigid[k]+alpha*residual;normal[k]=n[k]+alpha*(target_pnt.normal[k]-ne[k]);tangent[k]=t[k]+alpha*(target_pnt.tangent[k]-te[k]);}
                // Source authoring already certifies2mm rigidity. Preserve its
                // tiny endpoint residual rather than introducing an end pop.
                if(error>.002f*.002f||!pnt_basis(v,normal,tangent))return false;
            }
        }offset+=a.cut_support_xyz.size();
    }return true;
}
