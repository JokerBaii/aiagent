#pragma once

#include "cc/core/AuditModels.hpp"
#include "cc/core/Result.hpp"
#include "cc/loader/ImportLimits.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace cc {

struct WorkspaceChange {
    std::filesystem::path relativePath;
    std::string kind;
    std::string diff;
};

struct WorkspaceEditResult {
    std::filesystem::path repairedRoot;
    std::filesystem::path relativePath;
    std::filesystem::path patchFile;
    std::string diff;
    std::string preview;
};

/** 在隔离的 repaired project 中执行可复核文本修改。 */
class WorkspaceEditor {
  public:
    explicit WorkspaceEditor(ImportLimits limits = {});

    [[nodiscard]] Result<std::filesystem::path>
    prepare(const std::filesystem::path& projectRoot,
            const std::filesystem::path& workspaceRoot) const;

    [[nodiscard]] Result<WorkspaceEditResult>
    applyTextEdit(const std::filesystem::path& projectRoot,
                  const std::filesystem::path& workspaceRoot,
                  const std::filesystem::path& relativePath, const std::string& expectedText,
                  const std::string& replacementText, std::size_t expectedOccurrences = 1U) const;

    [[nodiscard]] Result<WorkspaceEditResult>
    createTextFile(const std::filesystem::path& projectRoot,
                   const std::filesystem::path& workspaceRoot,
                   const std::filesystem::path& relativePath, const std::string& content) const;

    [[nodiscard]] Result<std::string> readTextFile(const std::filesystem::path& projectRoot,
                                                   const std::filesystem::path& workspaceRoot,
                                                   const std::filesystem::path& relativePath,
                                                   std::size_t maxBytes = 64U * 1024U) const;

    [[nodiscard]] Result<std::vector<WorkspaceChange>>
    changes(const std::filesystem::path& projectRoot,
            const std::filesystem::path& workspaceRoot) const;

    [[nodiscard]] Result<AuditResult>
    reAudit(const std::filesystem::path& projectRoot, const std::filesystem::path& workspaceRoot,
            const AuditOptions& options, const ProjectContext* baselineContext = nullptr) const;

  private:
    ImportLimits limits_;
};

} // namespace cc
