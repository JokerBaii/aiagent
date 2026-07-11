/**
 * @file ImportLimits.hpp
 * @brief 项目导入和压缩包展开的统一资源预算。
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace cc {

/**
 * @brief 所有输入通道共用的资源上限。
 *
 * 调用方可以在测试或受控环境中收紧预算，但不应把任一字段设为零来表示“无限制”。
 */
struct ImportLimits {
    // 该上限约束目录条目清单的内存和遍历成本，而不是常见仓库的功能门槛。十万条目足以覆盖
    // 大多数包含多语言源码与素材的项目，同时仍能对异常目录树保持明确边界。
    std::size_t maxFileCount{100'000U};
    std::uint64_t maxSingleFileBytes{64ULL * 1024ULL * 1024ULL};
    std::uint64_t maxTotalBytes{512ULL * 1024ULL * 1024ULL};
    std::uint64_t maxArchiveBytes{64ULL * 1024ULL * 1024ULL};
    std::uint64_t maxExpandedBytes{512ULL * 1024ULL * 1024ULL};
    std::size_t maxPathDepth{32U};
    double maxCompressionRatio{100.0};
};

} // namespace cc
