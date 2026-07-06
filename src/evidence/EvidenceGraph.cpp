/**
 * @file EvidenceGraph.cpp
 * @brief 声明与证据关系图摘要实现。
 */

#include "cc/evidence/EvidenceGraph.hpp"

#include <algorithm>
#include <utility>

namespace cc {

EvidenceGraph::EvidenceGraph(std::vector<EvidenceMatch> matches) : matches_(std::move(matches)) {}

const std::vector<EvidenceMatch>& EvidenceGraph::matches() const {
    return matches_;
}

std::optional<EvidenceMatch> EvidenceGraph::findByClaimId(const std::string& claimId) const {
    const auto iter =
        std::find_if(matches_.begin(), matches_.end(),
                     [&](const EvidenceMatch& match) { return match.claimId == claimId; });
    if (iter == matches_.end()) {
        return std::nullopt;
    }
    return *iter;
}

double EvidenceGraph::coveragePercent() const {
    if (matches_.empty()) {
        return 0.0;
    }
    double covered = 0.0;
    for (const auto& match : matches_) {
        if (match.status == EvidenceStatus::Supported) {
            covered += 1.0;
        } else if (match.status == EvidenceStatus::Partial) {
            covered += 0.5;
        }
    }
    return covered * 100.0 / static_cast<double>(matches_.size());
}

} // namespace cc
