#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace Engine {
namespace Tests {

inline bool WaitUntil(const std::function<bool()>& predicate, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return predicate();
}

struct LatencyStats {
    size_t sampleCount = 0;
    double minMs = 0.0;
    double maxMs = 0.0;
    double meanMs = 0.0;
    double p50Ms = 0.0;
    double p95Ms = 0.0;
    double p99Ms = 0.0;
};

inline double Percentile(std::vector<double> values, double percentile) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const double rank = percentile * static_cast<double>(values.size() - 1);
    const size_t lower = static_cast<size_t>(std::floor(rank));
    const size_t upper = static_cast<size_t>(std::ceil(rank));
    if (lower == upper) {
        return values[lower];
    }
    const double fraction = rank - static_cast<double>(lower);
    return values[lower] * (1.0 - fraction) + values[upper] * fraction;
}

inline LatencyStats ComputeLatencyStats(std::vector<double> samplesMs) {
    LatencyStats stats;
    stats.sampleCount = samplesMs.size();
    if (samplesMs.empty()) {
        return stats;
    }

    stats.minMs = *std::min_element(samplesMs.begin(), samplesMs.end());
    stats.maxMs = *std::max_element(samplesMs.begin(), samplesMs.end());

    double sum = 0.0;
    for (double sample : samplesMs) {
        sum += sample;
    }
    stats.meanMs = sum / static_cast<double>(samplesMs.size());
    stats.p50Ms = Percentile(samplesMs, 0.50);
    stats.p95Ms = Percentile(samplesMs, 0.95);
    stats.p99Ms = Percentile(samplesMs, 0.99);
    return stats;
}

inline std::string FormatLatencyStats(const LatencyStats& stats) {
    return "count=" + std::to_string(stats.sampleCount) +
           " min=" + std::to_string(stats.minMs) + "ms" +
           " mean=" + std::to_string(stats.meanMs) + "ms" +
           " p50=" + std::to_string(stats.p50Ms) + "ms" +
           " p95=" + std::to_string(stats.p95Ms) + "ms" +
           " p99=" + std::to_string(stats.p99Ms) + "ms" +
           " max=" + std::to_string(stats.maxMs) + "ms";
}

} // namespace Tests
} // namespace Engine
