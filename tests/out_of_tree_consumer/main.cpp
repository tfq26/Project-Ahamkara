#include "ae/core/log.h"
#include "ae/core/time.h"
#include "ae/network/network_clock.h"
#include "ae/runtime/application.h"
#include "ae/runtime/game_module.h"

#include "game_module.h"

#include <iostream>

int main() {
    ae::log_info_cat("consumer", "Ahamkara package consumer smoke start");
    const double now = ae::now_seconds();
    ae::NetworkClock clock;
    clock.record_snapshot(1u, 60.0F, now);
    const double estimated = clock.estimate_server_time(now, 60.0F);
    (void)estimated;

    MinimalGameModule module;
    ae::GameModuleHostServices host;
    (void)module.initialize(host);
    ae::log_info_cat("consumer", "Module initialized");

    ae::Application app(ae::RuntimeMode::Tests);
    (void)app.start();

    for (int i = 0; i < 5; ++i) {
        ae::GameModuleFrameContext ctx{static_cast<double>(i + 1), 16.0F, static_cast<ae::u64>(i + 1)};
        (void)module.tick(ctx);
    }
    ae::log_info_cat("consumer", "Module ticked 5 times");

    module.shutdown();
    app.shutdown();

    std::cout << "ahamkara_consumer: ok\n";
    return 0;
}
