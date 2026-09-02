#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ae {

// =============================================================================
// Budget threshold types
// =============================================================================

/// How a noisy/timing budget threshold is evaluated.
enum class BudgetThresholdType : std::uint8_t {
    Absolute,     ///< Direct comparison: mean <= threshold.
    MeanPlusSigma, ///< mean + k * stddev <= threshold.
    Percentile,   ///< P-th percentile <= threshold.
    Delta,        ///< Absolute delta from expected <= threshold.
};

// =============================================================================
// Single benchmark budget specification
// =============================================================================

struct BenchmarkBudget {
    std::string name;              ///< Matches BenchmarkResult::name.
    std::string unit;              ///< Unit of measurement.
    BudgetThresholdType type{BudgetThresholdType::Absolute};
    double warn_threshold{0.0};    ///< 0 = no warning.
    double fail_threshold{0.0};    ///< 0 = no hard failure.
    double expected{0.0};          ///< Expected value (for counter/delta types).
    double k_factor{2.0};          ///< k for MeanPlusSigma.
    double percentile_p{95.0};     ///< Percentile for Percentile type.

    /// Evaluate a benchmark result value against this budget.
    /// Returns true if the value is within budget, false if it fails.
    /// `diagnostic` is populated with a message if not within warning.
    /// `is_failure` is set to true if the hard failure threshold was exceeded.
    [[nodiscard]] bool evaluate(double value, std::string& diagnostic, bool& is_failure) const {
        is_failure = false;
        diagnostic.clear();

        double effective_threshold = 0.0;
        bool threshold_valid = true;

        switch (type) {
            case BudgetThresholdType::Absolute:
                effective_threshold = fail_threshold > 0.0 ? fail_threshold : warn_threshold;
                if (effective_threshold <= 0.0) return true; // disabled
                if (value <= effective_threshold) return true;
                break;

            case BudgetThresholdType::MeanPlusSigma:
                // evaluated externally with sigma; here we just compare
                effective_threshold = fail_threshold > 0.0 ? fail_threshold : warn_threshold;
                if (effective_threshold <= 0.0) return true;
                if (value <= effective_threshold) return true;
                break;

            case BudgetThresholdType::Percentile:
                effective_threshold = fail_threshold > 0.0 ? fail_threshold : warn_threshold;
                if (effective_threshold <= 0.0) return true;
                if (value <= effective_threshold) return true;
                break;

            case BudgetThresholdType::Delta:
                effective_threshold = fail_threshold > 0.0 ? fail_threshold : warn_threshold;
                if (effective_threshold <= 0.0) return true;
                if (std::abs(value - expected) <= effective_threshold) return true;
                break;

            default:
                threshold_valid = false;
                break;
        }

        if (!threshold_valid) return true;

        // Check if we hit the hard failure threshold
        const double limit = fail_threshold > 0.0 ? fail_threshold : warn_threshold;
        is_failure = (fail_threshold > 0.0);

        std::ostringstream oss;
        if (is_failure) {
            oss << "FAIL: ";
        } else {
            oss << "WARN: ";
        }
        oss << "value " << value << " " << unit << " exceeds budget";
        if (type == BudgetThresholdType::Absolute || type == BudgetThresholdType::MeanPlusSigma || type == BudgetThresholdType::Percentile) {
            oss << " (limit " << effective_threshold << " " << unit << ")";
        } else if (type == BudgetThresholdType::Delta) {
            oss << " (expected " << expected << " " << unit << ", delta " << std::abs(value - expected)
                << " exceeds limit " << effective_threshold << " " << unit << ")";
        }
        diagnostic = oss.str();
        return false;
    }

    /// Serialize to JSON.
    [[nodiscard]] std::string to_json() const {
        std::ostringstream oss;
        oss << "    {\n";
        oss << "      \"name\": \"" << name << "\",\n";
        oss << "      \"unit\": \"" << unit << "\",\n";
        oss << "      \"type\": \"";
        switch (type) {
            case BudgetThresholdType::Absolute: oss << "absolute"; break;
            case BudgetThresholdType::MeanPlusSigma: oss << "mean_plus_sigma"; break;
            case BudgetThresholdType::Percentile: oss << "percentile"; break;
            case BudgetThresholdType::Delta: oss << "delta"; break;
        }
        oss << "\",\n";
        oss << "      \"warn_threshold\": " << warn_threshold << ",\n";
        oss << "      \"fail_threshold\": " << fail_threshold << ",\n";
        oss << "      \"expected\": " << expected << ",\n";
        oss << "      \"k_factor\": " << k_factor << ",\n";
        oss << "      \"percentile_p\": " << percentile_p << "\n";
        oss << "    }";
        return oss.str();
    }
};

// =============================================================================
// Budget configuration — collection of benchmark budgets.
// =============================================================================

class BudgetConfig {
public:
    /// Add or update a budget specification.
    void set_budget(const BenchmarkBudget& budget) {
        budgets_[budget.name] = budget;
    }

    /// Get budget for a benchmark name, or nullptr if not found.
    [[nodiscard]] const BenchmarkBudget* get(const std::string& name) const {
        auto it = budgets_.find(name);
        if (it != budgets_.end()) return &it->second;
        return nullptr;
    }

    /// Get mutable budget for editing.
    [[nodiscard]] BenchmarkBudget* get_mutable(const std::string& name) {
        auto it = budgets_.find(name);
        if (it != budgets_.end()) return &it->second;
        return nullptr;
    }

    /// Load budgets from a JSON file. Returns true on success.
    bool load_from_file(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return false;

        std::stringstream buffer;
        buffer << file.rdbuf();
        return parse_json(buffer.str());
    }

    /// Load budgets from a JSON string.
    /// Format:
    /// {
    ///   "budgets": [
    ///     {"name": "...", "unit": "...", "type": "absolute", ...}
    ///   ]
    /// }
    bool parse_json(const std::string& json) {
        // Simple JSON parser for the budget config format.
        // This avoids external dependencies.
        // Expected format:
        // { "budgets": [ { ... }, ... ] }

        std::size_t pos = 0;
        // Skip whitespace and find "budgets" array
        pos = json.find("\"budgets\"");
        if (pos == std::string::npos) return false;

        pos = json.find('[', pos);
        if (pos == std::string::npos) return false;
        ++pos;

        // Parse each object
        while (pos < json.size()) {
            // Find '{'
            pos = json.find('{', pos);
            if (pos == std::string::npos || pos >= json.size()) break;

            std::size_t end = json.find('}', pos);
            if (end == std::string::npos) break;

            std::string obj = json.substr(pos, end - pos + 1);
            parse_budget_object(obj);
            pos = end + 1;
        }

        return true;
    }

    /// Serialize all budgets to JSON.
    [[nodiscard]] std::string to_json() const {
        std::ostringstream oss;
        oss << "{\n";
        oss << "  \"budgets\": [\n";
        bool first = true;
        for (const auto& [name, budget] : budgets_) {
            (void)name;
            if (!first) oss << ",\n";
            oss << budget.to_json();
            first = false;
        }
        oss << "\n  ]\n";
        oss << "}\n";
        return oss.str();
    }

    /// Number of budgets configured.
    [[nodiscard]] std::size_t size() const { return budgets_.size(); }

    /// Clear all budgets.
    void clear() { budgets_.clear(); }

    /// Get all budget names.
    [[nodiscard]] std::vector<std::string> names() const {
        std::vector<std::string> result;
        for (const auto& [name, _] : budgets_) {
            result.push_back(name);
        }
        return result;
    }

private:
    std::unordered_map<std::string, BenchmarkBudget> budgets_;

    void parse_budget_object(const std::string& obj) {
        BenchmarkBudget budget;

        budget.name = extract_string(obj, "name");
        budget.unit = extract_string(obj, "unit");

        std::string type_str = extract_string(obj, "type");
        if (type_str == "absolute" || type_str.empty()) {
            budget.type = BudgetThresholdType::Absolute;
        } else if (type_str == "mean_plus_sigma") {
            budget.type = BudgetThresholdType::MeanPlusSigma;
        } else if (type_str == "percentile") {
            budget.type = BudgetThresholdType::Percentile;
        } else if (type_str == "delta") {
            budget.type = BudgetThresholdType::Delta;
        }

        budget.warn_threshold = extract_double(obj, "warn_threshold");
        budget.fail_threshold = extract_double(obj, "fail_threshold");
        budget.expected = extract_double(obj, "expected");
        budget.k_factor = extract_double(obj, "k_factor", 2.0);
        budget.percentile_p = extract_double(obj, "percentile_p", 95.0);

        if (!budget.name.empty()) {
            budgets_[budget.name] = budget;
        }
    }

    static std::string extract_string(const std::string& obj, const std::string& key) {
        std::string search = "\"" + key + "\":";
        auto pos = obj.find(search);
        if (pos == std::string::npos) return {};

        pos += search.size();
        // Skip whitespace
        while (pos < obj.size() && (obj[pos] == ' ' || obj[pos] == '\t' || obj[pos] == '\n')) ++pos;
        if (pos >= obj.size() || obj[pos] != '"') return {};

        ++pos;
        std::string result;
        while (pos < obj.size() && obj[pos] != '"') {
            if (obj[pos] == '\\' && pos + 1 < obj.size()) {
                ++pos;
            }
            result += obj[pos];
            ++pos;
        }
        return result;
    }

    static double extract_double(const std::string& obj, const std::string& key, double default_val = 0.0) {
        std::string search = "\"" + key + "\":";
        auto pos = obj.find(search);
        if (pos == std::string::npos) return default_val;

        pos += search.size();
        while (pos < obj.size() && (obj[pos] == ' ' || obj[pos] == '\t' || obj[pos] == '\n')) ++pos;
        if (pos >= obj.size()) return default_val;

        // Handle negative numbers
        std::string num;
        if (obj[pos] == '-') { num += '-'; ++pos; }
        while (pos < obj.size() && (std::isdigit(static_cast<unsigned char>(obj[pos])) || obj[pos] == '.' || obj[pos] == 'e' || obj[pos] == 'E' || obj[pos] == '+')) {
            num += obj[pos];
            ++pos;
        }
        return num.empty() ? default_val : std::stod(num);
    }
};

} // namespace ae
