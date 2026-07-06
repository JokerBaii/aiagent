/**
 * @file GeneratedVendoredDetector.cpp
 * @brief 生成物和第三方依赖识别实现。
 */

#include "cc/inventory/GeneratedVendoredDetector.hpp"
#include "cc/util/StringUtil.hpp"

namespace cc {

bool GeneratedVendoredDetector::isGenerated(const std::filesystem::path& path) const {
    const auto text = util::lowerAscii(path.generic_string());
    return util::contains(text, "/build/") || util::contains(text, "/dist/") ||
           util::contains(text, "/out/") || util::contains(text, "/target/") ||
           util::contains(text, "/.next/") || util::contains(text, "/coverage/") ||
           util::contains(text, "/__pycache__/") || util::contains(text, "/generated/");
}

bool GeneratedVendoredDetector::isVendored(const std::filesystem::path& path) const {
    const auto text = util::lowerAscii(path.generic_string());
    return util::contains(text, "/node_modules/") || util::contains(text, "/vendor/") ||
           util::contains(text, "/third_party/") || util::contains(text, "/external/") ||
           util::contains(text, "/.venv/") || util::contains(text, "/venv/");
}

} // namespace cc
