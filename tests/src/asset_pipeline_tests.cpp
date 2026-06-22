#include "ae/render/compiled_level.h"
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
           vector_equal(lhs.uvs, rhs.uvs) &&
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

int test_compiled_level_roundtrip() {
    ae::render::LevelAsset source;
    source.name = "TestLevel";
    source.sky_color_r = 0.3F;
    source.sky_color_g = 0.5F;
    source.sky_color_b = 0.8F;
    source.ambient_r = 0.02F;
    source.ambient_g = 0.03F;
    source.ambient_b = 0.04F;
    source.gravity = 22.0F;
    source.skybox_material = "materials/skybox";
    source.ground_material = "materials/ground";

    ae::render::LevelSpawnPoint sp1;
    sp1.pos_x = -12.0F;
    sp1.pos_y = 1.5F;
    sp1.pos_z = 0.0F;
    sp1.yaw = 90.0F;
    sp1.team = 1;
    source.spawn_points.push_back(sp1);

    ae::render::LevelSpawnPoint sp2;
    sp2.pos_x = 12.0F;
    sp2.pos_y = 1.5F;
    sp2.pos_z = 0.0F;
    sp2.yaw = 270.0F;
    sp2.team = 2;
    source.spawn_points.push_back(sp2);

    ae::render::LevelCollisionBox cb1;
    cb1.min_x = -4.0F;
    cb1.min_z = -4.0F;
    cb1.max_x = 4.0F;
    cb1.max_z = 4.0F;
    cb1.top_y = 1.5F;
    cb1.bottom_y = 0.0F;
    cb1.wall = false;
    cb1.jump_through = false;
    cb1.auto_step = false;
    cb1.surface_material = 0;
    source.collision_boxes.push_back(cb1);

    ae::render::LevelCollisionBox cb2;
    cb2.min_x = -0.8F;
    cb2.min_z = -0.8F;
    cb2.max_x = 0.8F;
    cb2.max_z = 0.8F;
    cb2.top_y = 3.5F;
    cb2.bottom_y = 0.0F;
    cb2.wall = true;
    cb2.jump_through = false;
    cb2.auto_step = false;
    cb2.surface_material = 1;
    source.collision_boxes.push_back(cb2);

    ae::render::LevelMeshInstance mi;
    mi.mesh_asset_id = "models/box";
    mi.material_asset_id = "materials/wall";
    mi.pos_x = 5.0F;
    mi.pos_y = 2.0F;
    mi.pos_z = 3.0F;
    mi.yaw = 45.0F;
    mi.pitch = 10.0F;
    mi.roll = 0.0F;
    mi.scale_x = 2.0F;
    mi.scale_y = 1.0F;
    mi.scale_z = 2.0F;
    source.mesh_instances.push_back(mi);

    const auto output_path = std::filesystem::temp_directory_path() / "ahamkara_level_roundtrip.aelevel";

    std::string error;
    if (!ae::render::save_compiled_level(output_path.string(), source, error)) {
        return fail("save_compiled_level failed: " + error);
    }

    ae::render::CompiledLevelLoader loader;
    ae::render::LevelAsset loaded;
    if (!loader.load(output_path.string(), loaded)) {
        return fail("CompiledLevelLoader failed: " + loader.last_error());
    }

    std::filesystem::remove(output_path);

    if (loaded.name != source.name) {
        return fail("level name mismatch");
    }

    if (!float_equal(loaded.sky_color_r, source.sky_color_r) ||
        !float_equal(loaded.sky_color_g, source.sky_color_g) ||
        !float_equal(loaded.sky_color_b, source.sky_color_b) ||
        !float_equal(loaded.ambient_r, source.ambient_r) ||
        !float_equal(loaded.ambient_g, source.ambient_g) ||
        !float_equal(loaded.ambient_b, source.ambient_b) ||
        !float_equal(loaded.gravity, source.gravity)) {
        return fail("level world settings mismatch");
    }

    if (loaded.skybox_material != source.skybox_material ||
        loaded.ground_material != source.ground_material) {
        return fail("level material refs mismatch");
    }

    if (loaded.spawn_points.size() != source.spawn_points.size()) {
        return fail("level spawn point count mismatch");
    }

    for (std::size_t i = 0; i < loaded.spawn_points.size(); ++i) {
        const auto& a = loaded.spawn_points[i];
        const auto& b = source.spawn_points[i];
        if (!float_equal(a.pos_x, b.pos_x) ||
            !float_equal(a.pos_y, b.pos_y) ||
            !float_equal(a.pos_z, b.pos_z) ||
            !float_equal(a.yaw, b.yaw) ||
            a.team != b.team) {
            return fail("level spawn point mismatch at index " + std::to_string(i));
        }
    }

    if (loaded.collision_boxes.size() != source.collision_boxes.size()) {
        return fail("level collision box count mismatch");
    }

    for (std::size_t i = 0; i < loaded.collision_boxes.size(); ++i) {
        const auto& a = loaded.collision_boxes[i];
        const auto& b = source.collision_boxes[i];
        if (!float_equal(a.min_x, b.min_x) ||
            !float_equal(a.min_z, b.min_z) ||
            !float_equal(a.max_x, b.max_z) ||
            !float_equal(a.max_z, b.max_z) ||
            !float_equal(a.top_y, b.top_y) ||
            !float_equal(a.bottom_y, b.bottom_y) ||
            a.wall != b.wall ||
            a.jump_through != b.jump_through ||
            a.auto_step != b.auto_step ||
            a.surface_material != b.surface_material) {
            return fail("level collision box mismatch at index " + std::to_string(i));
        }
    }

    if (loaded.mesh_instances.size() != source.mesh_instances.size()) {
        return fail("level mesh instance count mismatch");
    }

    for (std::size_t i = 0; i < loaded.mesh_instances.size(); ++i) {
        const auto& a = loaded.mesh_instances[i];
        const auto& b = source.mesh_instances[i];
        if (a.mesh_asset_id != b.mesh_asset_id ||
            a.material_asset_id != b.material_asset_id ||
            !float_equal(a.pos_x, b.pos_x) ||
            !float_equal(a.pos_y, b.pos_y) ||
            !float_equal(a.pos_z, b.pos_z) ||
            !float_equal(a.yaw, b.yaw) ||
            !float_equal(a.pitch, b.pitch) ||
            !float_equal(a.roll, b.roll) ||
            !float_equal(a.scale_x, b.scale_x) ||
            !float_equal(a.scale_y, b.scale_y) ||
            !float_equal(a.scale_z, b.scale_z)) {
            return fail("level mesh instance mismatch at index " + std::to_string(i));
        }
    }

    return 0;
}

int test_compiled_level_rejects_bad_magic() {
    const auto output_path = std::filesystem::temp_directory_path() / "ahamkara_level_bad_magic.aelevel";
    {
        std::ofstream file(output_path, std::ios::binary);
        const std::uint32_t bad_magic = 0;
        const std::uint32_t version = ae::render::CompiledLevelFormat::version;
        file.write(reinterpret_cast<const char*>(&bad_magic), sizeof(bad_magic));
        file.write(reinterpret_cast<const char*>(&version), sizeof(version));
    }

    ae::render::CompiledLevelLoader loader;
    ae::render::LevelAsset loaded;
    if (loader.load(output_path.string(), loaded)) {
        std::filesystem::remove(output_path);
        return fail("level loader accepted invalid magic");
    }

    std::filesystem::remove(output_path);
    return 0;
}

int test_level_import_roundtrip() {
#ifndef AHAMKARA_ASSET_IMPORTER_PATH
    return fail("AHAMKARA_ASSET_IMPORTER_PATH is not defined");
#else
    const auto temp_root = std::filesystem::temp_directory_path() / "ahamkara_level_import_test";
    std::error_code error;
    std::filesystem::remove_all(temp_root, error);
    std::filesystem::create_directories(temp_root / "compiled", error);
    if (error) {
        return fail("failed to create temp level directory");
    }

    const auto source_path = temp_root / "test.lvl";
    const auto manifest_path = temp_root / "manifest.assets";
    const auto output_path = temp_root / "compiled" / "test.aelevel";

    {
        std::ofstream source_file(source_path);
        source_file << "name=TestImport\n";
        source_file << "sky_color=0.2 0.3 0.7\n";
        source_file << "ambient=0.01 0.02 0.03\n";
        source_file << "gravity=18.0\n";
        source_file << "skybox_material=materials/sky\n";
        source_file << "\n";
        source_file << "[spawn]\n";
        source_file << "-5.0 2.0 1.0 45.0 team=1\n";
        source_file << "8.0 2.0 -2.0 180.0 team=2\n";
        source_file << "\n";
        source_file << "[collision]\n";
        source_file << "-3.0 -3.0 3.0 3.0 2.0 0.0 wall=false jump_through=false auto_step=false surface=1\n";
        source_file << "-1.0 -1.0 1.0 1.0 4.0 0.0 wall=true jump_through=false auto_step=false surface=2\n";
        source_file << "5.0 5.0 9.0 9.0 1.0 0.0 wall=false jump_through=true auto_step=false surface=0\n";
        source_file << "\n";
        source_file << "[mesh]\n";
        source_file << "mesh_id=models/platform material_id=materials/stone pos=0.0 0.0 0.0 yaw=0.0 pitch=0.0 roll=0.0 scale=1.0 1.0 1.0\n";
    }
    {
        std::ofstream manifest_file(manifest_path);
        manifest_file << "level test.lvl compiled/test.aelevel\n";
    }

    const std::string command =
        quote_path(AHAMKARA_ASSET_IMPORTER_PATH) + " --manifest " + quote_path(manifest_path);
    if (std::system(command.c_str()) != 0) {
        std::filesystem::remove_all(temp_root, error);
        return fail("level importer command failed");
    }

    ae::render::CompiledLevelLoader loader;
    ae::render::LevelAsset level;
    if (!loader.load(output_path.string(), level)) {
        std::filesystem::remove_all(temp_root, error);
        return fail("CompiledLevelLoader failed: " + loader.last_error());
    }

    std::filesystem::remove_all(temp_root, error);

    if (level.name != "TestImport") {
        return fail("level name was unexpected");
    }

    if (!float_equal(level.sky_color_r, 0.2F) ||
        !float_equal(level.sky_color_g, 0.3F) ||
        !float_equal(level.sky_color_b, 0.7F) ||
        !float_equal(level.ambient_r, 0.01F) ||
        !float_equal(level.ambient_g, 0.02F) ||
        !float_equal(level.ambient_b, 0.03F) ||
        !float_equal(level.gravity, 18.0F)) {
        return fail("level world settings were unexpected");
    }

    if (level.skybox_material != "materials/sky") {
        return fail("level skybox material ref was unexpected");
    }

    if (level.spawn_points.size() != 2) {
        return fail("level spawn point count was unexpected");
    }

    if (!float_equal(level.spawn_points[0].pos_x, -5.0F) ||
        !float_equal(level.spawn_points[0].pos_y, 2.0F) ||
        !float_equal(level.spawn_points[0].pos_z, 1.0F) ||
        !float_equal(level.spawn_points[0].yaw, 45.0F) ||
        level.spawn_points[0].team != 1) {
        return fail("level spawn point[0] was unexpected");
    }

    if (!float_equal(level.spawn_points[1].pos_x, 8.0F) ||
        !float_equal(level.spawn_points[1].pos_y, 2.0F) ||
        !float_equal(level.spawn_points[1].pos_z, -2.0F) ||
        !float_equal(level.spawn_points[1].yaw, 180.0F) ||
        level.spawn_points[1].team != 2) {
        return fail("level spawn point[1] was unexpected");
    }

    if (level.collision_boxes.size() != 3) {
        return fail("level collision box count was unexpected");
    }

    if (!level.collision_boxes[0].wall &&
        level.collision_boxes[1].wall &&
        level.collision_boxes[2].jump_through) {
        // Expected flag setup confirmed; check surface materials
        if (level.collision_boxes[0].surface_material != 1 ||
            level.collision_boxes[1].surface_material != 2 ||
            level.collision_boxes[2].surface_material != 0) {
            return fail("level collision box surface materials were unexpected");
        }
    } else {
        return fail("level collision box flags were unexpected");
    }

    if (level.mesh_instances.size() != 1) {
        return fail("level mesh instance count was unexpected");
    }

    if (level.mesh_instances[0].mesh_asset_id != "models/platform" ||
        level.mesh_instances[0].material_asset_id != "materials/stone") {
        return fail("level mesh instance refs were unexpected");
    }

    return 0;
#endif
}

int test_asset_packing_alignment() {
#ifndef AHAMKARA_ASSET_IMPORTER_PATH
    return fail("AHAMKARA_ASSET_IMPORTER_PATH is not defined");
#else
    const auto temp_root = std::filesystem::temp_directory_path() / "ahamkara_pack_alignment_test";
    std::error_code error;
    std::filesystem::remove_all(temp_root, error);
    std::filesystem::create_directories(temp_root / "compiled", error);
    if (error) {
        return fail("failed to create temp pack directory");
    }

    const auto source_path1 = temp_root / "test1.tga";
    const auto source_path2 = temp_root / "test2.mat";
    const auto manifest_path = temp_root / "manifest.assets";

    if (!write_test_tga(source_path1)) {
        std::filesystem::remove_all(temp_root, error);
        return fail("failed to write test TGA source");
    }

    {
        std::ofstream file2(source_path2);
        file2 << "base_color=0.8 0.7 0.6 1.0\n";
    }

    {
        std::ofstream manifest_file(manifest_path);
        manifest_file << "texture test1.tga compiled/test1.aetex\n";
        manifest_file << "material test2.mat compiled/test2.aemat\n";
    }

    const std::string command =
        quote_path(AHAMKARA_ASSET_IMPORTER_PATH) + " --manifest " + quote_path(manifest_path);
    if (std::system(command.c_str()) != 0) {
        std::filesystem::remove_all(temp_root, error);
        return fail("asset importer --manifest command failed");
    }

    const auto pkg_path = temp_root / "compiled" / "assets.pkg";
    std::ifstream file(pkg_path, std::ios::binary);
    if (!file) {
        std::filesystem::remove_all(temp_root, error);
        return fail("assets.pkg was not created by auto-pack");
    }

    char magic[4];
    std::uint32_t version = 0;
    std::uint32_t entry_count = 0;
    file.read(magic, 4);
    file.read(reinterpret_cast<char*>(&version), 4);
    file.read(reinterpret_cast<char*>(&entry_count), 4);

    if (magic[0] != 'A' || magic[1] != 'P' || magic[2] != 'K' || magic[3] != 'G') {
        std::filesystem::remove_all(temp_root, error);
        return fail("assets.pkg magic mismatch");
    }
    if (version != 1) {
        std::filesystem::remove_all(temp_root, error);
        return fail("assets.pkg version mismatch");
    }
    if (entry_count != 2) {
        std::filesystem::remove_all(temp_root, error);
        return fail("assets.pkg entry count mismatch");
    }

    struct TestEntry {
        std::string asset_id;
        std::uint64_t offset;
        std::uint64_t size;
    };
    std::vector<TestEntry> entries;

    for (std::uint32_t i = 0; i < entry_count; ++i) {
        std::uint32_t id_len = 0;
        file.read(reinterpret_cast<char*>(&id_len), 4);
        std::vector<char> id_buf(id_len);
        file.read(id_buf.data(), id_len);
        std::uint64_t offset = 0;
        std::uint64_t size = 0;
        file.read(reinterpret_cast<char*>(&offset), 8);
        file.read(reinterpret_cast<char*>(&size), 8);

        entries.push_back({std::string(id_buf.begin(), id_buf.end()), offset, size});
    }

    for (const auto& entry : entries) {
        if (entry.offset % 4096 != 0) {
            std::filesystem::remove_all(temp_root, error);
            return fail("assets.pkg entry offset " + std::to_string(entry.offset) + " is not aligned to 4KB boundary");
        }

        file.seekg(static_cast<std::streamoff>(entry.offset));
        std::vector<char> data(entry.size);
        file.read(data.data(), entry.size);

        // Determine which compiled file to compare against
        std::filesystem::path compiled_path;
        if (entry.asset_id.find("test1") != std::string::npos) {
            compiled_path = temp_root / "compiled" / "test1.aetex";
        } else if (entry.asset_id.find("test2") != std::string::npos) {
            compiled_path = temp_root / "compiled" / "test2.aemat";
        } else {
            std::filesystem::remove_all(temp_root, error);
            return fail("unexpected asset_id in package: " + entry.asset_id);
        }

        std::uintmax_t expected_size = std::filesystem::file_size(compiled_path);
        if (entry.size != expected_size) {
            std::filesystem::remove_all(temp_root, error);
            return fail("size mismatch for " + entry.asset_id + ": expected " +
                        std::to_string(expected_size) + ", got " + std::to_string(entry.size));
        }

        std::ifstream comp_file(compiled_path, std::ios::binary);
        std::vector<char> expected_data(expected_size);
        comp_file.read(expected_data.data(), expected_size);

        if (data != expected_data) {
            std::filesystem::remove_all(temp_root, error);
            return fail("data corrupted for " + entry.asset_id);
        }
    }

    std::filesystem::remove_all(temp_root, error);
    return 0;
#endif
}

int test_compiled_mesh_uv_roundtrip() {
    ae::render::GltfModel source;
    ae::render::GltfMesh mesh;
    mesh.positions = {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F};
    mesh.normals   = {0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F};
    mesh.uvs       = {0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F};
    mesh.indices   = {0, 1, 2};
    source.meshes.push_back(mesh);

    const auto output_path = std::filesystem::temp_directory_path() / "ahamkara_uv_roundtrip.aemesh";
    std::string error;
    if (!ae::render::save_compiled_mesh(output_path.string(), source, error)) {
        return fail("save_compiled_mesh (uv) failed: " + error);
    }

    ae::render::CompiledMeshLoader loader;
    ae::render::GltfModel loaded;
    if (!loader.load(output_path.string(), loaded)) {
        return fail("CompiledMeshLoader (uv) failed: " + loader.last_error());
    }
    std::filesystem::remove(output_path);

    if (loaded.meshes.size() != 1 ||
        !vector_equal(loaded.meshes[0].uvs, source.meshes[0].uvs)) {
        return fail("uv roundtrip did not preserve uvs");
    }
    return 0;
}

} // namespace

int main() {
    if (const int result = test_compiled_mesh_roundtrip(); result != 0) {
        return result;
    }

    if (const int result = test_compiled_mesh_uv_roundtrip(); result != 0) {
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

    if (const int result = test_compiled_level_roundtrip(); result != 0) {
        return result;
    }

    if (const int result = test_compiled_level_rejects_bad_magic(); result != 0) {
        return result;
    }

    if (const int result = test_level_import_roundtrip(); result != 0) {
        return result;
    }

    if (const int result = test_asset_packing_alignment(); result != 0) {
        return result;
    }

    return 0;
}
