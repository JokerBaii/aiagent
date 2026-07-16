pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: root
    required property var compiler
    readonly property var assetsModel: compiler.assets
    readonly property var roleDistributionModel: compiler.roleDistribution
    signal previewRequested(string relativePath)

    function formatBytes(value) {
        var bytes = Number(value || 0)
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB"
        if (bytes < 1024 * 1024 * 1024) return (bytes / 1024 / 1024).toFixed(1) + " MB"
        return (bytes / 1024 / 1024 / 1024).toFixed(1) + " GB"
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 20

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
                    model: root.roleDistributionModel
                    reuseItems: true
                    ScrollBar.vertical: ScrollBar {}
                    add: Transition {
                        NumberAnimation { properties: "opacity"; from: 0; to: 1; duration: Theme.normal }
                        NumberAnimation { properties: "y"; from: 8; duration: Theme.normal; easing.type: Easing.OutCubic }
                    }
                    delegate: Rectangle {
                        id: roleDelegate
                        required property int index
                        required property var modelData

                        width: roleList.width
                        height: 44
                        radius: Theme.radiusSm
                        color: roleDelegate.index % 2 === 0 ? Theme.surfaceMuted : Theme.surface
                        border.color: Theme.borderSubtle
                        border.width: 1
                        Behavior on color { ColorAnimation { duration: Theme.fast } }
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            Text {
                                Layout.fillWidth: true
                                text: roleDelegate.modelData.role
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontMd
                                elide: Text.ElideRight
                            }
                            Pill {
                                text: roleDelegate.modelData.count
                                bg: Theme.accentSoft
                                fg: Theme.accentActive
                            }
                        }
                    }
                    EmptyState {
                        anchors.fill: parent
                        visible: roleList.count === 0
                        text: "还没有材料"
                        hint: "添加项目后，这里会按文件用途整理材料。"
                    }
                }
            }
        }

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
                    model: root.assetsModel
                    reuseItems: true
                    ScrollBar.vertical: ScrollBar {}
                    add: Transition {
                        NumberAnimation { properties: "opacity"; from: 0; to: 1; duration: Theme.normal }
                        NumberAnimation { properties: "y"; from: 8; duration: Theme.normal; easing.type: Easing.OutCubic }
                    }
                    delegate: Rectangle {
                        id: assetDelegate
                        required property var modelData

                        width: assetList.width
                        implicitHeight: assetCol.implicitHeight + 20
                        radius: Theme.radiusSm
                        color: assetAction.containsMouse ? Theme.surfaceHover : Theme.surface
                        border.color: Theme.borderSubtle
                        border.width: 1
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
                                text: assetDelegate.modelData.path
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontMd
                                font.bold: true
                                wrapMode: Text.WrapAnywhere
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Pill { text: assetDelegate.modelData.role; bg: Theme.surfaceMuted; fg: Theme.textSecondary }
                                Pill { text: assetDelegate.modelData.format; bg: Theme.surfaceMuted; fg: Theme.textSecondary }
                                Item { Layout.fillWidth: true }
                                Text {
                                    text: root.formatBytes(assetDelegate.modelData.size)
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSm
                                }
                                Icon {
                                    name: "chevronRight"
                                    size: 11
                                    color: Theme.textTertiary
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                visible: assetDelegate.modelData.risk
                                         && assetDelegate.modelData.risk.length > 0
                                text: "注意：" + assetDelegate.modelData.risk
                                color: Theme.warning
                                font.pixelSize: Theme.fontSm
                                wrapMode: Text.WordWrap
                            }
                        }
                        ActionArea {
                            id: assetAction
                            anchors.fill: parent
                            accessibleName: "预览 " + assetDelegate.modelData.path
                            onClicked: root.previewRequested(assetDelegate.modelData.path)
                        }
                    }
                    EmptyState {
                        anchors.fill: parent
                        visible: assetList.count === 0
                        text: "还没有材料"
                        hint: "添加项目后，这里会列出每份文件及其用途。"
                    }
                }
            }
        }
    }
}
