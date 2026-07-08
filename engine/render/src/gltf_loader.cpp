#include "ae/render/gltf_loader.h"
#include "ae/core/log.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <variant>


#define AE_LOG_CATEGORY "Render"

namespace ae::render {
namespace {

// ============================================================
// Minimal JSON parser — just enough for glTF 2.0
// ============================================================

enum class JsonType { Null, Bool, Number, String, Array, Object };

struct JsonValue {
    JsonType type {JsonType::Null};
    bool bool_val {false};
    double number_val {0.0};
    std::string string_val {};
    std::vector<JsonValue> array_val {};
    std::unordered_map<std::string, JsonValue> object_val {};
};

class JsonParser {
public:
    explicit JsonParser(const std::string& input) : input_(input), pos_(0) {}

    JsonValue parse() {
        skip_whitespace();
        auto val = parse_value();
        return val;
    }

    const std::string& error() const { return error_; }

private:
    void skip_whitespace() {
        while (pos_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[pos_]))) {
            ++pos_;
        }
    }

    char peek() {
        skip_whitespace();
        return pos_ < input_.size() ? input_[pos_] : '\0';
    }

    char advance() {
        skip_whitespace();
        return pos_ < input_.size() ? input_[pos_++] : '\0';
    }

    JsonValue parse_value() {
        const char c = peek();
        switch (c) {
            case 'n': return parse_null();
            case 't': case 'f': return parse_bool();
            case '"': return parse_string();
            case '[': return parse_array();
            case '{': return parse_object();
            default:
                if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
                    return parse_number();
                }
                error_ = std::string("Unexpected character: '") + c + "'";
                return {};
        }
    }

    JsonValue parse_null() {
        if (input_.substr(pos_, 4) == "null") { pos_ += 4; return {}; }
        error_ = "Expected null";
        return {};
    }

    JsonValue parse_bool() {
        if (input_.substr(pos_, 4) == "true")  { pos_ += 4; JsonValue v; v.type = JsonType::Bool; v.bool_val = true;  return v; }
        if (input_.substr(pos_, 5) == "false") { pos_ += 5; JsonValue v; v.type = JsonType::Bool; v.bool_val = false; return v; }
        error_ = "Expected bool";
        return {};
    }

    JsonValue parse_number() {
        const std::size_t start = pos_;
        if (peek() == '-') advance();
        while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_]))) ++pos_;
        if (pos_ < input_.size() && input_[pos_] == '.') {
            ++pos_;
            while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_]))) ++pos_;
        }
        if (pos_ < input_.size() && (input_[pos_] == 'e' || input_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < input_.size() && (input_[pos_] == '+' || input_[pos_] == '-')) ++pos_;
            while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_]))) ++pos_;
        }
        JsonValue v;
        v.type = JsonType::Number;
        v.number_val = std::stod(input_.substr(start, pos_ - start));
        return v;
    }

    JsonValue parse_string() {
        advance(); // skip opening "
        std::string result;
        while (pos_ < input_.size()) {
            const char c = input_[pos_++];
            if (c == '"') {
                JsonValue v;
                v.type = JsonType::String;
                v.string_val = result;
                return v;
            }
            if (c == '\\' && pos_ < input_.size()) {
                const char esc = input_[pos_++];
                switch (esc) {
                    case '"':  result += '"';  break;
                    case '\\': result += '\\'; break;
                    case '/':  result += '/';  break;
                    case 'n':  result += '\n'; break;
                    case 't':  result += '\t'; break;
                    default:   result += esc;  break;
                }
            } else {
                result += c;
            }
        }
        error_ = "Unterminated string";
        return {};
    }

    JsonValue parse_array() {
        advance(); // skip [
        JsonValue v;
        v.type = JsonType::Array;
        skip_whitespace();
        if (peek() == ']') { advance(); return v; }
        while (true) {
            v.array_val.push_back(parse_value());
            skip_whitespace();
            const char c = advance();
            if (c == ']') break;
            if (c != ',') { error_ = "Expected , or ]"; return v; }
        }
        return v;
    }

    JsonValue parse_object() {
        advance(); // skip {
        JsonValue v;
        v.type = JsonType::Object;
        skip_whitespace();
        if (peek() == '}') { advance(); return v; }
        while (true) {
            skip_whitespace();
            JsonValue key = parse_string();
            skip_whitespace();
            if (advance() != ':') { error_ = "Expected :"; return v; }
            v.object_val[key.string_val] = parse_value();
            skip_whitespace();
            const char c = advance();
            if (c == '}') break;
            if (c != ',') { error_ = "Expected , or }"; return v; }
        }
        return v;
    }

    const std::string& input_;
    std::size_t pos_;
    std::string error_;
};

// Convenience accessors for JsonValue
bool json_get_bool(const JsonValue& v, const char* key, bool default_val = false) {
    if (v.type != JsonType::Object) return default_val;
    auto it = v.object_val.find(key);
    if (it == v.object_val.end() || it->second.type != JsonType::Bool) return default_val;
    return it->second.bool_val;
}

double json_get_number(const JsonValue& v, const char* key, double default_val = 0.0) {
    if (v.type != JsonType::Object) return default_val;
    auto it = v.object_val.find(key);
    if (it == v.object_val.end() || it->second.type != JsonType::Number) return default_val;
    return it->second.number_val;
}

int json_get_int(const JsonValue& v, const char* key, int default_val = 0) {
    return static_cast<int>(json_get_number(v, key, static_cast<double>(default_val)));
}

std::string json_get_string(const JsonValue& v, const char* key, const std::string& default_val = "") {
    if (v.type != JsonType::Object) return default_val;
    auto it = v.object_val.find(key);
    if (it == v.object_val.end() || it->second.type != JsonType::String) return default_val;
    return it->second.string_val;
}

const JsonValue* json_get_object(const JsonValue& v, const char* key) {
    if (v.type != JsonType::Object) return nullptr;
    auto it = v.object_val.find(key);
    if (it == v.object_val.end() || it->second.type != JsonType::Object) return nullptr;
    return &it->second;
}

const JsonValue* json_get_array(const JsonValue& v, const char* key) {
    if (v.type != JsonType::Object) return nullptr;
    auto it = v.object_val.find(key);
    if (it == v.object_val.end() || it->second.type != JsonType::Array) return nullptr;
    return &it->second;
}

// ============================================================
// glTF 2.0 loader
// ============================================================

struct GltfAccessor {
    int buffer_view {-1};
    int component_type {0};
    int count {0};
    std::string type;
    int byte_offset {0};
};

struct GltfBufferView {
    int buffer {0};
    int byte_offset {0};
    int byte_length {0};
};

struct GltfBuffer {
    std::string uri;
    int byte_length {0};
};

struct GltfPrimitive {
    int position_accessor {-1};
    int normal_accessor {-1};
    int texcoord_accessor {-1};
    int joints_accessor {-1};
    int weights_accessor {-1};
    int indices_accessor {-1};
    int material {-1};
};

struct GltfMeshInfo {
    std::vector<GltfPrimitive> primitives;
};

struct GltfNode {
    int mesh {-1};
    std::vector<int> children;
};

struct GltfMaterial {
    float base_color_r {1.0F};
    float base_color_g {1.0F};
    float base_color_b {1.0F};
};

bool read_file(const std::string& path, std::vector<std::uint8_t>& data) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    const auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    data.resize(static_cast<std::size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), size);
    return file.good();
}

int component_size(int component_type) {
    switch (component_type) {
        case 5120: case 5121: return 1; // BYTE, UNSIGNED_BYTE
        case 5122: case 5123: return 2; // SHORT, UNSIGNED_SHORT
        case 5125: case 5126: return 4; // UNSIGNED_INT, FLOAT
        default: return 0;
    }
}

int component_count(const std::string& type) {
    if (type == "SCALAR") return 1;
    if (type == "VEC2")   return 2;
    if (type == "VEC3")   return 3;
    if (type == "VEC4")   return 4;
    if (type == "MAT2")   return 4;
    if (type == "MAT3")   return 9;
    if (type == "MAT4")   return 16;
    return 0;
}

template<typename T>
void read_accessor_data(const std::vector<std::uint8_t>& buffer_data,
                         const GltfAccessor& accessor,
                         const GltfBufferView& buffer_view,
                         std::vector<T>& out) {
    const int comp_size = component_size(accessor.component_type);
    const int comp_count = component_count(accessor.type);
    const int stride = comp_size * comp_count;
    const std::size_t offset = static_cast<std::size_t>(buffer_view.byte_offset + accessor.byte_offset);
    const std::size_t total_bytes = static_cast<std::size_t>(accessor.count) * static_cast<std::size_t>(stride);

    out.resize(static_cast<std::size_t>(accessor.count * comp_count));

    for (int i = 0; i < accessor.count; ++i) {
        for (int c = 0; c < comp_count; ++c) {
            const std::size_t byte_pos = offset + static_cast<std::size_t>(i * stride + c * comp_size);
            if (byte_pos + comp_size > buffer_data.size()) return;

            float val = 0.0F;
            if (accessor.component_type == 5126) { // FLOAT
                std::uint32_t bits = 0;
                std::memcpy(&bits, &buffer_data[byte_pos], 4);
                val = *reinterpret_cast<const float*>(&bits);
            } else if (accessor.component_type == 5123) { // UNSIGNED_SHORT
                std::uint16_t bits = 0;
                std::memcpy(&bits, &buffer_data[byte_pos], 2);
                val = static_cast<float>(bits) / 65535.0F;
            } else if (accessor.component_type == 5121) { // UNSIGNED_BYTE
                val = static_cast<float>(buffer_data[byte_pos]) / 255.0F;
            } else if (accessor.component_type == 5125) { // UNSIGNED_INT
                std::uint32_t bits = 0;
                std::memcpy(&bits, &buffer_data[byte_pos], 4);
                val = static_cast<float>(bits);
            }
            out[static_cast<std::size_t>(i * comp_count + c)] = val;
        }
    }
}

}  // namespace

// ============================================================
// Public API
// ============================================================

bool GltfLoader::load(const std::string& gltf_path, GltfModel& model) {
    // Read JSON
    std::ifstream file(gltf_path);
    if (!file.is_open()) {
        error_ = "Failed to open: " + gltf_path;
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string json = buffer.str();

    // Find base directory for resolving buffer URIs
    const auto last_slash = gltf_path.find_last_of("/\\");
    const std::string base_path = (last_slash != std::string::npos) ? gltf_path.substr(0, last_slash + 1) : "";

    return load_from_string(json, base_path, model);
}

bool GltfLoader::load_from_string(const std::string& json, const std::string& base_path, GltfModel& model) {
    // Parse JSON
    JsonParser parser(json);
    JsonValue root = parser.parse();
    if (!parser.error().empty()) {
        error_ = "JSON parse error: " + parser.error();
        return false;
    }

    // Parse buffers
    std::vector<GltfBuffer> buffers;
    if (const auto* bufs = json_get_array(root, "buffers")) {
        for (const auto& b : bufs->array_val) {
            GltfBuffer buf;
            buf.uri = json_get_string(b, "uri");
            buf.byte_length = json_get_int(b, "byteLength");
            buffers.push_back(buf);
        }
    }

    // Parse buffer views
    std::vector<GltfBufferView> buffer_views;
    if (const auto* bvs = json_get_array(root, "bufferViews")) {
        for (const auto& bv : bvs->array_val) {
            GltfBufferView view;
            view.buffer = json_get_int(bv, "buffer");
            view.byte_offset = json_get_int(bv, "byteOffset");
            view.byte_length = json_get_int(bv, "byteLength");
            buffer_views.push_back(view);
        }
    }

    // Parse accessors
    std::vector<GltfAccessor> accessors;
    if (const auto* accs = json_get_array(root, "accessors")) {
        for (const auto& a : accs->array_val) {
            GltfAccessor acc;
            acc.buffer_view = json_get_int(a, "bufferView", -1);
            acc.component_type = json_get_int(a, "componentType");
            acc.count = json_get_int(a, "count");
            acc.type = json_get_string(a, "type");
            acc.byte_offset = json_get_int(a, "byteOffset");
            accessors.push_back(acc);
        }
    }

    // Parse materials (basic PBR base color only)
    std::vector<GltfMaterial> materials;
    if (const auto* mats = json_get_array(root, "materials")) {
        for (const auto& mat : mats->array_val) {
            GltfMaterial m;
            if (const auto* pbr = json_get_object(mat, "pbrMetallicRoughness")) {
                if (const auto* bc = json_get_array(*pbr, "baseColorFactor")) {
                    if (bc->array_val.size() >= 3) {
                        m.base_color_r = static_cast<float>(bc->array_val[0].number_val);
                        m.base_color_g = static_cast<float>(bc->array_val[1].number_val);
                        m.base_color_b = static_cast<float>(bc->array_val[2].number_val);
                    }
                }
            }
            materials.push_back(m);
        }
    }

    // Parse meshes
    std::vector<GltfMeshInfo> mesh_infos;
    if (const auto* meshes = json_get_array(root, "meshes")) {
        for (const auto& m : meshes->array_val) {
            GltfMeshInfo info;
            if (const auto* prims = json_get_array(m, "primitives")) {
                for (const auto& p : prims->array_val) {
                    GltfPrimitive prim;
                    if (const auto* attrs = json_get_object(p, "attributes")) {
                        prim.position_accessor = json_get_int(*attrs, "POSITION", -1);
                        prim.normal_accessor = json_get_int(*attrs, "NORMAL", -1);
                        prim.texcoord_accessor = json_get_int(*attrs, "TEXCOORD_0", -1);
                        prim.joints_accessor = json_get_int(*attrs, "JOINTS_0", -1);
                        prim.weights_accessor = json_get_int(*attrs, "WEIGHTS_0", -1);
                    }
                    prim.indices_accessor = json_get_int(p, "indices", -1);
                    prim.material = json_get_int(p, "material", -1);
                    info.primitives.push_back(prim);
                }
            }
            mesh_infos.push_back(info);
        }
    }

    // Parse nodes to find which meshes to load
    std::vector<GltfNode> nodes;
    std::vector<int> root_nodes;
    if (const auto* ns = json_get_array(root, "nodes")) {
        for (const auto& n : ns->array_val) {
            GltfNode node;
            node.mesh = json_get_int(n, "mesh", -1);
            if (const auto* children = json_get_array(n, "children")) {
                for (const auto& c : children->array_val) {
                    if (c.type == JsonType::Number) {
                        node.children.push_back(static_cast<int>(c.number_val));
                    }
                }
            }
            nodes.push_back(node);
        }
    }

    // Find root nodes from the default scene
    if (const auto* scene_val = json_get_object(root, "scene")) {
        // scene is an index into scenes array, but for minimal support just get scene 0
    }
    int scene_index = json_get_int(root, "scene", 0);
    if (const auto* scenes = json_get_array(root, "scenes")) {
        if (scene_index >= 0 && static_cast<std::size_t>(scene_index) < scenes->array_val.size()) {
            const auto& scene = scenes->array_val[static_cast<std::size_t>(scene_index)];
            if (const auto* scene_nodes = json_get_array(scene, "nodes")) {
                for (const auto& n : scene_nodes->array_val) {
                    if (n.type == JsonType::Number) {
                        root_nodes.push_back(static_cast<int>(n.number_val));
                    }
                }
            }
        }
    }

    // If no scene, use all root nodes (nodes not referenced as children)
    if (root_nodes.empty()) {
        std::vector<bool> is_child(nodes.size(), false);
        for (const auto& node : nodes) {
            for (int child : node.children) {
                if (child >= 0 && static_cast<std::size_t>(child) < is_child.size()) {
                    is_child[static_cast<std::size_t>(child)] = true;
                }
            }
        }
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            if (!is_child[i]) root_nodes.push_back(static_cast<int>(i));
        }
    }

    // Load buffer data
    std::vector<std::vector<std::uint8_t>> buffer_data(buffers.size());
    for (std::size_t i = 0; i < buffers.size(); ++i) {
        if (!buffers[i].uri.empty()) {
            if (!read_file(base_path + buffers[i].uri, buffer_data[i])) {
                error_ = "Failed to read buffer: " + base_path + buffers[i].uri;
                return false;
            }
        }
    }

    // Parse skins (after buffer data is available)
    if (const auto* skins_arr = json_get_array(root, "skins")) {
        for (const auto& s : skins_arr->array_val) {
            GltfSkin skin;
            std::vector<int> joint_node_indices;
            if (const auto* joints = json_get_array(s, "joints")) {
                for (const auto& j : joints->array_val) {
                    if (j.type == JsonType::Number) {
                        joint_node_indices.push_back(static_cast<int>(j.number_val));
                    }
                }
            }
            // Read inverseBindMatrices accessor
            int ibm_accessor = json_get_int(s, "inverseBindMatrices", -1);
            std::vector<float> ibm_data;
            if (ibm_accessor >= 0 && static_cast<std::size_t>(ibm_accessor) < accessors.size()) {
                const auto& acc = accessors[static_cast<std::size_t>(ibm_accessor)];
                if (acc.buffer_view >= 0 && static_cast<std::size_t>(acc.buffer_view) < buffer_views.size()) {
                    const auto& bv = buffer_views[static_cast<std::size_t>(acc.buffer_view)];
                    if (bv.buffer >= 0 && static_cast<std::size_t>(bv.buffer) < buffer_data.size()) {
                        read_accessor_data(buffer_data[static_cast<std::size_t>(bv.buffer)], acc, bv, ibm_data);
                    }
                }
            }
            // Build joints: one per joint node index
            for (std::size_t ji = 0; ji < joint_node_indices.size(); ++ji) {
                GltfJoint joint;
                int node_idx = joint_node_indices[ji];
                joint.node_index = node_idx;
                if (node_idx >= 0 && static_cast<std::size_t>(node_idx) < nodes.size()) {
                    // Determine parent index from node hierarchy within the skin
                    bool found_parent = false;
                    for (std::size_t pj = 0; pj < joint_node_indices.size(); ++pj) {
                        int pidx = joint_node_indices[pj];
                        if (pidx >= 0 && static_cast<std::size_t>(pidx) < nodes.size()) {
                            for (int child : nodes[static_cast<std::size_t>(pidx)].children) {
                                if (child == node_idx) {
                                    joint.parent_index = static_cast<int>(pj);
                                    found_parent = true;
                                    break;
                                }
                            }
                        }
                        if (found_parent) break;
                    }
                }
                // Copy 16 floats of inverse bind matrix for this joint
                std::size_t ibm_start = ji * 16;
                if (ibm_start + 16 <= ibm_data.size()) {
                    joint.inverse_bind_matrix.assign(
                        ibm_data.begin() + static_cast<std::ptrdiff_t>(ibm_start),
                        ibm_data.begin() + static_cast<std::ptrdiff_t>(ibm_start + 16));
                }
                skin.joints.push_back(std::move(joint));
            }
            model.skins.push_back(std::move(skin));
        }
    }

    // Parse animations (after buffer data is available)
    if (const auto* anims_arr = json_get_array(root, "animations")) {
        for (const auto& a : anims_arr->array_val) {
            GltfAnimation anim;
            anim.name = json_get_string(a, "name");

            // Parse samplers
            if (const auto* samplers_arr = json_get_array(a, "samplers")) {
                for (const auto& smp : samplers_arr->array_val) {
                    GltfAnimationSampler sampler;
                    int input_acc = json_get_int(smp, "input", -1);
                    int output_acc = json_get_int(smp, "output", -1);
                    sampler.interpolation = json_get_string(smp, "interpolation");
                    if (sampler.interpolation.empty()) {
                        sampler.interpolation = "LINEAR";
                    }

                    // Read input keyframe times
                    if (input_acc >= 0 && static_cast<std::size_t>(input_acc) < accessors.size()) {
                        const auto& acc = accessors[static_cast<std::size_t>(input_acc)];
                        if (acc.buffer_view >= 0 && static_cast<std::size_t>(acc.buffer_view) < buffer_views.size()) {
                            const auto& bv = buffer_views[static_cast<std::size_t>(acc.buffer_view)];
                            if (bv.buffer >= 0 && static_cast<std::size_t>(bv.buffer) < buffer_data.size()) {
                                read_accessor_data(buffer_data[static_cast<std::size_t>(bv.buffer)], acc, bv, sampler.input_times);
                            }
                        }
                    }
                    // Read output keyframe values
                    if (output_acc >= 0 && static_cast<std::size_t>(output_acc) < accessors.size()) {
                        const auto& acc = accessors[static_cast<std::size_t>(output_acc)];
                        if (acc.buffer_view >= 0 && static_cast<std::size_t>(acc.buffer_view) < buffer_views.size()) {
                            const auto& bv = buffer_views[static_cast<std::size_t>(acc.buffer_view)];
                            if (bv.buffer >= 0 && static_cast<std::size_t>(bv.buffer) < buffer_data.size()) {
                                read_accessor_data(buffer_data[static_cast<std::size_t>(bv.buffer)], acc, bv, sampler.output_values);
                            }
                        }
                    }
                    anim.samplers.push_back(std::move(sampler));
                }
            }

            // Parse channels
            if (const auto* channels_arr = json_get_array(a, "channels")) {
                for (const auto& ch : channels_arr->array_val) {
                    GltfAnimationChannel channel;
                    channel.sampler_index = json_get_int(ch, "sampler", -1);
                    if (const auto* target = json_get_object(ch, "target")) {
                        channel.node_index = json_get_int(*target, "node", -1);
                        channel.path = json_get_string(*target, "path");
                    }
                    anim.channels.push_back(std::move(channel));
                }
            }

            model.animations.push_back(std::move(anim));
        }
    }

    // Extract meshes from the node tree
    model.meshes.clear();

    // Depth-first traversal to collect meshes
    std::vector<int> stack = root_nodes;
    std::vector<bool> visited(nodes.size(), false);

    while (!stack.empty()) {
        int node_idx = stack.back();
        stack.pop_back();

        if (node_idx < 0 || static_cast<std::size_t>(node_idx) >= nodes.size()) continue;
        if (visited[static_cast<std::size_t>(node_idx)]) continue;
        visited[static_cast<std::size_t>(node_idx)] = true;

        const auto& node = nodes[static_cast<std::size_t>(node_idx)];

        // Push children (reverse order for correct traversal)
        for (auto it = node.children.rbegin(); it != node.children.rend(); ++it) {
            stack.push_back(*it);
        }

        if (node.mesh < 0 || static_cast<std::size_t>(node.mesh) >= mesh_infos.size()) continue;

        const auto& info = mesh_infos[static_cast<std::size_t>(node.mesh)];

        for (const auto& prim : info.primitives) {
            GltfMesh mesh;

            // Load positions
            if (prim.position_accessor >= 0 && static_cast<std::size_t>(prim.position_accessor) < accessors.size()) {
                const auto& acc = accessors[static_cast<std::size_t>(prim.position_accessor)];
                if (acc.buffer_view >= 0 && static_cast<std::size_t>(acc.buffer_view) < buffer_views.size()) {
                    const auto& bv = buffer_views[static_cast<std::size_t>(acc.buffer_view)];
                    if (bv.buffer >= 0 && static_cast<std::size_t>(bv.buffer) < buffer_data.size()) {
                        read_accessor_data(buffer_data[static_cast<std::size_t>(bv.buffer)], acc, bv, mesh.positions);
                    }
                }
            }

            // Load normals
            if (prim.normal_accessor >= 0 && static_cast<std::size_t>(prim.normal_accessor) < accessors.size()) {
                const auto& acc = accessors[static_cast<std::size_t>(prim.normal_accessor)];
                if (acc.buffer_view >= 0 && static_cast<std::size_t>(acc.buffer_view) < buffer_views.size()) {
                    const auto& bv = buffer_views[static_cast<std::size_t>(acc.buffer_view)];
                    if (bv.buffer >= 0 && static_cast<std::size_t>(bv.buffer) < buffer_data.size()) {
                        read_accessor_data(buffer_data[static_cast<std::size_t>(bv.buffer)], acc, bv, mesh.normals);
                    }
                }
            }

            // Load texture coordinates (TEXCOORD_0)
            if (prim.texcoord_accessor >= 0 && static_cast<std::size_t>(prim.texcoord_accessor) < accessors.size()) {
                const auto& acc = accessors[static_cast<std::size_t>(prim.texcoord_accessor)];
                if (acc.buffer_view >= 0 && static_cast<std::size_t>(acc.buffer_view) < buffer_views.size()) {
                    const auto& bv = buffer_views[static_cast<std::size_t>(acc.buffer_view)];
                    if (bv.buffer >= 0 && static_cast<std::size_t>(bv.buffer) < buffer_data.size()) {
                        read_accessor_data(buffer_data[static_cast<std::size_t>(bv.buffer)], acc, bv, mesh.uvs);
                    }
                }
            }

            // Load joints (JOINTS_0)
            if (prim.joints_accessor >= 0 && static_cast<std::size_t>(prim.joints_accessor) < accessors.size()) {
                const auto& acc = accessors[static_cast<std::size_t>(prim.joints_accessor)];
                if (acc.buffer_view >= 0 && static_cast<std::size_t>(acc.buffer_view) < buffer_views.size()) {
                    const auto& bv = buffer_views[static_cast<std::size_t>(acc.buffer_view)];
                    if (bv.buffer >= 0 && static_cast<std::size_t>(bv.buffer) < buffer_data.size()) {
                        read_accessor_data(buffer_data[static_cast<std::size_t>(bv.buffer)], acc, bv, mesh.joint_indices);
                    }
                }
            }

            // Load weights (WEIGHTS_0)
            if (prim.weights_accessor >= 0 && static_cast<std::size_t>(prim.weights_accessor) < accessors.size()) {
                const auto& acc = accessors[static_cast<std::size_t>(prim.weights_accessor)];
                if (acc.buffer_view >= 0 && static_cast<std::size_t>(acc.buffer_view) < buffer_views.size()) {
                    const auto& bv = buffer_views[static_cast<std::size_t>(acc.buffer_view)];
                    if (bv.buffer >= 0 && static_cast<std::size_t>(bv.buffer) < buffer_data.size()) {
                        read_accessor_data(buffer_data[static_cast<std::size_t>(bv.buffer)], acc, bv, mesh.joint_weights);
                    }
                }
            }

            // Load indices
            if (prim.indices_accessor >= 0 && static_cast<std::size_t>(prim.indices_accessor) < accessors.size()) {
                const auto& acc = accessors[static_cast<std::size_t>(prim.indices_accessor)];
                if (acc.buffer_view >= 0 && static_cast<std::size_t>(acc.buffer_view) < buffer_views.size()) {
                    const auto& bv = buffer_views[static_cast<std::size_t>(acc.buffer_view)];
                    if (bv.buffer >= 0 && static_cast<std::size_t>(bv.buffer) < buffer_data.size()) {
                        read_accessor_data(buffer_data[static_cast<std::size_t>(bv.buffer)], acc, bv, mesh.indices);
                    }
                }
            }

            // Apply material color
            if (prim.material >= 0 && static_cast<std::size_t>(prim.material) < materials.size()) {
                const auto& mat = materials[static_cast<std::size_t>(prim.material)];
                mesh.color_r = mat.base_color_r;
                mesh.color_g = mat.base_color_g;
                mesh.color_b = mat.base_color_b;
                mesh.has_material_color = true;
            }

            if (!mesh.positions.empty()) {
                model.meshes.push_back(std::move(mesh));
            }
        }
    }

    if (model.meshes.empty()) {
        error_ = "No mesh data loaded";
        return false;
    }

    return true;
}

}  // namespace ae::render
