#include "cc/claim/ClaimExtractor.hpp"

#include "cc/inventory/MaterialTrustPolicy.hpp"
#include "cc/util/StringUtil.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <optional>
#include <set>
#include <sstream>

namespace cc {
namespace {

[[nodiscard]] bool containsAny(const std::string& text,
                               std::initializer_list<std::string_view> values) {
    for (const auto value : values) {
        if (util::contains(text, value)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool containsDigit(const std::string& text) {
    return std::any_of(text.begin(), text.end(),
                       [](unsigned char value) { return std::isdigit(value) != 0; });
}

[[nodiscard]] bool negated(const std::string& line) {
    const auto lower = util::lowerAscii(line);
    return containsAny(lower, {"未申请", "未获得", "未授权", "未取得", "未签署", "未合作",
                               "未上线", "未部署", "未实现", "未产生", "未形成", "没有营收",
                               "没有收入", "无营收", "无收入", "无专利", "无软著", "暂无",
                               "尚未", "暂未", "不具备", "不涉及", "不代表", "不承诺", "禁止",
                               "不得", "取消合作", "计划申请", "拟申请", "计划上线", "拟上线",
                               "拟合作", "寻求合作"});
}

[[nodiscard]] std::optional<ClaimType> classify(const std::string& line) {
    const auto lower = util::lowerAscii(line);
    const bool numeric = containsDigit(lower);
    if (negated(lower)) {
        return std::nullopt;
    }
    if (util::contains(lower, "专利") &&
        containsAny(lower, {"已申请", "已授权", "获得", "拥有", "取得", "申请号", "专利号"})) {
        return ClaimType::Patent;
    }
    if (containsAny(lower, {"软著", "软件著作权"}) &&
        containsAny(lower, {"已申请", "已登记", "获得", "拥有", "取得", "登记号", "证书号"})) {
        return ClaimType::Copyright;
    }
    if (containsAny(lower, {"合作", "协议"}) &&
        containsAny(lower, {"已签署", "已签订", "达成合作", "建立合作", "合作协议", "合同编号"})) {
        return ClaimType::Partnership;
    }
    if (containsAny(lower, {"市场规模", "tam", "sam", "som"}) &&
        (numeric || containsAny(lower, {"预计", "达到", "约为", "规模为", "很大"}))) {
        return ClaimType::MarketScale;
    }
    if (containsAny(lower, {"已有用户", "注册用户", "活跃用户", "累计用户", "付费用户"}) &&
        (numeric || containsAny(lower, {"超过", "达到", "累计", "已有"}))) {
        return ClaimType::UserTraction;
    }
    if (containsAny(lower, {"营收", "营业收入", "销售收入", "实现收入"}) &&
        (numeric || containsAny(lower, {"实现", "预计", "达到"}))) {
        return ClaimType::Revenue;
    }
    if (containsAny(lower, {"降低成本", "降本", "节省成本", "成本下降"}) && numeric) {
        return ClaimType::CostReduction;
    }
    if (containsAny(lower, {"原型", "样机"}) &&
        containsAny(lower, {"已完成", "已开发", "已制作", "形成", "完成"})) {
        return ClaimType::Prototype;
    }
    if (containsAny(lower, {"实验结果", "测试结果", "baseline", "准确率", "召回率", "提升"}) &&
        (numeric || containsAny(lower, {"优于", "达到", "验证"}))) {
        return ClaimType::ResearchResult;
    }
    if (containsAny(lower, {"已部署", "已上线", "正式上线", "投入运行", "生产环境"})) {
        return ClaimType::Deployment;
    }
    if (containsAny(lower, {"社会影响", "受益人数", "服务对象", "覆盖人群"}) &&
        (numeric || containsAny(lower, {"覆盖", "服务", "受益"}))) {
        return ClaimType::SocialImpact;
    }
    if (containsAny(lower, {"技术创新", "技术领先", "性能提升", "识别准确率", "核心技术"}) &&
        (numeric || containsAny(lower, {"实现", "达到", "领先", "自主研发"}))) {
        return ClaimType::TechnicalCapability;
    }
    if (util::contains(lower, "商业模式") &&
        containsAny(lower, {"：", ":", "为", "采用", "通过", "收入来源"})) {
        return ClaimType::BusinessModel;
    }
    return std::nullopt;
}

[[nodiscard]] std::string normalizedClaim(std::string text) {
    text = util::lowerAscii(util::trim(std::move(text)));
    std::string normalized;
    normalized.reserve(text.size());
    for (const auto raw : text) {
        const auto value = static_cast<unsigned char>(raw);
        if (value < 0x80U && (std::isspace(value) != 0 || std::ispunct(value) != 0)) {
            continue;
        }
        normalized.push_back(raw);
    }
    return normalized;
}

[[nodiscard]] bool trustedDocument(const TextDocument& document,
                                   const ProjectInventory* inventory) {
    if (!isReliableTextDocument(document) || isExcludedTruthPath(document.sourceFile)) {
        return false;
    }
    if (inventory == nullptr || inventory->assets.empty()) {
        return true;
    }
    const auto* asset = findAsset(*inventory, document.sourceFile);
    return asset != nullptr && isTrustedClaimSource(*asset);
}

[[nodiscard]] double sourceConfidence(const TextDocument& document,
                                      const ProjectInventory* inventory) {
    if (inventory == nullptr) {
        return 0.76;
    }
    const auto* asset = findAsset(*inventory, document.sourceFile);
    if (asset == nullptr) {
        return 0.70;
    }
    if (asset->role == AssetRole::ProjectDeclaration || asset->role == AssetRole::BusinessPlan) {
        return 0.84;
    }
    if (asset->role == AssetRole::ResearchPaper) {
        return 0.82;
    }
    return 0.76;
}

[[nodiscard]] std::vector<ProjectClaim>
extractClaims(const std::vector<TextDocument>& corpus, const ProjectInventory* inventory) {
    constexpr std::size_t kMaxClaims = 120U;
    std::vector<ProjectClaim> claims;
    std::set<std::string> seen;

    for (const auto& document : corpus) {
        if (!trustedDocument(document, inventory)) {
            continue;
        }
        for (const auto& rawLine : util::splitLines(document.text)) {
            const auto line = util::trim(rawLine);
            if (line.size() < 6U || line.size() > 360U) {
                continue;
            }
            const auto type = classify(line);
            if (!type.has_value()) {
                continue;
            }
            const auto key = std::to_string(static_cast<int>(*type)) + ":" + normalizedClaim(line);
            if (key.size() < 8U || !seen.insert(key).second) {
                continue;
            }

            ProjectClaim claim;
            std::ostringstream id;
            id << "CLM-" << std::setw(3) << std::setfill('0') << claims.size() + 1U;
            claim.claimId = id.str();
            claim.claimText = line;
            claim.claimType = *type;
            claim.sourceFile = document.sourceFile;
            claim.confidence = sourceConfidence(document, inventory);
            claim.initialRisk = "该声明需要内容相关且可独立复核的证据。";
            claims.push_back(std::move(claim));
            if (claims.size() >= kMaxClaims) {
                return claims;
            }
        }
    }
    return claims;
}

} // namespace

std::vector<ProjectClaim> ClaimExtractor::extract(const std::vector<TextDocument>& corpus) const {
    return extractClaims(corpus, nullptr);
}

std::vector<ProjectClaim> ClaimExtractor::extract(const std::vector<TextDocument>& corpus,
                                                  const ProjectInventory& inventory) const {
    return extractClaims(corpus, &inventory);
}

} // namespace cc
