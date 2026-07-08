import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."

Item {
    id: root
    property alias text: input.text
    property bool busy: false
    property string accessMode: "ask"
    signal submit()
    signal command(string value)
    signal modeChange(string value)

    width: parent ? parent.width : implicitWidth
    height: implicitHeight
    implicitHeight: slashPopup.visible ? 424 : 114

    readonly property var commands: [
        { label: "/audit", badge: "即时", hint: "运行可信审计" },
        { label: "/ask", badge: "模式", hint: "切换到 Ask 沙箱问答模式" },
        { label: "/plan", badge: "计划", hint: "只生成计划，不执行工具" },
        { label: "/code", badge: "模式", hint: "切换到 Code 工作区执行模式" },
        { label: "/bypass", badge: "危险", hint: "切换到完全授权 Bypass 模式" },
        { label: "/agent", badge: "任务", hint: "提交智能体任务" },
        { label: "/status", badge: "会话", hint: "查看权限与项目状态" },
        { label: "/compact", badge: "会话", hint: "压缩当前上下文" },
        { label: "/clear", badge: "会话", hint: "清空当前会话并重新开始" },
        { label: "/help", badge: "即时", hint: "查看全部命令" }
    ]
    readonly property var modes: [
        { key: "code", label: "Code", icon: "◇" },
        { key: "ask", label: "Ask", icon: "?" },
        { key: "plan", label: "Plan", icon: "≡" },
        { key: "bypass", label: "Bypass", icon: "☆" }
    ]
    readonly property string slashQuery: input.text.trim().indexOf("/") === 0
                                        ? input.text.trim().slice(1).toLowerCase()
                                        : ""
    readonly property var filteredCommands: root.commands.filter(function(command) {
        if (root.slashQuery.length === 0)
            return true
        return command.label.toLowerCase().indexOf("/" + root.slashQuery) === 0
                || command.hint.toLowerCase().indexOf(root.slashQuery) >= 0
                || command.badge.toLowerCase().indexOf(root.slashQuery) >= 0
    })

    Rectangle {
        id: slashPopup
        visible: input.text.trim().indexOf("/") === 0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: shell.top
        anchors.bottomMargin: 10
        height: Math.min(380, 44 + root.filteredCommands.length * 42)
        radius: 12
        color: Theme.surface
        border.color: Theme.borderStrong
        clip: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            Text {
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                leftPadding: 12
                text: "内置命令"
                color: Theme.textMuted
                font.pixelSize: Theme.fontXs
                font.bold: true
                verticalAlignment: Text.AlignVCenter
            }
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }
            ListView {
                id: commandList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: root.filteredCommands
                delegate: Rectangle {
                    width: commandList.width
                    height: 42
                    color: commandMouse.containsMouse ? Theme.surfaceMuted : Theme.surface
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 12
                        Rectangle {
                            width: 26
                            height: 26
                            radius: 8
                            color: Theme.surfaceMuted
                            Text {
                                anchors.centerIn: parent
                                text: "/"
                                color: Theme.textMuted
                                font.pixelSize: 13
                                font.bold: true
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            RowLayout {
                                spacing: 8
                                Text {
                                    text: modelData.label
                                    color: Theme.textPrimary
                                    font.family: Theme.monoFamily
                                    font.pixelSize: Theme.fontMd
                                    font.bold: true
                                }
                                Pill {
                                    text: modelData.badge
                                    bg: modelData.badge === "计划" || modelData.badge === "危险"
                                        ? Theme.warningSoft : Theme.surfaceMuted
                                    fg: modelData.badge === "计划" || modelData.badge === "危险"
                                        ? Theme.warning : Theme.textMuted
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.hint
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontXs
                                elide: Text.ElideRight
                            }
                        }
                    }
                    MouseArea {
                        id: commandMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (modelData.label === "/agent" || modelData.label === "/plan"
                                    || modelData.label === "/ask" || modelData.label === "/code"
                                    || modelData.label === "/bypass") {
                                input.text = modelData.label + " "
                                input.forceActiveFocus()
                                input.cursorPosition = input.text.length
                            } else {
                                root.command(modelData.label)
                                input.text = ""
                            }
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id: shell
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: toolbar.top
        anchors.bottomMargin: 10
        height: 68
        radius: 24
        color: Theme.input
        border.color: input.activeFocus ? Theme.accent : Theme.borderStrong
        border.width: input.activeFocus ? 2 : 1
        Behavior on border.color { ColorAnimation { duration: Theme.fast } }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 12
            spacing: 10

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                TextArea {
                    id: input
                    wrapMode: TextArea.Wrap
                    color: Theme.textPrimary
                    placeholderTextColor: Theme.textMuted
                    placeholderText: "追加消息..."
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontLg
                    selectByMouse: true
                    background: Rectangle { color: "transparent" }
                    Keys.onReturnPressed: function(event) {
                        if (event.modifiers & Qt.ShiftModifier) {
                            event.accepted = false
                        } else {
                            event.accepted = true
                            root.submit()
                        }
                    }
                }
            }

            Rectangle {
                Layout.alignment: Qt.AlignVCenter
                width: 44
                height: 44
                radius: 15
                color: root.busy ? Theme.dangerSoft
                     : sendMouse.containsMouse ? Theme.accentHover
                     : Theme.accent
                opacity: input.text.trim().length > 0 || root.busy ? 1 : 0.38
                Text {
                    anchors.centerIn: parent
                    text: root.busy ? "■" : "→"
                    color: root.busy ? Theme.danger : (Theme.isDark && Theme.colorTheme === "black" ? "#101010" : "#FFFFFF")
                    font.pixelSize: root.busy ? 14 : 24
                    font.bold: true
                }
                MouseArea {
                    id: sendMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    enabled: input.text.trim().length > 0 && !root.busy
                    onClicked: root.submit()
                }
            }
        }
    }

    RowLayout {
        id: toolbar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 36
        spacing: 12

        Text {
            text: "⌘"
            color: Theme.textMuted
            font.pixelSize: 18
            Layout.leftMargin: 12
        }

        Repeater {
            model: root.modes
            delegate: Rectangle {
                implicitWidth: modeRow.implicitWidth + 28
                implicitHeight: 32
                radius: 12
                property bool active: root.accessMode === modelData.key
                color: active ? (modelData.key === "bypass" ? Theme.warningSoft : Theme.accentSoft)
                              : Theme.surface
                border.color: active ? (modelData.key === "bypass" ? Theme.warning : Theme.accent)
                                     : Theme.border
                Row {
                    id: modeRow
                    anchors.centerIn: parent
                    spacing: 7
                    Text {
                        text: modelData.icon
                        color: active ? (modelData.key === "bypass" ? Theme.warning : Theme.accent)
                                      : Theme.textMuted
                        font.pixelSize: 13
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: modelData.label
                        color: active ? (modelData.key === "bypass" ? Theme.warning : Theme.accent)
                                      : Theme.textMuted
                        font.pixelSize: Theme.fontMd
                        font.bold: active
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: "⌄"
                        color: active ? (modelData.key === "bypass" ? Theme.warning : Theme.accent)
                                      : Theme.textMuted
                        font.pixelSize: 11
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.modeChange(modelData.key)
                }
            }
        }

        Text {
            text: "↻  回退"
            color: Theme.textMuted
            font.pixelSize: Theme.fontMd
        }

        Item { Layout.fillWidth: true }

        Text {
            text: "☰  Plan"
            color: root.accessMode === "plan" ? Theme.accent : Theme.textMuted
            font.pixelSize: Theme.fontMd
        }
        Text {
            text: "◷  DeepseekV4Flash ⌄"
            color: Theme.textMuted
            font.pixelSize: Theme.fontLg
        }
    }
}
