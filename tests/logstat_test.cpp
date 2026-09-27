#include <logstat/cli.hpp>
#include <logstat/config.hpp>
#include <logstat/file_reader.hpp>
#include <logstat/line_parser.hpp>
#include <logstat/report.hpp>
#include <logstat/ring_buffer.hpp>
#include <logstat/stats.hpp>
#include <logstat/string_utils.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{

    int failures = 0;

    void check(bool condition, const char *expression, const char *file, int line)
    {
        if (!condition)
        {
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", file, line, expression);
            ++failures;
        }
    }

#define CHECK(expr) check((expr), #expr, __FILE__, __LINE__)

    bool near(double left, double right)
    {
        return std::fabs(left - right) < 1e-9;
    }

    using namespace logstat;

    void test_string_utils()
    {
        CHECK(str::trim("  abc \t") == "abc");
        CHECK(str::trim("") == "");
        CHECK(str::split("a,b,,c", ',').size() == 4);
        CHECK(str::starts_with("latency_ms=12", "latency_ms"));
        CHECK(str::equals_ignore_case("Info", "INFO"));
        CHECK(!str::equals_ignore_case("Info", "Infos"));
        CHECK(str::to_lower("MiXeD") == "mixed");
        CHECK(str::parse_int("42").value() == 42);
        CHECK(str::parse_int("-7").value() == -7);
        CHECK(str::parse_int("4x").has_error());
        CHECK(str::parse_int("").has_error());
        CHECK(near(str::parse_double("12.5").value(), 12.5));
        CHECK(str::parse_double("abc").has_error());
        CHECK(str::join({"a", "b", "c"}, ", ") == "a, b, c");
        CHECK(str::format_fixed(3.14159, 2) == "3.14");
    }

    void test_line_parser()
    {
        CHECK(parse_level("INFO") == Level::Info);
        CHECK(parse_level("warning") == Level::Warn);
        CHECK(parse_level("ERR") == Level::Error);
        CHECK(parse_level("nope") == Level::Unknown);
        CHECK(std::string(level_name(Level::Fatal)) == "FATAL");

        auto record = parse_line("2026-09-27T10:00:00Z INFO http: request served latency_ms=12.5 caller=gateway");
        CHECK(record.has_value());
        if (record.has_value())
        {
            const LogRecord &r = record.value();
            CHECK(r.timestamp == "2026-09-27T10:00:00Z");
            CHECK(r.level == Level::Info);
            CHECK(r.component == "http");
            CHECK(r.message == "request served");
            CHECK(r.latency_ms.has_value() && near(*r.latency_ms, 12.5));
            CHECK(r.fields.at("caller") == "gateway");
        }

        auto bare = parse_line("2026-09-27T10:00:01Z WARN db: slow query");
        CHECK(bare.has_value());
        CHECK(bare.has_value() && !bare.value().latency_ms.has_value());

        CHECK(parse_line("garbage").has_error());
        CHECK(parse_line("").has_error());
        CHECK(parse_line("2026-09-27T10:00:01Z INFO nocolon message").has_error());
    }

    void test_ring_buffer()
    {
        RingBuffer<int, 3> ring;
        CHECK(ring.empty());
        ring.push(1);
        ring.push(2);
        ring.push(3);
        ring.push(4);
        CHECK(ring.size() == 3);
        CHECK(ring[0] == 2);
        CHECK(ring[2] == 4);
        const std::vector<int> last_two = ring.recent(2);
        CHECK(last_two.size() == 2 && last_two[0] == 3 && last_two[1] == 4);
        ring.clear();
        CHECK(ring.empty());
    }

    void test_percentile()
    {
        const std::vector<double> sorted = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0};
        CHECK(near(percentile(sorted, 0.0), 1.0));
        CHECK(near(percentile(sorted, 50.0), 6.0));
        CHECK(near(percentile(sorted, 90.0), 10.0));
        CHECK(near(percentile({}, 50.0), 0.0));
    }

    void test_aggregator()
    {
        Aggregator agg;
        const char *lines[] = {
            "2026-09-27T09:59:59Z INFO gateway: forwarded",
            "2026-09-27T10:00:00Z INFO http: request served latency_ms=10 caller=gateway",
            "2026-09-27T10:00:01Z INFO http: request served latency_ms=20 caller=gateway",
            "2026-09-27T10:00:02Z ERROR db: connection reset latency_ms=300",
            "2026-09-27T10:00:03Z WARN cache: miss",
            "2026-09-27T10:00:04Z INFO http: request served latency_ms=30",
        };
        for (const char *line : lines)
        {
            auto record = parse_line(line);
            CHECK(record.has_value());
            if (record.has_value())
            {
                agg.add(record.value());
            }
        }
        CHECK(agg.total() == 6);
        CHECK(agg.count(Level::Info) == 4);
        CHECK(agg.count(Level::Error) == 1);
        CHECK(agg.failures() == 1);

        const LatencySummary latency = agg.latency();
        CHECK(latency.count == 4);
        CHECK(near(latency.min, 10.0));
        CHECK(near(latency.max, 300.0));
        CHECK(near(latency.mean, 90.0));
        CHECK(near(latency.p50, 30.0));

        const auto top = agg.top_messages(1);
        CHECK(top.size() == 1 && top[0].first == "request served" && top[0].second == 3);

        bool saw_http = false;
        for (const ComponentStats &component : agg.components())
        {
            if (component.name == "http")
            {
                saw_http = true;
                CHECK(component.records == 3);
                CHECK(component.errors == 0);
            }
            if (component.name == "gateway")
            {
                CHECK(component.calls_out == 2);
            }
        }
        CHECK(saw_http);
        CHECK(agg.recent_errors().size() == 1);

        Aggregator other;
        auto extra = parse_line("2026-09-27T10:00:05Z FATAL db: down latency_ms=5");
        CHECK(extra.has_value());
        if (extra.has_value())
        {
            other.add(extra.value());
        }
        agg.merge(other);
        CHECK(agg.total() == 7);
        CHECK(agg.failures() == 2);
        CHECK(near(agg.latency().min, 5.0));
    }

    void test_split_chunks()
    {
        const std::vector<Chunk> chunks = split_chunks(100, 4);
        CHECK(chunks.size() == 4);
        CHECK(chunks[0].begin == 0 && chunks[0].end == 25);
        CHECK(chunks[3].begin == 75 && chunks[3].end == 100);
        CHECK(split_chunks(10, 0).size() == 1);
    }

    void test_config()
    {
        auto parsed = Config::parse("# comment\nformat = json\n[report]\ntop = 5\nverbose=yes\n");
        CHECK(parsed.has_value());
        if (parsed.has_value())
        {
            const Config &cfg = parsed.value();
            CHECK(cfg.get("format") == "json");
            CHECK(cfg.get_int("report.top", 0) == 5);
            CHECK(cfg.get_bool("report.verbose", false));
            CHECK(!cfg.has("missing"));
            CHECK(cfg.get("missing", "dflt") == "dflt");
        }
        CHECK(Config::parse("[broken\n").has_error());
        CHECK(Config::parse("novalue\n").has_error());
    }

    void test_cli()
    {
        auto parsed = parse_args({"-f", "json", "-j", "4", "--top", "3", "a.log", "b.log"});
        CHECK(parsed.has_value());
        if (parsed.has_value())
        {
            const Options &opts = parsed.value();
            CHECK(opts.format == "json");
            CHECK(opts.threads == 4);
            CHECK(opts.top_n == 3);
            CHECK(opts.inputs.size() == 2);
        }
        CHECK(parse_args({}).has_error());
        CHECK(parse_args({"-j"}).has_error());
        CHECK(parse_args({"-j", "-1", "a.log"}).has_error());
        CHECK(parse_args({"--bogus", "a.log"}).has_error());
        auto help = parse_args({"--help"});
        CHECK(help.has_value() && help.value().show_help);
    }

    void test_formatters()
    {
        Aggregator agg;
        auto record = parse_line("2026-09-27T10:00:00Z INFO http: hello latency_ms=1");
        if (record.has_value())
        {
            agg.add(record.value());
        }
        ReportOptions options;
        options.title = "t";
        CHECK(make_formatter("nope") == nullptr);
        for (const char *name : {"text", "json", "csv", "tsv"})
        {
            auto formatter = make_formatter(name);
            CHECK(formatter != nullptr);
            if (formatter)
            {
                const std::string out = formatter->format(agg, options);
                CHECK(!out.empty());
                CHECK(out.find("hello") != std::string::npos);
            }
        }
    }

} // namespace

int main()
{
    test_string_utils();
    test_line_parser();
    test_ring_buffer();
    test_percentile();
    test_aggregator();
    test_split_chunks();
    test_config();
    test_cli();
    test_formatters();
    if (failures != 0)
    {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    std::puts("logstat tests passed");
    return EXIT_SUCCESS;
}
