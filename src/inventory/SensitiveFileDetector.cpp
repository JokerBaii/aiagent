#include "cc/inventory/SensitiveFileDetector.hpp"

#include "cc/inventory/SecretScanner.hpp"

namespace cc {

bool SensitiveFileDetector::isSensitive(const std::filesystem::path& path) const {
    return SecretScanner{}.isSensitiveFile(path);
}

} // namespace cc
