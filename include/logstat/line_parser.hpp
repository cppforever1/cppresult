#pragma once

#include <cppresult/result.hpp>

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace logstat
{

    enum class Level
    {
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Fatal,
        Unknown
    };

    inline constexpr std::size_t level_count = 7;

    [[nodiscard]] Level parse_level(std::string_view token);
    [[nodiscard]] const char *level_name(Level level);
    [[nodiscard]] std::size_t level_index(Level level);

    /// Returns true when a textual level name denotes a failure (ERROR or FATAL).
    [[nodiscard]] bool is_failure_level(const char *name);

    struct LogRecord
    {
        std::string timestamp;
        Level level = Level::Unknown;
        std::string component;
        std::string message;
        std::optional<double> latency_ms;
        std::map<std::string, std::string> fields;
    };

    /// Parses one line of the form
    ///   2026-09-27T10:00:00Z INFO http: request served latency_ms=12.5 caller=gateway
    [[nodiscard]] cppresult::Result<LogRecord> parse_line(std::string_view line);

    /// Returns the lower-cased component name of a raw line without allocating a record.
    [[nodiscard]] std::string_view normalized_component(const std::string &line);

} // namespace logstat
