#include <logstat/cli.hpp>
#include <logstat/config.hpp>
#include <logstat/string_utils.hpp>

namespace logstat
{

    namespace
    {
        using OptionsResult = cppresult::Result<Options>;

        OptionsResult fail(const std::string &message)
        {
            return OptionsResult::Failure(message);
        }

        cppresult::Result<std::size_t> parse_size(std::string_view flag, std::string_view text)
        {
            auto parsed = str::parse_int(text);
            if (parsed.has_error())
            {
                return cppresult::Result<std::size_t>::Failure(std::string(flag) + ": " + parsed.error());
            }
            if (parsed.value() < 0)
            {
                return cppresult::Result<std::size_t>::Failure(std::string(flag) + ": must not be negative");
            }
            return cppresult::Result<std::size_t>::Ok(static_cast<std::size_t>(parsed.value()));
        }
    } // namespace

    cppresult::Result<Options> parse_args(const std::vector<std::string> &args)
    {
        Options options;
        for (std::size_t i = 0; i < args.size(); ++i)
        {
            const std::string &arg = args[i];
            auto next_value = [&](std::string_view flag) -> cppresult::Result<std::string> {
                if (i + 1 >= args.size())
                {
                    return cppresult::Result<std::string>::Failure(std::string(flag) + " requires a value");
                }
                return cppresult::Result<std::string>::Ok(args[++i]);
            };

            if (arg == "-h" || arg == "--help")
            {
                options.show_help = true;
                return OptionsResult::Ok(std::move(options));
            }
            if (arg == "-v" || arg == "--verbose")
            {
                options.verbose = true;
                continue;
            }
            if (arg == "--fail-on-errors")
            {
                options.fail_on_errors = true;
                continue;
            }
            if (arg == "-f" || arg == "--format")
            {
                auto value = next_value(arg);
                if (value.has_error())
                {
                    return fail(value.error());
                }
                options.format = value.value();
                continue;
            }
            if (arg == "-j" || arg == "--threads")
            {
                auto value = next_value(arg);
                if (value.has_error())
                {
                    return fail(value.error());
                }
                auto count = parse_size(arg, value.value());
                if (count.has_error())
                {
                    return fail(count.error());
                }
                options.threads = count.value();
                continue;
            }
            if (arg == "-n" || arg == "--top")
            {
                auto value = next_value(arg);
                if (value.has_error())
                {
                    return fail(value.error());
                }
                auto count = parse_size(arg, value.value());
                if (count.has_error())
                {
                    return fail(count.error());
                }
                options.top_n = count.value();
                continue;
            }
            if (arg == "--min-count")
            {
                auto value = next_value(arg);
                if (value.has_error())
                {
                    return fail(value.error());
                }
                auto count = parse_size(arg, value.value());
                if (count.has_error())
                {
                    return fail(count.error());
                }
                options.min_count = count.value();
                continue;
            }
            if (arg == "-p" || arg == "--percentile")
            {
                auto value = next_value(arg);
                if (value.has_error())
                {
                    return fail(value.error());
                }
                auto parsed = str::parse_double(value.value());
                if (parsed.has_error())
                {
                    return fail(arg + ": " + parsed.error());
                }
                options.percentile = parsed.value();
                continue;
            }
            if (arg == "-t" || arg == "--title")
            {
                auto value = next_value(arg);
                if (value.has_error())
                {
                    return fail(value.error());
                }
                options.title = value.value();
                continue;
            }
            if (arg == "-c" || arg == "--config")
            {
                auto value = next_value(arg);
                if (value.has_error())
                {
                    return fail(value.error());
                }
                options.config_path = value.value();
                continue;
            }
            if (str::starts_with(arg, "-") && arg.size() > 1)
            {
                return fail("unknown option: " + arg);
            }
            options.inputs.push_back(arg);
        }

        if (options.inputs.empty())
        {
            return fail("at least one input file is required");
        }
        return OptionsResult::Ok(std::move(options));
    }

    cppresult::Result<Options> apply_config(Options options, const std::string &path)
    {
        auto config = Config::load(path);
        if (config.has_error())
        {
            return fail("config: " + config.error());
        }
        const Config &cfg = config.value();
        if (options.format == "text")
        {
            options.format = cfg.get("format", options.format);
        }
        if (options.threads == 1)
        {
            options.threads = static_cast<std::size_t>(cfg.get_int("threads", 1));
        }
        if (options.top_n == 10)
        {
            options.top_n = static_cast<std::size_t>(cfg.get_int("top", 10));
        }
        if (options.title == "logstat report")
        {
            options.title = cfg.get("title", options.title);
        }
        options.verbose = options.verbose || cfg.get_bool("verbose", false);
        options.fail_on_errors = options.fail_on_errors || cfg.get_bool("fail_on_errors", false);
        return OptionsResult::Ok(std::move(options));
    }

    std::string usage()
    {
        return "usage: logstat [options] <log file>...\n"
               "\n"
               "  -f, --format <text|json|csv|tsv>  output format (default text)\n"
               "  -j, --threads <n>                 worker threads (default 1)\n"
               "  -n, --top <n>                     number of top messages (default 10)\n"
               "      --min-count <n>               drop messages seen fewer times\n"
               "  -p, --percentile <p>              extra latency percentile to print\n"
               "  -t, --title <text>                report title\n"
               "  -c, --config <file>               INI configuration file\n"
               "  -v, --verbose                     progress output on stderr\n"
               "      --fail-on-errors              exit 2 when ERROR/FATAL records exist\n"
               "  -h, --help                        this text\n";
    }

} // namespace logstat
