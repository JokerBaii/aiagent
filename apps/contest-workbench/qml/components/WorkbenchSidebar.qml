pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."

Rectangle {
    id: root

    required property var compiler
    property string taskSearch: ""

    signal newSessionRequested()
    signal addProjectRequested()
    signal contextRequested()
    signal settingsRequested()

    color: Theme.sidebar

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.topMargin: 14
        anchors.bottomMargin: 14
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            AiAvatar { size: 30 }
            Text {
                text: "项目审计"
                color: Theme.sidebarTextActive
                font.pixelSize: Theme.fontXl
                font.bold: true
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            radius: Theme.radiusLg
            color: newTaskMouse.containsMouse ? Theme.accentHover : Theme.accent
            Behavior on color { ColorAnimation { duration: Theme.fast } }
            RowLayout {
                anchors.centerIn: parent
                spacing: 8
                Icon { name: "plus"; size: 16; strokeWidth: 2; color: Theme.isDark && Theme.colorTheme === "black" ? "#101010" : "#FFFFFF" }
                Text {
                    text: "新任务"
                    color: Theme.isDark && Theme.colorTheme === "black" ? "#101010" : "#FFFFFF"
                    font.pixelSize: Theme.fontLg
                    font.bold: true
                }
            }
            ActionArea {
                id: newTaskMouse
                anchors.fill: parent
                accessibleName: "开始新任务"
                onClicked: root.newSessionRequested()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            radius: Theme.radiusMd
            color: addMouse.containsMouse ? Theme.surfaceHover : Theme.surfaceMuted
            border.color: Theme.border
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 12
                spacing: 9
                Icon { name: "folderPlus"; size: 15; color: Theme.textMuted }
                Text {
                    Layout.fillWidth: true
                    text: "添加项目"
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMd
                    font.bold: true
                }
                Text {
                    text: Qt.platform.os === "osx" ? "⌘O" : "Ctrl+O"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontXs
                }
            }
            ActionArea {
                id: addMouse
                anchors.fill: parent
                accessibleName: "添加项目文件夹"
                onClicked: root.addProjectRequested()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border
            Icon {
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                name: "search"
                size: 14
                color: Theme.textMuted
            }
            TextField {
                anchors.fill: parent
                Accessible.name: "搜索任务"
                leftPadding: 38
                rightPadding: 12
                placeholderText: "搜索任务..."
                placeholderTextColor: Theme.textMuted
                color: Theme.textPrimary
                font.pixelSize: Theme.fontMd
                selectByMouse: true
                background: null
                onTextChanged: root.taskSearch = text
            }
        }

        ListView {
            id: sessionList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 3
            model: root.compiler.sessionList.filter(function(item) {
                var query = root.taskSearch.trim().toLowerCase()
                return query.length === 0
                        || String(item.title).toLowerCase().indexOf(query) >= 0
                        || String(item.subtitle).toLowerCase().indexOf(query) >= 0
            })
            section.property: "active"
            delegate: Rectangle {
                id: sessionDelegate
                required property var modelData

                width: sessionList.width
                implicitHeight: 44
                radius: Theme.radius
                color: sessionDelegate.modelData.active ? Theme.accentSoft
                     : sessionMouse.containsMouse ? Theme.surfaceMuted
                     : "transparent"
                border.color: sessionDelegate.modelData.active ? Theme.accentGhost : "transparent"
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 8
                    Text {
                        Layout.fillWidth: true
                        text: sessionDelegate.modelData.title
                        color: sessionDelegate.modelData.active ? Theme.textPrimary : Theme.sidebarText
                        font.pixelSize: Theme.fontMd
                        font.bold: sessionDelegate.modelData.active
                        elide: Text.ElideRight
                    }
                    Text {
                        text: sessionDelegate.modelData.subtitle
                        color: Theme.textTertiary
                        font.pixelSize: Theme.fontXs
                        elide: Text.ElideRight
                    }
                }
                ActionArea {
                    id: sessionMouse
                    anchors.fill: parent
                    accessibleName: "打开任务上下文：" + sessionDelegate.modelData.title
                    onClicked: root.compiler.activateSession(sessionDelegate.modelData.sessionId)
                }
            }
            EmptyState {
                anchors.fill: parent
                visible: sessionList.count === 0
                text: "还没有任务"
                hint: "选择完整项目文件夹、项目压缩包或单个项目文件后开始。"
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.border
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 38
                radius: Theme.radius
                color: packMouse.containsMouse ? Theme.surfaceHover : "transparent"
                RowLayout {
                    anchors.centerIn: parent
                    spacing: 8
                    Icon { name: "think"; size: 15; color: Theme.textMuted }
                    Text { text: "上下文"; color: Theme.sidebarText; font.pixelSize: Theme.fontSm; font.bold: true }
                }
                ActionArea {
                    id: packMouse
                    anchors.fill: parent
                    accessibleName: "打开上下文面板"
                    onClicked: root.contextRequested()
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 38
                radius: Theme.radius
                color: settingsMouse.containsMouse ? Theme.surfaceHover : "transparent"
                RowLayout {
                    anchors.centerIn: parent
                    spacing: 8
                    Icon { name: "settings"; size: 15; color: Theme.textMuted }
                    Text { text: "设置"; color: Theme.sidebarText; font.pixelSize: Theme.fontSm; font.bold: true }
                }
                ActionArea {
                    id: settingsMouse
                    anchors.fill: parent
                    accessibleName: "打开设置"
                    onClicked: root.settingsRequested()
                }
            }
        }
    }
}
