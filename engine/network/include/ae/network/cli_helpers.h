#pragma once

#include "ae/core/cli_utils.h"
#include "ae/network/network_simulator.h"

namespace ae {

/**
 * @brief Build a SimulatorConfig from CLI --simulate-* arguments.
 *
 * Scans argv for --simulate, --simulate-loss, --simulate-latency,
 * --simulate-latency-max, and --simulate-jitter flags and populates
 * the config accordingly.
 */
inline SimulatorConfig build_sim_config(int argc, char** argv) {
    SimulatorConfig cfg {};
    for (int i = 1; i < argc; ++i) {
        cfg.loss_rate       = parse_float_arg(argv[i], "simulate-loss",     cfg.loss_rate);
        cfg.latency_min_ms  = parse_float_arg(argv[i], "simulate-latency",  cfg.latency_min_ms);
        if (cfg.latency_min_ms > 0.0F && cfg.latency_max_ms == 0.0F) {
            cfg.latency_max_ms = cfg.latency_min_ms;
        }
        cfg.latency_max_ms  = parse_float_arg(argv[i], "simulate-latency-max", cfg.latency_max_ms);
        cfg.jitter_ms       = parse_float_arg(argv[i], "simulate-jitter",   cfg.jitter_ms);
        if (parse_bool_arg(argv[i], "simulate")) {
            cfg.enabled = true;
        }
    }
    return cfg;
}

}  // namespace ae
