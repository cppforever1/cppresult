#include <cppresult/result.hpp>

#include <cassert>
#include <exception>
#include <memory>
#include <string>
#include <string_view>

int main()
{
    auto success = cppresult::Result<int>::Ok(42);
    assert(success.has_value());
    assert(success);
    assert(success.value() == 42);

    auto failure = cppresult::Result<int>::Failure(
        "bad input", std::exception());
    assert(!failure.has_value());
    assert(!failure);
    assert(failure.error() == "bad input");
    assert(failure.value_or(7) == 7);
    assert(failure.exception().has_value());
    assert(std::string(failure.exception()->what()) == "std::exception");
    assert(failure.source_location().file_name() == std::string_view(__FILE__));

    auto failure_without_exception = cppresult::Result<int>::Failure("no details");
    assert(!failure_without_exception.exception().has_value());
    assert(failure_without_exception.error() == "no details");

    auto mapped = success.map([](int value)
                              { return std::to_string(value); });
    assert(mapped.has_value());
    assert(mapped.value() == "42");

    auto mapped_failure = failure.map([](int value)
                                      { return value * 2; });
    assert(!mapped_failure.has_value());
    assert(mapped_failure.error() == "bad input");
    assert(mapped_failure.exception().has_value());
    assert(mapped_failure.source_location().file_name() == std::string_view(__FILE__));

    auto move_only = cppresult::Result<std::unique_ptr<int>>::Ok(
        std::make_unique<int>(9));
    assert(*move_only.value() == 9);
}
