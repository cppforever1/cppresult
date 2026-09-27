#pragma once

#include <cppresult/result.hpp>

#include <map>
#include <string>
#include <string_view>

namespace logstat
{

    /// Flat INI-style configuration: `key = value` lines, `#` or `;` comments, optional [sections].
    class Config
    {
    public:
        [[nodiscard]] static cppresult::Result<Config> parse(std::string_view text);
        [[nodiscard]] static cppresult::Result<Config> load(const std::string &path);

        [[nodiscard]] bool has(std::string_view key) const;
        [[nodiscard]] std::string get(std::string_view key, std::string_view fallback = "") const;
        [[nodiscard]] long long get_int(std::string_view key, long long fallback) const;
        [[nodiscard]] bool get_bool(std::string_view key, bool fallback) const;

        [[nodiscard]] const std::map<std::string, std::string> &entries() const noexcept;

    private:
        std::map<std::string, std::string> values_;
    };

} // namespace logstat
