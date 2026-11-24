// CPU-only inspection of an actual Valwood skeletal GLB. No GL or game session.
#include "engine/world_animation.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace wa = world_animation;
constexpr std::size_t sample_limit = 100000;

std::string quote(const std::string &value) {
    std::ostringstream out;
    out << '"';
    for (unsigned char c : value) {
        switch (c) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (c < 32) out << "\\u00" << std::hex << std::setw(2)
                            << std::setfill('0') << unsigned(c) << std::dec;
            else out << c;
        }
    }
    out << '"';
    return out.str();
}

bool finite(wa::Vec3 v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

struct Bounds {
    std::array<double, 3> low = {INFINITY, INFINITY, INFINITY};
    std::array<double, 3> high = {-INFINITY, -INFINITY, -INFINITY};
    std::uint64_t points = 0;
    void add(wa::Vec3 v) {
        if (!finite(v)) throw std::runtime_error("Non-finite transformed position");
        const std::array<double, 3> values = {v.x, v.y, v.z};
        for (unsigned i = 0; i < 3; ++i) {
            low[i] = std::min(low[i], values[i]);
            high[i] = std::max(high[i], values[i]);
        }
        ++points;
    }
    void json(std::ostream &out) const {
        if (!points) { out << "null"; return; }
        out << "{\"minimum\":[" << low[0] << ',' << low[1] << ',' << low[2]
            << "],\"maximum\":[" << high[0] << ',' << high[1] << ',' << high[2]
            << "]}";
    }
};

struct Measurements {
    Bounds mesh_bounds, joint_bounds;
    std::uint64_t samples = 0, vertices = 0, matrices = 0, sockets = 0;
    double normal_min = INFINITY, normal_max = 0;
};

void matrix_check(const wa::Mat4 &matrix) {
    for (float value : matrix.m)
        if (!std::isfinite(value)) throw std::runtime_error("Non-finite joint/skin/socket matrix");
}

void inspect_pose(const wa::Asset &asset, const wa::Pose &pose,
                  const std::vector<wa::Socket> &sockets, Measurements &result) {
    if (pose.nodes != asset.skeleton.joints.size() || pose.joints != asset.skeleton.palette.size())
        throw std::runtime_error("Sampled pose does not match imported skeleton");
    for (unsigned i = 0; i < pose.nodes; ++i) {
        matrix_check(pose.global[i]);
        result.joint_bounds.add(wa::transform_point(pose.global[i], {}));
        ++result.matrices;
    }
    for (unsigned i = 0; i < pose.joints; ++i) {
        matrix_check(pose.skin[i]);
        ++result.matrices;
    }
    std::string error;
    for (const auto &socket : sockets) {
        wa::Mat4 transform;
        if (!wa::socket_transform(asset.skeleton, pose, socket, transform, error))
            throw std::runtime_error("Socket " + socket.joint + ": " + error);
        matrix_check(transform);
        ++result.sockets;
    }
    for (std::size_t i = 0; i < asset.mesh.vertices.size(); ++i) {
        wa::Vec3 position, normal;
        if (!wa::deform_vertex(asset.mesh.vertices[i], pose, position, normal, error))
            throw std::runtime_error("Vertex " + std::to_string(i) + ": " + error);
        if (!finite(position) || !finite(normal))
            throw std::runtime_error("Non-finite deformed vertex/normal");
        const double length = std::hypot(double(normal.x), double(normal.y), double(normal.z));
        if (!(length > 0)) throw std::runtime_error("Zero deformed normal");
        result.normal_min = std::min(result.normal_min, length);
        result.normal_max = std::max(result.normal_max, length);
        // deform_vertex returns mesh-local coordinates; report asset coordinates.
        result.mesh_bounds.add(wa::transform_point(pose.global[asset.skeleton.mesh_node], position));
        ++result.vertices;
    }
    ++result.samples;
}

void measurement_json(std::ostream &out, const Measurements &m) {
    out << "{\"samples\":" << m.samples << ",\"deformed_vertices\":" << m.vertices
        << ",\"joint_and_skin_matrix_checks\":" << m.matrices
        << ",\"socket_checks\":" << m.sockets << ",\"mesh_bounds\":";
    m.mesh_bounds.json(out);
    out << ",\"joint_position_bounds\":";
    m.joint_bounds.json(out);
    out << ",\"deformed_normal_length\":[" << m.normal_min << ',' << m.normal_max << "]}";
}

std::vector<double> sample_times(const wa::Clip &clip, double hz) {
    if (!std::isfinite(clip.duration * hz) || clip.duration * hz > double(sample_limit - 2))
        throw std::runtime_error("Clip exceeds the 100000-sample inspection budget; lower --hz");
    std::vector<double> times = {0, clip.duration};
    const auto steps = std::uint64_t(std::ceil(clip.duration * hz));
    for (std::uint64_t i = 1; i < steps; ++i) times.push_back(double(i) / hz);
    // Include authored times as well as the dense uniform grid. This exercises
    // STEP boundaries and exact endpoints that a uniform grid can miss.
    for (const auto &channel : clip.channels)
        for (float time : channel.times) times.push_back(time);
    std::sort(times.begin(), times.end());
    times.erase(std::unique(times.begin(), times.end()), times.end());
    if (times.size() > sample_limit)
        throw std::runtime_error("Authored keys exceed the 100000-sample inspection budget");
    return times;
}

struct ClipResult { Measurements measurements; std::size_t timeline_samples = 0; };

double parse_hz(const char *value) {
    char *end = nullptr;
    const double result = std::strtod(value, &end);
    if (end == value || *end || !std::isfinite(result) || result < 1 || result > 240)
        throw std::runtime_error("--hz requires a finite number between 1 and 240");
    return result;
}

int run(int argc, char **argv, std::string &path) {
    double hz = 60;
    std::vector<std::string> requested_sockets;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help") {
            std::cout << "Usage: check_hero_skeletal ASSET.glb [--hz 60] [--socket JOINT ...]\n"
                      << "CPU import/deformation inspection; JSON to stdout; no visual quality verdict.\n";
            return 0;
        } else if (arg == "--hz" && i + 1 < argc) {
            hz = parse_hz(argv[++i]);
        } else if (arg == "--socket" && i + 1 < argc) {
            requested_sockets.emplace_back(argv[++i]);
        } else if (!arg.empty() && arg.front() == '-') {
            throw std::runtime_error("Unknown/incomplete option: " + arg);
        } else if (path.empty()) {
            path = arg;
        } else {
            throw std::runtime_error("Only one GLB may be inspected per invocation");
        }
    }
    if (path.empty()) throw std::runtime_error("Missing ASSET.glb; use --help for usage");
    const auto started = std::chrono::steady_clock::now();
    wa::Asset asset;
    std::string error;
    if (!wa::load_glb(path.c_str(), asset, error)) throw std::runtime_error("Import: " + error);
    if (!wa::validate(asset, error)) throw std::runtime_error("Validation: " + error);
    if (asset.mesh.vertices.empty() || asset.mesh.indices.empty())
        throw std::runtime_error("Hero asset has no renderable skinned triangles");

    std::map<std::string, unsigned> names;
    for (const auto &joint : asset.skeleton.joints) ++names[joint.name];
    std::vector<wa::Socket> sockets;
    for (const auto &entry : names) if (entry.second == 1) sockets.push_back({entry.first, {}});
    for (const auto &name : requested_sockets)
        if (names[name] != 1) throw std::runtime_error("Requested socket joint is missing or ambiguous: " + name);

    wa::Pose pose;
    Measurements rest;
    if (!wa::rest_pose(asset.skeleton, pose, error)) throw std::runtime_error("Rest pose: " + error);
    inspect_pose(asset, pose, sockets, rest);
    std::vector<ClipResult> results(asset.clips.size());
    std::uint64_t total_samples = 1;
    for (unsigned i = 0; i < asset.clips.size(); ++i) {
        const auto &clip = asset.clips[i];
        const auto times = sample_times(clip, hz);
        if (total_samples + times.size() + 1 > sample_limit)
            throw std::runtime_error("Asset exceeds the 100000-sample inspection budget; lower --hz");
        auto &result = results[i];
        result.timeline_samples = times.size();
        for (double time : times) {
            try {
                if (!wa::sample(asset.skeleton, clip, time, false, pose, error))
                    throw std::runtime_error(error);
                inspect_pose(asset, pose, sockets, result.measurements);
            } catch (const std::exception &e) {
                std::ostringstream detail;
                detail << "Clip " << i << " (" << clip.name << ") at " << time << "s: " << e.what();
                throw std::runtime_error(detail.str());
            }
        }
        // Exercise the runtime's loop boundary separately, without assuming
        // this clip was authored as a seamless looping animation.
        if (!wa::sample(asset.skeleton, clip, clip.duration, true, pose, error))
            throw std::runtime_error("Loop boundary " + clip.name + ": " + error);
        inspect_pose(asset, pose, sockets, result.measurements);
        total_samples += result.measurements.samples;
    }

    std::ostringstream out;
    out << std::setprecision(10)
        << "{\"schema\":\"valwood.hero-skeletal-inspection.v1\",\"passed\":true,\"asset\":" << quote(path)
        << ",\"scope\":\"CPU import, validation and sampled deformation\",\"visual_quality_evaluated\":false"
        << ",\"sample_hz\":" << hz << ",\"includes_authored_key_times\":true"
        << ",\"has_animations\":" << (asset.clips.empty() ? "false" : "true")
        << ",\"total_samples\":" << total_samples << ",\"skeleton\":{\"nodes\":"
        << asset.skeleton.joints.size() << ",\"palette_joints\":" << asset.skeleton.palette.size()
        << ",\"mesh_node\":" << asset.skeleton.mesh_node << ",\"socket_names_checked\":" << sockets.size()
        << ",\"duplicate_joint_names\":[";
    bool comma = false;
    for (const auto &entry : names) if (entry.second > 1) {
        if (comma) out << ',';
        comma = true;
        out << quote(entry.first);
    }
    out << "],\"joints\":[";
    for (unsigned i = 0; i < asset.skeleton.joints.size(); ++i) {
        if (i) out << ',';
        const auto &joint = asset.skeleton.joints[i];
        out << "{\"index\":" << i << ",\"name\":" << quote(joint.name) << ",\"parent\":" << joint.parent << '}';
    }
    out << "]},\"geometry\":{\"vertices\":" << asset.mesh.vertices.size()
        << ",\"triangles\":" << asset.mesh.indices.size() / 3
        << ",\"primitives\":" << asset.mesh.primitives.size() << "},\"materials\":[";
    std::uint64_t image_bytes = 0;
    for (const auto &image : asset.images) image_bytes += image.encoded.size();
    for (unsigned i = 0; i < asset.materials.size(); ++i) {
        if (i) out << ',';
        const auto &m = asset.materials[i];
        out << "{\"index\":" << i << ",\"base_image\":" << m.base.image << ",\"normal_image\":" << m.normal.image
            << ",\"metallic_roughness_image\":" << m.metallic_roughness.image
            << ",\"occlusion_image\":" << m.occlusion.image << ",\"emissive_image\":" << m.emissive.image
            << ",\"roughness_factor\":" << m.roughness << ",\"metallic_factor\":" << m.metallic
            << ",\"alpha_cutoff\":" << m.alpha_cutoff << ",\"double_sided\":" << (m.double_sided ? "true" : "false") << '}';
    }
    out << "],\"embedded_images\":" << asset.images.size() << ",\"encoded_image_bytes\":" << image_bytes
        << ",\"rest_pose\":";
    measurement_json(out, rest);
    out << ",\"clips\":[";
    for (unsigned i = 0; i < asset.clips.size(); ++i) {
        if (i) out << ',';
        const auto &clip = asset.clips[i];
        std::uint64_t keys = 0;
        for (const auto &channel : clip.channels) keys += channel.times.size();
        out << "{\"index\":" << i << ",\"name\":" << quote(clip.name)
            << ",\"duration_seconds\":" << clip.duration << ",\"channels\":" << clip.channels.size()
            << ",\"authored_keys\":" << keys << ",\"timeline_samples\":" << results[i].timeline_samples
            << ",\"loop_boundary_samples\":1,\"measurements\":";
        measurement_json(out, results[i].measurements);
        out << '}';
    }
    const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    out << "],\"elapsed_seconds\":" << elapsed
        << ",\"limitations\":[\"CPU sampling does not approve animation flow, foot contact, grips, clipping or presentation.\","
        << "\"No GPU rendering, texture decoding, retarget quality, combat or performance assessment.\","
        << "\"All vertex checks are discrete samples; intermediate unsampled times are not proven.\","
        << "\"Duplicate joint names receive matrix checks but cannot be queried as unambiguous named sockets.\"]}\n";
    std::cout << out.str();
    return 0;
}
} // namespace

int main(int argc, char **argv) {
    std::string path;
    try { return run(argc, argv, path); }
    catch (const std::exception &e) {
        std::cout << "{\"schema\":\"valwood.hero-skeletal-inspection.v1\",\"passed\":false,\"asset\":"
                  << quote(path) << ",\"visual_quality_evaluated\":false,\"error\":" << quote(e.what()) << "}\n";
        return 1;
    }
}
