#include "asset_importer_dispatch.h"
#include "asset_importer_texture.h"
#include "asset_importer_material.h"
#include "asset_importer_level.h"
#include "ae/render/compiled_mesh.h"
#include "ae/render/gltf_loader.h"

#include <filesystem>
#include <iostream>

namespace asset_importer {

bool compile_model(const ImportEntry& entry) {
    ae::render::GltfLoader loader;
    ae::render::GltfModel model;

    if (!loader.load(entry.source.string(), model)) {
        std::cerr << "glTF import failed for " << entry.source << ": " << loader.last_error() << '\n';
        return false;
    }

    std::string error;
    if (!ae::render::save_compiled_mesh(entry.output.string(), model, error)) {
        std::cerr << "Failed to compile " << entry.source << " -> " << entry.output << ": " << error << '\n';
        return false;
    }

    std::cout << "model  " << entry.source << " -> " << entry.output << '\n';
    return true;
}

bool copy_asset(const ImportEntry& entry) {
    if (!std::filesystem::exists(entry.source)) {
        std::cerr << "Source asset does not exist: " << entry.source << '\n';
        return false;
    }

    std::error_code error;
    if (!entry.output.parent_path().empty()) {
        std::filesystem::create_directories(entry.output.parent_path(), error);
        if (error) {
            std::cerr << "Failed to create output directory for " << entry.output << ": " << error.message() << '\n';
            return false;
        }
    }

    std::filesystem::copy_file(entry.source, entry.output, std::filesystem::copy_options::overwrite_existing, error);
    if (error) {
        std::cerr << "Failed to copy " << entry.source << " -> " << entry.output << ": " << error.message() << '\n';
        return false;
    }

    if (!entry.metadata.empty()) {
        const auto metadata_output = entry.output.parent_path() / entry.metadata.filename();
        std::filesystem::copy_file(entry.metadata, metadata_output, std::filesystem::copy_options::overwrite_existing, error);
        if (error) {
            std::cerr << "Failed to copy metadata " << entry.metadata << " -> " << metadata_output << ": " << error.message()
                      << '\n';
            return false;
        }
    }

    std::cout << entry.kind << " " << entry.source << " -> " << entry.output << '\n';
    return true;
}

bool import_entry(const ImportEntry& entry) {
    if (entry.kind == "model") {
        return compile_model(entry);
    }

    if (entry.kind == "texture") {
        return compile_texture(entry);
    }

    if (entry.kind == "material") {
        return compile_material(entry);
    }

    if (entry.kind == "level") {
        return compile_level(entry);
    }

    if (entry.kind == "sprite" || entry.kind == "audio" || entry.kind == "data") {
        return copy_asset(entry);
    }

    std::cerr << "Unknown asset kind '" << entry.kind << "' for " << entry.source << '\n';
    return false;
}

void print_usage() {
    std::cout << "Usage:\n"
              << "  ahamkara_asset_importer --manifest <path>\n"
              << "  ahamkara_asset_importer --model <source.gltf> <output.aemesh>\n"
              << "  ahamkara_asset_importer --pack <registry.tsv> <output.pkg>\n\n"
              << "Manifest lines use:\n"
              << "  <kind> <source> <output> [metadata]\n\n"
              << "Kinds:\n"
              << "  model    glTF 2.0 source compiled to Ahamkara .aemesh\n"
              << "  texture  TGA source compiled to Ahamkara .aetex\n"
              << "  material text source compiled to Ahamkara .aemat\n"
              << "  level    text source compiled to Ahamkara .aelevel\n"
              << "  sprite   copied passthrough; optional metadata copied beside output\n"
              << "  audio    copied passthrough for first pipeline slice\n"
              << "  data     copied passthrough for miscellaneous runtime data\n";
}

} // namespace asset_importer
