/**
 * @file JsonUtil.hpp
 * @brief JSON 数组辅助转换。
 */

#pragma once

#include "cc/core/JsonValue.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace cc::util {

/** @brief 将字符串数组转换为 JSON 数组。 */
[[nodiscard]] JsonValue stringArrayToJson(const std::vector<std::string>& values);
/** @brief 将路径数组转换为 JSON 字符串数组。 */
[[nodiscard]] JsonValue pathArrayToJson(const std::vector<std::filesystem::path>& values);
/** @brief 从 JSON 数组中提取字符串，非字符串元素会被忽略。 */
[[nodiscard]] std::vector<std::string> jsonStringArray(const JsonValue& value);

} // namespace cc::util
