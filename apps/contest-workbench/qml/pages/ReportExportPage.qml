pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: root
    required property var compiler

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
            RowLayout {
                anchors.fill: parent
                spacing: 14
                Rectangle {
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    radius: 10
                    color: Theme.accentSoft
                    Text { anchors.centerIn: parent; text: "⤓"; color: Theme.accentActive; font.pixelSize: 18 }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text { text: "便于阅读的检查报告"; color: Theme.textPrimary; font.pixelSize: Theme.fontXl; font.bold: true }
                    Text { text: "包含分数、发现的问题、证明材料和修改建议"; color: Theme.textSecondary; font.pixelSize: Theme.fontSm }
                }
                FieldInput {
                    id: markdownPath
                    Layout.preferredWidth: 240
                    text: "workbench_report.md"
                }
                PrimaryButton {
                    text: "保存报告"
                    onClicked: root.compiler.exportMarkdown(markdownPath.text)
                }
            }
        }

        Card {
            Layout.fillWidth: true
            RowLayout {
                anchors.fill: parent
                spacing: 14
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
                    Text { text: "只有需要比较两次检查结果时才使用"; color: Theme.textSecondary; font.pixelSize: Theme.fontSm }
                }
                FieldInput {
                    id: jsonPath
                    Layout.preferredWidth: 240
                    text: "workbench_audit.json"
                }
                PrimaryButton {
                    text: "保存结果"
                    onClicked: root.compiler.exportJson(jsonPath.text)
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
