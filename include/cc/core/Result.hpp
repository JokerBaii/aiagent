/**
 * @file Result.hpp
 * @brief 可失败操作的统一返回类型。
 *
 * 核心模块需要显式暴露失败原因，避免用空对象掩盖路径、规则、权限或输入错误。
 */

#pragma once

#include <string>
#include <utility>

namespace cc {

/**
 * @brief 表示成功值或失败信息。
 */
template <typename T> class Result {
  public:
    /** @brief 构造成功结果。 */
    static Result success(T value) {
        Result result;
        result.ok_ = true;
        result.value_ = std::move(value);
        return result;
    }

    /** @brief 构造失败结果并保留错误原因。 */
    static Result failure(std::string message) {
        Result result;
        result.ok_ = false;
        result.error_ = std::move(message);
        return result;
    }

    /** @brief 判断操作是否成功。 */
    [[nodiscard]] bool ok() const {
        return ok_;
    }
    /** @brief 读取成功值。调用方应先检查 ok()。 */
    [[nodiscard]] const T& value() const {
        return value_;
    }
    /** @brief 读取可修改成功值。调用方应先检查 ok()。 */
    [[nodiscard]] T& value() {
        return value_;
    }
    /** @brief 读取失败原因。 */
    [[nodiscard]] const std::string& error() const {
        return error_;
    }

  private:
    bool ok_{false};
    T value_{};
    std::string error_;
};

/**
 * @brief 无返回值操作的 Result 特化。
 */
template <> class Result<void> {
  public:
    /** @brief 构造无返回值的成功结果。 */
    static Result success() {
        Result result;
        result.ok_ = true;
        return result;
    }

    /** @brief 构造无返回值的失败结果。 */
    static Result failure(std::string message) {
        Result result;
        result.ok_ = false;
        result.error_ = std::move(message);
        return result;
    }

    /** @brief 判断操作是否成功。 */
    [[nodiscard]] bool ok() const {
        return ok_;
    }
    /** @brief 读取失败原因。 */
    [[nodiscard]] const std::string& error() const {
        return error_;
    }

  private:
    bool ok_{false};
    std::string error_;
};

} // namespace cc
