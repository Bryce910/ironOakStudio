#ifndef VALWOOD_WORLD_SHADER_GRAPH_H
#define VALWOOD_WORLD_SHADER_GRAPH_H
#include <array>
#include <string>
#include <vector>
namespace world_graph {
enum class Op : unsigned { Value,Color,Time,UV,WorldPosition,BaseColor,Roughness,Metallic,NormalStrength,Add,Multiply,Mix,Sine,Noise,Clamp,Fresnel,TextureRGB,TextureR,TextureG,TextureB,TextureAlpha };
struct Node {unsigned id=0;Op op=Op::Value;std::array<int,3> inputs={-1,-1,-1};std::array<float,4> value={1,1,1,1};float x=0,y=0;};
// Outputs: albedo, roughness, metallic, normal multiplier, emission, vertex XYZ.
struct Graph {std::string alias;std::vector<Node> nodes;std::array<int,6> outputs={-1,-1,-1,-1,-1,-1};float output_x=640,output_y=20;};
struct Code {std::string surface,vertex;};
const char *name(Op);unsigned arity(Op);
bool texture_op(Op); // Texture nodes take UV XYZ input; value[3] is an image index.
bool validate(const Graph &,std::string &error);
bool compile(const std::vector<Graph> &,Code &,std::string &error);
Graph preset(const std::string &alias,unsigned kind); // tint / animated emission / wind displacement
}
#endif
