/**
 * @file JsonValue.cpp
 * @brief JsonValue 与 nlohmann/json 之间的解析和序列化适配。
 */

#include "cc/core/JsonValue.hpp"

#include <cmath>

#include <nlohmann/json.hpp>

namespace cc {
namespace {

const std::string kEmptyString;
const JsonValue::Array kEmptyArray;
const JsonValue::Object kEmptyObject;
const JsonValue kNullJson;

[[nodiscard]] JsonValue fromThirdPartyJson(const nlohmann::json& value) {
    if (value.is_null()) {
        return JsonValue{nullptr};
    }
    if (value.is_boolean()) {
        return JsonValue{value.get<bool>()};
    }
    if (value.is_number()) {
        return JsonValue{value.get<double>()};
    }
    if (value.is_string()) {
        return JsonValue{value.get<std::string>()};
    }
    if (value.is_array()) {
        JsonValue::Array array;
        array.reserve(value.size());
        for (const auto& item : value) {
            array.push_back(fromThirdPartyJson(item));
        }
        return JsonValue{std::move(array)};
    }
    JsonValue::Object object;
    for (const auto& [key, child] : value.items()) {
        object.emplace(key, fromThirdPartyJson(child));
    }
    return JsonValue{std::move(object)};
}

[[nodiscard]] nlohmann::json toThirdPartyJson(const JsonValue& value) {
    if (value.isNull()) {
        return nullptr;
    }
    if (value.isBool()) {
        return value.asBool();
    }
    if (value.isNumber()) {
        const auto number = value.asNumber();
        if (std::fabs(number - std::round(number)) < 0.000001) {
            return static_cast<long long>(std::llround(number));
        }
        return number;
    }
    if (value.isString()) {
        return value.asString();
    }
    if (value.isArray()) {
        nlohmann::json array = nlohmann::json::array();
        for (const auto& child : value.asArray()) {
            array.push_back(toThirdPartyJson(child));
        }
        return array;
    }
    nlohmann::json object = nlohmann::json::object();
    for (const auto& [key, child] : value.asObject()) {
        object[key] = toThirdPartyJson(child);
    }
    return object;
}

} // namespace

JsonValue::JsonValue() : storage_(nullptr) {}
JsonValue::JsonValue(std::nullptr_t) : storage_(nullptr) {}
JsonValue::JsonValue(bool value) : storage_(value) {}
JsonValue::JsonValue(int value) : storage_(static_cast<double>(value)) {}
JsonValue::JsonValue(double value) : storage_(value) {}
JsonValue::JsonValue(std::string value) : storage_(std::move(value)) {}
JsonValue::JsonValue(const char* value) : storage_(std::string(value)) {}
JsonValue::JsonValue(Array value) : storage_(std::move(value)) {}
JsonValue::JsonValue(Object value) : storage_(std::move(value)) {}

bool JsonValue::isNull() const {
    return std::holds_alternative<std::nullptr_t>(storage_);
}
bool JsonValue::isBool() const {
    return std::holds_alternative<bool>(storage_);
}
bool JsonValue::isNumber() const {
    return std::holds_alternative<double>(storage_);
}
bool JsonValue::isString() const {
    return std::holds_alternative<std::string>(storage_);
}
bool JsonValue::isArray() const {
    return std::holds_alternative<Array>(storage_);
}
bool JsonValue::isObject() const {
    return std::holds_alternative<Object>(storage_);
}

bool JsonValue::asBool(bool fallback) const {
    return isBool() ? std::get<bool>(storage_) : fallback;
}

double JsonValue::asNumber(double fallback) const {
    return isNumber() ? std::get<double>(storage_) : fallback;
}

const std::string& JsonValue::asString() const {
    return isString() ? std::get<std::string>(storage_) : kEmptyString;
}

const JsonValue::Array& JsonValue::asArray() const {
    return isArray() ? std::get<Array>(storage_) : kEmptyArray;
}

const JsonValue::Object& JsonValue::asObject() const {
    return isObject() ? std::get<Object>(storage_) : kEmptyObject;
}

JsonValue::Array& JsonValue::asArray() {
    if (!isArray()) {
        storage_ = Array{};
    }
    return std::get<Array>(storage_);
}

JsonValue::Object& JsonValue::asObject() {
    if (!isObject()) {
        storage_ = Object{};
    }
    return std::get<Object>(storage_);
}

const JsonValue& JsonValue::at(const std::string& key) const {
    if (!isObject()) {
        return kNullJson;
    }
    const auto& object = std::get<Object>(storage_);
    const auto iter = object.find(key);
    return iter == object.end() ? kNullJson : iter->second;
}

const JsonValue& JsonValue::at(std::size_t index) const {
    if (!isArray() || index >= std::get<Array>(storage_).size()) {
        return kNullJson;
    }
    return std::get<Array>(storage_)[index];
}

Result<JsonValue> parseJson(std::string_view text) {
    try {
        // JSON 规则包和审计结果必须交给成熟解析器处理，避免自写解析器遗漏
        // Unicode、转义和错误定位等边界，影响规则可复核性。
        return Result<JsonValue>::success(fromThirdPartyJson(nlohmann::json::parse(text)));
    } catch (const nlohmann::json::exception& error) {
        return Result<JsonValue>::failure(std::string{"JSON 解析失败: "} + error.what());
    }
}

std::string writeJson(const JsonValue& value, int indent) {
    const auto thirdParty = toThirdPartyJson(value);
    constexpr auto errorHandler = nlohmann::json::error_handler_t::replace;
    return indent <= 0 ? thirdParty.dump(-1, ' ', false, errorHandler)
                       : thirdParty.dump(indent, ' ', false, errorHandler);
}

std::string jsonEscape(std::string_view value) {
    const auto dumped = nlohmann::json{std::string{value}}.dump();
    return dumped.size() >= 2U ? dumped.substr(1U, dumped.size() - 2U) : dumped;
}

} // namespace cc
