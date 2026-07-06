/**
 * @file TimeUtil.hpp
 * @brief 时间与会话 ID 工具。
 */

#pragma once

#include <string>

namespace cc::util {

/**
 * @brief 生成审计会话 ID。
 *
 * 会话 ID 用于创建 .workspaces/<session_id>，把每次导入和修复产物隔离开。
 */
[[nodiscard]] std::string makeSessionId();

} // namespace cc::util
