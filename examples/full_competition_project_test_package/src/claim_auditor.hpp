#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct ClaimSummary {
    int userRows{0};
    int activeRows{0};
    std::vector<std::string> notes;
};

ClaimSummary summarizeSurvey(const std::filesystem::path& csvPath);
