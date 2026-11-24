#include <stdio.h>
#include <string.h>

#include "../ufbx.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../stb_image.h"

static void print_hex_prefix(const void *data, size_t size)
{
    const unsigned char *p = (const unsigned char *)data;
    size_t n = size < 16 ? size : 16;
    for (size_t i = 0; i < n; i++) {
        printf("%02x", p[i]);
        if (i + 1 < n) printf(" ");
    }
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "./Running.fbx";
    const char *dump_prefix = argc > 2 ? argv[2] : NULL;
    ufbx_load_opts opts = {0};
    ufbx_error err;
    opts.load_external_files = true;
    ufbx_scene *scene = ufbx_load_file(path, &opts, &err);
    if (!scene) {
        fprintf(stderr, "load failed: %s\n", err.description.data);
        return 1;
    }

    printf("materials=%zu textures=%zu videos=%zu\n",
        scene->materials.count, scene->textures.count, scene->videos.count);

    for (size_t mi = 0; mi < scene->materials.count; mi++) {
        ufbx_material *mat = scene->materials.data[mi];
        ufbx_texture *tex = mat->pbr.base_color.texture;
        if (!tex) tex = mat->fbx.diffuse_color.texture;
        printf("material[%zu] name='%s'\n", mi, mat->name.data);
        if (!tex) {
            printf("  no base/diffuse texture\n");
            continue;
        }

        printf("  texture name='%s'\n", tex->name.data);
        printf("  rel='%s'\n", tex->relative_filename.data ? tex->relative_filename.data : "");
        printf("  abs='%s'\n", tex->filename.data ? tex->filename.data : "");
        printf("  content_size=%zu prefix=", tex->content.size);
        if (tex->content.size > 0) print_hex_prefix(tex->content.data, tex->content.size);
        printf("\n");

        if (tex->content.size > 0) {
            int w = 0, h = 0, ch = 0;
            int ok = stbi_info_from_memory((const stbi_uc *)tex->content.data, (int)tex->content.size, &w, &h, &ch);
            printf("  stbi_info_from_memory=%d w=%d h=%d ch=%d\n", ok, w, h, ch);
            if (!ok) {
                printf("  stbi_failure='%s'\n", stbi_failure_reason());
            }
            if (dump_prefix) {
                char out_path[512];
                FILE *f;
                snprintf(out_path, sizeof(out_path), "%s_%zu.bin", dump_prefix, mi);
                f = fopen(out_path, "wb");
                if (f) {
                    fwrite(tex->content.data, 1, tex->content.size, f);
                    fclose(f);
                    printf("  dumped='%s'\n", out_path);
                }
            }
        }
    }

    for (size_t ni = 0; ni < scene->nodes.count; ni++) {
        ufbx_node *node = scene->nodes.data[ni];
        if (!node->mesh) continue;
        ufbx_mesh *mesh = node->mesh;
        printf("node[%zu] '%s' mesh='%s' materials=%zu uv_sets=%zu faces=%zu verts=%zu\n",
            ni, node->name.data, mesh->name.data, mesh->materials.count,
            mesh->uv_sets.count, mesh->faces.count, mesh->num_vertices);
        if (mesh->vertex_uv.exists) {
            double umin = 1e30, vmin = 1e30, umax = -1e30, vmax = -1e30;
            for (size_t ix = 0; ix < mesh->num_indices; ix++) {
                ufbx_vec2 uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, ix);
                if (uv.x < umin) umin = uv.x;
                if (uv.y < vmin) vmin = uv.y;
                if (uv.x > umax) umax = uv.x;
                if (uv.y > vmax) vmax = uv.y;
            }
            printf("  vertex_uv range: u=[%.3f..%.3f] v=[%.3f..%.3f]\n", umin, umax, vmin, vmax);
        } else {
            printf("  vertex_uv: <none>\n");
        }
        for (size_t ui = 0; ui < mesh->uv_sets.count; ui++) {
            const ufbx_uv_set *uv = &mesh->uv_sets.data[ui];
            double umin = 1e30, vmin = 1e30, umax = -1e30, vmax = -1e30;
            if (!uv->vertex_uv.exists) continue;
            for (size_t ix = 0; ix < mesh->num_indices; ix++) {
                ufbx_vec2 tc = ufbx_get_vertex_vec2(&uv->vertex_uv, ix);
                if (tc.x < umin) umin = tc.x;
                if (tc.y < vmin) vmin = tc.y;
                if (tc.x > umax) umax = tc.x;
                if (tc.y > vmax) vmax = tc.y;
            }
            printf("  uv_set[%zu] '%s' range: u=[%.3f..%.3f] v=[%.3f..%.3f]\n",
                ui, uv->name.data, umin, umax, vmin, vmax);
        }
    }

    ufbx_free_scene(scene);
    return 0;
}
