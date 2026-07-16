/**
 * @file CompileController.hpp
 * @brief QML 与 C++ Core 的桥接控制器。
 */

#pragma once

#include "WorkbenchSessionModels.hpp"
#include "cc/agent/AgentModels.hpp"
#include "cc/core/Result.hpp"
#include "cc/llm/LlmTypes.hpp"

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <atomic>
#include <memory>
#include <optional>
#include <vector>

class CompileController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_ADDED_IN_VERSION(1, 0)
    Q_PROPERTY(QString projectPath READ projectPath WRITE setProjectPath NOTIFY projectPathChanged)
    Q_PROPERTY(QString projectDirectory READ projectDirectory NOTIFY projectPathChanged)
    Q_PROPERTY(QUrl projectDirectoryUrl READ projectDirectoryUrl NOTIFY projectPathChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool hasAuditResult READ hasAuditResult NOTIFY resultChanged)
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
    Q_PROPERTY(QVariantMap repairWorkspace READ repairWorkspace NOTIFY resultChanged)
    Q_PROPERTY(QString llmApiKey READ llmApiKey WRITE setLlmApiKey NOTIFY llmConfigChanged)
    Q_PROPERTY(QString llmEndpoint READ llmEndpoint WRITE setLlmEndpoint NOTIFY llmConfigChanged)
    Q_PROPERTY(QString llmModel READ llmModel WRITE setLlmModel NOTIFY llmConfigChanged)
    Q_PROPERTY(bool llmConfigured READ llmConfigured NOTIFY llmConfigChanged)
    Q_PROPERTY(QVariantList llmAvailableModels READ llmAvailableModels NOTIFY llmModelsChanged)
    Q_PROPERTY(bool llmModelsLoading READ llmModelsLoading NOTIFY llmModelsChanged)
    Q_PROPERTY(QString llmModelsStatus READ llmModelsStatus NOTIFY llmModelsChanged)
    Q_PROPERTY(QString agentResult READ agentResult NOTIFY agentResultChanged)
    Q_PROPERTY(QString agentTrace READ agentTrace NOTIFY agentTraceChanged)
    Q_PROPERTY(QString accessMode READ accessMode WRITE setAccessMode NOTIFY accessModeChanged)
    Q_PROPERTY(QVariantList sessionList READ sessionList NOTIFY sessionChanged)
    Q_PROPERTY(
        QVariantMap selectedFilePreview READ selectedFilePreview NOTIFY selectedFilePreviewChanged)

  public:
    explicit CompileController(QObject* parent = nullptr);

    [[nodiscard]] QString projectPath() const;
    void setProjectPath(const QString& value);
    /** @brief 用户导入目录；单文件或压缩包使用其所在目录。 */
    [[nodiscard]] QString projectDirectory() const;
    [[nodiscard]] QUrl projectDirectoryUrl() const;
    [[nodiscard]] QString status() const;
    [[nodiscard]] bool hasAuditResult() const;
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
    [[nodiscard]] QVariantMap repairWorkspace() const;
    [[nodiscard]] QString llmApiKey() const;
    void setLlmApiKey(const QString& value);
    [[nodiscard]] QString llmEndpoint() const;
    void setLlmEndpoint(const QString& value);
    [[nodiscard]] QString llmModel() const;
    void setLlmModel(const QString& value);
    [[nodiscard]] bool llmConfigured() const;
    [[nodiscard]] QVariantList llmAvailableModels() const;
    [[nodiscard]] bool llmModelsLoading() const;
    [[nodiscard]] QString llmModelsStatus() const;
    [[nodiscard]] QString agentResult() const;
    [[nodiscard]] QString agentTrace() const;
    [[nodiscard]] QString accessMode() const;
    void setAccessMode(const QString& value);
    [[nodiscard]] QVariantList sessionList() const;
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
    Q_INVOKABLE void activateSession(const QString& sessionId);
    Q_INVOKABLE void rewindLastTurn();
    Q_INVOKABLE void approvePendingPlan();
    Q_INVOKABLE void previewProjectFile(const QString& relativePath);
    Q_INVOKABLE void clearSelectedFilePreview();
    Q_INVOKABLE void cancelCurrentJob();
    /** @brief 使用当前 endpoint 与 key 读取 provider 的模型目录，不依赖本地模型白名单。 */
    Q_INVOKABLE void refreshLlmModels();

  signals:
    void projectPathChanged();
    void statusChanged();
    void resultChanged();
    void workspaceChanged();
    void sessionChanged();
    void diffInputChanged();
    void auditDiffChanged();
    void llmConfigChanged();
    void llmModelsChanged();
    void agentResultChanged();
    void agentTraceChanged();
    void agentStateChanged();
    void accessModeChanged();
    void selectedFilePreviewChanged();

  private:
    void applyAuditStage(std::size_t stageIndex, const cc::AgentObservation& observation);
    void completeAuditRun(cc::Result<cc::AuditResult> result);
    void exportReport(const QString& outputPath, bool jsonFormat);
    void previewAgentPlan(const QString& message, const QString& context);
    void startDeferredAgentConversation(const QString& message, const QString& context,
                                        bool appendUserMessage);
    void finishDeferredAgentConversation();
    void flushQueuedComposerMessage();
    void archiveCurrentSession();
    void resetActiveSession(bool addGreeting);
    void emitFullSessionState();
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
    [[nodiscard]] cc::AgentRunRequest makeAgentRequest(const QString& goal,
                                                       const QString& context = {}) const;
    [[nodiscard]] cc::LlmConfig llmConfig(bool allowNetwork, bool allowLlm) const;
    void refreshLlmConfig(bool invalidateModels);
    /** @brief 结果对象切换时丢弃 QML 展示缓存；单个模型在首次访问时按需生成。 */
    void syncResultModelCache() const;

    QString projectPath_;
    QString oldAuditPath_;
    QString newAuditPath_;
    QString llmApiKey_;
    QString llmEndpoint_;
    QString llmModel_;
    std::optional<cc::LlmConfig> llmCredentialConfig_;
    std::optional<cc::LlmConfig> resolvedLlmConfig_;
    QVariantList llmAvailableModels_;
    QString llmModelsStatus_;
    std::shared_ptr<std::atomic_bool> llmModelsCancellation_;
    QString accessMode_{"full"};
    bool llmConfigured_{false};
    bool llmModelsLoading_{false};
    bool agentRunning_{false};
    bool brainWorkerRunning_{false};
    int activeAuditStep_{-1};
    int completedAuditSteps_{0};
    QString currentAgentAction_;
    QString pendingPlanGoal_;
    QVariantMap selectedFilePreview_;
    QString agentResult_;
    QString agentTrace_;
    QString status_{"等待导入项目"};
    std::shared_ptr<cc::AuditResult> result_;
    std::shared_ptr<cc::AuditResult> baselineResult_;
    std::optional<cc::AuditDiff> auditDiff_;
    mutable std::weak_ptr<cc::AuditResult> modelCacheResult_;
    mutable std::optional<int> cachedBlockerCount_;
    mutable std::optional<int> cachedWarningCount_;
    mutable std::optional<QString> cachedSummary_;
    mutable std::optional<QVariantList> cachedAssets_;
    mutable std::optional<QVariantList> cachedRoleDistribution_;
    mutable std::optional<QVariantMap> cachedCpir_;
    mutable std::optional<QVariantList> cachedClaimEvidence_;
    mutable std::optional<QVariantList> cachedConsistencyIssues_;
    mutable std::optional<QVariantList> cachedFindings_;
    mutable std::optional<QVariantList> cachedFixTasks_;
    mutable std::optional<QVariantList> cachedScorePenalties_;
    mutable std::optional<QVariantMap> cachedRepairWorkspace_;
    struct PendingComposerMessage {
        QString message;
        QString context;
        QString accessMode;
        bool appendUserMessage{true};
    };
    std::vector<PendingComposerMessage> pendingComposerMessages_;
    std::vector<workbench::SessionMessage> conversation_;

    struct SavedSession {
        QString id;
        QString projectPath;
        QString oldAuditPath;
        QString newAuditPath;
        QString status;
        QString accessMode;
        QString agentResult;
        QString agentTrace;
        QString pendingPlanGoal;
        QString compactedContext;
        QVariantMap selectedFilePreview;
        std::shared_ptr<cc::AuditResult> result;
        std::shared_ptr<cc::AuditResult> baselineResult;
        std::optional<cc::AuditDiff> auditDiff;
        std::vector<workbench::SessionMessage> conversation;
    };
    QString activeSessionId_;
    QString compactedContext_;
    std::vector<SavedSession> savedSessions_;
    std::shared_ptr<std::atomic_bool> activeCancellation_;
};
