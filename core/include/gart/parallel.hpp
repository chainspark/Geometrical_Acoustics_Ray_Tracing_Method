// Geometrical Acoustics Ray Tracing (gart) - multi-threaded tracing for native builds.
#pragma once

#include <algorithm>
#include <cstdint>
#include <thread>
#include <vector>

#include "gart/tracer.hpp"

namespace gart {

/// Traces all rays of the scene on `threads` threads (0 = hardware concurrency).
/// Rays are split into contiguous ranges; each thread has its own accumulator and
/// the partial sums are added in a fixed order.
template <class Receivers>
[[nodiscard]] Accumulator traceParallel(const Scene& scene, const Receivers& receivers, unsigned threads = 0) {
    const std::uint64_t n = scene.settings.rayCount;
    if (threads == 0) threads = std::max(1u, std::thread::hardware_concurrency());
    threads = static_cast<unsigned>(std::min<std::uint64_t>(threads, n));

    std::vector<Accumulator> partial(threads, Accumulator(receivers.size()));
    {
        std::vector<std::jthread> pool;
        pool.reserve(threads);
        for (unsigned t = 0; t < threads; ++t) {
            const std::uint64_t b = n * t / threads;
            const std::uint64_t e = n * (t + 1) / threads;
            pool.emplace_back([&, b, e, t] { trace(scene, receivers, b, e, partial[t]); });
        }
    } // jthreads join here

    Accumulator total(receivers.size());
    for (const auto& p : partial) total += p;
    return total;
}

} // namespace gart
