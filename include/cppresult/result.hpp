#pragma once

#include <exception>
#include <functional>
#include <source_location>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace cppresult
{

    template <typename T>
    class [[nodiscard]] Result
    {
    public:
        using value_type = T;

        [[nodiscard]] static Result Ok(T value)
        {
            return Result(value_tag{}, std::move(value));
        }

        [[nodiscard]] static Result Failure(
            std::string err,
            std::exception_ptr ex = {},
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
            return std::get<0>(storage_);
        }

        [[nodiscard]] const T &value() const &
        {
            return std::get<0>(storage_);
        }

        [[nodiscard]] T &&value() &&
        {
            return std::get<0>(std::move(storage_));
        }

        [[nodiscard]] const std::string &error() const &
        {
            return std::get<1>(storage_).err;
        }

        [[nodiscard]] const std::exception_ptr &exception() const &
        {
            return std::get<1>(storage_).ex;
        }

        [[nodiscard]] std::source_location source_location() const
        {
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

        template <typename U>
        [[nodiscard]] T value_or(U &&fallback) &&
        {
            if (has_value())
            {
                return std::move(*this).value();
            }
            return static_cast<T>(std::forward<U>(fallback));
        }

        template <typename F>
        [[nodiscard]] auto map(F &&function) const & -> Result<std::invoke_result_t<F, const T &>>
        {
            using Mapped = std::invoke_result_t<F, const T &>;
            if (has_value())
            {
                if constexpr (std::is_void_v<Mapped>)
                {
                    std::invoke(std::forward<F>(function), value());
                    return Result<Mapped>::Ok();
                }
                else
                {
                    return Result<Mapped>::Ok(std::invoke(
                        std::forward<F>(function), value()));
                }
            }

            const auto &failure = std::get<1>(storage_);
            return Result<Mapped>::Failure(
                failure.err, failure.ex, failure.location);
        }

        template <typename F>
        [[nodiscard]] auto map(F &&function) && -> Result<std::invoke_result_t<F, T &&>>
        {
            using Mapped = std::invoke_result_t<F, T &&>;
            if (has_value())
            {
                if constexpr (std::is_void_v<Mapped>)
                {
                    std::invoke(std::forward<F>(function), std::move(*this).value());
                    return Result<Mapped>::Ok();
                }
                else
                {
                    return Result<Mapped>::Ok(std::invoke(
                        std::forward<F>(function), std::move(*this).value()));
                }
            }

            auto &failure = std::get<1>(storage_);
            return Result<Mapped>::Failure(
                std::move(failure.err), std::move(failure.ex), failure.location);
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
            std::exception_ptr ex;
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

    template <>
    class [[nodiscard]] Result<void> : private Result<std::monostate>
    {
        using Base = Result<std::monostate>;

    public:
        using value_type = void;
        using Base::error;
        using Base::exception;
        using Base::has_error;
        using Base::has_failure;
        using Base::has_value;
        using Base::operator bool;
        using Base::source_location;

        [[nodiscard]] static Result Ok()
        {
            return Result(Base::Ok({}));
        }

        [[nodiscard]] static Result Failure(
            std::string err,
            std::exception_ptr ex = {},
            std::source_location location = std::source_location::current())
        {
            return Result(Base::Failure(std::move(err), std::move(ex), location));
        }

        void value() const
        {
            (void)Base::value();
        }

        template <typename F>
        [[nodiscard]] auto map(F &&function) const & -> Result<std::invoke_result_t<F>>
        {
            return Base::map([&](std::monostate) -> std::invoke_result_t<F>
                             { return std::invoke(std::forward<F>(function)); });
        }

        template <typename F>
        [[nodiscard]] auto map(F &&function) && -> Result<std::invoke_result_t<F>>
        {
            return std::move(static_cast<Base &>(*this)).map(
                [&](std::monostate) -> std::invoke_result_t<F>
                { return std::invoke(std::forward<F>(function)); });
        }

    private:
        explicit Result(Base result)
            : Base(std::move(result))
        {
        }
    };

} // namespace cppresult
