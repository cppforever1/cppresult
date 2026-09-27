#pragma once

#include <logstat/stats.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

namespace logstat
{

    struct ReportOptions
    {
        std::string title = "logstat report";
        std::size_t top_n = 10;
        bool include_components = true;
        bool include_recent_errors = true;
    };

    /// Output strategy for a finished aggregation.
    class Formatter
    {
    public:
        [[nodiscard]] virtual std::string format(const Aggregator &stats, const ReportOptions &options) = 0;
        [[nodiscard]] virtual const char *name() const = 0;
    };

    class TextFormatter : public Formatter
    {
    public:
        [[nodiscard]] std::string format(const Aggregator &stats, const ReportOptions &options) override;
        [[nodiscard]] const char *name() const override;
    };

    class JsonFormatter : public Formatter
    {
    public:
        [[nodiscard]] std::string format(const Aggregator &stats, const ReportOptions &options) override;
        [[nodiscard]] const char *name() const override;
    };

    class CsvFormatter : public Formatter
    {
    public:
        explicit CsvFormatter(char separator);
        ~CsvFormatter();
        CsvFormatter(const CsvFormatter &) = delete;
        CsvFormatter &operator=(const CsvFormatter &) = delete;
        [[nodiscard]] std::string format(const Aggregator &stats, const ReportOptions &options) override;
        [[nodiscard]] const char *name() const override;

    private:
        std::string separator_;
        char *scratch_;
    };

    /// Returns nullptr when `name` is not a known formatter.
    [[nodiscard]] std::unique_ptr<Formatter> make_formatter(std::string_view name);

    /// Prints a decorated title line to stdout.
    void print_banner(const std::string &title);

    /// Prints a fixed-width header derived from the report title.
    void print_header(const ReportOptions &options);

} // namespace logstat
