import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: root
    required property var compiler

    RowLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 20

        // 角色分布
        ColumnLayout {
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            spacing: 12
            SectionTitle { title: "材料类型"; subtitle: "系统自动识别出的材料构成" }
            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                padding: 0
                ListView {
                    id: roleList
                    anchors.fill: parent
                    anchors.margins: 12
                    clip: true
                    spacing: 8
                    model: root.compiler.roleDistribution
                    ScrollBar.vertical: ScrollBar {}
                    add: Transition {
                        NumberAnimation { properties: "opacity"; from: 0; to: 1; duration: Theme.normal }
                        NumberAnimation { properties: "y"; from: 8; duration: Theme.normal; easing.type: Easing.OutCubic }
                    }
                    delegate: Rectangle {
                        width: roleList.width
                        height: 44
                        radius: Theme.radiusSm
                        color: Theme.surfaceMuted
                        Behavior on color { ColorAnimation { duration: Theme.fast } }
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            Text {
                                Layout.fillWidth: true
                                text: modelData.role
                                color: Theme.textPrimary
                                font.pixelSize: 13
                                elide: Text.ElideRight
                            }
                            Pill {
                                text: modelData.count
                                bg: Theme.accentSoft
                                fg: Theme.accentActive
                            }
                        }
                    }
                    EmptyState {
                        anchors.fill: parent
                        visible: roleList.count === 0
                        text: "暂无资产"
                    }
                }
            }
        }

        // 资产列表
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12
            SectionTitle { title: "材料清单"; subtitle: "每份文件的用途、格式和注意事项" }
            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                padding: 0
                ListView {
                    id: assetList
                    anchors.fill: parent
                    anchors.margins: 12
                    clip: true
                    spacing: 8
                    model: root.compiler.assets
                    ScrollBar.vertical: ScrollBar {}
                    add: Transition {
                        NumberAnimation { properties: "opacity"; from: 0; to: 1; duration: Theme.normal }
                        NumberAnimation { properties: "y"; from: 8; duration: Theme.normal; easing.type: Easing.OutCubic }
                    }
                    delegate: Rectangle {
                        width: assetList.width
                        implicitHeight: assetCol.implicitHeight + 20
                        radius: Theme.radiusSm
                        color: Theme.surfaceMuted
                        Behavior on color { ColorAnimation { duration: Theme.fast } }
                        ColumnLayout {
                            id: assetCol
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 6
                            Text {
                                Layout.fillWidth: true
                                text: modelData.path
                                color: Theme.textPrimary
                                font.pixelSize: 13
                                font.bold: true
                                wrapMode: Text.WrapAnywhere
                            }
                            RowLayout {
                                spacing: 8
                                Pill { text: modelData.role; bg: Theme.surface; fg: Theme.textSecondary }
                                Pill { text: modelData.format; bg: Theme.surface; fg: Theme.textSecondary }
                                Pill {
                                    visible: modelData.risk && modelData.risk.length > 0
                                    text: modelData.risk
                                    bg: Theme.warningSoft
                                    fg: Theme.warning
                                }
                            }
                        }
                    }
                    EmptyState {
                        anchors.fill: parent
                        visible: assetList.count === 0
                        text: "暂无资产"
                    }
                }
            }
        }
    }
}
