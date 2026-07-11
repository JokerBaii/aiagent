#include "claim_auditor.hpp"

#include <fstream>
#include <sstream>

namespace {

bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

} // namespace

ClaimSummary summarizeSurvey(const std::filesystem::path& csvPath) {
    ClaimSummary summary;
    std::ifstream input(csvPath);
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || contains(line, "sample_id")) {
            continue;
        }
        ++summary.userRows;
        if (contains(line, "已有用户") || contains(line, "登记注册用户")) {
            ++summary.activeRows;
            summary.notes.push_back(line);
        }
    }
    return summary;
}
