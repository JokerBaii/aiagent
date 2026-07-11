/**
 * @file FileUtil.cpp
 * @brief 文件读写工具实现。
 */

#include "cc/util/FileUtil.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <limits>

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
    if (content.size() > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
        return Result<void>::failure("输出内容超过平台写入上限: " + pathString(path));
    }

    static std::atomic<std::uint64_t> sequence{0U};
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    auto temporary = path;
    temporary += ".tmp." + std::to_string(nonce) + "." +
                 std::to_string(sequence.fetch_add(1U, std::memory_order_relaxed));

    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) {
        return Result<void>::failure("无法写入文件: " + pathString(path));
    }
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    output.flush();
    if (!output) {
        output.close();
        std::filesystem::remove(temporary, ec);
        return Result<void>::failure("文件写入或刷盘失败: " + pathString(path));
    }
    output.close();
    if (!output) {
        std::filesystem::remove(temporary, ec);
        return Result<void>::failure("文件关闭失败: " + pathString(path));
    }
    std::filesystem::rename(temporary, path, ec);
    if (ec) {
        std::filesystem::remove(temporary, ec);
        return Result<void>::failure("无法原子替换输出文件: " + pathString(path));
    }
    return Result<void>::success();
}

} // namespace cc::util
