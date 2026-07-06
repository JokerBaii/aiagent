/**
 * @file JsonValue.hpp
 * @brief 项目 JSON 值与解析接口。
 *
 * 核心库保留稳定的 JsonValue 业务类型，底层解析和序列化委托 nlohmann/json，
 * 避免自写解析器遗漏规则包中的转义、Unicode 和错误边界。
 */

#pragma once

#include "cc/core/Result.hpp"

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace cc {

/**
 * @brief 项目稳定 JSON 值。
 *
 * JSON 值只承担规则包和报告的结构化表示，不包含任何审计业务判断。
 */
class JsonValue {
  public:
    using Array = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue>;
    using Storage = std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;

    JsonValue();
    JsonValue(std::nullptr_t);
    JsonValue(bool value);
    JsonValue(int value);
    JsonValue(double value);
    JsonValue(std::string value);
    JsonValue(const char* value);
    JsonValue(Array value);
    JsonValue(Object value);

    /** @brief 判断是否为空值。 */
    [[nodiscard]] bool isNull() const;
    /** @brief 判断是否为布尔值。 */
    [[nodiscard]] bool isBool() const;
    /** @brief 判断是否为数字。 */
    [[nodiscard]] bool isNumber() const;
    /** @brief 判断是否为字符串。 */
    [[nodiscard]] bool isString() const;
    /** @brief 判断是否为数组。 */
    [[nodiscard]] bool isArray() const;
    /** @brief 判断是否为对象。 */
    [[nodiscard]] bool isObject() const;
    /** @brief 按布尔值读取，类型不符时返回 fallback。 */
    [[nodiscard]] bool asBool(bool fallback = false) const;
    /** @brief 按数字读取，类型不符时返回 fallback。 */
    [[nodiscard]] double asNumber(double fallback = 0.0) const;
    /** @brief 按字符串读取，类型不符时返回空字符串引用。 */
    [[nodiscard]] const std::string& asString() const;
    /** @brief 按数组读取，类型不符时返回空数组引用。 */
    [[nodiscard]] const Array& asArray() const;
    /** @brief 按对象读取，类型不符时返回空对象引用。 */
    [[nodiscard]] const Object& asObject() const;
    /** @brief 获取可修改数组引用。 */
    [[nodiscard]] Array& asArray();
    /** @brief 获取可修改对象引用。 */
    [[nodiscard]] Object& asObject();
    /** @brief 按对象 key 读取子值，缺失时返回 null 值。 */
    [[nodiscard]] const JsonValue& at(const std::string& key) const;
    /** @brief 按数组下标读取子值，越界时返回 null 值。 */
    [[nodiscard]] const JsonValue& at(std::size_t index) const;

  private:
    Storage storage_;
};

/** @brief 解析 JSON 文本。 */
[[nodiscard]] Result<JsonValue> parseJson(std::string_view text);
/** @brief 序列化 JSON 值。 */
[[nodiscard]] std::string writeJson(const JsonValue& value, int indent = 2);
/** @brief 转义 JSON 字符串内容。 */
[[nodiscard]] std::string jsonEscape(std::string_view value);

} // namespace cc
