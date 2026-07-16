pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: root
    required property var compiler
    readonly property var findingsModel: compiler.findings

    function sevColors(s) {
        if (s === "必须处理")
            return { bg: Theme.dangerSoft, fg: Theme.danger }
        if (s === "提示")
            return { bg: Theme.accentSoft, fg: Theme.accent }
        return { bg: Theme.warningSoft, fg: Theme.warning }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 12

        SectionTitle {
            title: "提交前要改的问题"
            subtitle: "先处理红色问题，再补齐黄色建议"
        }

        Card {
            Layout.fillWidth: true
            Layout.fillHeight: true
            padding: 0
            ListView {
                id: list
                anchors.fill: parent
                anchors.margins: 12
                clip: true
                spacing: 10
                model: root.findingsModel
                reuseItems: true
                ScrollBar.vertical: ScrollBar {}
                add: Transition {
                    NumberAnimation { properties: "opacity"; from: 0; to: 1; duration: Theme.normal }
                    NumberAnimation { properties: "y"; from: 10; duration: Theme.normal; easing.type: Easing.OutCubic }
                }

                delegate: Rectangle {
                    id: findingDelegate
                    required property var modelData

                    width: list.width
                    implicitHeight: col.implicitHeight + 24
                    radius: Theme.radiusSm
                    color: Theme.surface
                    border.color: Theme.borderSubtle
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: Theme.fast } }

                    ColumnLayout {
                        id: col
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Pill {
                                text: findingDelegate.modelData.severity
                                bg: root.sevColors(findingDelegate.modelData.severity).bg
                                fg: root.sevColors(findingDelegate.modelData.severity).fg
                            }
                            Item { Layout.fillWidth: true }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: findingDelegate.modelData.title
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontLg
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }
                        Text {
                            Layout.fillWidth: true
                            text: findingDelegate.modelData.reason
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontMd
                            wrapMode: Text.WordWrap
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: findingDelegate.modelData.evidence
                                     && findingDelegate.modelData.evidence.length > 0
                            text: "相关文件：" + findingDelegate.modelData.evidence
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSm
                            wrapMode: Text.WrapAnywhere
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: findingDelegate.modelData.missing
                                     && findingDelegate.modelData.missing.length > 0
                            text: "还缺：" + findingDelegate.modelData.missing
                            color: Theme.warning
                            font.pixelSize: Theme.fontSm
                            wrapMode: Text.WordWrap
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: findingDelegate.modelData.fix && findingDelegate.modelData.fix.length > 0
                            text: "可以这样处理：" + findingDelegate.modelData.fix
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontMd
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                EmptyState {
                    anchors.fill: parent
                    visible: list.count === 0
                    text: "没有发现需要修改的问题"
                    hint: "当前材料没有发现明显问题，提交前仍建议人工复核关键事实。"
                }
            }
        }
    }
}
