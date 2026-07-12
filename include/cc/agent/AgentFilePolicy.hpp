/**
 * @file AgentFilePolicy.hpp
 * @brief Agent 文件读取前共用的文本与敏感内容策略。
 */

#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

namespace cc::agent_file_policy {

/** @brief 清理无效 UTF-8，保证工具输出可以安全进入 JSON。 */
[[nodiscard]] std::string sanitizeUtf8(std::string_view value);
/** @brief 按字节上限截断文本，但不会从 UTF-8 字符中间切断。 */
[[nodiscard]] std::string truncateText(const std::string& value, std::size_t limit);
/** @brief 判断文件是否适合由 Agent 作为文本读取。 */
[[nodiscard]] bool isReadableTextLike(const std::filesystem::path& path);
/** @brief 仅根据相对路径判断是否命中敏感文件命名规则。 */
[[nodiscard]] bool hasSensitivePathComponent(const std::filesystem::path& path);
/** @brief 判断一段待输出或待写入文本是否含常见密钥标记。 */
[[nodiscard]] bool textContainsSecretMarker(std::string sample);
/** @brief 判断路径名或有界内容采样是否可能包含凭证、私钥等敏感信息。 */
[[nodiscard]] bool isSensitiveFile(const std::filesystem::path& path);

} // namespace cc::agent_file_policy
