#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include <cstdio>
#include <vector>
#include <array>
#include <algorithm>
#include <limits>
static bool visible(cgltf_data*d,const cgltf_node*n){const auto*s=d->scene?d->scene:(d->scenes_count?d->scenes:nullptr);if(!s)return true;while(n){for(size_t i=0;i<s->nodes_count;i++)if(s->nodes[i]==n)return true;n=n->parent;}return false;}
int main(int c,char**v){if(c!=3)return 1;cgltf_options o={};cgltf_data*d=nullptr;if(cgltf_parse_file(&o,v[1],&d)||cgltf_load_buffers(&o,d,v[1])||cgltf_validate(d))return 2;std::vector<std::array<float,3>>p;float low=std::numeric_limits<float>::max();for(size_t i=0;i<d->nodes_count;i++){auto*n=d->nodes+i;if(!n->mesh||!visible(d,n))continue;float m[16];cgltf_node_transform_world(n,m);for(size_t j=0;j<n->mesh->primitives_count;j++){const auto&b=n->mesh->primitives[j];for(size_t a=0;a<b.attributes_count;a++){if(b.attributes[a].type!=cgltf_attribute_type_position)continue;auto*acc=b.attributes[a].data;for(size_t k=0;k<acc->count;k++){float q[3];if(!cgltf_accessor_read_float(acc,k,q,3))return 3;std::array<float,3>w;for(unsigned axis=0;axis<3;axis++)w[axis]=m[axis]*q[0]+m[4+axis]*q[1]+m[8+axis]*q[2]+m[12+axis];low=std::min(low,w[1]);p.push_back(w);}}}}for(auto&w:p)w[1]-=low;auto*f=std::fopen(v[2],"wb");if(!f)return 4;std::fwrite(p.data(),sizeof(p[0]),p.size(),f);std::fclose(f);cgltf_free(d);std::printf("%zu\n",p.size());return 0;}
