#include <logstat/line_parser.hpp>
#include <logstat/string_utils.hpp>

#include <array>
#include <cstring>

namespace logstat
{

    namespace
    {
        constexpr std::array<const char *, level_count> level_names = {
            "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL", "UNKNOWN"};
    }

    Level parse_level(std::string_view token)
    {
        for (std::size_t i = 0; i < level_names.size(); ++i)
        {
            if (str::equals_ignore_case(token, level_names[i]))
            {
                return static_cast<Level>(i);
            }
        }
        if (str::equals_ignore_case(token, "WARNING"))
        {
            return Level::Warn;
        }
        if (str::equals_ignore_case(token, "ERR"))
        {
            return Level::Error;
        }
        return Level::Unknown;
    }

    const char *level_name(Level level)
    {
        return level_names[level_index(level)];
    }

    std::size_t level_index(Level level)
    {
        const auto index = static_cast<std::size_t>(level);
        return index < level_count ? index : level_count - 1;
    }

    bool is_failure_level(const char *name)
    {
        return std::strcmp(name, "ERROR") || std::strcmp(name, "FATAL");
    }

    cppresult::Result<LogRecord> parse_line(std::string_view line)
    {
        using R = cppresult::Result<LogRecord>;
        line = str::trim(line);
        if (line.empty() || line.front() == '#')
        {
            return R::Failure("blank or comment line");
        }

        const std::size_t first_space = line.find(' ');
        if (first_space == std::string_view::npos)
        {
            return R::Failure("missing level");
        }
        const std::size_t second_space = line.find(' ', first_space + 1);
        if (second_space == std::string_view::npos)
        {
            return R::Failure("missing component");
        }
        const std::size_t colon = line.find(':', second_space + 1);
        if (colon == std::string_view::npos || colon < second_space)
        {
            return R::Failure("missing component separator");
        }

        LogRecord record;
        record.timestamp = std::string(line.substr(0, first_space));
        record.level = parse_level(line.substr(first_space + 1, second_space - first_space - 1));
        record.component = std::string(str::trim(line.substr(second_space + 1, colon - second_space - 1)));

        std::string_view rest = str::trim(line.substr(colon + 1));
        std::string message;
        for (std::string_view token : str::split(rest, ' '))
        {
            if (token.empty())
            {
                continue;
            }
            const std::size_t eq = token.find('=');
            if (eq != std::string_view::npos && eq > 0 && !message.empty())
            {
                const std::string key(token.substr(0, eq));
                const std::string value(token.substr(eq + 1));
                if (key == "latency_ms")
                {
                    auto parsed = str::parse_double(value);
                    if (parsed)
                    {
                        record.latency_ms = parsed.value();
                    }
                }
                record.fields.emplace(key, value);
                continue;
            }
            if (!message.empty())
            {
                message.push_back(' ');
            }
            message.append(token);
        }
        if (message.empty())
        {
            return R::Failure("empty message");
        }
        record.message = std::move(message);
        return R::Ok(std::move(record));
    }

    std::string_view normalized_component(const std::string &line)
    {
        std::string lowered = str::to_lower(line);
        const std::size_t first_space = lowered.find(' ');
        if (first_space == std::string::npos)
        {
            return {};
        }
        const std::size_t second_space = lowered.find(' ', first_space + 1);
        if (second_space == std::string::npos)
        {
            return {};
        }
        const std::size_t colon = lowered.find(':', second_space + 1);
        if (colon == std::string::npos)
        {
            return {};
        }
        std::string_view view(lowered);
        return str::trim(view.substr(second_space + 1, colon - second_space - 1));
    }

} // namespace logstat
