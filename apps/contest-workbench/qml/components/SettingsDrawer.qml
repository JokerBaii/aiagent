pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."

Rectangle {
    id: root

    required property var compiler
    property bool opened: false
    signal closeRequested()

    readonly property var colorChoices: [
        { key: "black", label: "Black", color: "#333333" },
        { key: "blue", label: "Blue", color: "#4E80F7" },
        { key: "orange", label: "Orange", color: "#C47252" },
        { key: "green", label: "Green", color: "#57A64B" }
    ]
    readonly property var backgroundChoices: [
        { key: "minimal", label: "纯白简约", color: "#FFFFFF" },
        { key: "vscode", label: "VS Code Dark", color: "#1E1E1E" },
        { key: "garden", label: "暖白", color: "#FAF9F6" },
        { key: "sakura", label: "粉樱", color: "#FFF4F7" },
        { key: "lake", label: "湖蓝", color: "#F3FBF8" },
        { key: "dusk", label: "暮紫", color: "#F7F1FB" },
        { key: "ink", label: "墨纸", color: "#F8F5EC" }
    ]
    readonly property var fontChoices: [
        { key: "microsoft", label: "微软雅黑 UI" },
        { key: "system", label: "系统清晰字体" },
        { key: "sourceHan", label: "思源黑体 / Noto" },
        { key: "lxgw", label: "霞鹜文楷" },
        { key: "mono", label: "等宽字体" }
    ]
    readonly property var accessModeChoices: [
        { key: "full", label: "完全访问", hint: "直接读取和修改原项目，可执行 Shell/Bash 与网络请求" },
        { key: "plan", label: "仅规划", hint: "用户主动选择时只输出计划，不执行操作" }
    ]

    color: "#00000000"
    enabled: opened
    visible: opacity > 0
    opacity: opened ? 1 : 0
    z: 20

    Behavior on opacity { NumberAnimation { duration: Theme.normal } }

    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: 0.22
        ActionArea {
            anchors.fill: parent
            accessibleName: "关闭设置"
            onClicked: root.closeRequested()
        }
    }

    Rectangle {
        id: drawer
        width: Math.min(520, parent.width - 80)
        height: parent.height
        x: root.opened ? parent.width - width : parent.width
        color: Theme.surface
        border.color: Theme.border

        Behavior on x {
            NumberAnimation { duration: Theme.normal; easing.type: Easing.OutCubic }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 22
            spacing: 18

            RowLayout {
                Layout.fillWidth: true
                Text { Layout.fillWidth: true; text: "设置"; color: Theme.textPrimary; font.pixelSize: Theme.fontTitle; font.bold: true }
                Rectangle {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    radius: Theme.radiusSm
                    color: closeMouse.containsMouse ? Theme.surfaceMuted : "transparent"
                    Icon { anchors.centerIn: parent; name: "close"; size: 15; color: Theme.textMuted }
                    ActionArea {
                        id: closeMouse
                        anchors.fill: parent
                        accessibleName: "关闭设置"
                        onClicked: root.closeRequested()
                    }
                }
            }

            ScrollView {
                id: settingsScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth

                ColumnLayout {
                    width: settingsScroll.availableWidth
                    spacing: 18

                    SectionTitle { title: "DeepSeek 智能体（可选）"; subtitle: "不配置也能使用完整的本地规则检查" }
                    FieldInput {
                        Layout.fillWidth: true
                        text: root.compiler.llmEndpoint
                        placeholderText: "https://api.deepseek.com/chat/completions"
                        onTextEdited: root.compiler.llmEndpoint = text
                    }
                    FieldInput {
                        Layout.fillWidth: true
                        text: root.compiler.llmModel
                        placeholderText: "输入 DeepSeek 模型 ID"
                        onTextEdited: root.compiler.llmModel = text
                    }
                    FieldInput {
                        Layout.fillWidth: true
                        echoMode: TextInput.Password
                        text: root.compiler.llmApiKey
                        placeholderText: "访问密钥（仅在本次运行中使用）"
                        onActiveFocusChanged: if (activeFocus && text === "********") text = ""
                        onTextEdited: root.compiler.llmApiKey = text
                    }
                    PrimaryButton {
                        Layout.fillWidth: true
                        enabled: !root.compiler.llmModelsLoading
                        text: root.compiler.llmModelsLoading ? "正在获取可用模型…" : "按当前凭证获取模型"
                        onClicked: root.compiler.refreshLlmModels()
                    }
                    ComboBox {
                        Layout.fillWidth: true
                        visible: root.compiler.llmAvailableModels.length > 0
                        model: root.compiler.llmAvailableModels
                        currentIndex: root.compiler.llmAvailableModels.indexOf(root.compiler.llmModel)
                        onActivated: root.compiler.llmModel = currentText
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: root.compiler.llmModelsStatus.length > 0
                        text: root.compiler.llmModelsStatus
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontXs
                        wrapMode: Text.WordWrap
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: brainConnectionText.implicitHeight + 18
                        radius: 12
                        color: root.compiler.llmConfigured ? Theme.successSoft : Theme.warningSoft
                        border.color: root.compiler.llmConfigured ? Theme.success : Theme.warning
                        Text {
                            id: brainConnectionText
                            anchors.fill: parent
                            anchors.margins: 9
                            text: root.compiler.llmConfigured
                              ? (root.compiler.accessMode === "full"
                                 ? "配置有效；完全访问模式已开放文件、命令和网络等全部权限。"
                                 : "配置有效，项目任务将默认联网使用该模型；原始材料不会被修改，最终分数仍由本地规则计算。")
                                  : "填写有效的 HTTPS 服务地址、模型 ID 和访问密钥后，将自动启用联网模型。"
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSm
                            wrapMode: Text.WordWrap
                        }
                    }

                    SectionTitle { title: "外观" }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Repeater {
                            model: [
                                { key: "light", label: "浅色" },
                                { key: "dark", label: "深色" }
                            ]
                            delegate: Rectangle {
                                id: appearanceDelegate
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.preferredHeight: 38
                                radius: 12
                                color: Theme.appearance === modelData.key ? Theme.accentSoft : Theme.surfaceMuted
                                border.color: Theme.appearance === modelData.key ? Theme.accent : Theme.border
                                Text { anchors.centerIn: parent; text: appearanceDelegate.modelData.label; color: Theme.appearance === appearanceDelegate.modelData.key ? Theme.accent : Theme.textMuted; font.pixelSize: Theme.fontMd; font.bold: true }
                                ActionArea {
                                    anchors.fill: parent
                                    accessibleName: "切换为" + appearanceDelegate.modelData.label
                                    onClicked: Theme.appearance = appearanceDelegate.modelData.key
                                }
                            }
                        }
                    }

                    SectionTitle { title: "强调色" }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 4
                        rowSpacing: 10
                        columnSpacing: 10
                        Repeater {
                            model: root.colorChoices
                            delegate: Rectangle {
                                id: colorDelegate
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.preferredHeight: 72
                                radius: 14
                                color: Theme.colorTheme === modelData.key ? Theme.accentSoft : Theme.surfaceMuted
                                border.color: Theme.colorTheme === modelData.key ? Theme.accent : Theme.border
                                Column {
                                    anchors.centerIn: parent
                                    spacing: 7
                                    Rectangle { anchors.horizontalCenter: parent.horizontalCenter; width: 24; height: 24; radius: 12; color: colorDelegate.modelData.color }
                                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: colorDelegate.modelData.label; color: Theme.textPrimary; font.pixelSize: Theme.fontSm; font.bold: true }
                                }
                                ActionArea {
                                    anchors.fill: parent
                                    accessibleName: "选择强调色 " + colorDelegate.modelData.label
                                    onClicked: Theme.colorTheme = colorDelegate.modelData.key
                                }
                            }
                        }
                    }

                    SectionTitle { title: "背景风格" }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 3
                        rowSpacing: 10
                        columnSpacing: 10
                        Repeater {
                            model: root.backgroundChoices
                            delegate: Rectangle {
                                id: backgroundDelegate
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.preferredHeight: 78
                                radius: 14
                                color: Theme.backgroundTheme === modelData.key ? Theme.accentSoft : Theme.surfaceMuted
                                border.color: Theme.backgroundTheme === modelData.key ? Theme.accent : Theme.border
                                Column {
                                    anchors.centerIn: parent
                                    spacing: 8
                                    Rectangle {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        width: 72
                                        height: 30
                                        radius: 8
                                        color: backgroundDelegate.modelData.color
                                        border.color: Theme.border
                                        Rectangle { anchors.left: parent.left; width: 20; height: parent.height; radius: 8; color: Theme.backgroundTheme === "vscode" && backgroundDelegate.modelData.key === "vscode" ? "#252526" : Qt.rgba(0, 0, 0, 0.06) }
                                        Rectangle { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 5; width: 12; height: 12; radius: 4; color: Theme.accent }
                                    }
                                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: backgroundDelegate.modelData.label; color: Theme.textPrimary; font.pixelSize: Theme.fontSm; font.bold: true }
                                }
                                ActionArea {
                                    anchors.fill: parent
                                    accessibleName: "选择背景 " + backgroundDelegate.modelData.label
                                    onClicked: Theme.backgroundTheme = backgroundDelegate.modelData.key
                                }
                            }
                        }
                    }

                    SectionTitle { title: "字体" }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Rectangle {
                            Layout.preferredWidth: 34
                            Layout.preferredHeight: 34
                            radius: 10
                            color: Theme.surfaceMuted
                            Text { anchors.centerIn: parent; text: "−"; color: Theme.textPrimary; font.pixelSize: 18; font.bold: true }
                            ActionArea { anchors.fill: parent; accessibleName: "减小界面字体"; onClicked: Theme.uiFontSize = Math.max(12, Theme.uiFontSize - 1) }
                        }
                        Text { text: Theme.uiFontSize + "px"; color: Theme.textPrimary; font.pixelSize: Theme.fontMd; font.bold: true; horizontalAlignment: Text.AlignHCenter; Layout.preferredWidth: 54 }
                        Rectangle {
                            Layout.preferredWidth: 34
                            Layout.preferredHeight: 34
                            radius: 10
                            color: Theme.surfaceMuted
                            Text { anchors.centerIn: parent; text: "+"; color: Theme.textPrimary; font.pixelSize: 18; font.bold: true }
                            ActionArea { anchors.fill: parent; accessibleName: "增大界面字体"; onClicked: Theme.uiFontSize = Math.min(24, Theme.uiFontSize + 1) }
                        }
                    }
                    ComboBox {
                        Layout.fillWidth: true
                        model: root.fontChoices.map(function(item) { return item.label })
                        currentIndex: root.fontChoices.findIndex(function(item) { return item.key === Theme.fontPreset })
                        onActivated: function(index) { Theme.fontPreset = root.fontChoices[index].key }
                    }

                    SectionTitle { title: "权限边界" }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: 10
                        columnSpacing: 10
                        Repeater {
                            model: root.accessModeChoices
                            delegate: Rectangle {
                                id: accessDelegate
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.preferredHeight: 74
                                radius: 14
                                color: root.compiler.accessMode === modelData.key
                                       ? ((modelData.key === "bypass" || modelData.key === "full")
                                          ? Theme.warningSoft : Theme.accentSoft)
                                       : Theme.surfaceMuted
                                border.color: root.compiler.accessMode === modelData.key
                                              ? ((modelData.key === "bypass" || modelData.key === "full")
                                                 ? Theme.warning : Theme.accent)
                                              : Theme.border
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 3
                                    Text {
                                        Layout.fillWidth: true
                                        text: accessDelegate.modelData.label
                                        color: root.compiler.accessMode === accessDelegate.modelData.key
                                               ? ((accessDelegate.modelData.key === "bypass"
                                                   || accessDelegate.modelData.key === "full")
                                                  ? Theme.warning : Theme.accent)
                                               : Theme.textPrimary
                                        font.pixelSize: Theme.fontMd
                                        font.bold: true
                                    }
                                    Text { Layout.fillWidth: true; text: accessDelegate.modelData.hint; color: Theme.textMuted; font.pixelSize: Theme.fontXs; wrapMode: Text.WordWrap }
                                }
                                ActionArea {
                                    anchors.fill: parent
                                    accessibleName: "切换权限模式到 " + accessDelegate.modelData.label
                                    onClicked: root.compiler.accessMode = accessDelegate.modelData.key
                                }
                            }
                        }
                    }

                    Repeater {
                        model: root.compiler.permissionCards
                        delegate: Rectangle {
                            id: permissionDelegate
                            required property var modelData
                            Layout.fillWidth: true
                            implicitHeight: permCol.implicitHeight + 18
                            radius: 12
                            color: modelData.allowed ? Theme.successSoft : Theme.dangerSoft
                            border.color: Theme.border
                            ColumnLayout {
                                id: permCol
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 4
                                RowLayout {
                                    Layout.fillWidth: true
                                    Text { Layout.fillWidth: true; text: permissionDelegate.modelData.name; color: Theme.textPrimary; font.pixelSize: Theme.fontMd; font.bold: true }
                                    Text { text: permissionDelegate.modelData.status; color: permissionDelegate.modelData.allowed ? Theme.success : Theme.danger; font.pixelSize: Theme.fontSm; font.bold: true }
                                }
                                Text { Layout.fillWidth: true; text: permissionDelegate.modelData.detail; color: Theme.textMuted; font.pixelSize: Theme.fontSm; wrapMode: Text.WordWrap }
                            }
                        }
                    }

                    Item { Layout.preferredHeight: 16 }
                }
            }
        }
    }
}
