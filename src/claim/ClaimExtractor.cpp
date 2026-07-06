/**
 * @file ClaimExtractor.cpp
 * @brief 项目承诺性声明抽取实现。
 */

#include "cc/claim/ClaimExtractor.hpp"
#include "cc/util/StringUtil.hpp"

#include <iomanip>
#include <sstream>

namespace cc {

std::vector<ProjectClaim> ClaimExtractor::extract(const std::vector<TextDocument>& corpus) const {
    std::vector<ProjectClaim> claims;
    std::size_t counter = 1;
    constexpr std::size_t kMaxClaims = 120U;

    for (const auto& doc : corpus) {
        for (const auto& rawLine : util::splitLines(doc.text)) {
            const auto line = util::trim(rawLine);
            if (line.size() < 6U || line.size() > 360U) {
                continue;
            }
            ClaimType type = ClaimType::Unknown;
            if (util::contains(line, "专利")) {
                type = ClaimType::Patent;
            } else if (util::contains(line, "软著")) {
                type = ClaimType::Copyright;
            } else if (util::contains(line, "合作") || util::contains(line, "协议")) {
                type = ClaimType::Partnership;
            } else if (util::contains(line, "市场规模") || util::contains(line, "TAM")) {
                type = ClaimType::MarketScale;
            } else if (util::contains(line, "已有用户") || util::contains(line, "注册用户")) {
                type = ClaimType::UserTraction;
            } else if (util::contains(line, "营收") || util::contains(line, "收入")) {
                type = ClaimType::Revenue;
            } else if (util::contains(line, "实验") || util::contains(line, "baseline")) {
                type = ClaimType::ResearchResult;
            } else if (util::contains(line, "部署") || util::contains(line, "上线")) {
                type = ClaimType::Deployment;
            } else if (util::contains(line, "社会影响") || util::contains(line, "服务对象")) {
                type = ClaimType::SocialImpact;
            } else if (util::contains(line, "技术创新") || util::contains(line, "技术领先")) {
                type = ClaimType::TechnicalCapability;
            } else if (util::contains(line, "商业模式") || util::contains(line, "付费方")) {
                type = ClaimType::BusinessModel;
            }
            if (type == ClaimType::Unknown) {
                continue;
            }

            ProjectClaim claim;
            std::ostringstream id;
            id << "CLM-" << std::setw(3) << std::setfill('0') << counter++;
            claim.claimId = id.str();
            claim.claimText = line;
            claim.claimType = type;
            claim.sourceFile = doc.sourceFile;
            claim.confidence = 0.75;
            claim.initialRisk = "声明需要可追溯证据支撑";
            claims.push_back(std::move(claim));
            if (claims.size() >= kMaxClaims) {
                return claims;
            }
        }
    }
    return claims;
}

} // namespace cc
