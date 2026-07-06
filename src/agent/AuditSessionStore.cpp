/**
 * @file AuditSessionStore.cpp
 * @brief 审计会话持久化实现。
 */

#include "cc/agent/AuditSessionStore.hpp"
#include "cc/report/JsonReporter.hpp"
#include "cc/util/FileUtil.hpp"
#include "cc/util/JsonUtil.hpp"

namespace cc {

Result<void> AuditSessionStore::save(const AuditResult& result,
                                     const std::filesystem::path& output) const {
    return JsonReporter{}.write(result, output);
}

Result<void> AuditSessionStore::save(const AuditSession& session,
                                     const std::filesystem::path& output) const {
    const JsonValue json =
        JsonValue::Object{{"session_id", session.sessionId},
                          {"tool_outputs", util::stringArrayToJson(session.toolOutputs)},
                          {"audit_result", JsonReporter{}.toJson(session.result)}};
    return util::writeTextFile(output, writeJson(json, 2) + "\n");
}

} // namespace cc
