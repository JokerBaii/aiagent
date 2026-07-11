#include "cc/inventory/GeneratedVendoredDetector.hpp"

#include "cc/util/StringUtil.hpp"

#include <array>

namespace cc {
namespace {

template <std::size_t Size>
[[nodiscard]] bool hasComponent(const std::filesystem::path& path,
                                const std::array<const char*, Size>& names) {
    for (const auto& component : path.lexically_normal()) {
        const auto value = util::lowerAscii(component.string());
        for (const auto* name : names) {
            if (value == name) {
                return true;
            }
        }
        if (value.rfind("cmake-build-", 0U) == 0U) {
            return true;
        }
    }
    return false;
}

} // namespace

bool GeneratedVendoredDetector::isGenerated(const std::filesystem::path& path) const {
    constexpr std::array<const char*, 10> names = {
        "build", "dist", "out", "target", ".next", "coverage", "__pycache__",
        "generated", ".cache", ".pytest_cache",
    };
    return hasComponent(path, names);
}

bool GeneratedVendoredDetector::isVendored(const std::filesystem::path& path) const {
    constexpr std::array<const char*, 9> names = {
        "node_modules", "vendor", "third_party", "external", ".venv",
        "venv",         ".tox",   "site-packages", "bower_components",
    };
    return hasComponent(path, names);
}

} // namespace cc
