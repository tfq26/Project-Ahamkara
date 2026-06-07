#include "ae/render/compiled_material.h"
#include "ae/render/compiled_mesh.h"
#include "ae/render/compiled_texture.h"
#include "ae/render/humanoid_mesh.h"

#include <cstdint>
#include <cmath>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

bool float_equal(float lhs, float rhs) {
    return std::fabs(lhs - rhs) < 0.00001F;
}

bool vector_equal(const std::vector<float>& lhs, const std::vector<float>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (!float_equal(lhs[i], rhs[i])) {
            return false;
        }
    }

    return true;
}

bool mesh_equal(const ae::render::GltfMesh& lhs, const ae::render::GltfMesh& rhs) {
    return vector_equal(lhs.positions, rhs.positions) &&
           vector_equal(lhs.normals, rhs.normals) &&
           vector_equal(lhs.joint_indices, rhs.joint_indices) &&
           vector_equal(lhs.joint_weights, rhs.joint_weights) &&
           lhs.indices == rhs.indices &&
           float_equal(lhs.color_r, rhs.color_r) &&
           float_equal(lhs.color_g, rhs.color_g) &&
           float_equal(lhs.color_b, rhs.color_b) &&
           lhs.has_material_color == rhs.has_material_color;
}

bool model_equal(const ae::render::GltfModel& lhs, const ae::render::GltfModel& rhs) {
    if (lhs.meshes.size() != rhs.meshes.size() ||
        lhs.skins.size() != rhs.skins.size() ||
        lhs.animations.size() != rhs.animations.size()) {
        return false;
    }

    for (std::size_t i = 0; i < lhs.meshes.size(); ++i) {
        if (!mesh_equal(lhs.meshes[i], rhs.meshes[i])) {
            return false;
        }
    }

    return true;
}

int fail(const std::string& message) {
    std::cerr << "asset_pipeline_tests failed: " << message << '\n';
    return 1;
}

std::string quote_path(const std::filesystem::path& path) {
    return "\"" + path.string() + "\"";
}

bool read_text_file(const std::filesystem::path& path, std::string& contents) {
    std::ifstream file(path);
    if (!file) {
        return false;
    }

    std::ostringstream stream;
    stream << file.rdbuf();
    contents = stream.str();
    return true;
}

bool write_test_tga(const std::filesystem::path& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    const std::uint8_t header[18] = {
        0,    // id length
        0,    // color map type
        2,    // image type: uncompressed true-color
        0, 0, 0, 0, 0, // color map spec
        0, 0, // x origin
        0, 0, // y origin
        2, 0, // width = 2
        1, 0, // height = 1
        24,   // bits per pixel
        0x20, // top-left origin
    };
    file.write(reinterpret_cast<const char*>(header), sizeof(header));

    const std::uint8_t pixels[6] = {
        0, 0, 255,   // red in BGR
        0, 255, 0,   // green in BGR
    };
    file.write(reinterpret_cast<const char*>(pixels), sizeof(pixels));
    return static_cast<bool>(file);
}

int test_compiled_mesh_roundtrip() {
    const auto source = ae::render::generate_humanoid_mesh(ae::render::HumanoidLod::Medium);
    const auto output_path = std::filesystem::temp_directory_path() / "ahamkara_asset_pipeline_roundtrip.aemesh";

    std::string error;
    if (!ae::render::save_compiled_mesh(output_path.string(), source, error)) {
        return fail("save_compiled_mesh failed: " + error);
    }

    ae::render::CompiledMeshLoader loader;
    ae::render::GltfModel loaded;
    if (!loader.load(output_path.string(), loaded)) {
        return fail("CompiledMeshLoader failed: " + loader.last_error());
    }

    std::filesystem::remove(output_path);

    if (!model_equal(source, loaded)) {
        return fail("roundtrip model differs from source");
    }

    return 0;
}

int test_compiled_mesh_rejects_bad_magic() {
    const auto output_path = std::filesystem::temp_directory_path() / "ahamkara_asset_pipeline_bad_magic.aemesh";
    {
        std::ofstream file(output_path, std::ios::binary);
        const std::uint32_t bad_magic = 0;
        const std::uint32_t version = ae::render::CompiledMeshFormat::version;
        file.write(reinterpret_cast<const char*>(&bad_magic), sizeof(bad_magic));
        file.write(reinterpret_cast<const char*>(&version), sizeof(version));
    }

    ae::render::CompiledMeshLoader loader;
    ae::render::GltfModel loaded;
    if (loader.load(output_path.string(), loaded)) {
        std::filesystem::remove(output_path);
        return fail("loader accepted invalid magic");
    }

    std::filesystem::remove(output_path);
    return 0;
}

int test_importer_writes_asset_registry() {
#ifndef AHAMKARA_ASSET_IMPORTER_PATH
    return fail("AHAMKARA_ASSET_IMPORTER_PATH is not defined");
#else
    const auto temp_root = std::filesystem::temp_directory_path() / "ahamkara_asset_registry_test";
    std::error_code error;
    std::filesystem::remove_all(temp_root, error);
    std::filesystem::create_directories(temp_root / "compiled", error);
    if (error) {
        return fail("failed to create temp registry directory");
    }

    const auto source_path = temp_root / "source.txt";
    const auto manifest_path = temp_root / "manifest.assets";
    {
        std::ofstream source_file(source_path);
        source_file << "registry test payload";
    }
    {
        std::ofstream manifest_file(manifest_path);
        manifest_file << "data source.txt compiled/source.txt\n";
    }

    const std::string command =
        std::string(AHAMKARA_ASSET_IMPORTER_PATH) + " --manifest " + quote_path(manifest_path);
    if (std::system(command.c_str()) != 0) {
        std::filesystem::remove_all(temp_root, error);
        return fail("asset importer command failed");
    }

    const auto registry_path = temp_root / "compiled" / "asset_registry.tsv";
    std::ifstream registry_file(registry_path);
    if (!registry_file) {
        std::filesystem::remove_all(temp_root, error);
        return fail("asset registry file was not created");
    }

    std::string header;
    std::string row;
    std::getline(registry_file, header);
    std::getline(registry_file, row);
    std::filesystem::remove_all(temp_root, error);

    if (header != "asset_id\tkind\tsource\toutput\tmetadata\tsource_hash\tmetadata_hash\toutput_size") {
        return fail("asset registry header was unexpected");
    }

    if (row.find("source\tdata\t") != 0) {
        return fail("asset registry row does not contain the expected asset id and kind");
    }

    if (row.find("source_hash") != std::string::npos || row.empty()) {
        return fail("asset registry row was not populated");
    }

    return 0;
#endif
}

int test_importer_skips_unchanged_asset() {
#ifndef AHAMKARA_ASSET_IMPORTER_PATH
    return fail("AHAMKARA_ASSET_IMPORTER_PATH is not defined");
#else
    const auto temp_root = std::filesystem::temp_directory_path() / "ahamkara_asset_skip_test";
    std::error_code error;
    std::filesystem::remove_all(temp_root, error);
    std::filesystem::create_directories(temp_root / "compiled", error);
    if (error) {
        return fail("failed to create temp skip directory");
    }

    const auto source_path = temp_root / "source.txt";
    const auto manifest_path = temp_root / "manifest.assets";
    const auto output_path = temp_root / "compiled" / "source.txt";
    const auto log_path = temp_root / "import.log";

    {
        std::ofstream source_file(source_path);
        source_file << "skip test payload";
    }
    {
        std::ofstream manifest_file(manifest_path);
        manifest_file << "data source.txt compiled/source.txt\n";
    }

    const std::string base_command =
        quote_path(AHAMKARA_ASSET_IMPORTER_PATH) + " --manifest " + quote_path(manifest_path) + " > " + quote_path(log_path);

    if (std::system(base_command.c_str()) != 0) {
        std::filesystem::remove_all(temp_root, error);
        return fail("first asset importer run failed");
    }

    const auto first_write_time = std::filesystem::last_write_time(output_path, error);
    if (error) {
        std::filesystem::remove_all(temp_root, error);
        return fail("failed to stat compiled output after first import");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    if (std::system(base_command.c_str()) != 0) {
        std::filesystem::remove_all(temp_root, error);
        return fail("second asset importer run failed");
    }

    const auto second_write_time = std::filesystem::last_write_time(output_path, error);
    if (error) {
        std::filesystem::remove_all(temp_root, error);
        return fail("failed to stat compiled output after second import");
    }

    std::string log_contents;
    if (!read_text_file(log_path, log_contents)) {
        std::filesystem::remove_all(temp_root, error);
        return fail("failed to read importer log output");
    }

    std::filesystem::remove_all(temp_root, error);

    if (second_write_time != first_write_time) {
        return fail("unchanged asset was rebuilt instead of skipped");
    }

    if (log_contents.find("skip") == std::string::npos) {
        return fail("second importer run did not report a skipped asset");
    }

    return 0;
#endif
}

int test_texture_import_roundtrip() {
#ifndef AHAMKARA_ASSET_IMPORTER_PATH
    return fail("AHAMKARA_ASSET_IMPORTER_PATH is not defined");
#else
    const auto temp_root = std::filesystem::temp_directory_path() / "ahamkara_texture_import_test";
    std::error_code error;
    std::filesystem::remove_all(temp_root, error);
    std::filesystem::create_directories(temp_root / "compiled", error);
    if (error) {
        return fail("failed to create temp texture directory");
    }

    const auto source_path = temp_root / "checker.tga";
    const auto manifest_path = temp_root / "manifest.assets";
    const auto output_path = temp_root / "compiled" / "checker.aetex";

    if (!write_test_tga(source_path)) {
        std::filesystem::remove_all(temp_root, error);
        return fail("failed to write test TGA source");
    }

    {
        std::ofstream manifest_file(manifest_path);
        manifest_file << "texture checker.tga compiled/checker.aetex\n";
    }

    const std::string command =
        quote_path(AHAMKARA_ASSET_IMPORTER_PATH) + " --manifest " + quote_path(manifest_path);
    if (std::system(command.c_str()) != 0) {
        std::filesystem::remove_all(temp_root, error);
        return fail("texture importer command failed");
    }

    ae::render::CompiledTextureLoader loader;
    ae::render::TextureAsset texture;
    if (!loader.load(output_path.string(), texture)) {
        std::filesystem::remove_all(temp_root, error);
        return fail("CompiledTextureLoader failed: " + loader.last_error());
    }

    std::filesystem::remove_all(temp_root, error);

    if (texture.width != 2 || texture.height != 1) {
        return fail("compiled texture dimensions were unexpected");
    }

    if (texture.format != ae::render::CompiledTextureFormat::Rgba8) {
        return fail("compiled texture format was unexpected");
    }

    const std::vector<std::uint8_t> expected_pixels = {
        255, 0, 0, 255,
        0, 255, 0, 255,
    };
    if (texture.pixels != expected_pixels) {
        return fail("compiled texture pixels were not preserved");
    }

    return 0;
#endif
}

int test_material_import_roundtrip() {
#ifndef AHAMKARA_ASSET_IMPORTER_PATH
    return fail("AHAMKARA_ASSET_IMPORTER_PATH is not defined");
#else
    const auto temp_root = std::filesystem::temp_directory_path() / "ahamkara_material_import_test";
    std::error_code error;
    std::filesystem::remove_all(temp_root, error);
    std::filesystem::create_directories(temp_root / "compiled", error);
    if (error) {
        return fail("failed to create temp material directory");
    }

    const auto source_path = temp_root / "wall.mat";
    const auto manifest_path = temp_root / "manifest.assets";
    const auto output_path = temp_root / "compiled" / "wall.aemat";

    {
        std::ofstream source_file(source_path);
        source_file << "base_color=0.8 0.7 0.6 1.0\n";
        source_file << "metallic=0.25\n";
        source_file << "roughness=0.5\n";
        source_file << "emissive_color=0.1 0.2 0.3\n";
        source_file << "double_sided=true\n";
        source_file << "albedo_texture=textures/wall_albedo\n";
        source_file << "normal_texture=textures/wall_normal\n";
        source_file << "orm_texture=textures/wall_orm\n";
        source_file << "emissive_texture=textures/wall_emissive\n";
    }
    {
        std::ofstream manifest_file(manifest_path);
        manifest_file << "material wall.mat compiled/wall.aemat\n";
    }

    const std::string command =
        quote_path(AHAMKARA_ASSET_IMPORTER_PATH) + " --manifest " + quote_path(manifest_path);
    if (std::system(command.c_str()) != 0) {
        std::filesystem::remove_all(temp_root, error);
        return fail("material importer command failed");
    }

    ae::render::CompiledMaterialLoader loader;
    ae::render::MaterialAsset material;
    if (!loader.load(output_path.string(), material)) {
        std::filesystem::remove_all(temp_root, error);
        return fail("CompiledMaterialLoader failed: " + loader.last_error());
    }

    std::filesystem::remove_all(temp_root, error);

    if (!float_equal(material.base_color_r, 0.8F) ||
        !float_equal(material.base_color_g, 0.7F) ||
        !float_equal(material.base_color_b, 0.6F) ||
        !float_equal(material.base_color_a, 1.0F) ||
        !float_equal(material.metallic, 0.25F) ||
        !float_equal(material.roughness, 0.5F) ||
        !float_equal(material.emissive_r, 0.1F) ||
        !float_equal(material.emissive_g, 0.2F) ||
        !float_equal(material.emissive_b, 0.3F) ||
        !material.double_sided) {
        return fail("compiled material scalar properties were unexpected");
    }

    if (material.albedo_texture != "textures/wall_albedo" ||
        material.normal_texture != "textures/wall_normal" ||
        material.orm_texture != "textures/wall_orm" ||
        material.emissive_texture != "textures/wall_emissive") {
        return fail("compiled material texture references were unexpected");
    }

    return 0;
#endif
}

} // namespace

int main() {
    if (const int result = test_compiled_mesh_roundtrip(); result != 0) {
        return result;
    }

    if (const int result = test_compiled_mesh_rejects_bad_magic(); result != 0) {
        return result;
    }

    if (const int result = test_importer_writes_asset_registry(); result != 0) {
        return result;
    }

    if (const int result = test_importer_skips_unchanged_asset(); result != 0) {
        return result;
    }

    if (const int result = test_texture_import_roundtrip(); result != 0) {
        return result;
    }

    if (const int result = test_material_import_roundtrip(); result != 0) {
        return result;
    }

    return 0;
}
