/**
 * @file TarFixture.hpp
 * @brief 测试用 tar 文件生成器。
 */

#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace contest_test {
namespace detail {

inline void writeField(std::array<char, 512>& header, std::size_t offset, std::size_t width,
                       const std::string& value) {
    for (std::size_t index = 0; index < width && index < value.size(); ++index) {
        header.at(offset + index) = value.at(index);
    }
}

inline void writeOctal(std::array<char, 512>& header, std::size_t offset, std::size_t width,
                       std::uint64_t value) {
    std::array<char, 32> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%0*llo", static_cast<int>(width - 1U),
                  static_cast<unsigned long long>(value));
    writeField(header, offset, width - 1U, buffer.data());
    header.at(offset + width - 1U) = ' ';
}

inline std::array<char, 512> tarHeader(const std::string& name, const std::string& content) {
    std::array<char, 512> header{};
    writeField(header, 0U, 100U, name);
    writeOctal(header, 100U, 8U, 0644U);
    writeOctal(header, 108U, 8U, 0U);
    writeOctal(header, 116U, 8U, 0U);
    writeOctal(header, 124U, 12U, content.size());
    writeOctal(header, 136U, 12U, 0U);
    for (std::size_t index = 148U; index < 156U; ++index) {
        header.at(index) = ' ';
    }
    header.at(156U) = '0';
    writeField(header, 257U, 6U, "ustar");
    writeField(header, 263U, 2U, "00");

    unsigned int checksum = 0U;
    for (const auto byte : header) {
        checksum += static_cast<unsigned char>(byte);
    }
    std::array<char, 16> checksumText{};
    std::snprintf(checksumText.data(), checksumText.size(), "%06o", checksum);
    writeField(header, 148U, 6U, checksumText.data());
    header.at(154U) = '\0';
    header.at(155U) = ' ';
    return header;
}

} // namespace detail

inline std::filesystem::path
writeTarFixture(const std::filesystem::path& path,
                const std::vector<std::pair<std::string, std::string>>& entries) {
    std::ofstream output(path, std::ios::binary);
    for (const auto& [name, content] : entries) {
        const auto header = detail::tarHeader(name, content);
        output.write(header.data(), static_cast<std::streamsize>(header.size()));
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
        const auto padding = (512U - (content.size() % 512U)) % 512U;
        std::array<char, 512> zeroBlock{};
        output.write(zeroBlock.data(), static_cast<std::streamsize>(padding));
    }
    std::array<char, 512> zeroBlock{};
    output.write(zeroBlock.data(), static_cast<std::streamsize>(zeroBlock.size()));
    output.write(zeroBlock.data(), static_cast<std::streamsize>(zeroBlock.size()));
    return path;
}

} // namespace contest_test
