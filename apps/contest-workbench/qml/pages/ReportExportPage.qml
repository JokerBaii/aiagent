pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: root
    required property var compiler

    FileDialog {
        id: markdownDialog
        title: "保存便于阅读的检查报告"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "md"
        nameFilters: ["Markdown 报告 (*.md)"]
        onAccepted: {
            markdownPath.text = selectedFile
            root.compiler.exportMarkdown(selectedFile)
        }
    }

    FileDialog {
        id: jsonDialog
        title: "保存用于前后对比的检查结果"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: ["JSON 检查结果 (*.json)"]
        onAccepted: {
            jsonPath.text = selectedFile
            root.compiler.exportJson(selectedFile)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        SectionTitle {
            title: "下载检查报告"
            subtitle: "保存后可发给团队成员，或用于修改前后对比"
        }

        Card {
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                spacing: 12
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Rectangle {
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        radius: 10
                        color: Theme.accentSoft
                        Text { anchors.centerIn: parent; text: "↓"; color: Theme.accentActive; font.pixelSize: 18 }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text { text: "便于阅读的检查报告"; color: Theme.textPrimary; font.pixelSize: Theme.fontXl; font.bold: true }
                        Text { Layout.fillWidth: true; text: "包含分数、发现的问题、证明材料和修改建议"; color: Theme.textSecondary; font.pixelSize: Theme.fontSm; wrapMode: Text.WordWrap }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    FieldInput {
                        id: markdownPath
                        Layout.fillWidth: true
                        text: root.compiler.projectContext.workspaceRoot
                              ? root.compiler.projectContext.workspaceRoot + "/workbench_report.md"
                              : "workbench_report.md"
                    }
                    PrimaryButton {
                        text: "保存"
                        enabled: !root.compiler.agentRunning && !root.compiler.advisoryRunning
                        onClicked: root.compiler.exportMarkdown(markdownPath.text)
                    }
                    PrimaryButton {
                        text: "选择位置"
                        enabled: !root.compiler.agentRunning && !root.compiler.advisoryRunning
                        onClicked: markdownDialog.open()
                    }
                }
            }
        }

        Card {
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                spacing: 12
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Rectangle {
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        radius: 10
                        color: Theme.surfaceMuted
                        Text { anchors.centerIn: parent; text: "{ }"; color: Theme.textSecondary; font.pixelSize: Theme.fontXl; font.bold: true }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text { text: "用于前后对比的结果文件"; color: Theme.textPrimary; font.pixelSize: Theme.fontXl; font.bold: true }
                        Text { Layout.fillWidth: true; text: "只有需要比较两次检查结果时才使用"; color: Theme.textSecondary; font.pixelSize: Theme.fontSm; wrapMode: Text.WordWrap }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    FieldInput {
                        id: jsonPath
                        Layout.fillWidth: true
                        text: root.compiler.projectContext.workspaceRoot
                              ? root.compiler.projectContext.workspaceRoot + "/workbench_audit.json"
                              : "workbench_audit.json"
                    }
                    PrimaryButton {
                        text: "保存"
                        enabled: !root.compiler.agentRunning && !root.compiler.advisoryRunning
                        onClicked: root.compiler.exportJson(jsonPath.text)
                    }
                    PrimaryButton {
                        text: "选择位置"
                        enabled: !root.compiler.agentRunning && !root.compiler.advisoryRunning
                        onClicked: jsonDialog.open()
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
