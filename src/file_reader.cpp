#include <logstat/file_reader.hpp>

#include <cerrno>
#include <cstdio>
#include <cstring>

namespace logstat
{

    namespace
    {
        std::string errno_message(const char *what, const std::string &path)
        {
            return std::string(what) + " '" + path + "': " + std::strerror(errno);
        }

        /// Reads lines from an open stream until `limit` bytes have been consumed
        /// (the line containing byte `limit` is included) or the stream ends.
        std::vector<std::string> read_lines_until(std::FILE *stream, std::size_t start, std::size_t limit)
        {
            std::vector<std::string> lines;
            std::string current;
            std::size_t position = start;
            int ch = 0;
            while ((ch = std::fgetc(stream)) != EOF)
            {
                ++position;
                if (ch == '\n')
                {
                    if (!current.empty() && current.back() == '\r')
                    {
                        current.pop_back();
                    }
                    lines.push_back(current);
                    current.clear();
                    if (position >= limit)
                    {
                        break;
                    }
                }
                else
                {
                    current.push_back(static_cast<char>(ch));
                }
            }
            if (!current.empty())
            {
                lines.push_back(current);
            }
            return lines;
        }
    } // namespace

    std::vector<Chunk> split_chunks(std::size_t size, std::size_t parts)
    {
        std::vector<Chunk> chunks;
        if (parts == 0)
        {
            parts = 1;
        }
        const std::size_t step = size / parts;
        for (std::size_t i = 0; i < parts; ++i)
        {
            Chunk chunk;
            chunk.begin = i * step;
            chunk.end = chunk.begin + step;
            chunks.push_back(chunk);
        }
        return chunks;
    }

    cppresult::Result<std::size_t> file_size(const std::string &path)
    {
        std::FILE *stream = std::fopen(path.c_str(), "rb");
        if (stream == nullptr)
        {
            return cppresult::Result<std::size_t>::Failure(errno_message("cannot open", path));
        }
        if (std::fseek(stream, 0, SEEK_END) != 0)
        {
            return cppresult::Result<std::size_t>::Failure(errno_message("cannot seek", path));
        }
        const long end = std::ftell(stream);
        std::fclose(stream);
        if (end < 0)
        {
            return cppresult::Result<std::size_t>::Failure(errno_message("cannot tell", path));
        }
        return cppresult::Result<std::size_t>::Ok(static_cast<std::size_t>(end));
    }

    cppresult::Result<std::vector<std::string>> read_chunk_lines(const std::string &path, Chunk chunk)
    {
        using Lines = cppresult::Result<std::vector<std::string>>;
        std::FILE *stream = std::fopen(path.c_str(), "rb");
        if (stream == nullptr)
        {
            return Lines::Failure(errno_message("cannot open", path));
        }
        if (chunk.begin > 0)
        {
            if (std::fseek(stream, static_cast<long>(chunk.begin) - 1, SEEK_SET) != 0)
            {
                std::fclose(stream);
                return Lines::Failure(errno_message("cannot seek", path));
            }
            // Skip the partial line that the previous chunk owns.
            int ch = 0;
            while ((ch = std::fgetc(stream)) != EOF && ch != '\n')
            {
            }
        }
        const long resumed = std::ftell(stream);
        const std::size_t start = resumed < 0 ? chunk.begin : static_cast<std::size_t>(resumed);
        std::vector<std::string> lines = read_lines_until(stream, start, chunk.end);
        std::fclose(stream);
        return Lines::Ok(std::move(lines));
    }

    cppresult::Result<std::vector<std::string>> read_all_lines(const std::string &path)
    {
        using Lines = cppresult::Result<std::vector<std::string>>;
        std::FILE *stream = std::fopen(path.c_str(), "rb");
        if (stream == nullptr)
        {
            return Lines::Failure(errno_message("cannot open", path));
        }
        std::vector<std::string> lines = read_lines_until(stream, 0, static_cast<std::size_t>(-1));
        std::fclose(stream);
        return Lines::Ok(std::move(lines));
    }

} // namespace logstat
