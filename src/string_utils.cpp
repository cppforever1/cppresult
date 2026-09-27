#include <logstat/string_utils.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <system_error>

namespace logstat::str
{

    namespace
    {
        bool is_space(char c)
        {
            return std::isspace(static_cast<unsigned char>(c)) != 0;
        }
    } // namespace

    std::string_view trim(std::string_view text)
    {
        while (!text.empty() && is_space(text.front()))
        {
            text.remove_prefix(1);
        }
        while (!text.empty() && is_space(text.back()))
        {
            text.remove_suffix(1);
        }
        return text;
    }

    std::vector<std::string_view> split(std::string_view text, char delimiter)
    {
        std::vector<std::string_view> parts;
        std::size_t start = 0;
        while (start <= text.size())
        {
            const std::size_t end = text.find(delimiter, start);
            if (end == std::string_view::npos)
            {
                parts.push_back(text.substr(start));
                break;
            }
            parts.push_back(text.substr(start, end - start));
            start = end + 1;
        }
        return parts;
    }

    bool starts_with(std::string_view text, std::string_view prefix)
    {
        return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
    }

    bool equals_ignore_case(std::string_view left, std::string_view right)
    {
        if (left.size() != right.size())
        {
            return false;
        }
        for (std::size_t i = 0; i < left.size(); ++i)
        {
            const auto l = std::tolower(static_cast<unsigned char>(left[i]));
            const auto r = std::tolower(static_cast<unsigned char>(right[i]));
            if (l != r)
            {
                return false;
            }
        }
        return true;
    }

    std::string to_lower(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    }

    cppresult::Result<long long> parse_int(std::string_view text)
    {
        text = trim(text);
        if (text.empty())
        {
            return cppresult::Result<long long>::Failure("empty integer");
        }
        long long value = 0;
        const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (ec != std::errc{} || ptr != text.data() + text.size())
        {
            return cppresult::Result<long long>::Failure("invalid integer: " + std::string(text));
        }
        return cppresult::Result<long long>::Ok(value);
    }

    cppresult::Result<double> parse_double(std::string_view text)
    {
        text = trim(text);
        if (text.empty())
        {
            return cppresult::Result<double>::Failure("empty number");
        }
        // std::from_chars for floating point is not available on every toolchain we target.
        const std::string owned(text);
        char *end = nullptr;
        const double value = std::strtod(owned.c_str(), &end);
        if (end != owned.c_str() + owned.size() || std::isnan(value))
        {
            return cppresult::Result<double>::Failure("invalid number: " + owned);
        }
        return cppresult::Result<double>::Ok(value);
    }

    std::string join(const std::vector<std::string> &parts, std::string_view separator)
    {
        std::string out;
        for (std::size_t i = 0; i < parts.size(); ++i)
        {
            if (i != 0)
            {
                out.append(separator);
            }
            out.append(parts[i]);
        }
        return out;
    }

    std::string format_fixed(double value, int decimals)
    {
        char buffer[64];
        const int written = std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
        if (written < 0)
        {
            return "nan";
        }
        return std::string(buffer, static_cast<std::size_t>(std::min<int>(written, sizeof(buffer) - 1)));
    }

} // namespace logstat::str
