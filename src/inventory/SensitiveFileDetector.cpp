/**
 * @file SensitiveFileDetector.cpp
 * @brief 敏感文件识别实现。
 */

#include "cc/inventory/SensitiveFileDetector.hpp"
#include "cc/util/FileUtil.hpp"
#include "cc/util/StringUtil.hpp"

namespace cc {

bool SensitiveFileDetector::isSensitive(const std::filesystem::path& path) const {
    const auto fileName = util::lowerAscii(path.filename().string());
    const auto extension = util::lowerAscii(path.extension().string());
    if (fileName == ".env" || fileName == ".env.local" || fileName == "id_rsa" ||
        fileName == "credentials.json" || extension == ".pem" || extension == ".key") {
        return true;
    }
    if (util::contains(fileName, "secret") || util::contains(fileName, "token.") ||
        util::contains(fileName, "credentials")) {
        return true;
    }
    if (extension == ".json" || extension == ".yaml" || extension == ".yml" ||
        extension == ".toml" || extension == ".ini") {
        const auto sample = util::lowerAscii(util::readFileLimited(path, 8192U));
        return util::contains(sample, "password") || util::contains(sample, "api_key") ||
               util::contains(sample, "access_token") || util::contains(sample, "private_key");
    }
    return false;
}

} // namespace cc
