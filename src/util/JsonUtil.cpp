/**
 * @file JsonUtil.cpp
 * @brief JSON 数组辅助转换实现。
 */

#include "cc/util/JsonUtil.hpp"
#include "cc/util/FileUtil.hpp"

namespace cc::util {

JsonValue stringArrayToJson(const std::vector<std::string>& values) {
    JsonValue::Array array;
    for (const auto& value : values) {
        array.emplace_back(value);
    }
    return JsonValue{array};
}

JsonValue pathArrayToJson(const std::vector<std::filesystem::path>& values) {
    JsonValue::Array array;
    for (const auto& value : values) {
        array.emplace_back(pathString(value));
    }
    return JsonValue{array};
}

std::vector<std::string> jsonStringArray(const JsonValue& value) {
    std::vector<std::string> result;
    if (!value.isArray()) {
        return result;
    }
    for (const auto& item : value.asArray()) {
        if (item.isString()) {
            result.push_back(item.asString());
        }
    }
    return result;
}

} // namespace cc::util
