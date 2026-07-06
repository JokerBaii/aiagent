import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: root
    required property var compiler

    function prioColors(p) {
        if (p === "P0" || p === "高" || p === "high")
            return { bg: Theme.dangerSoft, fg: Theme.danger }
        if (p === "P1" || p === "中" || p === "medium")
            return { bg: Theme.warningSoft, fg: Theme.warning }
        return { bg: Theme.accentSoft, fg: Theme.accentActive }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 12

        SectionTitle {
            title: "补证任务"
            subtitle: "按优先级组织的材料补齐计划"
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
                model: root.compiler.fixTasks
                ScrollBar.vertical: ScrollBar {}
                add: Transition {
                    NumberAnimation { properties: "opacity"; from: 0; to: 1; duration: Theme.normal }
                    NumberAnimation { properties: "y"; from: 10; duration: Theme.normal; easing.type: Easing.OutCubic }
                }

                delegate: Rectangle {
                    width: list.width
                    implicitHeight: col.implicitHeight + 24
                    radius: Theme.radiusSm
                    color: Theme.surfaceMuted
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
                                text: modelData.priority
                                bg: root.prioColors(modelData.priority).bg
                                fg: root.prioColors(modelData.priority).fg
                            }
                            Item { Layout.fillWidth: true }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelData.title
                            color: Theme.textPrimary
                            font.pixelSize: 14
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: modelData.required && modelData.required.length > 0
                            text: "需要：" + modelData.required
                            color: Theme.textSecondary
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: modelData.rules && modelData.rules.length > 0
                            text: "关联检查：" + modelData.rules
                            color: Theme.textMuted
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                EmptyState {
                    anchors.fill: parent
                    visible: list.count === 0
                    text: "暂无补证任务"
                }
            }
        }
    }
}
