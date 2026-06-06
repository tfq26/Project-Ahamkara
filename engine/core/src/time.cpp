#include "ae/core/time.h"

#include <chrono>

namespace ae {

double now_seconds() {
    using clock = std::chrono::steady_clock;
    static const clock::time_point start_time = clock::now();
    const auto elapsed = clock::now() - start_time;
    return std::chrono::duration<double>(elapsed).count();
}

}  // namespace ae

