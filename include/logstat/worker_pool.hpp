#pragma once

#include <logstat/stats.hpp>

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace logstat
{

    /// Aggregated results shared by all workers plus a bounded diagnostic log.
    class SharedResults
    {
    public:
        void merge(const Aggregator &partial);
        void note(const std::string &message);
        void flush_log(std::vector<std::string> &sink);

        [[nodiscard]] Aggregator snapshot() const;

    private:
        void append_log_locked(const std::string &message);

        mutable std::mutex agg_mutex_;
        std::mutex log_mutex_;
        Aggregator aggregate_;
        std::vector<std::string> log_;
    };

    /// Minimal fixed-size thread pool for independent tasks.
    class WorkerPool
    {
    public:
        explicit WorkerPool(std::size_t threads);
        ~WorkerPool();

        WorkerPool(const WorkerPool &) = delete;
        WorkerPool &operator=(const WorkerPool &) = delete;

        void submit(std::function<void()> task);

        /// Blocks until every submitted task has finished.
        void wait();

        [[nodiscard]] std::size_t processed() const noexcept;
        [[nodiscard]] std::size_t size() const noexcept;

    private:
        void run();

        std::vector<std::thread> workers_;
        std::deque<std::function<void()>> queue_;
        std::mutex mutex_;
        std::condition_variable wake_;
        std::condition_variable idle_;
        std::size_t active_ = 0;
        std::size_t processed_ = 0;
        bool stopping_ = false;
    };

} // namespace logstat
