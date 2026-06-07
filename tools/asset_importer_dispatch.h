#pragma once

#include "asset_importer_common.h"

namespace asset_importer {

bool compile_model(const ImportEntry& entry);
bool copy_asset(const ImportEntry& entry);
bool import_entry(const ImportEntry& entry);
void print_usage();

} // namespace asset_importer
