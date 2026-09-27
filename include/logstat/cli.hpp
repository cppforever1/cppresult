#pragma once

#include <cppresult/result.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace logstat
{

    struct Options
    {
        std::vector<std::string> inputs;
        std::string format = "text";
        std::size_t threads = 1;
        std::size_t top_n = 10;
        std::size_t min_count = 1;
        double percentile = 95.0;
        std::string title = "logstat report";
        std::string config_path;
        bool verbose = false;
        bool fail_on_errors = false;
        bool show_help = false;
    };

    [[nodiscard]] cppresult::Result<Options> parse_args(const std::vector<std::string> &args);

    /// Applies `key = value` entries from a configuration file on top of defaults; CLI flags win.
    [[nodiscard]] cppresult::Result<Options> apply_config(Options options, const std::string &path);

    [[nodiscard]] std::string usage();

} // namespace logstat
