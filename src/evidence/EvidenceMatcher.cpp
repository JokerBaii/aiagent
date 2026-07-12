#include "cc/evidence/EvidenceMatcher.hpp"

#include "cc/inventory/MaterialTrustPolicy.hpp"
#include "cc/util/StringUtil.hpp"

#include <algorithm>
#include <optional>
#include <regex>
#include <set>

namespace cc {
namespace {

[[nodiscard]] std::vector<AssetRole> evidenceRolesForClaim(ClaimType type) {
    switch (type) {
    case ClaimType::UserTraction:
        return {AssetRole::UserResearch, AssetRole::ProofMaterial};
    case ClaimType::MarketScale:
        return {AssetRole::MarketResearch};
    case ClaimType::TechnicalCapability:
        return {AssetRole::SourceCode, AssetRole::BuildSystem, AssetRole::DeploymentDoc,
                AssetRole::ResearchPaper};
    case ClaimType::BusinessModel:
        return {AssetRole::BusinessPlan, AssetRole::FinancialPlan};
    case ClaimType::Revenue:
        return {AssetRole::FinancialPlan, AssetRole::ProofMaterial};
    case ClaimType::CostReduction:
        return {AssetRole::FinancialPlan, AssetRole::ExperimentData, AssetRole::ProofMaterial};
    case ClaimType::Patent:
    case ClaimType::Copyright:
        return {AssetRole::PatentCopyright, AssetRole::ProofMaterial};
    case ClaimType::Partnership:
        return {AssetRole::ProofMaterial, AssetRole::SocialPracticeProof};
    case ClaimType::Prototype:
        return {AssetRole::ProofMaterial, AssetRole::DeploymentDoc};
    case ClaimType::ResearchResult:
        return {AssetRole::ExperimentData, AssetRole::ResearchPaper};
    case ClaimType::SocialImpact:
        return {AssetRole::SocialPracticeProof, AssetRole::UserResearch};
    case ClaimType::Deployment:
        return {AssetRole::DeploymentDoc};
    case ClaimType::Unknown:
        return {AssetRole::ProofMaterial};
    }
    return {AssetRole::ProofMaterial};
}

[[nodiscard]] bool containsAny(const std::string& text,
                               std::initializer_list<std::string_view> values) {
    for (const auto value : values) {
        if (util::contains(text, value)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool hasTypeMarker(ClaimType type, const std::string& content) {
    switch (type) {
    case ClaimType::UserTraction:
        return containsAny(content, {"用户", "注册", "活跃", "付费"});
    case ClaimType::MarketScale:
        return containsAny(content, {"市场规模", "行业规模", "tam", "sam", "som"});
    case ClaimType::TechnicalCapability:
        return containsAny(content, {"技术", "算法", "模型", "architecture", "implementation"});
    case ClaimType::BusinessModel:
        return containsAny(content, {"商业模式", "付费", "订阅", "收入来源"});
    case ClaimType::Revenue:
        return containsAny(content, {"营收", "收入", "订单", "合同", "流水", "发票"});
    case ClaimType::CostReduction:
        return containsAny(content, {"降低成本", "降本", "节省", "成本下降"});
    case ClaimType::Patent:
        return util::contains(content, "专利");
    case ClaimType::Copyright:
        return containsAny(content, {"软著", "软件著作权"});
    case ClaimType::Partnership:
        return containsAny(content, {"合作", "协议", "合同"});
    case ClaimType::Prototype:
        return containsAny(content, {"原型", "样机", "prototype"});
    case ClaimType::ResearchResult:
        return containsAny(content, {"实验", "测试结果", "baseline", "准确率", "召回率"});
    case ClaimType::SocialImpact:
        return containsAny(content, {"社会影响", "服务对象", "受益", "活动记录"});
    case ClaimType::Deployment:
        return containsAny(content, {"部署", "上线", "生产环境", "health", "url"});
    case ClaimType::Unknown:
        return false;
    }
    return false;
}

[[nodiscard]] std::set<std::string> numbers(const std::string& text) {
    static const std::regex pattern{R"([0-9]+(?:[,.][0-9]+)*)"};
    std::set<std::string> result;
    for (auto iter = std::sregex_iterator(text.begin(), text.end(), pattern);
         iter != std::sregex_iterator(); ++iter) {
        auto value = iter->str();
        value.erase(std::remove(value.begin(), value.end(), ','), value.end());
        result.insert(std::move(value));
    }
    return result;
}

[[nodiscard]] std::set<std::string> semanticNumbers(const std::string& text) {
    auto result = numbers(text);
    if (result.size() <= 1U) {
        return result;
    }
    for (auto iter = result.begin(); iter != result.end();) {
        const bool likelyYear =
            iter->size() == 4U && (iter->starts_with("19") || iter->starts_with("20"));
        if (likelyYear) {
            iter = result.erase(iter);
        } else {
            ++iter;
        }
    }
    return result;
}

[[nodiscard]] std::set<std::string> latinTokens(const std::string& text) {
    static const std::regex pattern{R"([A-Za-z][A-Za-z0-9_-]{2,})"};
    static const std::set<std::string> ignored = {"the",  "and",  "for", "with", "from",
                                                  "this", "that", "tam", "sam",  "som"};
    std::set<std::string> result;
    for (auto iter = std::sregex_iterator(text.begin(), text.end(), pattern);
         iter != std::sregex_iterator(); ++iter) {
        const auto value = util::lowerAscii(iter->str());
        if (!ignored.contains(value)) {
            result.insert(value);
        }
    }
    return result;
}

[[nodiscard]] bool intersects(const std::set<std::string>& left,
                              const std::set<std::string>& right) {
    return std::any_of(left.begin(), left.end(),
                       [&](const auto& value) { return right.contains(value); });
}

[[nodiscard]] std::optional<std::string> partnerName(const std::string& claim) {
    const auto start = claim.find("与");
    if (start == std::string::npos) {
        return std::nullopt;
    }
    const auto nameStart = start + std::string{"与"}.size();
    auto end = claim.find("签", nameStart);
    if (end == std::string::npos) {
        end = claim.find("达成", nameStart);
    }
    if (end == std::string::npos) {
        end = claim.find("合作", nameStart);
    }
    if (end == std::string::npos || end <= nameStart || end - nameStart > 72U) {
        return std::nullopt;
    }
    const auto name = util::trim(claim.substr(nameStart, end - nameStart));
    return name.size() >= 6U ? std::optional<std::string>{name} : std::nullopt;
}

[[nodiscard]] bool contentRelevant(const ProjectClaim& claim, const std::string& content) {
    const auto lowerClaim = util::lowerAscii(claim.claimText);
    const auto lowerContent = util::lowerAscii(content);
    const auto claimNumbers = semanticNumbers(lowerClaim);
    const auto contentNumbers = semanticNumbers(lowerContent);
    if (!claimNumbers.empty() && !intersects(claimNumbers, contentNumbers)) {
        return false;
    }
    if (claim.claimType == ClaimType::Partnership) {
        const auto partner = partnerName(claim.claimText);
        if (partner.has_value() && !util::contains(content, *partner)) {
            return false;
        }
    }
    const bool sharedToken = intersects(latinTokens(lowerClaim), latinTokens(lowerContent));
    return hasTypeMarker(claim.claimType, lowerContent) ||
           (claim.claimType == ClaimType::TechnicalCapability && sharedToken);
}

[[nodiscard]] bool contradicts(const ProjectClaim& claim, const std::string& content) {
    const auto lower = util::lowerAscii(content);
    if (!hasTypeMarker(claim.claimType, lower) ||
        !containsAny(lower, {"未申请", "未授权", "未获得", "未签署", "未上线", "未部署", "未实现",
                             "没有营收", "无营收", "数据不一致", "无法验证", "已取消"})) {
        return false;
    }
    const auto claimNumbers = semanticNumbers(claim.claimText);
    if (!claimNumbers.empty() && !intersects(claimNumbers, semanticNumbers(content))) {
        return false;
    }
    const auto partner = partnerName(claim.claimText);
    return !partner.has_value() || util::contains(content, *partner);
}

[[nodiscard]] bool strongIndependentProof(const ProjectClaim& claim, const ProjectAsset& asset,
                                          const std::string& content) {
    if (asset.relativePath == claim.sourceFile) {
        return false;
    }
    const auto lower = util::lowerAscii(content);
    const auto claimNumbers = semanticNumbers(claim.claimText);
    const bool sharedNumber = intersects(claimNumbers, semanticNumbers(content));
    switch (claim.claimType) {
    case ClaimType::UserTraction:
        return sharedNumber &&
               (asset.role == AssetRole::UserResearch || asset.role == AssetRole::ProofMaterial) &&
               containsAny(lower, {"后台数据", "调查样本", "访谈记录", "数据导出"});
    case ClaimType::MarketScale:
        return sharedNumber && asset.role == AssetRole::MarketResearch &&
               containsAny(lower, {"数据来源", "报告显示", "统计年鉴", "引用"});
    case ClaimType::Revenue:
        return sharedNumber && asset.role == AssetRole::ProofMaterial &&
               containsAny(lower, {"订单编号", "合同编号", "银行流水", "发票号"});
    case ClaimType::CostReduction:
        return sharedNumber &&
               (asset.role == AssetRole::ExperimentData ||
                asset.role == AssetRole::ProofMaterial) &&
               containsAny(lower, {"对照", "测算", "原始成本", "实际成本"});
    case ClaimType::Patent:
        return asset.role == AssetRole::PatentCopyright &&
               std::any_of(claimNumbers.begin(), claimNumbers.end(),
                           [](const auto& value) { return value.size() >= 6U; }) &&
               sharedNumber && containsAny(lower, {"授权通知", "专利号", "申请号"});
    case ClaimType::Copyright:
        return asset.role == AssetRole::PatentCopyright &&
               std::any_of(claimNumbers.begin(), claimNumbers.end(),
                           [](const auto& value) { return value.size() >= 6U; }) &&
               sharedNumber && containsAny(lower, {"登记号", "软件著作权证书"});
    case ClaimType::Partnership: {
        const auto partner = partnerName(claim.claimText);
        return asset.role == AssetRole::ProofMaterial && partner.has_value() &&
               util::contains(content, *partner) &&
               containsAny(lower, {"盖章", "合同编号", "协议签署", "签约"});
    }
    case ClaimType::ResearchResult:
        return sharedNumber &&
               (asset.role == AssetRole::ExperimentData ||
                asset.role == AssetRole::ResearchPaper) &&
               containsAny(lower, {"baseline", "评价指标", "对照实验", "原始数据"});
    case ClaimType::SocialImpact:
        return sharedNumber && asset.role == AssetRole::SocialPracticeProof &&
               containsAny(lower, {"活动记录", "签到", "名单", "日期"});
    case ClaimType::TechnicalCapability:
    case ClaimType::BusinessModel:
    case ClaimType::Prototype:
    case ClaimType::Deployment:
    case ClaimType::Unknown:
        return false;
    }
    return false;
}

[[nodiscard]] const TextDocument* findDocument(const std::vector<TextDocument>& corpus,
                                               const std::filesystem::path& path) {
    const auto key = path.lexically_normal().generic_string();
    const auto found = std::find_if(corpus.begin(), corpus.end(), [&](const auto& document) {
        return document.sourceFile.lexically_normal().generic_string() == key;
    });
    return found == corpus.end() ? nullptr : &*found;
}

[[nodiscard]] bool usableCandidate(const ProjectAsset& asset, const std::vector<AssetRole>& roles) {
    return std::find(roles.begin(), roles.end(), asset.role) != roles.end() && !asset.sensitive &&
           !asset.generated && !asset.vendored && !isExcludedTruthPath(asset.relativePath);
}

[[nodiscard]] std::vector<EvidenceMatch> matchEvidence(const std::vector<ProjectClaim>& claims,
                                                       const ProjectInventory& inventory,
                                                       const std::vector<TextDocument>& corpus) {
    constexpr std::size_t kMaxEvidenceFiles = 24U;
    std::vector<EvidenceMatch> matches;
    matches.reserve(claims.size());
    for (const auto& claim : claims) {
        EvidenceMatch match;
        match.claimId = claim.claimId;
        match.missingEvidence = missingEvidenceForClaim(claim.claimType);
        const auto roles = evidenceRolesForClaim(claim.claimType);
        std::vector<std::filesystem::path> relevant;
        std::vector<std::filesystem::path> review;
        std::vector<std::filesystem::path> conflicts;
        bool supported = false;

        for (const auto& asset : inventory.assets) {
            if (!usableCandidate(asset, roles)) {
                continue;
            }
            const auto* document = findDocument(corpus, asset.relativePath);
            if (document == nullptr || !asset.auditable || needsTextReview(*document)) {
                if (review.size() < kMaxEvidenceFiles) {
                    review.push_back(asset.relativePath);
                }
                continue;
            }
            if (contradicts(claim, document->text)) {
                if (conflicts.size() < kMaxEvidenceFiles) {
                    conflicts.push_back(asset.relativePath);
                }
                continue;
            }
            if (!contentRelevant(claim, document->text)) {
                continue;
            }
            if (relevant.size() < kMaxEvidenceFiles) {
                relevant.push_back(asset.relativePath);
            }
            supported = supported || strongIndependentProof(claim, asset, document->text);
        }

        if (!conflicts.empty()) {
            match.status = EvidenceStatus::Conflicted;
            match.evidenceFiles = std::move(conflicts);
            match.reason = "候选证据中存在与声明相反的明确表述，需要核对原始材料。";
        } else if (supported) {
            match.status = EvidenceStatus::Supported;
            match.evidenceFiles = std::move(relevant);
            match.missingEvidence.clear();
            match.reason = "独立材料中存在与声明关键数值或主体一致的可追溯证据。";
        } else if (!relevant.empty()) {
            match.status = EvidenceStatus::Partial;
            match.evidenceFiles = std::move(relevant);
            match.reason = "发现内容相关材料，但尚未达到独立、可追溯的强证据标准。";
        } else if (!review.empty()) {
            match.status = EvidenceStatus::NeedReview;
            match.evidenceFiles = std::move(review);
            match.reason = "存在角色可能匹配的材料，但文本不可靠抽取或无法验证内容关联。";
        } else {
            match.status = EvidenceStatus::Unsupported;
            match.reason = "未发现与声明内容相关且可靠抽取的证据材料。";
        }
        matches.push_back(std::move(match));
    }
    return matches;
}

} // namespace

std::vector<std::string> missingEvidenceForClaim(ClaimType type) {
    switch (type) {
    case ClaimType::UserTraction:
        return {"用户数据", "后台截图", "问卷样本", "访谈记录"};
    case ClaimType::MarketScale:
        return {"行业报告", "统计数据", "TAM/SAM/SOM 拆解"};
    case ClaimType::TechnicalCapability:
        return {"源码", "构建入口", "部署说明"};
    case ClaimType::BusinessModel:
        return {"商业模式画布", "财务预测假设", "成本结构"};
    case ClaimType::Revenue:
        return {"订单", "合同", "流水", "收入测算表"};
    case ClaimType::CostReduction:
        return {"成本口径", "对照数据", "降本测算表"};
    case ClaimType::Patent:
        return {"专利申请受理或授权材料"};
    case ClaimType::Copyright:
        return {"软著证书或受理材料"};
    case ClaimType::Partnership:
        return {"合作协议", "盖章证明", "邮件记录"};
    case ClaimType::Prototype:
        return {"原型运行记录", "样机测试材料"};
    case ClaimType::ResearchResult:
        return {"实验数据", "评价指标", "baseline"};
    case ClaimType::SocialImpact:
        return {"服务对象证明", "活动记录", "影响数据"};
    case ClaimType::Deployment:
        return {"部署说明", "运行截图"};
    case ClaimType::Unknown:
        return {"可追溯证明材料"};
    }
    return {"可追溯证明材料"};
}

std::vector<EvidenceMatch> EvidenceMatcher::match(const std::vector<ProjectClaim>& claims,
                                                  const ProjectInventory& inventory) const {
    return matchEvidence(claims, inventory, {});
}

std::vector<EvidenceMatch> EvidenceMatcher::match(const std::vector<ProjectClaim>& claims,
                                                  const ProjectInventory& inventory,
                                                  const std::vector<TextDocument>& corpus) const {
    return matchEvidence(claims, inventory, corpus);
}

} // namespace cc
