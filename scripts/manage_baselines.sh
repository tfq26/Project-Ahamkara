#!/usr/bin/env bash
# =============================================================================
# manage_baselines.sh — Benchmark Baseline Management
# =============================================================================
#
# Establishes and updates performance baselines through an explicit reviewed
# workflow. Baselines are version-controlled budget configurations stored in
# benchmarks/budgets/.
#
# Usage:
#   ./scripts/manage_baselines.sh record [--label <label>]
#       Run all benchmarks and record results as a new baseline.
#       Results are saved to benchmarks/baselines/<label>-<timestamp>.json.
#
#   ./scripts/manage_baselines.sh compare <baseline_a> <baseline_b>
#       Compare two baselines and print a diff report.
#
#   ./scripts/manage_baselines.sh update-budgets [--baseline <file>]
#       Update benchmarks/budgets/default.json with values from a baseline run.
#       WARNING: This modifies version-controlled files. Use only through
#       a reviewed PR workflow (never on main directly).
#
#   ./scripts/manage_baselines.sh list
#       List all recorded baselines.
#
#   ./scripts/manage_baselines.sh show <baseline>
#       Show details of a specific baseline.
#
# Environment:
#   BUILD_DIR    - Build directory (default: build/release for release benchmarks)
#   BENCH_BINARY - Path to benchmark binary (default: $BUILD_DIR/tests/ahamkara_benchmark_tests)
#
# Examples:
#   # Record a baseline from a release build
#   cmake --preset release && cmake --build --preset release --target ahamkara_benchmark_tests
#   ./scripts/manage_baselines.sh record --label "servlenovo1-release"
#
#   # Compare two baselines
#   ./scripts/manage_baselines.sh compare baselines/2026-07-27-baseline.json baselines/2026-07-28-baseline.json
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Defaults
BUILD_DIR="${BUILD_DIR:-$REPO_DIR/build/release}"
BENCH_BINARY="${BENCH_BINARY:-$BUILD_DIR/tests/ahamkara_benchmark_tests}"
BASELINE_DIR="$REPO_DIR/benchmarks/baselines"
BUDGET_DIR="$REPO_DIR/benchmarks/budgets"
DEFAULT_BUDGET="$BUDGET_DIR/default.json"

mkdir -p "$BASELINE_DIR"

record() {
    local label="${1:-baseline}"
    local timestamp
    timestamp="$(date -u +%Y%m%dT%H%M%SZ)"
    local output_file="$BASELINE_DIR/${label}-${timestamp}.json"

    echo "=== Recording baseline: $label ==="
    echo "Timestamp: $timestamp"
    echo "Binary: $BENCH_BINARY"
    echo "Output: $output_file"

    if [[ ! -x "$BENCH_BINARY" ]]; then
        echo "ERROR: Benchmark binary not found at $BENCH_BINARY"
        echo "Hint: Build the benchmark target first:"
        echo "  cmake --build $BUILD_DIR --target ahamkara_benchmark_tests"
        exit 1
    fi

    # Run benchmarks with the default budget config (or no budget for pure recording)
    "$BENCH_BINARY" \
        --budgets "$DEFAULT_BUDGET" \
        --output "$output_file"

    echo ""
    echo "=== Baseline recorded ==="
    echo "File: $output_file"

    # Print summary
    python3 -c "
import json
with open('$output_file') as f:
    data = json.load(f)
print()
print('System: {os} | {cpu} | {cores} cores | {ram_gb:.1f} GB RAM'.format(
    os=data['system']['os_name'],
    cpu=data['system']['cpu_brand'],
    cores=data['system']['cpu_core_count'],
    ram_gb=data['system']['total_ram_bytes'] / 1e9
))
print('Build: {compiler} / {config}'.format(
    compiler=data['system']['compiler'],
    config=data['system']['build_config']
))
print()
print('{:<40} {:>12} {:>12} {:>12} {:>12} {}'.format(
    'Benchmark', 'Mean', 'Median', 'Min', 'Max', 'Unit'))
print('-' * 100)
for b in data['benchmarks']:
    s = b['stats'] if 'stats' in b else b
    print('{:<40} {:>12.4f} {:>12.4f} {:>12.4f} {:>12.4f} {}'.format(
        b['name'],
        b.get('mean', 0),
        b.get('median', 0),
        b.get('min', 0),
        b.get('max', 0),
        b.get('unit', '')
    ))
print()
print('All passed: {}'.format(data.get('all_passed', 'N/A')))
"
}

compare() {
    if [[ $# -lt 2 ]]; then
        echo "ERROR: compare requires two baseline files"
        echo "Usage: $0 compare <baseline_a> <baseline_b>"
        exit 1
    fi

    local a="$1"
    local b="$2"

    if [[ ! -f "$a" ]]; then
        a="$BASELINE_DIR/$a"
    fi
    if [[ ! -f "$b" ]]; then
        b="$BASELINE_DIR/$b"
    fi

    if [[ ! -f "$a" ]]; then echo "ERROR: baseline file not found: $a"; exit 1; fi
    if [[ ! -f "$b" ]]; then echo "ERROR: baseline file not found: $b"; exit 1; fi

    echo "=== Comparing baselines ==="
    echo "  A: $a"
    echo "  B: $b"
    echo ""

    python3 -c "
import json

with open('$a') as f:
    data_a = json.load(f)
with open('$b') as f:
    data_b = json.load(f)

bench_a = {b['name']: b for b in data_a['benchmarks']}
bench_b = {b['name']: b for b in data_b['benchmarks']}

print('{:<35} {:>15} {:>15} {:>15} {:>10}'.format(
    'Benchmark', 'Mean A', 'Mean B', 'Change %', 'Unit'))
print('-' * 95)

all_names = sorted(set(list(bench_a.keys()) + list(bench_b.keys())))
for name in all_names:
    ba = bench_a.get(name)
    bb = bench_b.get(name)
    if ba and bb:
        mean_a = ba.get('mean', 0)
        mean_b = bb.get('mean', 0)
        unit = ba.get('unit', bb.get('unit', ''))
        if mean_a != 0:
            pct = ((mean_b - mean_a) / abs(mean_a)) * 100.0
        else:
            pct = 0.0 if mean_b == 0 else float('inf')
        print('{:<35} {:>15.4f} {:>15.4f} {:>14.2f}% {:>10}'.format(
            name, mean_a, mean_b, pct, unit))
    elif ba:
        print('{:<35} {:>15.4f} {:>15} {:>14} {:>10}'.format(
            name, ba.get('mean', 0), 'N/A', 'N/A', ba.get('unit', '')))
    else:
        print('{:<35} {:>15} {:>15.4f} {:>14} {:>10}'.format(
            name, 'N/A', bb.get('mean', 0), 'N/A', bb.get('unit', '')))

print()
print('System A: {os} | {cpu}'.format(**data_a['system']))
print('System B: {os} | {cpu}'.format(**data_b['system']))
"
}

update_budgets() {
    local baseline_file=""

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --baseline) baseline_file="$2"; shift 2 ;;
            *) echo "Unknown option: $1"; exit 1 ;;
        esac
    done

    if [[ -z "$baseline_file" ]]; then
        # Use the most recent baseline
        baseline_file="$(ls -t "$BASELINE_DIR"/*.json 2>/dev/null | head -1)"
        if [[ -z "$baseline_file" ]]; then
            echo "ERROR: No baselines found. Run 'record' first or specify --baseline."
            exit 1
        fi
        echo "Using most recent baseline: $baseline_file"
    fi

    echo ""
    echo "WARNING: This will modify ${DEFAULT_BUDGET} with values from"
    echo "         the baseline. This file is version-controlled."
    echo ""
    echo "This operation should only be performed through a reviewed PR."
    echo "Run this script, commit the changes, and open a PR."
    echo ""

    read -r -p "Are you sure you want to continue? [y/N] " confirm
    if [[ ! "$confirm" =~ ^[Yy]$ ]]; then
        echo "Aborted."
        exit 0
    fi

    # Read baseline and update budget thresholds
    python3 -c "
import json

with open('$baseline_file') as f:
    baseline = json.load(f)

with open('$DEFAULT_BUDGET') as f:
    budget_config = json.load(f)

# Create a lookup from baseline benchmark results
bench_lookup = {b['name']: b for b in baseline['benchmarks']}

# Update budget thresholds with 2x mean as a safe headroom
for budget in budget_config['budgets']:
    name = budget['name']
    if name in bench_lookup:
        bench = bench_lookup[name]
        mean_val = bench.get('mean', 0)
        unit = bench.get('unit', '')

        # Determine if higher is better
        higher_better = 'sec' in unit or 'throughput' in unit or 'FPS' in unit

        if higher_better:
            # For throughput: set fail to 10% of mean (very loose min)
            # and warn to 50% of mean
            if mean_val > 0:
                new_fail = mean_val * 0.1
                new_warn = mean_val * 0.5
            else:
                new_fail = budget.get('fail_threshold', 0)
                new_warn = budget.get('warn_threshold', 0)
        else:
            # For latency: set fail to 10x mean, warn to 5x mean
            if mean_val > 0:
                new_fail = mean_val * 10.0
                new_warn = mean_val * 5.0
            else:
                new_fail = budget.get('fail_threshold', 0)
                new_warn = budget.get('warn_threshold', 0)

        # Convert ms to us if needed
        budget['fail_threshold'] = round(new_fail, 6)
        budget['warn_threshold'] = round(new_warn, 6)

        print(f'  Updated {name}: mean={mean_val:.6f} -> '
              f'warn={new_warn:.6f}, fail={new_fail:.6f}')

with open('$DEFAULT_BUDGET', 'w') as f:
    json.dump(budget_config, f, indent=2)
    f.write('\n')

print()
print(f'Updated {DEFAULT_BUDGET}')
print('Review the changes with: git diff')
print('Commit and open a PR to establish the new baseline.')
"
}

list_baselines() {
    echo "=== Recorded Baselines ==="
    echo ""

    local files=("$BASELINE_DIR"/*.json)
    if [[ ${#files[@]} -eq 0 ]] || [[ ! -f "${files[0]}" ]]; then
        echo "No baselines found. Run '$0 record' first."
        exit 0
    fi

    for f in "${files[@]}"; do
        local name
        name="$(basename "$f" .json)"
        local size
        size="$(du -h "$f" | cut -f1)"
        local date
        date="$(date -r "$f" -u '+%Y-%m-%d %H:%M:%S UTC' 2>/dev/null || echo 'unknown')"

        # Extract key info from JSON
        local info
        info=$(python3 -c "
import json
with open('$f') as fh:
    d = json.load(fh)
sys = d['system']
results = d.get('all_passed', 'N/A')
print(f'{sys[\"os_name\"]} | {sys[\"cpu_brand\"]} | {sys[\"build_config\"]} | passed={results}')
" 2>/dev/null || echo "unreadable")

        printf "  %-50s %-8s %-25s %s\n" "$name" "$size" "$date" "$info"
    done
}

show_baseline() {
    local file="$1"
    if [[ -z "$file" ]]; then
        echo "ERROR: specify a baseline file name or path"
        echo "Usage: $0 show <baseline>"
        exit 1
    fi
    if [[ ! -f "$file" ]]; then
        file="$BASELINE_DIR/$file"
    fi
    if [[ ! -f "$file" ]]; then
        echo "ERROR: baseline not found: $file"
        exit 1
    fi

    python3 -c "
import json
with open('$file') as f:
    d = json.load(f)
print(json.dumps(d, indent=2))
"
}

# =============================================================================
# Main
# =============================================================================

case "${1:-help}" in
    record)
        shift
        label=""
        while [[ $# -gt 0 ]]; do
            case "$1" in
                --label) label="$2"; shift 2 ;;
                *) echo "Unknown option: $1"; exit 1 ;;
            esac
        done
        record "${label:-baseline}"
        ;;
    compare)
        shift
        compare "$@"
        ;;
    update-budgets)
        shift
        update_budgets "$@"
        ;;
    list)
        list_baselines
        ;;
    show)
        shift
        show_baseline "$1"
        ;;
    help|--help|-h)
        echo "Ahamkara Benchmark Baseline Manager"
        echo ""
        echo "Usage: $0 <command> [options]"
        echo ""
        echo "Commands:"
        echo "  record [--label <label>]    Run benchmarks and save as baseline"
        echo "  compare <a> <b>             Compare two baselines"
        echo "  update-budgets [--baseline <f>] Update budget config from baseline"
        echo "  list                        List all baselines"
        echo "  show <baseline>             Show baseline details"
        echo "  help                        Show this help"
        ;;
    *)
        echo "Unknown command: $1"
        echo "Usage: $0 <command> [options]"
        echo "Run '$0 help' for details."
        exit 1
        ;;
esac
