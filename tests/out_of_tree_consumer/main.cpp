#include "ae/core/log.h"
#include "ae/core/time.h"
#include "ae/network/network_clock.h"
#include "ae/runtime/application.h"

#include <iostream>

int main() {
    ae::log_info_cat("consumer", "Ahamkara package consumer smoke start");
    const double now = ae::now_seconds();
    ae::NetworkClock clock;
    clock.record_snapshot(1u, 60.0F, now);
    const double estimated = clock.estimate_server_time(now, 60.0F);
    (void)estimated;

    ae::Application app(ae::RuntimeMode::Tests);
    app.start();
    app.shutdown();

    std::cout << "ahamkara_consumer: ok\n";
    return 0;
}
