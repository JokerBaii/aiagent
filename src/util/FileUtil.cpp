/**
 * @file FileUtil.cpp
 * @brief 文件读写工具实现。
 */

#include "cc/util/FileUtil.hpp"

#include <fstream>

namespace cc::util {

std::string pathString(const std::filesystem::path& path) {
    return path.generic_string();
}

std::string readFileLimited(const std::filesystem::path& path, std::size_t limit) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }
    std::string content;
    content.resize(limit);
    input.read(content.data(), static_cast<std::streamsize>(limit));
    content.resize(static_cast<std::size_t>(input.gcount()));
    return content;
}

Result<void> writeTextFile(const std::filesystem::path& path, const std::string& content) {
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            return Result<void>::failure("无法创建输出目录: " + ec.message());
        }
    }
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        return Result<void>::failure("无法写入文件: " + pathString(path));
    }
    output << content;
    return Result<void>::success();
}

} // namespace cc::util
