/**
 * @file TimeUtil.cpp
 * @brief 会话 ID 生成实现。
 */

#include "cc/util/TimeUtil.hpp"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>

namespace cc::util {

std::string makeSessionId() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    const auto ticks = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
    std::random_device random;
    const auto nonce = (static_cast<std::uint64_t>(random()) << 32U) ^
                       static_cast<std::uint64_t>(random()) ^ ticks;

    std::ostringstream id;
    id << "session-" << millis << '-' << std::hex << std::setw(16) << std::setfill('0') << nonce;
    return id.str();
}

} // namespace cc::util
