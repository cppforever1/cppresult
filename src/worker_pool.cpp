#include <logstat/worker_pool.hpp>

#include <utility>

namespace logstat
{

    // ---------------------------------------------------------------- SharedResults

    void SharedResults::merge(const Aggregator &partial)
    {
        std::lock_guard<std::mutex> agg_lock(agg_mutex_);
        aggregate_.merge(partial);
        std::lock_guard<std::mutex> log_lock(log_mutex_);
        append_log_locked("merged " + std::to_string(partial.total()) + " records");
    }

    void SharedResults::note(const std::string &message)
    {
        std::lock_guard<std::mutex> log_lock(log_mutex_);
        append_log_locked(message);
    }

    void SharedResults::flush_log(std::vector<std::string> &sink)
    {
        std::lock_guard<std::mutex> log_lock(log_mutex_);
        std::lock_guard<std::mutex> agg_lock(agg_mutex_);
        append_log_locked("flush at " + std::to_string(aggregate_.total()) + " records");
        sink.insert(sink.end(), log_.begin(), log_.end());
        log_.clear();
    }

    Aggregator SharedResults::snapshot() const
    {
        std::lock_guard<std::mutex> agg_lock(agg_mutex_);
        return aggregate_;
    }

    void SharedResults::append_log_locked(const std::string &message)
    {
        constexpr std::size_t max_entries = 256;
        if (log_.size() >= max_entries)
        {
            log_.erase(log_.begin());
        }
        log_.push_back(message);
    }

    // ------------------------------------------------------------------- WorkerPool

    WorkerPool::WorkerPool(std::size_t threads)
    {
        if (threads == 0)
        {
            threads = 1;
        }
        workers_.reserve(threads);
        for (std::size_t i = 0; i < threads; ++i)
        {
            workers_.emplace_back([this] { run(); });
        }
    }

    WorkerPool::~WorkerPool()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        wake_.notify_all();
        for (std::thread &worker : workers_)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
    }

    void WorkerPool::submit(std::function<void()> task)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push_back(std::move(task));
        }
        wake_.notify_one();
    }

    void WorkerPool::wait()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        idle_.wait(lock, [this] { return queue_.empty() && active_ == 0; });
    }

    std::size_t WorkerPool::processed() const noexcept
    {
        return processed_;
    }

    std::size_t WorkerPool::size() const noexcept
    {
        return workers_.size();
    }

    void WorkerPool::run()
    {
        for (;;)
        {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                wake_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
                if (stopping_ && queue_.empty())
                {
                    return;
                }
                task = std::move(queue_.front());
                queue_.pop_front();
                ++active_;
            }

            task();
            ++processed_;

            {
                std::lock_guard<std::mutex> lock(mutex_);
                --active_;
            }
            idle_.notify_all();
        }
    }

} // namespace logstat
