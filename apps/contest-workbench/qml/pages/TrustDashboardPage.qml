import QtQuick
import QtQuick.Controls
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

        RowLayout {
            Layout.fillWidth: true
            spacing: 16
            StatTile {
                label: "可信评分"
                value: root.compiler.trustScore + " / 100"
                accent: Theme.accent
            }
            StatTile {
                label: "必须处理"
                value: root.compiler.blockerCount
                accent: Theme.danger
                tint: root.compiler.blockerCount > 0 ? Theme.dangerSoft : Theme.surface
            }
            StatTile {
                label: "需要关注"
                value: root.compiler.warningCount
                accent: Theme.warning
                tint: root.compiler.warningCount > 0 ? Theme.warningSoft : Theme.surface
            }
        }

        Card {
            Layout.fillWidth: true
            visible: root.compiler.summary.length > 0
            ColumnLayout {
                anchors.fill: parent
                spacing: 6
                SectionTitle { title: "审计概要" }
                Text {
                    Layout.fillWidth: true
                    text: root.compiler.summary
                    color: Theme.textPrimary
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                    lineHeight: 1.3
                }
            }
        }

            SectionTitle {
                Layout.leftMargin: 4
                title: "扣分明细"
            subtitle: "影响评分的主要原因"
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
                spacing: 8
                model: root.compiler.scorePenalties
                ScrollBar.vertical: ScrollBar {}
                add: Transition {
                    NumberAnimation { properties: "opacity"; from: 0; to: 1; duration: Theme.normal }
                    NumberAnimation { properties: "y"; from: 10; duration: Theme.normal; easing.type: Easing.OutCubic }
                }

                delegate: Rectangle {
                    width: list.width
                    implicitHeight: row.implicitHeight + 20
                    radius: Theme.radiusSm
                    color: Theme.surfaceMuted
                    Behavior on color { ColorAnimation { duration: Theme.fast } }

                    RowLayout {
                        id: row
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 12

                        Pill {
                            text: modelData.dimension
                            bg: Theme.accentSoft
                            fg: Theme.accentActive
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelData.reason
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                        }
                        Text {
                            text: "-" + modelData.points
                            color: Theme.danger
                            font.pixelSize: 15
                            font.bold: true
                        }
                    }
                }

                EmptyState {
                    anchors.fill: parent
                    visible: list.count === 0
                    text: "暂无扣分项"
                }
            }
        }
    }
}
