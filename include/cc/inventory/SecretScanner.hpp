#pragma once

#include <cstddef>
#include <filesystem>
#include <string_view>

namespace cc {

struct SecretScanResult {
    bool sensitive{false};
    bool truncated{false};
};

class SecretScanner {
  public:
    static constexpr std::size_t defaultTextScanBytes = 1024U * 1024U;
    static constexpr std::size_t defaultFileScanBytes = 32U * 1024U;

    [[nodiscard]] bool hasSensitivePath(const std::filesystem::path& path) const;
    [[nodiscard]] SecretScanResult scanText(std::string_view text,
                                            std::size_t maxBytes = defaultTextScanBytes) const;
    [[nodiscard]] SecretScanResult
    scanFileContent(const std::filesystem::path& path,
                    std::size_t maxBytes = defaultFileScanBytes) const;
    [[nodiscard]] bool isSensitiveFile(const std::filesystem::path& path) const;
};

} // namespace cc
