/**
 * @file StringUtil.hpp
 * @brief 小型字符串工具。
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace cc::util {

/** @brief 将 ASCII 字符转换为小写，用于文件扩展名和英文关键字匹配。 */
[[nodiscard]] std::string lowerAscii(std::string value);
/** @brief 去除字符串两端空白。 */
[[nodiscard]] std::string trim(std::string value);
/** @brief 判断 text 是否包含 needle。 */
[[nodiscard]] bool contains(std::string_view text, std::string_view needle);
/** @brief 先做 ASCII 小写后判断是否包含英文 needle。 */
[[nodiscard]] bool containsLower(const std::string& text, const std::string& needle);
/** @brief 按行拆分文本，供抽取器逐行分析材料。 */
[[nodiscard]] std::vector<std::string> splitLines(const std::string& text);
/** @brief 使用分隔符连接字符串数组。 */
[[nodiscard]] std::string join(const std::vector<std::string>& values, std::string_view separator);

} // namespace cc::util
