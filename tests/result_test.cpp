#include <cppresult/result.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            std::cerr << __FILE__ << ':' << __LINE__ << ": " #condition << '\n';                                       \
            std::abort();                                                                                              \
        }                                                                                                              \
    } while (false)

template <typename Exception, typename F> void check_throws(F &&function)
{
    try
    {
        function();
    }
    catch (const Exception &)
    {
        return;
    }
    CHECK(false);
}

cppresult::Result<int> forwarded_failure(std::source_location location = std::source_location::current())
{
    return cppresult::Result<int>::Failure("forwarded", {}, location);
}

struct ThrowOnMove
{
    int value;

    explicit ThrowOnMove(int n) : value(n)
    {
    }
    ThrowOnMove(ThrowOnMove &&other) : value(other.value)
    {
        if (value < 0)
        {
            throw std::runtime_error("move failed");
        }
    }
    ThrowOnMove &operator=(ThrowOnMove &&) = default;
};

void test_assignment()
{
    using Result = cppresult::Result<ThrowOnMove>;
    auto source = Result::Ok(ThrowOnMove(5));
    auto destination = Result::Failure("initial failure");
    source.value().value = -1;
    check_throws<std::runtime_error>([&] { destination = std::move(source); });
    CHECK(!destination.has_value());
    CHECK(!destination.has_error());
    check_throws<std::bad_variant_access>([&] { (void)destination.value(); });
    check_throws<std::bad_variant_access>([&] { (void)destination.error(); });
    destination = Result::Failure("recovered");
    CHECK(destination.error() == "recovered");
    source.value().value = 11;
    destination = std::move(source);
    CHECK(destination.value().value == 11);

    auto void_result = cppresult::Result<void>::Ok();
    const auto void_failure = cppresult::Result<void>::Failure("copy failure");
    void_result = void_failure;
    CHECK(void_result.error() == "copy failure");
    void_result = cppresult::Result<void>::Ok();
    CHECK(void_result);
}

void test_failure()
{
    auto exception = std::make_exception_ptr(std::runtime_error("original details"));
    const auto line = __LINE__ + 1;
    auto failure = cppresult::Result<int>::Failure("bad input", exception);
    CHECK(!failure.has_value());
    CHECK(failure.has_error());
    CHECK(failure.has_failure());
    CHECK(!failure);
    CHECK(failure.error() == "bad input");
    CHECK(failure.value_or(7) == 7);
    CHECK(failure.exception() == exception);
    CHECK(failure.source_location().file_name() == std::string_view(__FILE__));
    CHECK(failure.source_location().line() == line);
    CHECK(std::string_view(failure.source_location().function_name()).find("test_failure") != std::string_view::npos);
    try
    {
        std::rethrow_exception(failure.exception());
    }
    catch (const std::runtime_error &error)
    {
        CHECK(std::string_view(error.what()) == "original details");
    }

    const auto forwarded_line = __LINE__ + 1;
    auto forwarded = forwarded_failure();
    CHECK(forwarded.source_location().line() == forwarded_line);
    CHECK(!forwarded.exception());

    bool called = false;
    auto mapped_failure = failure.map([&](int) {
        called = true;
        return std::string("unexpected");
    });
    CHECK(!called);
    CHECK(mapped_failure.error() == "bad input");
    CHECK(mapped_failure.exception() == exception);
    CHECK(mapped_failure.source_location().line() == line);
    CHECK(mapped_failure.source_location().function_name() ==
          std::string_view(failure.source_location().function_name()));

    auto moved_failure = std::move(failure).map([&](int) {
        called = true;
        return 3;
    });
    CHECK(!called);
    CHECK(moved_failure.error() == "bad input");
    CHECK(moved_failure.exception() == exception);
    CHECK(moved_failure.source_location().line() == line);
}

void test_access()
{
    auto failure = cppresult::Result<int>::Failure("no value");
    check_throws<std::bad_variant_access>([&] { (void)failure.value(); });
    check_throws<std::bad_variant_access>([&] { (void)std::as_const(failure).value(); });
    check_throws<std::bad_variant_access>([&] { (void)std::move(failure).value(); });

    auto success = cppresult::Result<int>::Ok(42);
    check_throws<std::bad_variant_access>([&] { (void)success.error(); });
    check_throws<std::bad_variant_access>([&] { (void)success.exception(); });
    check_throws<std::bad_variant_access>([&] { (void)success.source_location(); });
}

void test_values()
{
    auto success = cppresult::Result<int>::Ok(42);
    CHECK(success.has_value());
    CHECK(!success.has_error());
    CHECK(success);
    CHECK(success.value() == 42);
    CHECK(success.value_or(7) == 42);
    static_assert(std::is_same_v<decltype(success.value()), int &>);
    static_assert(std::is_same_v<decltype(std::as_const(success).value()), const int &>);
    static_assert(std::is_same_v<decltype(std::move(success).value()), int &&>);
    auto mapped = success.map([](int value) { return std::to_string(value + 1); });
    CHECK(mapped.value() == "43");
    check_throws<std::runtime_error>(
        [&] { (void)success.map([](int) -> int { throw std::runtime_error("callback"); }); });

    using MoveResult = cppresult::Result<std::unique_ptr<int>>;
    auto move_only = cppresult::Result<std::unique_ptr<int>>::Ok(std::make_unique<int>(9));
    auto borrowed = move_only.map([](const std::unique_ptr<int> &value) { return *value + 2; });
    CHECK(borrowed.value() == 11);
    CHECK(*move_only.value() == 9);
    auto consumed =
        std::move(move_only).map([](std::unique_ptr<int> value) { return std::make_unique<int>(*value + 4); });
    CHECK(*consumed.value() == 13);
    CHECK(!move_only.value());

    auto fallback = std::make_unique<int>(17);
    auto selected = std::move(consumed).value_or(std::move(fallback));
    CHECK(*selected == 13);
    CHECK(*fallback == 17);
    auto missing = MoveResult::Failure("missing");
    selected = std::move(missing).value_or(std::move(fallback));
    CHECK(*selected == 17);
    CHECK(!fallback);
}

void test_void()
{
    auto success = cppresult::Result<void>::Ok();
    CHECK(success);
    success.value();
    CHECK(success.map([] { return 23; }).value() == 23);
    CHECK(*std::move(success).map([] { return std::make_unique<int>(29); }).value() == 29);
    int calls = 0;
    auto void_mapped = success.map([&] { ++calls; });
    CHECK(void_mapped);
    auto moved_void = std::move(success).map([&] { ++calls; });
    CHECK(moved_void);
    CHECK(calls == 2);

    auto value = cppresult::Result<int>::Ok(31);
    auto discarded = value.map([&](int n) { calls += n; });
    CHECK(discarded);
    CHECK(calls == 33);
    auto move_only = cppresult::Result<std::unique_ptr<int>>::Ok(std::make_unique<int>(37));
    auto consumed = std::move(move_only).map([&](std::unique_ptr<int> n) { calls += *n; });
    CHECK(consumed);
    CHECK(calls == 70);
    CHECK(!move_only.value());

    const auto exception = std::make_exception_ptr(std::logic_error("void details"));
    const auto line = __LINE__ + 1;
    auto failure = cppresult::Result<void>::Failure("void error", exception);
    CHECK(!failure);
    CHECK(failure.has_error());
    CHECK(failure.has_failure());
    CHECK(failure.exception() == exception);
    CHECK(failure.source_location().line() == line);
    check_throws<std::bad_variant_access>([&] { failure.value(); });
    auto failed_map = failure.map([&] { ++calls; });
    CHECK(failed_map.error() == "void error");
    CHECK(failed_map.exception() == exception);
    CHECK(failed_map.source_location().line() == line);
    auto failed_value_map = failure.map([&] {
        ++calls;
        return 41;
    });
    CHECK(failed_value_map.error() == "void error");
    auto failed_move = std::move(failure).map([&] {
        ++calls;
        return 43;
    });
    CHECK(failed_move.error() == "void error");
    CHECK(failed_move.exception() == exception);
    CHECK(failed_move.source_location().line() == line);
    auto failed_void_move = std::move(failed_map).map([&] { ++calls; });
    CHECK(failed_void_move.error() == "void error");
    CHECK(calls == 70);

    auto failed_int = cppresult::Result<int>::Failure("int error", exception);
    auto failed_discard = failed_int.map([&](int) { ++calls; });
    CHECK(failed_discard.error() == "int error");
    auto moved_discard = std::move(failed_int).map([&](int) { ++calls; });
    CHECK(moved_discard.exception() == exception);
    CHECK(moved_discard.error() == "int error");
    CHECK(calls == 70);
}

int main()
{
    test_assignment();
    test_failure();
    test_access();
    test_values();
    test_void();
}
