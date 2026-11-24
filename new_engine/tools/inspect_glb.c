#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <float.h>

#define CGLTF_IMPLEMENTATION
#include "../cgltf.h"

static void print_indent(int depth)
{
    for (int i = 0; i < depth; i++) printf("  ");
}

static int node_index_of(const cgltf_data *data, const cgltf_node *node)
{
    if (!data || !node) return -1;
    return (int)(node - data->nodes);
}

static const char *attr_type_name(cgltf_attribute_type type)
{
    switch (type) {
        case cgltf_attribute_type_position: return "POSITION";
        case cgltf_attribute_type_normal: return "NORMAL";
        case cgltf_attribute_type_tangent: return "TANGENT";
        case cgltf_attribute_type_texcoord: return "TEXCOORD";
        case cgltf_attribute_type_color: return "COLOR";
        case cgltf_attribute_type_joints: return "JOINTS";
        case cgltf_attribute_type_weights: return "WEIGHTS";
        default: return "OTHER";
    }
}

static const char *component_type_name(cgltf_component_type type)
{
    switch (type) {
        case cgltf_component_type_r_8: return "i8";
        case cgltf_component_type_r_8u: return "u8";
        case cgltf_component_type_r_16: return "i16";
        case cgltf_component_type_r_16u: return "u16";
        case cgltf_component_type_r_32u: return "u32";
        case cgltf_component_type_r_32f: return "f32";
        default: return "?";
    }
}

static void mat4_mul16(const float *a, const float *b, float *out)
{
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            float v = 0.0f;
            for (int k = 0; k < 4; k++) {
                v += a[k * 4 + r] * b[c * 4 + k];
            }
            out[c * 4 + r] = v;
        }
    }
}

static void inspect_node_tree(const cgltf_data *data, const cgltf_node *node, int depth)
{
    if (!data || !node) return;
    int idx = node_index_of(data, node);
    print_indent(depth);
    printf("node[%d] name='%s' mesh=%s skin=%s children=%zu",
           idx,
           node->name ? node->name : "",
           node->mesh ? "yes" : "no",
           node->skin ? "yes" : "no",
           node->children_count);
    if (node->has_translation) {
        printf(" t=(%.4f %.4f %.4f)",
               node->translation[0], node->translation[1], node->translation[2]);
    }
    if (node->has_scale) {
        printf(" s=(%.4f %.4f %.4f)",
               node->scale[0], node->scale[1], node->scale[2]);
    }
    if (node->has_rotation) {
        printf(" r=(%.4f %.4f %.4f %.4f)",
               node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]);
    }
    printf("\n");

    for (cgltf_size i = 0; i < node->children_count; i++) {
        inspect_node_tree(data, node->children[i], depth + 1);
    }
}

static void inspect_path(const char *path)
{
    cgltf_options opts = {0};
    cgltf_data *data = NULL;
    cgltf_result res = cgltf_parse_file(&opts, path, &data);
    if (res != cgltf_result_success || !data) {
        fprintf(stderr, "parse failed for %s: %d\n", path, (int)res);
        return;
    }
    res = cgltf_load_buffers(&opts, data, path);
    if (res != cgltf_result_success) {
        fprintf(stderr, "load buffers failed for %s: %d\n", path, (int)res);
        cgltf_free(data);
        return;
    }

    printf("FILE %s\n", path);
    printf("scenes=%zu default_scene=%d nodes=%zu meshes=%zu skins=%zu animations=%zu materials=%zu images=%zu\n",
           data->scenes_count,
           data->scene ? (int)(data->scene - data->scenes) : -1,
           data->nodes_count,
           data->meshes_count,
           data->skins_count,
           data->animations_count,
           data->materials_count,
           data->images_count);

    for (cgltf_size mi = 0; mi < data->meshes_count; mi++) {
        cgltf_mesh *mesh = &data->meshes[mi];
        printf("mesh[%zu] name='%s' primitives=%zu\n",
               mi, mesh->name ? mesh->name : "", mesh->primitives_count);
        for (cgltf_size pi = 0; pi < mesh->primitives_count; pi++) {
            cgltf_primitive *prim = &mesh->primitives[pi];
            bool has_pos = false, has_norm = false, has_uv = false, has_joints = false, has_weights = false;
            cgltf_size vcount = 0;
            for (cgltf_size ai = 0; ai < prim->attributes_count; ai++) {
                cgltf_attribute *attr = &prim->attributes[ai];
                if (!attr->data) continue;
                if (attr->type == cgltf_attribute_type_position) { has_pos = true; vcount = attr->data->count; }
                if (attr->type == cgltf_attribute_type_normal) has_norm = true;
                if (attr->type == cgltf_attribute_type_texcoord) has_uv = true;
                if (attr->type == cgltf_attribute_type_joints) has_joints = true;
                if (attr->type == cgltf_attribute_type_weights) has_weights = true;
            }
            printf("  prim[%zu] verts=%zu indices=%zu pos=%d nrm=%d uv=%d joints=%d weights=%d material='%s'\n",
                   pi,
                   vcount,
                   prim->indices ? prim->indices->count : vcount,
                   has_pos, has_norm, has_uv, has_joints, has_weights,
                   prim->material && prim->material->name ? prim->material->name : "");
            for (cgltf_size ai = 0; ai < prim->attributes_count; ai++) {
                cgltf_attribute *attr = &prim->attributes[ai];
                if (!attr->data) continue;
                printf("    attr[%zu] %s_%d type=%s components=%d count=%zu normalized=%d stride=%zu\n",
                       ai,
                       attr_type_name(attr->type),
                       attr->index,
                       component_type_name(attr->data->component_type),
                       cgltf_num_components(attr->data->type),
                       attr->data->count,
                       attr->data->normalized ? 1 : 0,
                       attr->data->stride);
            }
            for (cgltf_size ai = 0; ai < prim->attributes_count; ai++) {
                cgltf_attribute *attr = &prim->attributes[ai];
                if (!attr->data || (attr->type != cgltf_attribute_type_joints && attr->type != cgltf_attribute_type_weights)) continue;
                cgltf_size sample_count = attr->data->count < 3 ? attr->data->count : 3;
                for (cgltf_size si = 0; si < sample_count; si++) {
                    if (attr->type == cgltf_attribute_type_joints) {
                        cgltf_uint v[4] = {0,0,0,0};
                        cgltf_accessor_read_uint(attr->data, si, v, 4);
                        printf("      sample[%zu] JOINTS_%d=(%u %u %u %u)\n",
                               si, attr->index, (unsigned)v[0], (unsigned)v[1], (unsigned)v[2], (unsigned)v[3]);
                    } else {
                        cgltf_float v[4] = {0,0,0,0};
                        cgltf_accessor_read_float(attr->data, si, v, 4);
                        printf("      sample[%zu] WEIGHTS_%d=(%.4f %.4f %.4f %.4f)\n",
                               si, attr->index, (float)v[0], (float)v[1], (float)v[2], (float)v[3]);
                    }
                }
            }
        }
    }

    for (cgltf_size si = 0; si < data->skins_count; si++) {
        cgltf_skin *skin = &data->skins[si];
        printf("skin[%zu] name='%s' joints=%zu skeleton_node=%d ibm_count=%zu\n",
               si,
               skin->name ? skin->name : "",
               skin->joints_count,
               skin->skeleton ? node_index_of(data, skin->skeleton) : -1,
               skin->inverse_bind_matrices ? skin->inverse_bind_matrices->count : 0);
        for (cgltf_size ni = 0; ni < data->nodes_count; ni++) {
            cgltf_node *node = &data->nodes[ni];
            if (node->skin != skin || !node->mesh) continue;
            cgltf_float mesh_world[16];
            cgltf_node_transform_world(node, mesh_world);
            cgltf_size count = skin->joints_count < 3 ? skin->joints_count : 3;
            for (cgltf_size ji = 0; ji < count; ji++) {
                cgltf_float joint_world[16];
                cgltf_float ibm[16];
                float composed[16];
                float max_diff = 0.0f;
                cgltf_node_transform_world(skin->joints[ji], joint_world);
                if (!skin->inverse_bind_matrices || !cgltf_accessor_read_float(skin->inverse_bind_matrices, ji, ibm, 16)) {
                    continue;
                }
                mat4_mul16((const float *)joint_world, (const float *)ibm, composed);
                for (int mi = 0; mi < 16; mi++) {
                    float d = fabsf(composed[mi] - (float)mesh_world[mi]);
                    if (d > max_diff) max_diff = d;
                }
                printf("  bind_check mesh_node[%d] joint[%zu]='%s' max|joint*ibm-mesh|=%.6f\n",
                       (int)ni, ji, skin->joints[ji] && skin->joints[ji]->name ? skin->joints[ji]->name : "", max_diff);
            }
            break;
        }
    }

    for (cgltf_size ai = 0; ai < data->animations_count; ai++) {
        cgltf_animation *anim = &data->animations[ai];
        printf("anim[%zu] name='%s' channels=%zu samplers=%zu\n",
               ai, anim->name ? anim->name : "", anim->channels_count, anim->samplers_count);
        for (cgltf_size ci = 0; ci < anim->channels_count; ci++) {
            cgltf_animation_channel *ch = &anim->channels[ci];
            if (!ch->target_node || !ch->sampler || !ch->sampler->output) continue;
            if (ch->target_path != cgltf_animation_path_type_translation) continue;
            const char *node_name = ch->target_node->name ? ch->target_node->name : "";
            if (node_name[0] == '\0') continue;
            if (ch->sampler->output->count == 0) continue;
            float minv[3] = { FLT_MAX, FLT_MAX, FLT_MAX };
            float maxv[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
            for (cgltf_size ki = 0; ki < ch->sampler->output->count; ki++) {
                cgltf_float v[3] = {0};
                if (!cgltf_accessor_read_float(ch->sampler->output, ki, v, 3)) continue;
                for (int k = 0; k < 3; k++) {
                    if ((float)v[k] < minv[k]) minv[k] = (float)v[k];
                    if ((float)v[k] > maxv[k]) maxv[k] = (float)v[k];
                }
            }
            printf("  translation target node[%d] '%s' min=(%.4f %.4f %.4f) max=(%.4f %.4f %.4f)\n",
                   node_index_of(data, ch->target_node),
                   node_name,
                   minv[0], minv[1], minv[2],
                   maxv[0], maxv[1], maxv[2]);
        }
    }

    if (data->scene) {
        printf("scene tree:\n");
        for (cgltf_size i = 0; i < data->scene->nodes_count; i++) {
            inspect_node_tree(data, data->scene->nodes[i], 0);
        }
    }

    {
        float xmin = FLT_MAX, ymin = FLT_MAX, zmin = FLT_MAX;
        float xmax = -FLT_MAX, ymax = -FLT_MAX, zmax = -FLT_MAX;
        bool saw = false;
        for (cgltf_size ni = 0; ni < data->nodes_count; ni++) {
            cgltf_node *node = &data->nodes[ni];
            if (!node->mesh) continue;
            cgltf_float world[16];
            cgltf_node_transform_world(node, world);
            for (cgltf_size pi = 0; pi < node->mesh->primitives_count; pi++) {
                cgltf_primitive *prim = &node->mesh->primitives[pi];
                for (cgltf_size ai = 0; ai < prim->attributes_count; ai++) {
                    cgltf_attribute *attr = &prim->attributes[ai];
                    if (attr->type != cgltf_attribute_type_position || !attr->data) continue;
                    for (cgltf_size vi = 0; vi < attr->data->count; vi++) {
                        cgltf_float p[3] = {0};
                        if (!cgltf_accessor_read_float(attr->data, vi, p, 3)) continue;
                        float x = (float)(world[0]*p[0] + world[4]*p[1] + world[8]*p[2] + world[12]);
                        float y = (float)(world[1]*p[0] + world[5]*p[1] + world[9]*p[2] + world[13]);
                        float z = (float)(world[2]*p[0] + world[6]*p[1] + world[10]*p[2] + world[14]);
                        if (x < xmin) xmin = x; if (x > xmax) xmax = x;
                        if (y < ymin) ymin = y; if (y > ymax) ymax = y;
                        if (z < zmin) zmin = z; if (z > zmax) zmax = z;
                        saw = true;
                    }
                    break;
                }
            }
        }
        if (saw) {
            printf("world bounds min=(%.4f %.4f %.4f) max=(%.4f %.4f %.4f)\n",
                   xmin, ymin, zmin, xmax, ymax, zmax);
        }
    }

    printf("\n");
    cgltf_free(data);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <file.glb> [more.glb]\n", argv[0]);
        return 1;
    }
    for (int i = 1; i < argc; i++) inspect_path(argv[i]);
    return 0;
}
