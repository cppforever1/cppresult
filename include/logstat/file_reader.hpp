#pragma once

#include <cppresult/result.hpp>

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace logstat
{

    /// Byte range [begin, end) of a file.
    struct Chunk
    {
        std::size_t begin = 0;
        std::size_t end = 0;
    };

    /// Splits `size` bytes into `parts` contiguous chunks of equal length.
    [[nodiscard]] std::vector<Chunk> split_chunks(std::size_t size, std::size_t parts);

    /// Size of the file in bytes, or an error message.
    [[nodiscard]] cppresult::Result<std::size_t> file_size(const std::string &path);

    /// Reads complete lines that start inside `chunk`; the last line may extend past `chunk.end`.
    [[nodiscard]] cppresult::Result<std::vector<std::string>> read_chunk_lines(const std::string &path, Chunk chunk);

    /// Reads every line of the file.
    [[nodiscard]] cppresult::Result<std::vector<std::string>> read_all_lines(const std::string &path);

} // namespace logstat
