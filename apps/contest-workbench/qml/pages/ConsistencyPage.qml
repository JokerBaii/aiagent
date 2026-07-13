pragma ComponentBehavior: Bound

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
        if (s === "提示")
            return { bg: Theme.accentSoft, fg: Theme.accent }
        return { bg: Theme.warningSoft, fg: Theme.warning }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 12

        SectionTitle {
            title: "不同材料有没有对不上"
            subtitle: "重点看名称、时间、数字和成果描述是否一致"
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
                    id: consistencyDelegate
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
                                text: consistencyDelegate.modelData.severity
                                bg: root.sevColors(consistencyDelegate.modelData.severity).bg
                                fg: root.sevColors(consistencyDelegate.modelData.severity).fg
                            }
                            Item { Layout.fillWidth: true }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: consistencyDelegate.modelData.description
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontLg
                            wrapMode: Text.WordWrap
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: consistencyDelegate.modelData.files
                                     && consistencyDelegate.modelData.files.length > 0
                            text: "涉及：" + consistencyDelegate.modelData.files
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSm
                            wrapMode: Text.WrapAnywhere
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: consistencyDelegate.modelData.fix && consistencyDelegate.modelData.fix.length > 0
                            text: "可以这样处理：" + consistencyDelegate.modelData.fix
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontMd
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                EmptyState {
                    anchors.fill: parent
                    visible: list.count === 0
                    text: "材料之间没有发现明显冲突"
                    hint: "名称、数字、日期和成果描述暂时能对得上。"
                }
            }
        }
    }
}
