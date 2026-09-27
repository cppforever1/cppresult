#pragma once

#include <cppresult/result.hpp>

#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace logstat::str
{

    [[nodiscard]] std::string_view trim(std::string_view text);

    [[nodiscard]] std::vector<std::string_view> split(std::string_view text, char delimiter);

    [[nodiscard]] bool starts_with(std::string_view text, std::string_view prefix);

    [[nodiscard]] bool equals_ignore_case(std::string_view left, std::string_view right);

    [[nodiscard]] std::string to_lower(std::string text);

    [[nodiscard]] cppresult::Result<long long> parse_int(std::string_view text);

    [[nodiscard]] cppresult::Result<double> parse_double(std::string_view text);

    [[nodiscard]] std::string join(const std::vector<std::string> &parts, std::string_view separator);

    /// Copies `source` into a fixed-size character array, truncating when it does not fit.
    template <std::size_t N>
    void copy_fixed(char (&destination)[N], std::string_view source)
    {
        std::strncpy(destination, source.data(), N);
    }

    /// Formats a double with a fixed number of decimals without locale influence.
    [[nodiscard]] std::string format_fixed(double value, int decimals);

} // namespace logstat::str
