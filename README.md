# cppresult

`cppresult` is a small C++20 header-only result type for returning either a
value or a structured failure without throwing exceptions. It is inspired by
Rust's `Result`, but uses a single template parameter: the failure contains a
`std::string`, an `std::optional<std::exception>`, and a `std::source_location` of the failure.

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
    "bad input", std::exception{});

if (!result) {
    const std::string &message = result.error();
    const auto &exception = result.exception();
    auto location = result.source_location();
}
```

Use `value_or(fallback)` to provide a fallback value. Use `map(function)` to
transform successful values; failure details are propagated unchanged.

The type supports move-only values, such as `std::unique_ptr<T>`.

The library target is `cppresult::cppresult`. It can also be installed and consumed with `find_package(cppresult CONFIG REQUIRED)`.

```sh
cmake --install build --prefix build/stage
```

In VS Code, open `tests/result_test.cpp` and run the `C/C++: g++ build active file`
task to build a debugger-friendly standalone test binary.
