#pragma once

namespace ae {

enum class RuntimeMode {
    Client,
    DedicatedServer,
    Editor,
    AssetCooker,
    Tests
};

class Application {
public:
    explicit Application(RuntimeMode runtime_mode);

    void start();
    void shutdown();

    [[nodiscard]] bool is_running() const;
    [[nodiscard]] RuntimeMode mode() const;

private:
    RuntimeMode runtime_mode_;
    bool running_ {false};
};

}  // namespace ae

