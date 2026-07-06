/**
 * @file ExternalToolAdapter.cpp
 * @brief 外部工具适配器登记与权限检查实现。
 */

#include "cc/agent/ExternalToolAdapter.hpp"
#include "cc/agent/HumanApprovalGate.hpp"

namespace cc {

std::vector<ExternalToolCapability> ExternalToolAdapter::reservedCapabilities() const {
    return {{"external_search", ToolPermission::NetworkAccess, "联网检索可追溯来源"},
            {"ocr_scan", ToolPermission::ExecuteCommand, "OCR 扫描件文字抽取"},
            {"github_lookup", ToolPermission::NetworkAccess, "远程仓库元数据核验"},
            {"browser_review", ToolPermission::NetworkAccess, "网页证据人工复核"}};
}

bool ExternalToolAdapter::canUse(const ExternalToolCapability& capability,
                                 bool userConfirmed) const {
    return HumanApprovalGate{}.approve(capability.permission, userConfirmed);
}

} // namespace cc
