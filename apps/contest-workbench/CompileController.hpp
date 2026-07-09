/**
 * @file CompileController.hpp
 * @brief QML 与 C++ Core 的桥接控制器。
 */

#pragma once

#include "WorkbenchSessionModels.hpp"
#include "cc/agent/AgentModels.hpp"
#include "cc/agent/StagedAuditPipeline.hpp"
#include "cc/core/Result.hpp"
#include "cc/llm/AuditAdvisory.hpp"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <memory>
#include <optional>
#include <vector>

class CompileController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString projectPath READ projectPath WRITE setProjectPath NOTIFY projectPathChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int trustScore READ trustScore NOTIFY resultChanged)
    Q_PROPERTY(int blockerCount READ blockerCount NOTIFY resultChanged)
    Q_PROPERTY(int warningCount READ warningCount NOTIFY resultChanged)
    Q_PROPERTY(QString summary READ summary NOTIFY resultChanged)
    Q_PROPERTY(QVariantList assets READ assets NOTIFY resultChanged)
    Q_PROPERTY(QVariantList roleDistribution READ roleDistribution NOTIFY resultChanged)
    Q_PROPERTY(QVariantMap cpir READ cpir NOTIFY resultChanged)
    Q_PROPERTY(QVariantList claimEvidence READ claimEvidence NOTIFY resultChanged)
    Q_PROPERTY(QVariantList consistencyIssues READ consistencyIssues NOTIFY resultChanged)
    Q_PROPERTY(QVariantList findings READ findings NOTIFY resultChanged)
    Q_PROPERTY(QVariantList fixTasks READ fixTasks NOTIFY resultChanged)
    Q_PROPERTY(QVariantList scorePenalties READ scorePenalties NOTIFY resultChanged)
    Q_PROPERTY(QVariantMap projectContext READ projectContext NOTIFY workspaceChanged)
    Q_PROPERTY(QVariantList sessionHistory READ sessionHistory NOTIFY sessionChanged)
    Q_PROPERTY(QVariantList toolCards READ toolCards NOTIFY workspaceChanged)
    Q_PROPERTY(QVariantList permissionCards READ permissionCards NOTIFY workspaceChanged)
    Q_PROPERTY(QVariantList artifacts READ artifacts NOTIFY workspaceChanged)
    Q_PROPERTY(QString agentSummary READ agentSummary NOTIFY workspaceChanged)
    Q_PROPERTY(bool agentRunning READ agentRunning NOTIFY agentStateChanged)
    Q_PROPERTY(int agentProgress READ agentProgress NOTIFY agentStateChanged)
    Q_PROPERTY(QString currentAgentAction READ currentAgentAction NOTIFY agentStateChanged)
    Q_PROPERTY(QString oldAuditPath READ oldAuditPath WRITE setOldAuditPath NOTIFY diffInputChanged)
    Q_PROPERTY(QString newAuditPath READ newAuditPath WRITE setNewAuditPath NOTIFY diffInputChanged)
    Q_PROPERTY(QVariantMap auditDiff READ auditDiff NOTIFY auditDiffChanged)
    Q_PROPERTY(QString llmApiKey READ llmApiKey WRITE setLlmApiKey NOTIFY llmConfigChanged)
    Q_PROPERTY(QString llmEndpoint READ llmEndpoint WRITE setLlmEndpoint NOTIFY llmConfigChanged)
    Q_PROPERTY(QString llmModel READ llmModel WRITE setLlmModel NOTIFY llmConfigChanged)
    Q_PROPERTY(bool llmApproved READ llmApproved WRITE setLlmApproved NOTIFY llmConfigChanged)
    Q_PROPERTY(QString agentResult READ agentResult NOTIFY agentResultChanged)
    Q_PROPERTY(QString agentTrace READ agentTrace NOTIFY agentTraceChanged)
    Q_PROPERTY(QString accessMode READ accessMode WRITE setAccessMode NOTIFY accessModeChanged)
    Q_PROPERTY(QVariantList sessionList READ sessionList NOTIFY sessionChanged)
    Q_PROPERTY(QVariantMap advisory READ advisory NOTIFY advisoryChanged)
    Q_PROPERTY(bool advisoryRunning READ advisoryRunning NOTIFY advisoryChanged)
    Q_PROPERTY(QVariantMap selectedFilePreview READ selectedFilePreview
                   NOTIFY selectedFilePreviewChanged)

  public:
    explicit CompileController(QObject* parent = nullptr);

    [[nodiscard]] QString projectPath() const;
    void setProjectPath(const QString& value);
    [[nodiscard]] QString status() const;
    [[nodiscard]] int trustScore() const;
    [[nodiscard]] int blockerCount() const;
    [[nodiscard]] int warningCount() const;
    [[nodiscard]] QString summary() const;
    [[nodiscard]] QVariantList assets() const;
    [[nodiscard]] QVariantList roleDistribution() const;
    [[nodiscard]] QVariantMap cpir() const;
    [[nodiscard]] QVariantList claimEvidence() const;
    [[nodiscard]] QVariantList consistencyIssues() const;
    [[nodiscard]] QVariantList findings() const;
    [[nodiscard]] QVariantList fixTasks() const;
    [[nodiscard]] QVariantList scorePenalties() const;
    [[nodiscard]] QVariantMap projectContext() const;
    [[nodiscard]] QVariantList sessionHistory() const;
    [[nodiscard]] QVariantList toolCards() const;
    [[nodiscard]] QVariantList permissionCards() const;
    [[nodiscard]] QVariantList artifacts() const;
    [[nodiscard]] QString agentSummary() const;
    [[nodiscard]] bool agentRunning() const;
    [[nodiscard]] int agentProgress() const;
    [[nodiscard]] QString currentAgentAction() const;
    [[nodiscard]] QString oldAuditPath() const;
    void setOldAuditPath(const QString& value);
    [[nodiscard]] QString newAuditPath() const;
    void setNewAuditPath(const QString& value);
    [[nodiscard]] QVariantMap auditDiff() const;
    [[nodiscard]] QString llmApiKey() const;
    void setLlmApiKey(const QString& value);
    [[nodiscard]] QString llmEndpoint() const;
    void setLlmEndpoint(const QString& value);
    [[nodiscard]] QString llmModel() const;
    void setLlmModel(const QString& value);
    [[nodiscard]] bool llmApproved() const;
    void setLlmApproved(bool value);
    [[nodiscard]] QString agentResult() const;
    [[nodiscard]] QString agentTrace() const;
    [[nodiscard]] QString accessMode() const;
    void setAccessMode(const QString& value);
    [[nodiscard]] QVariantList sessionList() const;
    [[nodiscard]] QVariantMap advisory() const;
    [[nodiscard]] bool advisoryRunning() const;
    [[nodiscard]] QVariantMap selectedFilePreview() const;

    Q_INVOKABLE void runAudit();
    Q_INVOKABLE void runDiff();
    Q_INVOKABLE void runBrainTask(const QString& goal);
    Q_INVOKABLE void exportMarkdown(const QString& outputPath);
    Q_INVOKABLE void exportJson(const QString& outputPath);
    Q_INVOKABLE void submitMessage(const QString& message);
    /** @brief 接收原生文件/目录选择器返回的 URL 或本地路径。 */
    Q_INVOKABLE void selectProject(const QString& urlOrPath);
    /** @brief 开始一个新的会话，清空当前对话与结果。 */
    Q_INVOKABLE void newSession();
    /** @brief 运行混合研判：LLM 先判断、确定性规则校验（需授权 LLM 且已有审计结果）。 */
    Q_INVOKABLE void runAdvisory();
    Q_INVOKABLE void rewindLastTurn();
    Q_INVOKABLE void approvePendingPlan();
    Q_INVOKABLE void previewProjectFile(const QString& relativePath);
    Q_INVOKABLE void clearSelectedFilePreview();

  signals:
    void projectPathChanged();
    void statusChanged();
    void resultChanged();
    void workspaceChanged();
    void sessionChanged();
    void diffInputChanged();
    void auditDiffChanged();
    void llmConfigChanged();
    void agentResultChanged();
    void agentTraceChanged();
    void agentStateChanged();
    void accessModeChanged();
    void advisoryChanged();
    void selectedFilePreviewChanged();

  private:
    void advanceAuditRun();
    void finishAuditRun();
    void runInlineAdvisory();
    void runGeneralAssistant(const QString& message, const QString& context);
    void runOptimization();
    void previewAgentPlan(const QString& message, const QString& context);
    void startDeferredAgentConversation(const QString& message, const QString& context,
                                        bool appendUserMessage);
    void finishDeferredAgentConversation();
    void flushQueuedComposerMessage();
    void runGeneralAssistant(const QString& message, const QString& context,
                             bool appendUserMessage);
    void previewAgentPlan(const QString& message, const QString& context, bool appendUserMessage);
    void runAgentConversation(const QString& message, const QString& context,
                              bool appendUserMessage = true);
    void appendAgentEvent(const cc::AgentEvent& event);
    void applyAgentRunResult(cc::Result<cc::AgentRunResult> run, const QString& planner,
                             bool appendEvents = true);
    [[nodiscard]] QString sessionStatusText() const;
    [[nodiscard]] QString compactedContextText() const;
    [[nodiscard]] QString accessModeLabel() const;
    [[nodiscard]] cc::AgentRunRequest makeAgentRequest(const QString& goal) const;

    QString projectPath_;
    QString oldAuditPath_;
    QString newAuditPath_;
    QString llmApiKey_;
    QString llmEndpoint_{"https://api.deepseek.com/chat/completions"};
    QString llmModel_{"deepseek-v4-flash"};
    QString llmProvider_{"deepseek"};
    QString llmApiKeyHeader_{"Authorization"};
    QString llmApiKeyPrefix_{"Bearer "};
    QString accessMode_{"ask"};
    bool llmApproved_{true};
    bool agentRunning_{false};
    bool brainWorkerRunning_{false};
    int activeAuditStep_{-1};
    int completedAuditSteps_{0};
    QString currentAgentAction_;
    QString pendingPlanGoal_;
    QVariantMap selectedFilePreview_;
    QTimer auditTimer_;
    std::unique_ptr<cc::StagedAuditPipeline> auditRun_;
    QString agentResult_;
    QString agentTrace_;
    QString status_{"等待导入项目"};
    std::optional<cc::AuditResult> result_;
    std::optional<cc::AuditDiff> auditDiff_;
    std::optional<cc::ReconciledAdvisory> advisory_;
    bool advisoryRunning_{false};
    struct PendingComposerMessage {
        QString message;
        QString context;
    };
    std::vector<PendingComposerMessage> pendingComposerMessages_;
    std::vector<workbench::SessionMessage> conversation_;
};
