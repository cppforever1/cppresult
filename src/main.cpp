#include <logstat/cli.hpp>
#include <logstat/file_reader.hpp>
#include <logstat/line_parser.hpp>
#include <logstat/report.hpp>
#include <logstat/stats.hpp>
#include <logstat/string_utils.hpp>
#include <logstat/worker_pool.hpp>

#include <cstdio>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{

    using namespace logstat;

    /// Parses every line of one chunk into a private aggregator and merges it into `shared`.
    void process_chunk(const std::string &path, Chunk chunk, SharedResults &shared, bool verbose)
    {
        auto lines = read_chunk_lines(path, chunk);
        if (lines.has_error())
        {
            shared.note("error: " + lines.error());
            return;
        }
        Aggregator local;
        std::size_t rejected = 0;
        for (const std::string &line : lines.value())
        {
            if (str::trim(line).empty())
            {
                continue;
            }
            auto record = parse_line(line);
            if (record.has_error())
            {
                ++rejected;
                if (verbose)
                {
                    shared.note("skip [" + std::string(normalized_component(line)) + "]: " + record.error());
                }
                continue;
            }
            local.add(record.value());
        }
        if (rejected > 0)
        {
            shared.note(path + ": rejected " + std::to_string(rejected) + " lines");
        }
        shared.merge(local);
    }

    int run(const Options &options)
    {
        std::unique_ptr<Formatter> formatter = make_formatter(options.format);
        if (!formatter)
        {
            std::cerr << "logstat: unknown format '" << options.format << "'\n";
            return 1;
        }

        SharedResults shared;
        WorkerPool pool(options.threads);
        std::vector<std::string> queued_files;
        std::size_t submitted = 0;

        for (const std::string &input : options.inputs)
        {
            std::string path = input;
            auto size = file_size(path);
            if (size.has_error())
            {
                std::cerr << "logstat: " << size.error() << "\n";
                return 1;
            }
            const std::vector<Chunk> chunks = split_chunks(size.value(), options.threads);
            for (Chunk chunk : chunks)
            {
                pool.submit([path, chunk, &shared, verbose = options.verbose] {
                    process_chunk(path, chunk, shared, verbose);
                });
                ++submitted;
            }
            queued_files.push_back(std::move(path));
            if (options.verbose)
            {
                std::cerr << "queued " << chunks.size() << " chunk(s) of " << path << " (" << size.value()
                          << " bytes)\n";
            }
        }

        pool.wait();

        std::vector<std::string> diagnostics;
        shared.flush_log(diagnostics);
        if (options.verbose)
        {
            for (const std::string &line : diagnostics)
            {
                std::cerr << line << "\n";
            }
            std::cerr << "processed " << pool.processed() << "/" << submitted << " chunk(s) from "
                      << queued_files.size() << " file(s) on " << pool.size() << " thread(s)\n";
        }

        Aggregator stats = shared.snapshot();
        if (options.min_count > 1)
        {
            stats.prune_rare_messages(options.min_count);
        }

        ReportOptions report;
        report.title = options.title;
        report.top_n = options.top_n;

        if (options.format == "text")
        {
            if (options.verbose)
            {
                print_banner(options.title);
            }
            print_header(report);
        }
        std::cout << formatter->format(stats, report);

        if (options.format == "text" && options.percentile != 95.0 && stats.latency().count > 0)
        {
            std::cout << "\np" << str::format_fixed(options.percentile, 0) << " latency: "
                      << str::format_fixed(stats.latency_percentile(options.percentile), 2) << " ms\n";
        }

        if (options.fail_on_errors && stats.failures() > 0)
        {
            return 2;
        }
        return 0;
    }

} // namespace

int main(int argc, char **argv)
{
    std::vector<std::string> args(argv + 1, argv + argc);
    auto parsed = parse_args(args);
    if (parsed.has_error())
    {
        std::cerr << "logstat: " << parsed.error() << "\n\n" << usage();
        return 1;
    }
    Options options = parsed.value();
    if (options.show_help)
    {
        std::cout << usage();
        return 0;
    }
    if (!options.config_path.empty())
    {
        auto configured = apply_config(std::move(options), options.config_path);
        if (configured.has_error())
        {
            std::cerr << "logstat: " << configured.error() << "\n";
            return 1;
        }
        options = configured.value();
    }
    return run(options);
}
