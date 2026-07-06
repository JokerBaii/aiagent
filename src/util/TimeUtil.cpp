/**
 * @file TimeUtil.cpp
 * @brief 会话 ID 生成实现。
 */

#include "cc/util/TimeUtil.hpp"

#include <chrono>

namespace cc::util {

std::string makeSessionId() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    return "session-" + std::to_string(millis);
}

} // namespace cc::util
