# cppresult

`cppresult` is a small C++20 header-only result type for returning either a
value or a structured failure instead of throwing to report ordinary errors. It is inspired by
Rust's `Result`, but uses a single template parameter: the failure contains a
`std::string`, a nullable `std::exception_ptr`, and a `std::source_location` of the failure.

This is not a blanket no-throw guarantee: allocation, value construction/copy/move,
and callbacks can throw. Accessing the wrong alternative with `value()`, `error()`,
`exception()`, or `source_location()` throws `std::bad_variant_access` in both
debug and release builds. `Result` is `[[nodiscard]]` to encourage callers to
check or explicitly discard returned results.

Storage uses `std::variant`, not the never-valueless storage of C++23
`std::expected`. If a throwing assignment leaves it valueless, both `has_value()`
and `has_error()` return false, and alternative access throws. After an assignment
exception, reassign a valid result before using it as a success or failure.

## Requirements

- C++20 compiler
- CMake 3.20 or newer

## Build and test

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

For a debug build:

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
ctest --test-dir build-debug --output-on-failure
```

## Usage

```cpp
#include <cppresult/result.hpp>
#include <stdexcept>
#include <string>

cppresult::Result<int> parse_count(const std::string &input)
{
    if (input.empty()) {
        return cppresult::Result<int>::Failure("empty input");
    }
    return cppresult::Result<int>::Ok(42);
}
```

```cpp
auto result = cppresult::Result<int>::Failure(
    "bad input", std::make_exception_ptr(std::runtime_error("invalid count")));

if (!result) {
    const std::string &message = result.error();
    const auto &exception = result.exception();
    auto location = result.source_location();
    if (exception) {
        // Rethrows the original dynamic type and preserves its what() message.
        // Handle it here or let the caller handle it.
        std::rethrow_exception(exception);
    }
}
```

In a `catch` block, use `std::current_exception()` to capture the active exception.
Omit the second argument (or pass `{}`) when there is no exception. This replaces
the earlier `std::optional<std::exception>` API, which sliced derived exceptions:
replace `Failure(message, exception_object)` with
`Failure(message, std::make_exception_ptr(exception_object))`, and
`exception().has_value()` with a boolean check of `exception()`. Pass the concrete
exception type to `make_exception_ptr`; use `current_exception()` when catching by
base-class reference. Replace explicit `std::nullopt` arguments with `{}`.

`Failure()` captures its caller's location. Wrappers should accept a defaulted
`std::source_location::current()` argument and forward it as the third argument.

Use `value_or(fallback)` to provide a fallback value. Use `map(function)` to
transform successful values; failures bypass the callback and retain their
original message, exception, and source location. Callbacks may return `void`.

The type supports move-only values, such as `std::unique_ptr<T>`. Lvalue `map`
passes `const T &` and lvalue `value_or` copies the successful value. Use
`std::move(result).map(function)` to pass `T &&`, or
`std::move(result).value_or(fallback)` to move the successful value. Rvalue `map`
also moves failure details; do not rely on the source message or exception after
moving them. These operations do not automatically turn thrown exceptions into
`Failure` results.

Use `Result<void>` for operations with no success value:

```cpp
auto done = cppresult::Result<void>::Ok();
done.value(); // Checks success without returning a value.
auto count = done.map([] { return 42; }); // Result<int>
auto failed = cppresult::Result<void>::Failure("operation failed");
```

`Result<void>::map` invokes its callback without arguments, only on success.

The library target is `cppresult::cppresult`. It can also be installed and consumed with `find_package(cppresult CONFIG REQUIRED)`.

```sh
cmake --install build --prefix build/stage
```

In VS Code, open `tests/result_test.cpp` and run the `C/C++: g++ build active file`
task to build a debugger-friendly standalone test binary.

## Example application: logstat

`logstat` is a small multi-threaded log analysis CLI built on `cppresult`. It
parses structured log lines, aggregates level counts, latency percentiles, top
messages and per-component statistics, and prints a text, JSON, CSV or TSV report.
It exists to exercise the result type in a realistic program with file IO,
threading, configuration parsing and output formatting.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/logstat --format json --threads 4 --top 5 examples/sample.log
./build/logstat --title "Nightly summary" --percentile 99 --fail-on-errors examples/sample.log
```

Input lines have the form
`2026-09-27T10:00:00Z INFO http: request served latency_ms=12.5 caller=gateway`.
Run `logstat --help` for all options; `--config file.ini` reads `key = value`
defaults. Disable the CLI with `-DCPPRESULT_BUILD_LOGSTAT=OFF`. Its tests live in
`tests/logstat_test.cpp` and run through `ctest` with the library tests.
