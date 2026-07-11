/**
 * @file TestMain.cpp
 * @brief 模块测试入口。
 */

#include "TestSupport.hpp"

#include <cstdlib>

namespace {

class TestWorkspace final {
  public:
    TestWorkspace()
        : root_{std::filesystem::temp_directory_path() / "contest-project-trust-tests"} {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
        std::filesystem::create_directories(root_, error);
        if (error) {
            throw std::runtime_error("cannot create isolated test workspace: " + error.message());
        }
#ifdef _WIN32
        if (_putenv_s("CONTEST_WORKSPACE_ROOT", root_.string().c_str()) != 0) {
            throw std::runtime_error("cannot configure isolated test workspace");
        }
#else
        if (setenv("CONTEST_WORKSPACE_ROOT", root_.string().c_str(), 1) != 0) {
            throw std::runtime_error("cannot configure isolated test workspace");
        }
#endif
    }

    ~TestWorkspace() {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
    }

    TestWorkspace(const TestWorkspace&) = delete;
    TestWorkspace& operator=(const TestWorkspace&) = delete;

  private:
    std::filesystem::path root_;
};

} // namespace

int main() {
    try {
        const TestWorkspace workspace;
        runCoreTests();
        runLoaderTests();
        runInventoryTests();
        runTextTests();
        runCpirTests();
        runClaimTests();
        runEvidenceTests();
        runConsistencyTests();
        runRulesTests();
        runRuleConditionEvaluatorTests();
        runAuditTests();
        runRepairTests();
        runReportTests();
        runAgentTests();
        runWorkspaceEditorTests();
        runLlmTests();
        runLlmProviderProfileTests();
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILED: " << error.what() << '\n';
        return 1;
    }
    std::cout << "All tests passed\n";
    return 0;
}
