pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: root
    required property var compiler
    readonly property var penaltiesModel: compiler.scorePenalties

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.width < 720 ? 16 : 24
        spacing: 18

        GridLayout {
            Layout.fillWidth: true
            columns: root.width >= 720 ? 3 : 1
            columnSpacing: 14
            rowSpacing: 12
            StatTile {
                label: "当前得分"
                value: String(root.compiler.trustScore)
                suffix: "分"
                hint: "满分 100"
                accent: Theme.accent
            }
            StatTile {
                label: "提交前要处理"
                value: String(root.compiler.blockerCount)
                suffix: "个"
                hint: root.compiler.blockerCount > 0 ? "优先处理" : "目前没有"
                accent: Theme.danger
                tint: root.compiler.blockerCount > 0 ? Theme.dangerSoft : Theme.surface
            }
            StatTile {
                label: "建议补齐"
                value: String(root.compiler.warningCount)
                suffix: "个"
                hint: root.compiler.warningCount > 0 ? "逐项完善" : "目前没有"
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
                SectionTitle { title: "这次检查到" }
                Text {
                    Layout.fillWidth: true
                    text: root.compiler.summary
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontLg
                    wrapMode: Text.WordWrap
                    lineHeight: 1.3
                }
            }
        }

        SectionTitle {
            Layout.leftMargin: 2
            title: "分数为什么变化"
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
                model: root.penaltiesModel
                reuseItems: true
                ScrollBar.vertical: ScrollBar {}
                add: Transition {
                    NumberAnimation { properties: "opacity"; from: 0; to: 1; duration: Theme.normal }
                    NumberAnimation { properties: "y"; from: 10; duration: Theme.normal; easing.type: Easing.OutCubic }
                }

                delegate: Rectangle {
                    id: penaltyDelegate
                    required property int index
                    required property var modelData

                    width: list.width
                    implicitHeight: row.implicitHeight + 20
                    radius: Theme.radius
                    color: penaltyDelegate.index % 2 === 0 ? Theme.surfaceMuted : Theme.surface
                    border.color: Theme.borderSubtle
                    border.width: 1
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
                            text: penaltyDelegate.modelData.dimension
                            bg: Theme.surface
                            fg: Theme.textSecondary
                        }
                        Text {
                            Layout.fillWidth: true
                            text: penaltyDelegate.modelData.reason
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontMd
                            wrapMode: Text.WordWrap
                        }
                        Text {
                            text: "-" + penaltyDelegate.modelData.points
                            color: Theme.danger
                            font.pixelSize: Theme.fontXl
                            font.bold: true
                        }
                    }
                }

                EmptyState {
                    anchors.fill: parent
                    visible: list.count === 0
                    text: "这次没有扣分"
                    hint: "当前材料没有触发会影响分数的检查项。"
                }
            }
        }
    }
}
