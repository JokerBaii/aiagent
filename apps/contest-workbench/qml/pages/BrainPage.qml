pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: root
    required property var compiler

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
                        placeholderText: "https://api.openai.com/v1/chat/completions"
                        onTextEdited: root.compiler.llmEndpoint = text
                    }

                    Text { text: "模型名称"; color: Theme.textSecondary; font.pixelSize: Theme.fontSm; font.bold: true }
                    FieldInput {
                        Layout.fillWidth: true
                        text: root.compiler.llmModel
                        placeholderText: "gpt-4o-mini"
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

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: authRow.implicitHeight + 20
                        radius: Theme.radiusSm
                        color: root.compiler.llmApproved ? Theme.successSoft : Theme.warningSoft
                        RowLayout {
                            id: authRow
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 8
                            CheckBox {
                                Accessible.name: "允许本次联网并调用大模型"
                                checked: root.compiler.llmApproved
                                onToggled: root.compiler.llmApproved = checked
                            }
                            Text {
                                Layout.fillWidth: true
                                text: "允许本次联网使用智能辅助服务。原始项目不会被修改，最终分数仍以本地规则检查为准。"
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontSm
                                wrapMode: Text.WordWrap
                            }
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
                        text: root.compiler.llmApproved ? "开始智能审计" : "运行本地规则审计"
                        onClicked: root.compiler.runBrainTask(brainTask.text)
                    }

                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }

                    Text {
                        text: "混合研判"
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSm
                        font.bold: true
                    }
                    Text {
                        Layout.fillWidth: true
                        text: "LLM 先基于审计结果给出风险判断和评分建议，确定性规则和证据逐条校验；冲突项降级并标注，最终评分仍以规则引擎为准。"
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontXs
                        wrapMode: Text.WordWrap
                    }
                    PrimaryButton {
                        Layout.fillWidth: true
                        enabled: !root.compiler.advisoryRunning
                        text: root.compiler.advisoryRunning ? "研判中…" : "运行混合研判"
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
                title: "混合研判结论"
                subtitle: root.compiler.advisory.available ? root.compiler.advisory.summary : "运行后显示 LLM 研判与规则校验对齐结果"
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
                            text: "确定性评分 " + root.compiler.advisory.finalScore
                            bg: Theme.accentSoft
                            fg: Theme.accentActive
                        }
                        Pill {
                            visible: root.compiler.advisory.suggestedScore > 0
                            text: "LLM 建议 " + root.compiler.advisory.suggestedScore
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

            SectionTitle { title: "运行结果" }
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

            SectionTitle { title: "Trace" }
            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
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
