import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: root
    required property var compiler

    function sevColors(s) {
        if (s === "必须处理")
            return { bg: Theme.dangerSoft, fg: Theme.danger }
        return { bg: Theme.warningSoft, fg: Theme.warning }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 12

        SectionTitle {
            title: "材料一致性"
            subtitle: "跨材料的冲突、矛盾与修复建议"
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
                model: root.compiler.consistencyIssues
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
                                text: modelData.severity
                                bg: root.sevColors(modelData.severity).bg
                                fg: root.sevColors(modelData.severity).fg
                            }
                            Item { Layout.fillWidth: true }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelData.description
                            color: Theme.textPrimary
                            font.pixelSize: 14
                            wrapMode: Text.WordWrap
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: modelData.fix && modelData.fix.length > 0
                            text: "建议：" + modelData.fix
                            color: Theme.textSecondary
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                EmptyState {
                    anchors.fill: parent
                    visible: list.count === 0
                    text: "未发现一致性问题"
                }
            }
        }
    }
}
