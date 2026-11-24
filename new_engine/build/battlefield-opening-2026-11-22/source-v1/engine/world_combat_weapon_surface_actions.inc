// Compact-only structural weapon surfaces. Original indexed triangles are
// accelerated in rest space; all narrowphase points use the exact current
// source-pair/rigid/clash/mirror transforms shared with the visible PNT draw.
namespace {
struct AnatomySurfacePose {
 const WorldCombatMotion*p=nullptr;const AnatomySnapshot*s=nullptr;const DeathActorBridge*frozen=nullptr;unsigned mesh=0,source=0;float weight=0;WorldModelRigidWeaponPose rest={},pair={};
 bool bridge=false;float bridge_rotation[4]={0,0,0,1},bridge_from[3]={},bridge_to[3]={};
 world_rigid_weapon_pose::Point local(world_rigid_weapon_pose::Point v,bool bounds)const{
  if(bounds)v=world_rigid_weapon_pose::interpolate(v,rest.pivot_a,rest.pivot_b,rest.rotation,1);
  if(!frozen)v=world_rigid_weapon_pose::interpolate(v,pair.pivot_a,pair.pivot_b,pair.rotation,weight);
  if(bridge)v=world_rigid_weapon_pose::interpolate(v,bridge_from,bridge_to,bridge_rotation,s->bridge_alpha);return v;
 }
 AnatomyPoint place(world_rigid_weapon_pose::Point v,bool bounds)const{v=local(v,bounds);return anatomy_place({v[0],v[1],v[2]},p->x+s->instance.x,s->instance.y,p->z+s->instance.z,s->instance.yaw,s->mirrored);}
 AnatomyPoint vertex(unsigned id)const{const auto&m=p->compact->packet.meshes()[mesh];if(frozen){const auto&v=frozen->frozen[m.offset+id];return place({v.position[0],v.position[1],v.position[2]},false);}return place(p->compact->packet.position(source,m.offset+id),false);}
};
bool anatomy_surface_pose(const WorldCombatMotion*p,const AnatomySnapshot&s,unsigned mesh,AnatomySurfacePose&out){
 if(!p->compact)return false;const auto&c=*p->compact;const unsigned slot=s.bridge?s.bridge_slot:s.slot;if(slot>=p->batch_count||mesh>=c.packet.meshes().size())return false;
 const auto pair=pose_pair(slot,p->pose_count);if(mesh>=c.rigid[slot].size()||mesh>=c.surface_rigid[pair[0]].size())return false;
 out={};out.p=p;out.s=&s;out.mesh=mesh;out.source=pair[0];out.weight=s.bridge?s.bridge_weight:s.weight;out.pair=c.rigid[slot][mesh];out.rest=c.surface_rigid[pair[0]][mesh];if(!out.pair.enabled||!out.rest.enabled)return false;
 if(s.bridge){const auto&m=c.packet.meshes()[mesh];std::array<unsigned,3>triangle={};bool found=false;for(const auto&tree:c.surface_trees)if(tree.mesh==mesh){triangle=tree.frames[pair[0]];found=true;break;}if(!found)return false;std::array<world_rigid_weapon_pose::Point,3>from,to;
  out.frozen=clash_frozen(p,s);if(s.frozen_actor&&!out.frozen)return false;
  const auto target_pair=pose_pair(s.slot,p->pose_count);const auto&target=c.rigid[s.slot][mesh];
  for(unsigned k=0;k<3;++k){const unsigned id=m.offset+triangle[k];if(out.frozen){const auto&v=out.frozen->frozen[id];from[k]={v.position[0],v.position[1],v.position[2]};}else from[k]=out.local(c.packet.position(pair[0],id),false);to[k]=world_rigid_weapon_pose::interpolate(c.packet.position(target_pair[0],id),target.pivot_a,target.pivot_b,target.rotation,s.weight);}
  if(out.frozen){std::array<world_rigid_weapon_pose::Point,3>base;for(unsigned k=0;k<3;++k)base[k]=c.packet.position(pair[0],m.offset+triangle[k]);if(!world_rigid_weapon_pose::rotation(base,from,out.rest.rotation))return false;std::copy_n(c.rigid[s.bridge_slot][mesh].pivot_a,3,out.rest.pivot_a);std::copy_n(out.frozen->frozen_pivot.data(),3,out.rest.pivot_b);}
  if(!world_rigid_weapon_pose::rotation(from,to,out.bridge_rotation))return false;for(unsigned k=0;k<3;++k){out.bridge_from[k]=out.frozen?out.frozen->frozen_pivot[k]:out.pair.pivot_a[k]+(out.pair.pivot_b[k]-out.pair.pivot_a[k])*out.weight;out.bridge_to[k]=target.pivot_a[k]+(target.pivot_b[k]-target.pivot_a[k])*s.weight;}out.bridge=true;
 }return true;
}
void anatomy_structural_weapon_contact(const WorldCombatMotion*p,const anatomy_math::Sweep&attack,const AnatomySnapshot&from,const AnatomySnapshot&to,anatomy_math::Contact&best){
 if(!p->compact||p->compact->surface_trees.empty()||(best.hit&&best.time<=0))return;
 double attack_lo[3]={1.e30,1.e30,1.e30},attack_hi[3]={-1.e30,-1.e30,-1.e30};auto coordinate=[](AnatomyPoint v,unsigned k){return k==0?v.x:k==1?v.y:v.z;};
 for(auto v:{attack.from.base,attack.from.tip,attack.to.base,attack.to.tip})for(unsigned k=0;k<3;++k){attack_lo[k]=std::min(attack_lo[k],coordinate(v,k));attack_hi[k]=std::max(attack_hi[k],coordinate(v,k));}
 for(const auto&tree:p->compact->surface_trees){AnatomySurfacePose a,b;if(!anatomy_surface_pose(p,from,tree.mesh,a)||!anatomy_surface_pose(p,to,tree.mesh,b)){++p->compact->stats.failures;continue;}
  std::array<unsigned,64>stack={};unsigned pending=1;while(pending){const unsigned id=stack[--pending];if(id>=tree.nodes.size()){++p->compact->stats.failures;break;}const auto&node=tree.nodes[id];double lo[3]={1.e30,1.e30,1.e30},hi[3]={-1.e30,-1.e30,-1.e30};
   for(unsigned corner=0;corner<8;++corner){world_rigid_weapon_pose::Point v;for(unsigned k=0;k<3;++k)v[k]=(corner&(1u<<k))?node.hi[k]:node.lo[k];for(auto q:{a.place(v,true),b.place(v,true)})for(unsigned k=0;k<3;++k){lo[k]=std::min(lo[k],coordinate(q,k));hi[k]=std::max(hi[k],coordinate(q,k));}}
   bool overlap=true;for(unsigned k=0;k<3;++k)if(attack_hi[k]+attack.radius+.00201<lo[k]||hi[k]+attack.radius+.00201<attack_lo[k])overlap=false;if(!overlap)continue;
   if(!node.count){if(pending+2>stack.size()){++p->compact->stats.failures;break;}stack[pending++]=node.left;stack[pending++]=node.right;continue;}
   const auto&m=p->compact->packet.meshes()[tree.mesh];for(unsigned n=0;n<node.count;++n){const unsigned triangle=tree.order[node.begin+n];std::array<AnatomyPoint,3>x,y;for(unsigned k=0;k<3;++k){const unsigned vertex=m.indices[triangle*3+k];x[k]=a.vertex(vertex);y[k]=b.vertex(vertex);}const auto steel=anatomy_moving_triangle(attack,x,y);if(steel.hit&&(!best.hit||steel.time<=best.time+1.e-9)){best=steel;if(best.time<=0)return;}}
  }
 }
}
}
