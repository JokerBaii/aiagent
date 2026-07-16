pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: root
    required property var compiler
    readonly property var fixTasksModel: compiler.fixTasks

    function prioColors(p) {
        if (p === "P0" || p === "高" || p === "high")
            return { bg: Theme.dangerSoft, fg: Theme.danger }
        if (p === "P1" || p === "中" || p === "medium")
            return { bg: Theme.warningSoft, fg: Theme.warning }
        return { bg: Theme.accentSoft, fg: Theme.accentActive }
    }

    function prioText(p) {
        if (p === "P0" || p === "高" || p === "high") return "先处理"
        if (p === "P1" || p === "中" || p === "medium") return "随后补齐"
        return "可以优化"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 12

        SectionTitle {
            title: "接下来怎么改"
            subtitle: "已经按处理顺序整理好，完成一项再看下一项"
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
                model: root.fixTasksModel
                reuseItems: true
                ScrollBar.vertical: ScrollBar {}
                add: Transition {
                    NumberAnimation { properties: "opacity"; from: 0; to: 1; duration: Theme.normal }
                    NumberAnimation { properties: "y"; from: 10; duration: Theme.normal; easing.type: Easing.OutCubic }
                }

                delegate: Rectangle {
                    id: taskDelegate
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
                                text: root.prioText(taskDelegate.modelData.priority)
                                bg: root.prioColors(taskDelegate.modelData.priority).bg
                                fg: root.prioColors(taskDelegate.modelData.priority).fg
                            }
                            Item { Layout.fillWidth: true }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: taskDelegate.modelData.title
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontLg
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: taskDelegate.modelData.reason
                                     && taskDelegate.modelData.reason.length > 0
                            text: taskDelegate.modelData.reason
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontMd
                            wrapMode: Text.WordWrap
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: taskDelegate.modelData.required && taskDelegate.modelData.required.length > 0
                            text: "需要准备：" + taskDelegate.modelData.required
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontMd
                            wrapMode: Text.WordWrap
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: taskDelegate.modelData.files && taskDelegate.modelData.files.length > 0
                            text: "涉及文件：" + taskDelegate.modelData.files
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSm
                            wrapMode: Text.WrapAnywhere
                        }
                    }
                }

                EmptyState {
                    anchors.fill: parent
                    visible: list.count === 0
                    text: "目前没有待办事项"
                    hint: "这次检查没有生成需要补材料或修改的任务。"
                }
            }
        }
    }
}
