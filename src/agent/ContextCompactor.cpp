/**
 * @file ContextCompactor.cpp
 * @brief 审计上下文摘要实现。
 */

#include "cc/agent/ContextCompactor.hpp"
#include "cc/core/Enums.hpp"

#include <sstream>

namespace cc {

std::string ContextCompactor::summarize(const ProjectInventory& inventory,
                                        const std::vector<TextDocument>& corpus) const {
    std::ostringstream output;
    output << "资产 " << inventory.assets.size() << " 个；文本 " << corpus.size() << " 份；角色：";
    bool first = true;
    for (const auto& [role, count] : inventory.roleCounts) {
        output << (first ? "" : "、") << toString(role) << "=" << count;
        first = false;
    }
    return output.str();
}

} // namespace cc
