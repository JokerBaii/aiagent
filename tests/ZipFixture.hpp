/**
 * @file ZipFixture.hpp
 * @brief 测试用 zip 文件生成器。
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <zlib.h>

namespace contest_test {

inline void appendU16(std::vector<unsigned char>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<unsigned char>(value & 0xffU));
    bytes.push_back(static_cast<unsigned char>((value >> 8U) & 0xffU));
}

inline void appendU32(std::vector<unsigned char>& bytes, std::uint32_t value) {
    appendU16(bytes, static_cast<std::uint16_t>(value & 0xffffU));
    appendU16(bytes, static_cast<std::uint16_t>((value >> 16U) & 0xffffU));
}

inline void appendText(std::vector<unsigned char>& bytes, const std::string& text) {
    bytes.insert(bytes.end(), text.begin(), text.end());
}

inline std::filesystem::path
writeStoredZipFixture(const std::filesystem::path& path,
                      const std::vector<std::pair<std::string, std::string>>& entries) {
    std::vector<unsigned char> bytes;
    struct CentralEntry {
        std::string name;
        std::string content;
        std::uint32_t localOffset{0};
        std::uint32_t crc{0};
    };
    std::vector<CentralEntry> centralEntries;
    centralEntries.reserve(entries.size());

    for (const auto& [name, content] : entries) {
        const auto localOffset = static_cast<std::uint32_t>(bytes.size());
        const auto crc = static_cast<std::uint32_t>(crc32(
            0L, reinterpret_cast<const Bytef*>(content.data()), static_cast<uInt>(content.size())));
        appendU32(bytes, 0x04034b50U);
        appendU16(bytes, 20U);
        appendU16(bytes, 0U);
        appendU16(bytes, 0U);
        appendU16(bytes, 0U);
        appendU16(bytes, 0U);
        appendU32(bytes, crc);
        appendU32(bytes, static_cast<std::uint32_t>(content.size()));
        appendU32(bytes, static_cast<std::uint32_t>(content.size()));
        appendU16(bytes, static_cast<std::uint16_t>(name.size()));
        appendU16(bytes, 0U);
        appendText(bytes, name);
        appendText(bytes, content);
        centralEntries.push_back({name, content, localOffset, crc});
    }

    const auto centralOffset = static_cast<std::uint32_t>(bytes.size());
    for (const auto& entry : centralEntries) {
        appendU32(bytes, 0x02014b50U);
        appendU16(bytes, 20U);
        appendU16(bytes, 20U);
        appendU16(bytes, 0U);
        appendU16(bytes, 0U);
        appendU16(bytes, 0U);
        appendU16(bytes, 0U);
        appendU32(bytes, entry.crc);
        appendU32(bytes, static_cast<std::uint32_t>(entry.content.size()));
        appendU32(bytes, static_cast<std::uint32_t>(entry.content.size()));
        appendU16(bytes, static_cast<std::uint16_t>(entry.name.size()));
        appendU16(bytes, 0U);
        appendU16(bytes, 0U);
        appendU16(bytes, 0U);
        appendU16(bytes, 0U);
        appendU32(bytes, 0U);
        appendU32(bytes, entry.localOffset);
        appendText(bytes, entry.name);
    }
    const auto centralSize = static_cast<std::uint32_t>(bytes.size()) - centralOffset;

    appendU32(bytes, 0x06054b50U);
    appendU16(bytes, 0U);
    appendU16(bytes, 0U);
    appendU16(bytes, static_cast<std::uint16_t>(centralEntries.size()));
    appendU16(bytes, static_cast<std::uint16_t>(centralEntries.size()));
    appendU32(bytes, centralSize);
    appendU32(bytes, centralOffset);
    appendU16(bytes, 0U);

    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    return path;
}

} // namespace contest_test
