/**
 * @file AnalyzerRegistry.hpp
 * @brief 专用分析器注册器。
 */

#pragma once

#include "cc/agent/IAnalyzer.hpp"

#include <memory>
#include <string>
#include <vector>

namespace cc {

/**
 * @brief 注册文档要求的专用 Analyzer，供 agentic runtime 固定调用。
 */
class AnalyzerRegistry {
  public:
    /** @brief 返回内置专用分析器名称。 */
    [[nodiscard]] std::vector<std::string> builtinAnalyzerNames() const;
    /** @brief 创建内置专用分析器实例。 */
    [[nodiscard]] std::vector<std::unique_ptr<IAnalyzer>> createAnalyzers() const;
};

} // namespace cc
