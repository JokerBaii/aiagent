/**
 * @file PathGuard.cpp
 * @brief 路径安全边界检查实现。
 */

#include "cc/loader/PathGuard.hpp"
#include "cc/util/StringUtil.hpp"

namespace cc {

Result<std::filesystem::path> PathGuard::normalize(const std::filesystem::path& path) {
    std::error_code ec;
    auto absolute = std::filesystem::absolute(path, ec);
    if (ec) {
        return Result<std::filesystem::path>::failure("无法规范化路径: " + ec.message());
    }
    auto normalized = std::filesystem::weakly_canonical(absolute, ec);
    if (ec) {
        normalized = absolute.lexically_normal();
    }
    return Result<std::filesystem::path>::success(normalized.lexically_normal());
}

bool PathGuard::isInsideRoot(const std::filesystem::path& root,
                             const std::filesystem::path& target) {
    const auto normalizedRoot = normalize(root);
    const auto normalizedTarget = normalize(target);
    if (!normalizedRoot.ok() || !normalizedTarget.ok()) {
        return false;
    }

    auto rootIter = normalizedRoot.value().begin();
    auto targetIter = normalizedTarget.value().begin();
    for (; rootIter != normalizedRoot.value().end(); ++rootIter, ++targetIter) {
        if (targetIter == normalizedTarget.value().end() || *rootIter != *targetIter) {
            return false;
        }
    }
    return true;
}

bool PathGuard::isSafeArchiveEntry(const std::string& entryName) {
    if (entryName.empty() || util::contains(entryName, "\\")) {
        return false;
    }
    std::string normalizedEntry = entryName;
    while (!normalizedEntry.empty() && normalizedEntry.back() == '/') {
        normalizedEntry.pop_back();
    }
    if (normalizedEntry.empty()) {
        return false;
    }
    const std::filesystem::path entryPath{normalizedEntry};
    if (entryPath.is_absolute()) {
        return false;
    }
    std::size_t start = 0U;
    while (start <= normalizedEntry.size()) {
        const auto end = normalizedEntry.find('/', start);
        const auto part = normalizedEntry.substr(start, end - start);
        if (part.empty() || part == "." || part == "..") {
            return false;
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1U;
    }
    return true;
}

} // namespace cc
