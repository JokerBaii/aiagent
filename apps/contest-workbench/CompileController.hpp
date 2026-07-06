/**
 * @file CompileController.hpp
 * @brief QML 与 C++ Core 的桥接控制器。
 */

#pragma once

#include "WorkbenchSessionModels.hpp"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

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
    Q_PROPERTY(QString advisorSummary READ advisorSummary NOTIFY workspaceChanged)
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
    Q_PROPERTY(QString llmAdvice READ llmAdvice NOTIFY llmAdviceChanged)

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
    [[nodiscard]] QString advisorSummary() const;
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
    [[nodiscard]] QString llmAdvice() const;

    Q_INVOKABLE void runAudit();
    Q_INVOKABLE void runDiff();
    Q_INVOKABLE void runLlmAdvice();
    Q_INVOKABLE void exportMarkdown(const QString& outputPath);
    Q_INVOKABLE void exportJson(const QString& outputPath);
    Q_INVOKABLE void submitMessage(const QString& message);

  signals:
    void projectPathChanged();
    void statusChanged();
    void resultChanged();
    void workspaceChanged();
    void sessionChanged();
    void diffInputChanged();
    void auditDiffChanged();
    void llmConfigChanged();
    void llmAdviceChanged();
    void agentStateChanged();

  private:
    void advanceAuditRun();
    void finishAuditRun();
    [[nodiscard]] QString advisorReply(const QString& message) const;

    QString projectPath_;
    QString oldAuditPath_;
    QString newAuditPath_;
    QString llmApiKey_;
    QString llmEndpoint_{"https://api.openai.com/v1/chat/completions"};
    QString llmModel_{"gpt-4o-mini"};
    bool llmApproved_{false};
    bool agentRunning_{false};
    int activeAuditStep_{-1};
    int completedAuditSteps_{0};
    QString currentAgentAction_;
    QTimer auditTimer_;
    QString llmAdvice_;
    QString status_{"等待导入项目"};
    std::optional<cc::AuditResult> result_;
    std::optional<cc::AuditDiff> auditDiff_;
    std::vector<workbench::SessionMessage> conversation_;
};
