#include <logstat/report.hpp>
#include <logstat/string_utils.hpp>

#include <cstdio>
#include <sstream>

namespace logstat
{

    namespace
    {
        std::string json_escape(std::string_view text)
        {
            std::string out;
            out.reserve(text.size() + 2);
            for (char ch : text)
            {
                switch (ch)
                {
                case '"':
                    out += "\\\"";
                    break;
                case '\\':
                    out += "\\\\";
                    break;
                case '\n':
                    out += "\\n";
                    break;
                case '\t':
                    out += "\\t";
                    break;
                default:
                    out.push_back(ch);
                }
            }
            return out;
        }

        std::string csv_escape(std::string_view text, const std::string &separator)
        {
            const bool needs_quotes = text.find('"') != std::string_view::npos ||
                                      text.find('\n') != std::string_view::npos ||
                                      text.find(separator) != std::string_view::npos;
            if (!needs_quotes)
            {
                return std::string(text);
            }
            std::string out = "\"";
            for (char ch : text)
            {
                if (ch == '"')
                {
                    out += "\"\"";
                }
                else
                {
                    out.push_back(ch);
                }
            }
            out += "\"";
            return out;
        }
    } // namespace

    // ---------------------------------------------------------------- TextFormatter

    std::string TextFormatter::format(const Aggregator &stats, const ReportOptions &options)
    {
        std::ostringstream out;
        out << "Records: " << stats.total() << "\n";
        for (std::size_t i = 0; i < level_count; ++i)
        {
            const Level level = static_cast<Level>(i);
            const std::size_t count = stats.count(level);
            if (count > 0)
            {
                out << "  " << level_name(level) << ": " << count << "\n";
            }
        }

        const LatencySummary latency = stats.latency();
        if (latency.count > 0)
        {
            out << "\nLatency (ms) over " << latency.count << " samples\n";
            out << "  min  " << str::format_fixed(latency.min, 2) << "\n";
            out << "  mean " << str::format_fixed(latency.mean, 2) << "\n";
            out << "  p50  " << str::format_fixed(latency.p50, 2) << "\n";
            out << "  p95  " << str::format_fixed(latency.p95, 2) << "\n";
            out << "  p99  " << str::format_fixed(latency.p99, 2) << "\n";
            out << "  max  " << str::format_fixed(latency.max, 2) << "\n";
        }

        const auto top = stats.top_messages(options.top_n);
        if (!top.empty())
        {
            out << "\nTop messages\n";
            for (const auto &[message, count] : top)
            {
                out << "  " << count << "  " << message << "\n";
            }
        }

        if (options.include_components && !stats.components().empty())
        {
            out << "\nComponents\n";
            for (const ComponentStats &component : stats.components())
            {
                out << "  " << component.name << "  records=" << component.records
                    << " errors=" << component.errors << " calls_out=" << component.calls_out << "\n";
            }
        }

        if (options.include_recent_errors)
        {
            const auto errors = stats.recent_errors();
            if (!errors.empty())
            {
                out << "\nRecent errors\n";
                for (const std::string &line : errors)
                {
                    out << "  " << line << "\n";
                }
            }
        }
        return out.str();
    }

    const char *TextFormatter::name() const
    {
        return "text";
    }

    // ---------------------------------------------------------------- JsonFormatter

    std::string JsonFormatter::format(const Aggregator &stats, const ReportOptions &options)
    {
        std::ostringstream out;
        out << "{\n";
        out << "  \"title\": \"" << json_escape(options.title) << "\",\n";
        out << "  \"records\": " << stats.total() << ",\n";
        out << "  \"levels\": {";
        bool first = true;
        for (std::size_t i = 0; i < level_count; ++i)
        {
            const Level level = static_cast<Level>(i);
            out << (first ? "" : ", ") << "\"" << level_name(level) << "\": " << stats.count(level);
            first = false;
        }
        out << "},\n";

        const LatencySummary latency = stats.latency();
        out << "  \"latency_ms\": {\"count\": " << latency.count
            << ", \"min\": " << str::format_fixed(latency.min, 3)
            << ", \"mean\": " << str::format_fixed(latency.mean, 3)
            << ", \"p50\": " << str::format_fixed(latency.p50, 3)
            << ", \"p95\": " << str::format_fixed(latency.p95, 3)
            << ", \"p99\": " << str::format_fixed(latency.p99, 3)
            << ", \"max\": " << str::format_fixed(latency.max, 3) << "},\n";

        out << "  \"top_messages\": [";
        first = true;
        for (const auto &[message, count] : stats.top_messages(options.top_n))
        {
            out << (first ? "" : ", ") << "{\"message\": \"" << json_escape(message) << "\", \"count\": " << count << "}";
            first = false;
        }
        out << "]";

        if (options.include_components)
        {
            out << ",\n  \"components\": [";
            first = true;
            for (const ComponentStats &component : stats.components())
            {
                out << (first ? "" : ", ") << "{\"name\": \"" << json_escape(component.name)
                    << "\", \"records\": " << component.records
                    << ", \"errors\": " << component.errors
                    << ", \"calls_out\": " << component.calls_out << "}";
                first = false;
            }
            out << "]";
        }
        out << "\n}\n";
        return out.str();
    }

    const char *JsonFormatter::name() const
    {
        return "json";
    }

    // ----------------------------------------------------------------- CsvFormatter

    CsvFormatter::CsvFormatter(char separator)
        : separator_(1, separator),
          scratch_(new char[64])
    {
    }

    CsvFormatter::~CsvFormatter()
    {
        delete[] scratch_;
    }

    std::string CsvFormatter::format(const Aggregator &stats, const ReportOptions &options)
    {
        std::ostringstream out;
        out << "section" << separator_ << "key" << separator_ << "value\n";
        out << "summary" << separator_ << "records" << separator_ << stats.total() << "\n";
        for (std::size_t i = 0; i < level_count; ++i)
        {
            const Level level = static_cast<Level>(i);
            out << "level" << separator_ << level_name(level) << separator_ << stats.count(level) << "\n";
        }
        const LatencySummary latency = stats.latency();
        std::snprintf(scratch_, 64, "%.3f", latency.p95);
        out << "latency" << separator_ << "p95" << separator_ << scratch_ << "\n";
        std::snprintf(scratch_, 64, "%.3f", latency.p99);
        out << "latency" << separator_ << "p99" << separator_ << scratch_ << "\n";
        for (const auto &[message, count] : stats.top_messages(options.top_n))
        {
            out << "message" << separator_ << csv_escape(message, separator_) << separator_ << count << "\n";
        }
        if (options.include_components)
        {
            for (const ComponentStats &component : stats.components())
            {
                out << "component" << separator_ << csv_escape(component.name, separator_) << separator_
                    << component.records << "\n";
            }
        }
        return out.str();
    }

    const char *CsvFormatter::name() const
    {
        return "csv";
    }

    // ---------------------------------------------------------------------- helpers

    std::unique_ptr<Formatter> make_formatter(std::string_view name)
    {
        if (str::equals_ignore_case(name, "text"))
        {
            return std::make_unique<TextFormatter>();
        }
        if (str::equals_ignore_case(name, "json"))
        {
            return std::make_unique<JsonFormatter>();
        }
        if (str::equals_ignore_case(name, "csv"))
        {
            return std::make_unique<CsvFormatter>(',');
        }
        if (str::equals_ignore_case(name, "tsv"))
        {
            return std::make_unique<CsvFormatter>('\t');
        }
        return nullptr;
    }

    void print_banner(const std::string &title)
    {
        std::printf("== ");
        std::printf(title.c_str());
        std::printf(" ==\n");
    }

    void print_header(const ReportOptions &options)
    {
        char header[24];
        str::copy_fixed(header, options.title);
        std::printf("%s\n", header);
        std::printf("%s\n\n", std::string(std::strlen(header), '=').c_str());
    }

} // namespace logstat
