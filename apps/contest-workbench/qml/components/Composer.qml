import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."

// 底部输入区，一比一对齐 TOKENICODE 的 InputBar：
// 上方 rounded-2xl 输入行（多行 + 行内发送按钮 + focus 发光边框），
// 下方工具行（附件、项目、模式选择器、思考档位、模型选择器），
// 以及输入 "/" 时向上弹出的斜杠命令面板。
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
    signal modelSelected(string value)

    width: parent ? parent.width : implicitWidth
    height: implicitHeight
    implicitHeight: shell.height + toolbar.height + 10

    readonly property var commands: [
        { label: "/audit", badge: "评审", hint: "运行项目缺点评审" },
        { label: "/agent", badge: "任务", hint: "提交智能体任务" },
        { label: "/plan", badge: "计划", hint: "先生成计划，确认后再执行" },
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

    function modelLabel(value) {
        if (value === "deepseek-v4-flash") return "DeepSeek V4 Flash"
        if (value === "deepseek-chat") return "DeepSeek Chat"
        if (value === "deepseek-reasoner") return "DeepSeek Reasoner"
        return value.length > 0 ? value : "未配置模型"
    }

    function focusInput() {
        input.forceActiveFocus()
        input.cursorPosition = input.text.length
    }

    // 斜杠命令面板：向上弹出，圆角卡片 + 阴影。
    Rectangle {
        id: slashPopup
        visible: input.text.trim().indexOf("/") === 0 && root.filteredCommands.length > 0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: shell.top
        anchors.bottomMargin: 10
        height: Math.min(360, 40 + root.filteredCommands.length * 44)
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.border
        clip: true

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
                delegate: Rectangle {
                    width: commandList.width
                    height: 44
                    color: commandMouse.containsMouse ? Theme.surfaceMuted : "transparent"
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 12
                        Rectangle {
                            width: 26
                            height: 26
                            radius: Theme.radiusSm
                            color: Theme.accentSoft
                            Text {
                                anchors.centerIn: parent
                                text: "/"
                                color: Theme.accent
                                font.family: Theme.monoFamily
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
                            if (modelData.label === "/agent") {
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

    // 输入行：rounded-2xl，focus 时强调色边框 + 柔光。
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
                    wrapMode: TextArea.Wrap
                    color: Theme.textPrimary
                    placeholderTextColor: Theme.textTertiary
                    placeholderText: root.busy ? "追加消息，将在当前回合后发送..." : "拖入项目或输入问题，/ 可唤起命令..."
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontLg
                    selectByMouse: true
                    verticalAlignment: TextArea.AlignVCenter
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

            // 发送按钮：运行中仍可追加消息，由后端队列接住。
            Rectangle {
                Layout.alignment: Qt.AlignBottom
                Layout.bottomMargin: 4
                width: 32
                height: 32
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
                         : (Theme.isDark && Theme.colorTheme === "black" ? "#101010" : "#FFFFFF")
                }
                MouseArea {
                    id: sendMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    enabled: input.text.trim().length > 0
                    onClicked: root.submit()
                }
            }
        }
    }

    // 工具行：附件、项目、模式选择器、思考、右侧模型。
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
            tooltip: "附加文件或材料包"
            onClicked: root.attachRequested()
        }

        ToolbarChip {
            iconName: "plan"
            label: "项目评审"
            tooltip: "立即运行项目材料审计"
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
                Icon { name: "chevronDown"; size: 8; color: Theme.textMuted }
            }
            MouseArea {
                id: modelMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: modelMenu.open()
            }
            ToolTip.visible: modelMouse.containsMouse
            ToolTip.text: "选择大模型"
            Menu {
                id: modelMenu
                y: -implicitHeight
                x: parent.width - implicitWidth
                MenuItem {
                    text: "DeepSeek V4 Flash"
                    checkable: true
                    checked: root.currentModel === "deepseek-v4-flash"
                    onTriggered: root.modelSelected("deepseek-v4-flash")
                }
                MenuItem {
                    text: "DeepSeek Chat"
                    checkable: true
                    checked: root.currentModel === "deepseek-chat"
                    onTriggered: root.modelSelected("deepseek-chat")
                }
                MenuItem {
                    text: "DeepSeek Reasoner"
                    checkable: true
                    checked: root.currentModel === "deepseek-reasoner"
                    onTriggered: root.modelSelected("deepseek-reasoner")
                }
            }
        }
    }

    // 工具行上的通用小图标按钮。
    component ToolbarChip: Rectangle {
        id: chip
        property string iconName: ""
        property string label: ""
        property string tooltip: ""
        signal clicked()
        Layout.alignment: Qt.AlignVCenter
        implicitWidth: chipRow.implicitWidth + (label.length > 0 ? 16 : 12)
        implicitHeight: 28
        radius: Theme.radiusSm
        color: chipMouse.containsMouse ? Theme.surfaceMuted : "transparent"
        RowLayout {
            id: chipRow
            anchors.centerIn: parent
            spacing: 5
            Icon { name: iconName; size: 14; color: Theme.textTertiary }
            Text {
                visible: label.length > 0
                text: label
                color: Theme.textMuted
                font.pixelSize: Theme.fontXs
            }
        }
        MouseArea {
            id: chipMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: chip.clicked()
        }
        ToolTip.visible: chipMouse.containsMouse && chip.tooltip.length > 0
        ToolTip.text: chip.tooltip
    }
}
