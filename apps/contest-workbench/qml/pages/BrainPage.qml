pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: root
    required property var compiler
    property bool showTrace: false

    RowLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 20

        ColumnLayout {
            Layout.preferredWidth: 380
            Layout.fillHeight: true
            spacing: 12

            SectionTitle {
                title: "智能辅助检查（可选）"
                subtitle: "本地规则检查无需配置；这里只用于补充解释和阅读建议"
            }

            Card {
                Layout.fillWidth: true
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

                    Text { text: "服务地址"; color: Theme.textSecondary; font.pixelSize: Theme.fontSm; font.bold: true }
                    FieldInput {
                        Layout.fillWidth: true
                        text: root.compiler.llmEndpoint
                        placeholderText: "https://服务地址/v1/chat/completions 或 /v1/messages"
                        onTextEdited: root.compiler.llmEndpoint = text
                    }

                    Text { text: "模型名称"; color: Theme.textSecondary; font.pixelSize: Theme.fontSm; font.bold: true }
                    FieldInput {
                        Layout.fillWidth: true
                        text: root.compiler.llmModel
                        placeholderText: "输入该服务支持的模型 ID"
                        onTextEdited: root.compiler.llmModel = text
                    }

                    Text { text: "访问密钥"; color: Theme.textSecondary; font.pixelSize: Theme.fontSm; font.bold: true }
                    FieldInput {
                        Layout.fillWidth: true
                        echoMode: TextInput.Password
                        text: root.compiler.llmApiKey
                        placeholderText: "仅保存在本次运行内，不会写入报告"
                        onActiveFocusChanged: if (activeFocus && text === "********") text = ""
                        onTextEdited: root.compiler.llmApiKey = text
                    }

                    PrimaryButton {
                        Layout.fillWidth: true
                        enabled: !root.compiler.llmModelsLoading
                                 && !root.compiler.agentRunning
                                 && !root.compiler.advisoryRunning
                        text: root.compiler.llmModelsLoading ? "正在获取可用模型…" : "按当前凭证获取模型"
                        onClicked: root.compiler.refreshLlmModels()
                    }
                    ComboBox {
                        Layout.fillWidth: true
                        visible: root.compiler.llmAvailableModels.length > 0
                        model: root.compiler.llmAvailableModels
                        currentIndex: root.compiler.llmAvailableModels.indexOf(root.compiler.llmModel)
                        onActivated: root.compiler.llmModel = currentText
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: root.compiler.llmModelsStatus.length > 0
                        text: root.compiler.llmModelsStatus
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontXs
                        wrapMode: Text.WordWrap
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: automaticConnectionText.implicitHeight + 20
                        radius: Theme.radiusSm
                        color: root.compiler.llmConfigured ? Theme.successSoft : Theme.warningSoft
                        Text {
                            id: automaticConnectionText
                            anchors.fill: parent
                            anchors.margins: 10
                            text: root.compiler.llmConfigured
                                  ? "配置有效，后续项目任务默认联网使用该模型。原始项目不会被修改，最终分数仍以本地规则检查为准。"
                                  : "配置完整且有效后自动启用联网模型；未配置时使用本地规则检查。"
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSm
                            wrapMode: Text.WordWrap
                        }
                    }

                    Text { text: "希望它帮你做什么"; color: Theme.textSecondary; font.pixelSize: Theme.fontSm; font.bold: true }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 116
                        radius: Theme.radiusSm
                        color: Theme.surface
                        border.color: brainTask.activeFocus ? Theme.accent : Theme.borderStrong
                        border.width: brainTask.activeFocus ? 2 : 1
                        ScrollView {
                            anchors.fill: parent
                            anchors.margins: 8
                            clip: true
                            TextArea {
                                id: brainTask
                                text: "请自动翻阅当前项目，检查 Markdown 文档并生成工作区修订稿。"
                                wrapMode: TextArea.Wrap
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontMd
                                background: Rectangle { color: "transparent" }
                            }
                        }
                    }

                    PrimaryButton {
                        Layout.fillWidth: true
                        enabled: !root.compiler.agentRunning && !root.compiler.advisoryRunning
                        text: root.compiler.llmConfigured ? "开始智能检查" : "运行本地规则检查"
                        onClicked: root.compiler.runBrainTask(brainTask.text)
                    }

                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }

                    Text {
                        text: "再听一个模型意见（可选）"
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSm
                        font.bold: true
                    }
                    Text {
                        Layout.fillWidth: true
                        text: "模型会再看一遍现有结果，但最终分数仍以本地规则和证明材料为准。"
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontXs
                        wrapMode: Text.WordWrap
                    }
                    PrimaryButton {
                        Layout.fillWidth: true
                        enabled: root.compiler.hasAuditResult && root.compiler.llmConfigured
                                 && !root.compiler.advisoryRunning && !root.compiler.agentRunning
                        text: root.compiler.advisoryRunning ? "正在复核…"
                              : !root.compiler.hasAuditResult ? "先完成项目检查"
                              : !root.compiler.llmConfigured ? "先配置模型"
                              : "让模型再复核一次"
                        onClicked: root.compiler.runAdvisory()
                    }
                }
            }
            Item { Layout.fillHeight: true }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            SectionTitle {
                title: "辅助判断"
                subtitle: root.compiler.advisory.available ? root.compiler.advisory.summary : "需要时可以让模型给出第二意见"
            }
            Card {
                Layout.fillWidth: true
                visible: root.compiler.advisory.available
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Pill {
                            text: "最终 " + root.compiler.advisory.finalScore + " 分"
                            bg: Theme.accentSoft
                            fg: Theme.accentActive
                        }
                        Pill {
                            visible: root.compiler.advisory.suggestedScore > 0
                            text: "模型建议 " + root.compiler.advisory.suggestedScore + " 分"
                            bg: Theme.surfaceMuted
                            fg: Theme.textSecondary
                        }
                        Pill {
                            text: "印证 " + root.compiler.advisory.confirmedCount
                            bg: Theme.successSoft
                            fg: Theme.success
                        }
                        Pill {
                            visible: root.compiler.advisory.conflictingCount > 0
                            text: "冲突 " + root.compiler.advisory.conflictingCount
                            bg: Theme.dangerSoft
                            fg: Theme.danger
                        }
                        Item { Layout.fillWidth: true }
                    }
                    Repeater {
                        model: root.compiler.advisory.items
                        delegate: Rectangle {
                            id: advisoryDelegate
                            required property var modelData

                            Layout.fillWidth: true
                            implicitHeight: advCol.implicitHeight + 18
                            radius: Theme.radiusSm
                            color: advisoryDelegate.modelData.verdict === "conflicting" ? Theme.dangerSoft
                                 : advisoryDelegate.modelData.verdict === "confirmed" ? Theme.successSoft
                                 : Theme.surfaceMuted
                            ColumnLayout {
                                id: advCol
                                x: 12; y: 9
                                width: parent.width - 24
                                spacing: 3
                                RowLayout {
                                    Layout.fillWidth: true
                                    Text {
                                        text: advisoryDelegate.modelData.title
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.fontMd
                                        font.bold: true
                                    }
                                    Item { Layout.fillWidth: true }
                                    Text {
                                        text: advisoryDelegate.modelData.verdict === "confirmed" ? "已印证"
                                            : advisoryDelegate.modelData.verdict === "conflicting" ? "与规则冲突"
                                            : "待核实"
                                        color: advisoryDelegate.modelData.verdict === "confirmed" ? Theme.success
                                             : advisoryDelegate.modelData.verdict === "conflicting" ? Theme.danger
                                             : Theme.textMuted
                                        font.pixelSize: Theme.fontXs
                                        font.bold: true
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: advisoryDelegate.modelData.reconciliation
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontSm
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }
            }

            SectionTitle { title: "智能助手的回答" }
            Card {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(180, root.height * 0.28)
                padding: 4
                ScrollView {
                    anchors.fill: parent
                    clip: true
                    TextArea {
                        readOnly: true
                        wrapMode: TextArea.Wrap
                        text: root.compiler.agentResult
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontMd
                        placeholderText: "运行后显示计划摘要、工具观察和下一步。"
                        background: Rectangle { color: "transparent" }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 38
                radius: Theme.radiusSm
                color: traceToggle.containsMouse ? Theme.surfaceHover : Theme.surfaceMuted
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.showTrace ? "收起技术记录" : "查看技术记录"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSm
                    font.weight: Font.DemiBold
                }
                Icon {
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    name: root.showTrace ? "chevronDown" : "chevronRight"
                    size: 12
                    color: Theme.textMuted
                }
                ActionArea {
                    id: traceToggle
                    anchors.fill: parent
                    accessibleName: root.showTrace ? "收起技术记录" : "查看技术记录"
                    onClicked: root.showTrace = !root.showTrace
                }
            }
            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: root.showTrace
                padding: 4
                ScrollView {
                    anchors.fill: parent
                    clip: true
                    TextArea {
                        readOnly: true
                        wrapMode: TextArea.Wrap
                        text: root.compiler.agentTrace
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSm
                        font.family: "monospace"
                        placeholderText: "工具调用 JSON 轨迹"
                        background: Rectangle { color: "transparent" }
                    }
                }
            }
        }
    }
}
