pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: root
    required property var compiler

    function statusColors(s) {
        if (s === "证据充分")
            return { bg: Theme.successSoft, fg: Theme.success }
        if (s === "证据不足" || s === "需要复核")
            return { bg: Theme.warningSoft, fg: Theme.warning }
        return { bg: Theme.dangerSoft, fg: Theme.danger }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 12

        SectionTitle {
            title: "声明与证据匹配"
            subtitle: "每条声明的支撑证据与缺口"
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
                model: root.compiler.claimEvidence
                ScrollBar.vertical: ScrollBar {}
                add: Transition {
                    NumberAnimation { properties: "opacity"; from: 0; to: 1; duration: Theme.normal }
                    NumberAnimation { properties: "y"; from: 10; duration: Theme.normal; easing.type: Easing.OutCubic }
                }

                delegate: Rectangle {
                    id: claimDelegate
                    required property var modelData

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
                            Pill { text: claimDelegate.modelData.type; bg: Theme.surface; fg: Theme.textSecondary }
                            Pill {
                                text: claimDelegate.modelData.status
                                bg: root.statusColors(claimDelegate.modelData.status).bg
                                fg: root.statusColors(claimDelegate.modelData.status).fg
                            }
                            Item { Layout.fillWidth: true }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: claimDelegate.modelData.text
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontLg
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }
                        Text {
                            Layout.fillWidth: true
                            text: "证据：" + (claimDelegate.modelData.evidence || "—")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontMd
                            wrapMode: Text.WordWrap
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: claimDelegate.modelData.missing && claimDelegate.modelData.missing.length > 0
                            text: "缺失：" + claimDelegate.modelData.missing
                            color: Theme.danger
                            font.pixelSize: Theme.fontMd
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                EmptyState {
                    anchors.fill: parent
                    visible: list.count === 0
                    text: "暂无声明"
                }
            }
        }
    }
}
