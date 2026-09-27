#pragma once

#include <logstat/line_parser.hpp>
#include <logstat/ring_buffer.hpp>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace logstat
{

    struct LatencySummary
    {
        std::size_t count = 0;
        double min = 0.0;
        double max = 0.0;
        double mean = 0.0;
        double p50 = 0.0;
        double p95 = 0.0;
        double p99 = 0.0;
    };

    struct ComponentStats
    {
        std::string name;
        std::size_t records = 0;
        std::size_t errors = 0;
        std::size_t calls_out = 0;
    };

    /// Percentile of an ascending-sorted sample using nearest-rank on a zero-based index.
    [[nodiscard]] double percentile(const std::vector<double> &sorted, double p);

    class Aggregator
    {
    public:
        Aggregator();

        void add(const LogRecord &record);
        void merge(const Aggregator &other);
        void reset();

        [[nodiscard]] std::size_t total() const noexcept;
        [[nodiscard]] std::size_t count(Level level) const noexcept;
        [[nodiscard]] std::size_t failures() const noexcept;

        [[nodiscard]] LatencySummary latency() const;
        [[nodiscard]] double latency_percentile(double p) const;

        [[nodiscard]] std::vector<std::pair<std::string, std::size_t>> top_messages(std::size_t n) const;
        void prune_rare_messages(std::size_t min_count);

        [[nodiscard]] const std::vector<ComponentStats> &components() const noexcept;
        [[nodiscard]] std::vector<std::string> recent_errors() const;

    private:
        ComponentStats &component(std::string_view name);

        std::array<std::size_t, level_count> level_counts_{};
        std::vector<double> latencies_;
        double min_latency_;
        double max_latency_;
        std::unordered_map<std::string, std::size_t> message_counts_;
        std::vector<ComponentStats> components_;
        RingBuffer<std::string, 8> recent_errors_;
    };

} // namespace logstat
