#pragma once

#include <cassert>
#include <exception>
#include <functional>
#include <optional>
#include <source_location>
#include <string>
#include <utility>
#include <variant>

namespace cppresult
{

    template <typename T>
    class Result
    {
    public:
        using value_type = T;

        [[nodiscard]] static Result Ok(T value)
        {
            return Result(value_tag{}, std::move(value));
        }

        [[nodiscard]] static Result Failure(
            std::string err,
            std::optional<std::exception> ex = std::nullopt,
            std::source_location location = std::source_location::current())
        {
            return Result(failure_tag{}, FailureData{std::move(err), std::move(ex), location});
        }

        [[nodiscard]] bool has_value() const noexcept
        {
            return storage_.index() == 0;
        }
        [[nodiscard]] bool has_error() const noexcept
        {
            return storage_.index() == 1;
        }
        
        [[nodiscard]] explicit operator bool() const noexcept
        {
            return has_value();
        }

        [[nodiscard]] bool has_failure() const noexcept
        {
            return has_error();
        }

        [[nodiscard]] T &value() &
        {
            assert(has_value());
            return std::get<0>(storage_);
        }

        [[nodiscard]] const T &value() const &
        {
            assert(has_value());
            return std::get<0>(storage_);
        }

        [[nodiscard]] T &&value() &&
        {
            assert(has_value());
            return std::get<0>(std::move(storage_));
        }

        [[nodiscard]] const std::string &error() const &
        {
            assert(!has_value());
            return std::get<1>(storage_).err;
        }

        [[nodiscard]] const std::optional<std::exception> &exception() const &
        {
            assert(!has_value());
            return std::get<1>(storage_).ex;
        }

        [[nodiscard]] std::source_location source_location() const
        {
            assert(!has_value());
            return std::get<1>(storage_).location;
        }

        template <typename U>
        [[nodiscard]] T value_or(U &&fallback) const &
        {
            if (has_value())
            {
                return value();
            }
            return static_cast<T>(std::forward<U>(fallback));
        }

        template <typename F>
        [[nodiscard]] auto map(F &&function) const & -> Result<std::invoke_result_t<F, const T &>>
        {
            using Mapped = std::invoke_result_t<F, const T &>;
            if (has_value())
            {
                return Result<Mapped>::Ok(std::invoke(
                    std::forward<F>(function), value()));
            }

            const auto &failure = std::get<1>(storage_);
            return Result<Mapped>::Failure(
                failure.err, failure.ex, failure.location);
        }

    private:
        struct value_tag
        {
        };

        struct failure_tag
        {
        };

        struct FailureData
        {
            std::string err;
            std::optional<std::exception> ex;
            std::source_location location;
        };

        Result(value_tag, value_type value)
            : storage_(std::in_place_index<0>, std::move(value))
        {
        }

        Result(failure_tag, FailureData failure)
            : storage_(std::in_place_index<1>, std::move(failure))
        {
        }

        std::variant<value_type, FailureData> storage_;
    };

} // namespace cppresult
