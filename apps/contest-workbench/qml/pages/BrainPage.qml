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
                title: "Agent Brain"
                subtitle: "模型逐步选择工具，本地运行时执行"
            }

            Card {
                Layout.fillWidth: true
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

                    Text { text: "Endpoint"; color: Theme.textSecondary; font.pixelSize: 12; font.bold: true }
                    FieldInput {
                        Layout.fillWidth: true
                        text: root.compiler.llmEndpoint
                        placeholderText: "https://api.openai.com/v1/chat/completions"
                        onTextEdited: root.compiler.llmEndpoint = text
                    }

                    Text { text: "Model"; color: Theme.textSecondary; font.pixelSize: 12; font.bold: true }
                    FieldInput {
                        Layout.fillWidth: true
                        text: root.compiler.llmModel
                        placeholderText: "gpt-4o-mini"
                        onTextEdited: root.compiler.llmModel = text
                    }

                    Text { text: "API Key"; color: Theme.textSecondary; font.pixelSize: 12; font.bold: true }
                    FieldInput {
                        Layout.fillWidth: true
                        echoMode: TextInput.Password
                        text: root.compiler.llmApiKey
                        placeholderText: "LLM API Key（不会写入报告）"
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
                                checked: root.compiler.llmApproved
                                onToggled: root.compiler.llmApproved = checked
                            }
                            Text {
                                Layout.fillWidth: true
                                text: "允许本次联网并调用大模型运行工具循环；工具执行仍受权限、路径和审计记录约束。"
                                color: Theme.textPrimary
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    Text { text: "Task"; color: Theme.textSecondary; font.pixelSize: 12; font.bold: true }
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
                                font.pixelSize: 13
                                background: Rectangle { color: "transparent" }
                            }
                        }
                    }

                    PrimaryButton {
                        Layout.fillWidth: true
                        text: root.compiler.llmApproved ? "Brain 接管任务" : "本地受控执行"
                        onClicked: root.compiler.runBrainTask(brainTask.text)
                    }

                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }

                    Text {
                        text: "混合研判"
                        color: Theme.textSecondary
                        font.pixelSize: 12
                        font.bold: true
                    }
                    Text {
                        Layout.fillWidth: true
                        text: "LLM 先基于审计结果给出风险判断和评分建议，确定性规则和证据逐条校验；冲突项降级并标注，最终评分仍以规则引擎为准。"
                        color: Theme.textMuted
                        font.pixelSize: 11
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

            // 混合研判结果
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
                            Layout.fillWidth: true
                            implicitHeight: advCol.implicitHeight + 18
                            radius: Theme.radiusSm
                            color: modelData.verdict === "conflicting" ? Theme.dangerSoft
                                 : modelData.verdict === "confirmed" ? Theme.successSoft
                                 : Theme.surfaceMuted
                            ColumnLayout {
                                id: advCol
                                x: 12; y: 9
                                width: parent.width - 24
                                spacing: 3
                                RowLayout {
                                    Layout.fillWidth: true
                                    Text {
                                        text: modelData.title
                                        color: Theme.textPrimary
                                        font.pixelSize: 13
                                        font.bold: true
                                    }
                                    Item { Layout.fillWidth: true }
                                    Text {
                                        text: modelData.verdict === "confirmed" ? "已印证"
                                            : modelData.verdict === "conflicting" ? "与规则冲突"
                                            : "待核实"
                                        color: modelData.verdict === "confirmed" ? Theme.success
                                             : modelData.verdict === "conflicting" ? Theme.danger
                                             : Theme.textMuted
                                        font.pixelSize: 11
                                        font.bold: true
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.reconciliation
                                    color: Theme.textSecondary
                                    font.pixelSize: 12
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
                Layout.preferredHeight: parent.height * 0.46
                padding: 4
                ScrollView {
                    anchors.fill: parent
                    clip: true
                    TextArea {
                        readOnly: true
                        wrapMode: TextArea.Wrap
                        text: root.compiler.agentResult
                        color: Theme.textPrimary
                        font.pixelSize: 13
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
                        font.pixelSize: 12
                        font.family: "monospace"
                        placeholderText: "工具调用 JSON 轨迹"
                        background: Rectangle { color: "transparent" }
                    }
                }
            }
        }
    }
}
