pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."

Item {
    id: root
    property alias text: input.text
    property bool busy: false
    property string currentModel: ""
    signal submit()
    signal command(string value)
    signal attachRequested()
    signal auditRequested()
    signal planRequested()
    signal rewindRequested()

    property int commandIndex: 0
    property bool slashPopupDismissed: false

    width: parent ? parent.width : implicitWidth
    height: implicitHeight
    implicitHeight: shell.height + toolbar.height + 10

    readonly property var commands: [
        { label: "/audit", badge: "评审", hint: "运行项目缺点评审" },
        { label: "/agent", badge: "任务", hint: "提交智能体任务" },
        { label: "/plan", badge: "计划", hint: "先生成计划，确认后再执行" },
        { label: "/optimize", badge: "修改", hint: "在安全副本中修改并二次审计" },
        { label: "/status", badge: "会话", hint: "查看项目和运行状态" },
        { label: "/compact", badge: "会话", hint: "压缩当前上下文" },
        { label: "/clear", badge: "会话", hint: "清空当前会话并重新开始" },
        { label: "/help", badge: "即时", hint: "查看全部命令" }
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

    onSlashQueryChanged: {
        commandIndex = 0
        slashPopupDismissed = false
    }

    function activateCommand(commandItem) {
        if (!commandItem)
            return
        if (commandItem.label === "/agent" || commandItem.label === "/plan"
                || commandItem.label === "/optimize") {
            input.text = commandItem.label + " "
            input.forceActiveFocus()
            input.cursorPosition = input.text.length
            return
        }
        root.command(commandItem.label)
        input.text = ""
    }

    function modelLabel(value) {
        return value.length > 0 ? value : "未配置模型"
    }

    function focusInput() {
        input.forceActiveFocus()
        input.cursorPosition = input.text.length
    }

    Rectangle {
        id: slashPopup
        visible: !root.slashPopupDismissed
                 && input.text.trim().indexOf("/") === 0
                 && root.filteredCommands.length > 0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: shell.top
        anchors.bottomMargin: 10
        height: Math.min(360, 40 + root.filteredCommands.length * 44)
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.border
        clip: true
        opacity: visible ? 1 : 0
        scale: visible ? 1 : 0.98
        Accessible.role: Accessible.List
        Accessible.name: "内置命令"

        Behavior on opacity { NumberAnimation { duration: Theme.fast } }
        Behavior on scale { NumberAnimation { duration: Theme.fast; easing.type: Easing.OutCubic } }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -1
            radius: parent.radius + 1
            color: "transparent"
            border.color: Qt.rgba(0, 0, 0, Theme.isDark ? 0.4 : 0.08)
            border.width: 1
            z: -1
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            Text {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                leftPadding: 14
                text: "内置命令"
                color: Theme.textTertiary
                font.pixelSize: Theme.fontXs
                font.bold: true
                verticalAlignment: Text.AlignVCenter
            }
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.borderSubtle }
            ListView {
                id: commandList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: root.filteredCommands
                currentIndex: root.commandIndex
                delegate: Rectangle {
                    id: commandDelegate
                    required property int index
                    required property var modelData

                    width: commandList.width
                    height: 44
                    color: commandDelegate.index === root.commandIndex || commandMouse.containsMouse
                           ? Theme.surfaceMuted : "transparent"
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 12
                        Rectangle {
                            Layout.preferredWidth: 26
                            Layout.preferredHeight: 26
                            radius: Theme.radiusSm
                            color: Theme.accentSoft
                            Text {
                                anchors.centerIn: parent
                                text: "/"
                                color: Theme.accent
                                font.family: Theme.monoFamily
                                font.pixelSize: Theme.fontMd
                                font.bold: true
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            RowLayout {
                                spacing: 8
                                Text {
                                    text: commandDelegate.modelData.label
                                    color: Theme.textPrimary
                                    font.family: Theme.monoFamily
                                    font.pixelSize: Theme.fontMd
                                    font.bold: true
                                }
                                Pill {
                                    text: commandDelegate.modelData.badge
                                    bg: commandDelegate.modelData.badge === "计划" || commandDelegate.modelData.badge === "危险"
                                        ? Theme.warningSoft : Theme.surfaceMuted
                                    fg: commandDelegate.modelData.badge === "计划" || commandDelegate.modelData.badge === "危险"
                                        ? Theme.warning : Theme.textMuted
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: commandDelegate.modelData.hint
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontXs
                                elide: Text.ElideRight
                            }
                        }
                    }
                    ActionArea {
                        id: commandMouse
                        anchors.fill: parent
                        accessibleName: commandDelegate.modelData.label + " "
                                        + commandDelegate.modelData.hint
                        onEntered: root.commandIndex = commandDelegate.index
                        onClicked: root.activateCommand(commandDelegate.modelData)
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
        anchors.bottomMargin: 8
        implicitHeight: Math.max(52, Math.min(160, inputScroll.contentHeight + 22))
        height: implicitHeight
        radius: Theme.radiusMd
        color: Theme.input
        border.color: input.activeFocus ? Theme.accent : Theme.border
        border.width: input.activeFocus ? 2 : 1
        Behavior on border.color { ColorAnimation { duration: Theme.fast } }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 8
            anchors.topMargin: 4
            anchors.bottomMargin: 4
            spacing: 8

            ScrollView {
                id: inputScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                TextArea {
                    id: input
                    Accessible.name: "向审计助手发送消息"
                    wrapMode: TextArea.Wrap
                    color: Theme.textPrimary
                    placeholderTextColor: Theme.textTertiary
                    placeholderText: root.busy ? "追加消息，将在当前回合后发送..." : "拖入项目或输入问题，/ 可唤起命令..."
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontLg
                    selectByMouse: true
                    verticalAlignment: TextArea.AlignVCenter
                    background: Rectangle { color: "transparent" }
                    Keys.onPressed: function(event) {
                        if (slashPopup.visible && event.key === Qt.Key_Down) {
                            root.commandIndex = Math.min(root.filteredCommands.length - 1,
                                                         root.commandIndex + 1)
                            commandList.positionViewAtIndex(root.commandIndex, ListView.Contain)
                            event.accepted = true
                            return
                        }
                        if (slashPopup.visible && event.key === Qt.Key_Up) {
                            root.commandIndex = Math.max(0, root.commandIndex - 1)
                            commandList.positionViewAtIndex(root.commandIndex, ListView.Contain)
                            event.accepted = true
                            return
                        }
                        if (event.key === Qt.Key_Escape && slashPopup.visible) {
                            root.slashPopupDismissed = true
                            event.accepted = true
                            return
                        }
                        if (event.key !== Qt.Key_Return && event.key !== Qt.Key_Enter)
                            return
                        if (input.inputMethodComposing) {
                            event.accepted = false
                            return
                        }
                        if (slashPopup.visible && root.commandIndex < root.filteredCommands.length) {
                            root.activateCommand(root.filteredCommands[root.commandIndex])
                            event.accepted = true
                            return
                        }
                        if (event.modifiers & Qt.ShiftModifier) {
                            event.accepted = false
                            return
                        }
                        event.accepted = true
                        root.submit()
                    }
                }
            }

            Rectangle {
                Layout.alignment: Qt.AlignBottom
                Layout.bottomMargin: 4
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                radius: 10
                color: input.text.trim().length === 0 ? Theme.surfaceMuted
                     : sendMouse.containsMouse ? Theme.accentHover
                     : Theme.accent
                opacity: input.text.trim().length > 0 ? 1 : 0.4
                Behavior on color { ColorAnimation { duration: Theme.fast } }
                Icon {
                    anchors.centerIn: parent
                    name: "arrowRight"
                    size: 16
                    strokeWidth: 2
                    color: input.text.trim().length === 0 ? Theme.textTertiary
                         : Theme.accentText
                }
                ActionArea {
                    id: sendMouse
                    anchors.fill: parent
                    enabled: input.text.trim().length > 0
                    accessibleName: root.busy ? "将消息加入发送队列" : "发送消息"
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
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        height: 32
        spacing: 8

        ToolbarChip {
            iconName: "attach"
            tooltip: "添加项目文件或项目压缩包"
            onClicked: root.attachRequested()
        }

        ToolbarChip {
            iconName: "plan"
            label: "项目评审"
            tooltip: "立即检查真实项目文件"
            onClicked: root.auditRequested()
        }

        ToolbarChip {
            iconName: "think"
            label: "计划"
            tooltip: "输入任务并先生成执行计划"
            onClicked: root.planRequested()
        }
        ToolbarChip {
            iconName: "rewind"
            label: "回退"
            tooltip: "撤销最近一轮对话"
            onClicked: root.rewindRequested()
        }

        Item { Layout.fillWidth: true }

        Rectangle {
            implicitWidth: modelRow.implicitWidth + 20
            implicitHeight: 28
            radius: Theme.radiusSm
            color: modelMouse.containsMouse ? Theme.surfaceHover : "transparent"
            RowLayout {
                id: modelRow
                anchors.centerIn: parent
                spacing: 6
                Icon { name: "dot"; size: 8; color: Theme.success }
                Text {
                    text: root.modelLabel(root.currentModel)
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSm
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.maximumWidth: 150
                }
            }
            ToolTip.visible: modelMouse.containsMouse
            ToolTip.text: "当前模型；可在设置中输入服务支持的任意模型 ID"
            ActionArea {
                id: modelMouse
                anchors.fill: parent
                accessibleName: "当前大模型：" + root.modelLabel(root.currentModel)
            }
        }
    }

    component ToolbarChip: Rectangle {
        id: chip
        property string iconName: ""
        property string label: ""
        property string tooltip: ""
        signal clicked()
        Layout.alignment: Qt.AlignVCenter
        implicitWidth: chipRow.implicitWidth + (chip.label.length > 0 ? 16 : 12)
        implicitHeight: 28
        radius: Theme.radiusSm
        color: chipMouse.containsMouse ? Theme.surfaceMuted : "transparent"
        RowLayout {
            id: chipRow
            anchors.centerIn: parent
            spacing: 5
            Icon { name: chip.iconName; size: 14; color: Theme.textTertiary }
            Text {
                visible: chip.label.length > 0
                text: chip.label
                color: Theme.textMuted
                font.pixelSize: Theme.fontXs
            }
        }
        ActionArea {
            id: chipMouse
            anchors.fill: parent
            accessibleName: chip.tooltip.length > 0 ? chip.tooltip : chip.label
            onClicked: chip.clicked()
        }
        ToolTip.visible: chipMouse.containsMouse && chip.tooltip.length > 0
        ToolTip.text: chip.tooltip
    }
}
