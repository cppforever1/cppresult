#include <logstat/config.hpp>
#include <logstat/file_reader.hpp>
#include <logstat/string_utils.hpp>

namespace logstat
{

    cppresult::Result<Config> Config::parse(std::string_view text)
    {
        Config config;
        std::string section;
        std::size_t line_number = 0;
        for (std::string_view raw : str::split(text, '\n'))
        {
            ++line_number;
            const std::string_view line = str::trim(raw);
            if (line.empty() || line.front() == '#' || line.front() == ';')
            {
                continue;
            }
            if (line.front() == '[')
            {
                if (line.back() != ']')
                {
                    return cppresult::Result<Config>::Failure(
                        "line " + std::to_string(line_number) + ": unterminated section header");
                }
                section = std::string(str::trim(line.substr(1, line.size() - 2)));
                continue;
            }
            const std::vector<std::string_view> parts = str::split(line, '=');
            if (parts.size() < 2)
            {
                return cppresult::Result<Config>::Failure(
                    "line " + std::to_string(line_number) + ": expected key = value");
            }
            std::string key = std::string(str::trim(parts[0]));
            if (key.empty())
            {
                return cppresult::Result<Config>::Failure(
                    "line " + std::to_string(line_number) + ": empty key");
            }
            if (!section.empty())
            {
                key = section + "." + key;
            }
            config.values_[key] = std::string(str::trim(parts[1]));
        }
        return cppresult::Result<Config>::Ok(std::move(config));
    }

    cppresult::Result<Config> Config::load(const std::string &path)
    {
        auto lines = read_all_lines(path);
        if (lines.has_error())
        {
            return cppresult::Result<Config>::Failure(lines.error());
        }
        return parse(str::join(lines.value(), "\n"));
    }

    bool Config::has(std::string_view key) const
    {
        return values_.find(std::string(key)) != values_.end();
    }

    std::string Config::get(std::string_view key, std::string_view fallback) const
    {
        const auto it = values_.find(std::string(key));
        if (it == values_.end())
        {
            return std::string(fallback);
        }
        return it->second;
    }

    long long Config::get_int(std::string_view key, long long fallback) const
    {
        const auto it = values_.find(std::string(key));
        if (it == values_.end())
        {
            return fallback;
        }
        return str::parse_int(it->second).value_or(fallback);
    }

    bool Config::get_bool(std::string_view key, bool fallback) const
    {
        const auto it = values_.find(std::string(key));
        if (it == values_.end())
        {
            return fallback;
        }
        const std::string lowered = str::to_lower(it->second);
        if (lowered == "true" || lowered == "yes" || lowered == "on" || lowered == "1")
        {
            return true;
        }
        if (lowered == "false" || lowered == "no" || lowered == "off" || lowered == "0")
        {
            return false;
        }
        return fallback;
    }

    const std::map<std::string, std::string> &Config::entries() const noexcept
    {
        return values_;
    }

} // namespace logstat
