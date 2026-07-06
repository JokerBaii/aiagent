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

        // 配置
        ColumnLayout {
            Layout.preferredWidth: 380
            Layout.fillHeight: true
            spacing: 12

            SectionTitle {
                title: "LLM Brain（可选）"
                subtitle: "默认不联网，需显式授权后才调用"
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
                                text: "我确认允许本次请求联网并调用大模型，Brain 只提供建议，不覆盖规则结论。"
                                color: Theme.textPrimary
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    PrimaryButton {
                        Layout.fillWidth: true
                        text: "生成 Brain 建议"
                        enabled: root.compiler.llmApproved
                        onClicked: root.compiler.runLlmAdvice()
                    }
                }
            }
            Item { Layout.fillHeight: true }
        }

        // 输出
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12
            SectionTitle { title: "Brain 建议输出" }
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
                        text: root.compiler.llmAdvice
                        color: Theme.textPrimary
                        font.pixelSize: 13
                        placeholderText: "运行审计并授权后，可在这里生成大模型辅助建议。"
                        background: Rectangle { color: "transparent" }
                    }
                }
            }
        }
    }
}
