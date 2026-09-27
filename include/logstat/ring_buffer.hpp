#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace logstat
{

    /// Fixed-capacity FIFO that overwrites the oldest element when full.
    template <typename T, std::size_t N>
    class RingBuffer
    {
        static_assert(N > 0, "ring buffer capacity must be positive");

    public:
        void push(const T &value)
        {
            data_[tail_] = value;
            tail_ = (tail_ + 1) % N;
            if (size_ == N)
            {
                head_ = (head_ + 1) % N;
            }
            else
            {
                ++size_;
            }
        }

        [[nodiscard]] std::size_t size() const noexcept
        {
            return size_;
        }

        [[nodiscard]] bool empty() const noexcept
        {
            return size_ == 0;
        }

        [[nodiscard]] static constexpr std::size_t capacity() noexcept
        {
            return N;
        }

        /// Element `index` positions after the oldest element.
        [[nodiscard]] const T &operator[](std::size_t index) const
        {
            return data_[(head_ + index) % N];
        }

        /// Returns the most recent `count` elements, oldest first.
        [[nodiscard]] std::vector<T> recent(std::size_t count) const
        {
            if (count > size_)
            {
                count = size_;
            }
            std::vector<T> out;
            out.reserve(count);
            for (std::size_t i = size_ - count; i < size_; ++i)
            {
                out.push_back((*this)[i]);
            }
            return out;
        }

        void clear() noexcept
        {
            head_ = 0;
            tail_ = 0;
            size_ = 0;
        }

    private:
        std::array<T, N> data_{};
        std::size_t head_ = 0;
        std::size_t tail_ = 0;
        std::size_t size_ = 0;
    };

} // namespace logstat
