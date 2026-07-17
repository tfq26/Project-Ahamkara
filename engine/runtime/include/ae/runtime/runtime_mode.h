#pragma once

namespace ae {

enum class RuntimeMode {
    Client,
    DedicatedServer,
    Editor,
    AssetCooker,
    Tests
};

const char* to_string(RuntimeMode mode);

} // namespace ae
