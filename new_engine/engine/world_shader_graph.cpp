#include "engine/world_shader_graph.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <sstream>
namespace world_graph {
namespace {
constexpr unsigned limit=32;
bool token(const std::string &s){return !s.empty()&&s.size()<96&&std::all_of(s.begin(),s.end(),[](char c){return(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-';});}
struct Plan {std::array<int,limit> index,type,state;std::vector<unsigned> order;Plan(){index.fill(-1);type.fill(0);state.fill(0);}};
bool plan(const Graph &g,Plan &p,std::string &e){
 if(!token(g.alias)||g.nodes.size()>limit||!std::isfinite(g.output_x)||!std::isfinite(g.output_y)){e="Invalid graph target, node budget or layout";return false;}
 for(unsigned i=0;i<g.nodes.size();i++){const auto &n=g.nodes[i];if(n.id>=limit||p.index[n.id]>=0||unsigned(n.op)>unsigned(Op::TextureAlpha)||!std::isfinite(n.x)||!std::isfinite(n.y)||std::abs(n.x)>20000||std::abs(n.y)>20000){e="Invalid/duplicate graph node";return false;}if(texture_op(n.op)&&(n.value[3]<0||n.value[3]>=16||std::floor(n.value[3])!=n.value[3])){e="Texture image index must be an integer0–15";return false;}p.index[n.id]=int(i);for(float v:n.value)if(!std::isfinite(v)||std::abs(v)>1000){e="Non-finite or excessive graph value";return false;}for(int id:n.inputs)if(id< -1||id>=int(limit)){e="Invalid graph connection";return false;}}
 std::function<bool(unsigned)> visit=[&](unsigned id){if(p.index[id]<0){e="Missing graph node";return false;}if(p.state[id]==2)return true;if(p.state[id]==1){e="Graph has a cycle";return false;}p.state[id]=1;const auto &n=g.nodes[unsigned(p.index[id])];const unsigned count=arity(n.op);for(unsigned k=0;k<count;k++){if(n.inputs[k]<0||!visit(unsigned(n.inputs[k]))){if(e.empty())e="Connect every required input";return false;}}
 int t=1;switch(n.op){case Op::Color:case Op::UV:case Op::WorldPosition:case Op::BaseColor:case Op::TextureRGB:t=3;break;case Op::Add:case Op::Multiply:t=std::max(p.type[unsigned(n.inputs[0])],p.type[unsigned(n.inputs[1])]);break;case Op::Mix:if(p.type[unsigned(n.inputs[2])]!=1){e="Mix factor must be scalar";return false;}t=std::max(p.type[unsigned(n.inputs[0])],p.type[unsigned(n.inputs[1])]);break;case Op::Sine:case Op::Clamp:t=p.type[unsigned(n.inputs[0])];break;default:break;}p.type[id]=t;p.state[id]=2;p.order.push_back(id);return true;};
 for(const auto &n:g.nodes)if(!visit(n.id))return false;
 for(unsigned k=0;k<6;k++){int id=g.outputs[k];if(id< -1||id>=int(limit)||(id>=0&&p.index[unsigned(id)]<0)){e="Output refers to a missing graph node";return false;}if(id>=0&&(k==1||k==2||k==3)&&p.type[unsigned(id)]!=1){e="Roughness, metallic and normal outputs need scalar nodes";return false;}}
 return true;
}
std::string number(float v){std::ostringstream s;s<<std::fixed<<std::setprecision(7)<<v;return s.str();}
std::string generate(const Graph &g,const Plan &p,bool vertex,std::string &error){
 std::array<bool,limit> used={};std::function<void(unsigned)> mark=[&](unsigned id){if(used[id])return;used[id]=true;const auto &n=g.nodes[unsigned(p.index[id])];for(unsigned k=0;k<arity(n.op);k++)mark(unsigned(n.inputs[k]));};
 if(vertex){if(g.outputs[5]<0)return {};mark(unsigned(g.outputs[5]));}else for(unsigned k=0;k<5;k++)if(g.outputs[k]>=0)mark(unsigned(g.outputs[k]));
 std::ostringstream out;
 for(unsigned id:p.order){if(!used[id])continue;const auto &n=g.nodes[unsigned(p.index[id])];if(vertex&&(n.op==Op::BaseColor||n.op==Op::Roughness||n.op==Op::Metallic||n.op==Op::NormalStrength||n.op==Op::Fresnel||texture_op(n.op))){error="Vertex offset cannot read a fragment-only source";return {};}
 auto input=[&](unsigned k,int type=0){int source=n.inputs[k];std::string v="n"+std::to_string(source);if(type==3&&p.type[unsigned(source)]==1)v="vec3("+v+")";return v;};std::string expression;
 switch(n.op){case Op::Value:expression=number(n.value[0]);break;case Op::Color:expression="vec3("+number(n.value[0])+","+number(n.value[1])+","+number(n.value[2])+")";break;case Op::Time:expression="mod(time,64.)";break;case Op::UV:expression="vec3(uv,0.)";break;case Op::WorldPosition:expression="world";break;case Op::BaseColor:expression="base";break;case Op::Roughness:expression="rough";break;case Op::Metallic:expression="metal";break;case Op::NormalStrength:expression="1.";break;case Op::Add:expression="("+input(0,p.type[id])+"+"+input(1,p.type[id])+")";break;case Op::Multiply:expression="("+input(0,p.type[id])+"*"+input(1,p.type[id])+")";break;case Op::Mix:expression="mix("+input(0,p.type[id])+","+input(1,p.type[id])+",clamp("+input(2)+",0.,1.))";break;case Op::Sine:expression="sin("+input(0)+")";break;case Op::Noise:expression="fract(sin(dot("+input(0,3)+",vec3(12.9898,78.233,37.719)))*43758.5453)";break;case Op::Clamp:expression="clamp("+input(0)+","+number(std::min(n.value[0],n.value[1]))+","+number(std::max(n.value[0],n.value[1]))+")";break;case Op::TextureRGB:case Op::TextureR:case Op::TextureG:case Op::TextureB:case Op::TextureAlpha:{const char *channels[]={"rgb","r","g","b","a"};auto uv="("+input(0,3)+").xy";expression="studioImage("+std::to_string(int(n.value[3]))+","+uv+",dFdx("+uv+"),dFdy("+uv+"))."+channels[unsigned(n.op)-unsigned(Op::TextureRGB)];break;}case Op::Fresnel:expression="pow(clamp(1.-max(dot(normal,view),0.),0.,1.),"+number(std::clamp(n.value[0],.1f,16.f))+")";break;}
 // Bound each intermediate, not only the PBR output: finite authoring values
 // can otherwise overflow in a repeated multiply chain and create NaNs.
 out<<(p.type[id]==3?"vec3":"float")<<" n"<<id<<"=clamp("<<expression<<",-65504.,65504.);\n";
 }
 auto value=[&](unsigned k,bool vec){unsigned id=unsigned(g.outputs[k]);std::string v="n"+std::to_string(id);return vec&&p.type[id]==1?"vec3("+v+")":v;};
 if(vertex)out<<"return clamp("<<value(5,true)<<",vec3(-.15),vec3(.15));\n";
 else{const char *names[]={"base","rough","metal","normalScale","emission"};for(unsigned k=0;k<5;k++)if(g.outputs[k]>=0)out<<names[k]<<"="<<value(k,k==0||k==4)<<";\n";}
 return out.str();
}
}
const char *name(Op o){static const char *names[]={"Value","Color","Time","UV","World position","Source albedo","Source roughness","Source metallic","Source normal strength","Add","Multiply","Mix","Sine","Noise","Clamp","Fresnel","Image RGB","Image red","Image green","Image blue","Image alpha"};return unsigned(o)<21?names[unsigned(o)]:"Invalid";}
bool texture_op(Op op){return unsigned(op)>=unsigned(Op::TextureRGB)&&unsigned(op)<=unsigned(Op::TextureAlpha);}
unsigned arity(Op o){switch(o){case Op::Add:case Op::Multiply:return 2;case Op::Mix:return 3;case Op::Sine:case Op::Noise:case Op::Clamp:case Op::TextureRGB:case Op::TextureR:case Op::TextureG:case Op::TextureB:case Op::TextureAlpha:return 1;default:return 0;}}
bool validate(const Graph &g,std::string &e){e.clear();Plan p;if(!plan(g,p,e))return false;generate(g,p,true,e);return e.empty();}
bool compile(const std::vector<Graph> &graphs,Code &code,std::string &e){
 e.clear();if(graphs.size()>8){e="Eight material graphs maximum";return false;}Code next;std::ostringstream fs,vs;fs<<"uniform int uStudioGraphIndex;\nbool studioGraphSurface(vec2 uv,vec3 world,vec3 normal,vec3 view,float time,inout vec3 base,inout float rough,inout float metal,inout float normalScale,inout vec3 emission){\n";vs<<"uniform int uStudioGraphIndex;\nvec3 studioGraphVertex(vec2 uv,vec3 world,float time){\n";
 for(unsigned i=0;i<graphs.size();i++){for(unsigned j=0;j<i;j++)if(graphs[i].alias==graphs[j].alias){e="Duplicate graph target";return false;}Plan p;if(!plan(graphs[i],p,e))return false;auto fragment=generate(graphs[i],p,false,e),vertex=generate(graphs[i],p,true,e);if(!e.empty())return false;fs<<"if(uStudioGraphIndex=="<<i<<"){\n"<<fragment<<"return true;}\n";if(graphs[i].outputs[5]>=0)vs<<"if(uStudioGraphIndex=="<<i<<"){\n"<<vertex<<"}\n";}
 fs<<"return false;}\n";vs<<"return vec3(0.);}\n";next.surface=fs.str();next.vertex=vs.str();code=std::move(next);return true;
}
Graph preset(const std::string &alias,unsigned kind){Graph g;g.alias=alias;Node a;a.id=0;a.op=kind==2?Op::Time:Op::BaseColor;a.x=20;a.y=20;g.nodes.push_back(a);Node b;b.id=1;b.op=Op::Color;b.value=kind==1?std::array<float,4>{.09f,.035f,.008f,1}:kind==2?std::array<float,4>{.03f,0,0,1}:std::array<float,4>{.95f,1,.92f,1};b.x=20;b.y=170;g.nodes.push_back(b);Node c;c.id=2;c.op=Op::Multiply;c.inputs={0,1,-1};c.x=280;c.y=40;g.nodes.push_back(c);if(kind==2){Node d;d.id=3;d.op=Op::Sine;d.inputs={0,-1,-1};d.x=20;d.y=310;g.nodes.push_back(d);g.nodes[2].inputs={3,1,-1};g.outputs[5]=2;}else if(kind==1){g.nodes[0].op=Op::Time;Node d;d.id=3;d.op=Op::Sine;d.inputs={0,-1,-1};d.x=20;d.y=310;g.nodes.push_back(d);g.nodes[2].inputs={3,1,-1};g.outputs[4]=2;}else g.outputs[0]=2;return g;}
}
