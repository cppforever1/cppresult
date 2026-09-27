#include <logstat/stats.hpp>

#include <algorithm>
#include <limits>
#include <numeric>

namespace logstat
{

    double percentile(const std::vector<double> &sorted, double p)
    {
        if (sorted.empty())
        {
            return 0.0;
        }
        if (p < 0.0)
        {
            p = 0.0;
        }
        if (p > 100.0)
        {
            p = 100.0;
        }
        const auto index = static_cast<std::size_t>(p / 100.0 * static_cast<double>(sorted.size()));
        return sorted[index];
    }

    Aggregator::Aggregator()
        : min_latency_(std::numeric_limits<double>::infinity()),
          max_latency_(0.0)
    {
    }

    ComponentStats &Aggregator::component(std::string_view name)
    {
        for (ComponentStats &existing : components_)
        {
            if (existing.name == name)
            {
                return existing;
            }
        }
        ComponentStats created;
        created.name = std::string(name);
        components_.push_back(std::move(created));
        return components_.back();
    }

    void Aggregator::add(const LogRecord &record)
    {
        ++level_counts_[level_index(record.level)];

        ComponentStats &owner = component(record.component);
        const auto caller = record.fields.find("caller");
        if (caller != record.fields.end())
        {
            ComponentStats &upstream = component(caller->second);
            ++upstream.calls_out;
        }
        ++owner.records;
        if (record.level == Level::Error || record.level == Level::Fatal)
        {
            ++owner.errors;
            recent_errors_.push(record.component + ": " + record.message);
        }

        if (record.latency_ms)
        {
            const double value = *record.latency_ms;
            latencies_.push_back(value);
            min_latency_ = std::min(min_latency_, value);
            max_latency_ = std::max(max_latency_, value);
        }

        ++message_counts_[record.message];
    }

    void Aggregator::merge(const Aggregator &other)
    {
        for (std::size_t i = 0; i < level_count; ++i)
        {
            level_counts_[i] += other.level_counts_[i];
        }
        latencies_.insert(latencies_.end(), other.latencies_.begin(), other.latencies_.end());
        min_latency_ = std::min(min_latency_, other.min_latency_);
        max_latency_ = std::max(max_latency_, other.max_latency_);
        for (const auto &[message, count] : other.message_counts_)
        {
            message_counts_[message] += count;
        }
        for (const ComponentStats &theirs : other.components_)
        {
            ComponentStats &mine = component(theirs.name);
            mine.records += theirs.records;
            mine.errors += theirs.errors;
            mine.calls_out += theirs.calls_out;
        }
        for (std::size_t i = 0; i < other.recent_errors_.size(); ++i)
        {
            recent_errors_.push(other.recent_errors_[i]);
        }
    }

    void Aggregator::reset()
    {
        level_counts_.fill(0);
        latencies_.clear();
        min_latency_ = 0.0;
        max_latency_ = 0.0;
        message_counts_.clear();
        components_.clear();
        recent_errors_.clear();
    }

    std::size_t Aggregator::total() const noexcept
    {
        return std::accumulate(level_counts_.begin(), level_counts_.end(), std::size_t{0});
    }

    std::size_t Aggregator::count(Level level) const noexcept
    {
        return level_counts_[level_index(level)];
    }

    std::size_t Aggregator::failures() const noexcept
    {
        return count(Level::Error) + count(Level::Fatal);
    }

    LatencySummary Aggregator::latency() const
    {
        LatencySummary summary;
        summary.count = latencies_.size();
        if (latencies_.empty())
        {
            return summary;
        }
        std::vector<double> sorted = latencies_;
        std::sort(sorted.begin(), sorted.end());
        summary.min = min_latency_;
        summary.max = max_latency_;
        summary.mean = std::accumulate(sorted.begin(), sorted.end(), 0.0) / static_cast<double>(sorted.size());
        summary.p50 = percentile(sorted, 50.0);
        summary.p95 = percentile(sorted, 95.0);
        summary.p99 = percentile(sorted, 99.0);
        return summary;
    }

    double Aggregator::latency_percentile(double p) const
    {
        std::vector<double> sorted = latencies_;
        std::sort(sorted.begin(), sorted.end());
        return percentile(sorted, p);
    }

    std::vector<std::pair<std::string, std::size_t>> Aggregator::top_messages(std::size_t n) const
    {
        std::vector<std::pair<std::string, std::size_t>> entries(message_counts_.begin(), message_counts_.end());
        std::sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
            if (left.second != right.second)
            {
                return left.second > right.second;
            }
            return left.first < right.first;
        });
        if (entries.size() > n)
        {
            entries.resize(n);
        }
        return entries;
    }

    void Aggregator::prune_rare_messages(std::size_t min_count)
    {
        for (auto it = message_counts_.begin(); it != message_counts_.end(); ++it)
        {
            if (it->second < min_count)
            {
                message_counts_.erase(it);
            }
        }
    }

    const std::vector<ComponentStats> &Aggregator::components() const noexcept
    {
        return components_;
    }

    std::vector<std::string> Aggregator::recent_errors() const
    {
        return recent_errors_.recent(recent_errors_.size());
    }

} // namespace logstat
