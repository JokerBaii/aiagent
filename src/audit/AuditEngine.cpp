/**
 * @file AuditEngine.cpp
 * @brief 核心审计引擎实现。
 */

#include "cc/audit/AuditEngine.hpp"
#include "cc/audit/StagedAuditEngine.hpp"

namespace cc {

Result<AuditResult> AuditEngine::run(const ProjectContext& context,
                                     const AuditOptions& options) const {
    // 步骤序列的唯一真相源在 StagedAuditEngine；本入口只做一次性驱动，供批处理调用方使用。
    StagedAuditEngine engine;
    engine.reset(context, options);
    while (engine.hasNext()) {
        auto stage = engine.advance();
        if (!stage.ok()) {
            return Result<AuditResult>::failure(stage.error());
        }
    }
    return Result<AuditResult>::success(engine.takeResult());
}

} // namespace cc
