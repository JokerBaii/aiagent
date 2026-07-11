#include "claim_auditor.hpp"

#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    const std::filesystem::path csvPath =
        argc > 1 ? std::filesystem::path(argv[1])
                 : std::filesystem::path("data/05_用户问卷访谈记录.csv");
    const auto summary = summarizeSurvey(csvPath);
    std::cout << "survey_rows=" << summary.userRows << "\n";
    std::cout << "traction_rows=" << summary.activeRows << "\n";
    for (const auto& note : summary.notes) {
        std::cout << "note=" << note << "\n";
    }
    return summary.userRows > 0 ? 0 : 1;
}
