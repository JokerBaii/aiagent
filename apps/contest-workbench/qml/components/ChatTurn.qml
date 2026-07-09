import QtQuick
import QtQuick.Layouts
import ".."

// 会话流中的单条消息，按 kind 内联渲染：
// user 右侧气泡、assistant 左侧头像+全宽正文、tool 缩进单行、plan 强调卡片、
// system 居中胶囊、artifact 产物卡片。C++ 侧 WorkbenchSessionModels 已提供各字段。
Item {
    id: root
    required property string kind
    required property string role
    required property string text
    required property string context
    property string detail: ""
    property bool ok: true
    signal approveRequested()
    signal reviseRequested()

    height: implicitHeight
    implicitHeight: Math.max(root.showAvatar ? avatar.height : 0, contentHeight)
    readonly property real contentHeight: content.item ? content.item.implicitHeight : 0

    readonly property bool isUser: kind === "user"
    readonly property bool isTool: kind === "tool"
    readonly property bool isPlan: kind === "plan"
    readonly property bool isSystem: kind === "system"
    readonly property bool isArtifact: kind === "artifact"
    readonly property bool showAvatar: !root.isUser && !root.isSystem && !root.isTool
                                      && !root.isPlan && !root.isArtifact
    // 头像宽度 32 + 间距 12 = 44，tool/system 行缩进到正文左缘，与参考 ml-11 一致。
    readonly property int contentIndent: 44

    // 仅 assistant 显示 </> 头像；tool/system/plan/artifact 走缩进而非头像。
    AiAvatar {
        id: avatar
        x: 0
        y: 0
        visible: root.showAvatar
        size: 32
    }

    Loader {
        id: content
        x: root.showAvatar ? root.contentIndent : 0
        y: 0
        width: Math.max(1, root.width - x)
        height: root.contentHeight
        sourceComponent: root.isUser ? userBubble
                       : root.isTool ? toolLine
                       : root.isPlan ? planCard
                       : root.isSystem ? systemLine
                       : root.isArtifact ? artifactCard
                       : assistantText
    }

    // user：右对齐气泡，rounded-2xl 且右下角收窄，shadow-md。
    Component {
        id: userBubble
        Item {
            width: content.width
            implicitHeight: bubble.implicitHeight
            Rectangle {
                id: bubble
                anchors.right: parent.right
                width: Math.min(parent.width * 0.75, Math.max(140, userText.implicitWidth + 28))
                implicitHeight: userText.implicitHeight + 20
                radius: Theme.radiusMd
                bottomRightRadius: 6
                color: Theme.userBubble
                Text {
                    id: userText
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    anchors.topMargin: 10
                    text: root.text
                    color: Theme.userBubbleText
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontLg
                    wrapMode: Text.WordWrap
                    lineHeight: 1.4
                    textFormat: Text.PlainText
                }
            }
        }
    }

    // assistant：全宽正文，text-base leading-relaxed，可带次要说明行。
    Component {
        id: assistantText
        Column {
            width: content.width
            spacing: 10
            Text {
                width: parent.width
                text: root.text.length > 0 ? root.text : root.detail
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontXl
                wrapMode: Text.WordWrap
                lineHeight: 1.5
                textFormat: Text.MarkdownText
            }
            Text {
                visible: root.context.length > 0
                text: root.context
                color: Theme.textMuted
                font.pixelSize: Theme.fontSm
            }
        }
    }

    // tool：缩进单行，展开箭头 + 工具图标 + 文案 + 结果指示，对齐参考 ToolUseMsg。
    Component {
        id: toolLine
        Item {
            width: content.width
            implicitHeight: toolRow.implicitHeight
            RowLayout {
                id: toolRow
                x: root.contentIndent
                width: parent.width - root.contentIndent
                spacing: 8
                Icon {
                    name: "chevronRight"
                    size: 11
                    color: Theme.textTertiary
                    Layout.alignment: Qt.AlignTop
                    Layout.topMargin: 3
                }
                Icon {
                    name: "toolStack"
                    size: 13
                    color: Theme.textTertiary
                    Layout.alignment: Qt.AlignTop
                    Layout.topMargin: 2
                }
                Text {
                    Layout.fillWidth: true
                    text: root.text + (root.detail.length > 0 ? "  ·  " + root.detail : "")
                    color: root.ok ? Theme.textMuted : Theme.danger
                    font.pixelSize: Theme.fontMd
                    wrapMode: Text.WordWrap
                    lineHeight: 1.3
                }
                Icon {
                    name: root.ok ? "checkSmall" : "close"
                    size: 12
                    color: root.ok ? Theme.success : Theme.danger
                    Layout.alignment: Qt.AlignTop
                    Layout.topMargin: 3
                }
            }
        }
    }

    // system：居中胶囊反馈（模式切换 / 命令结果），对齐参考 CommandFeedbackMsg。
    Component {
        id: systemLine
        Item {
            width: content.width
            implicitHeight: 34
            Rectangle {
                anchors.centerIn: parent
                width: Math.min(content.width, sysRow.implicitWidth + 28)
                height: 30
                radius: 15
                color: root.ok ? Theme.surfaceMuted : Theme.dangerSoft
                border.color: root.ok ? Theme.border : Theme.danger
                border.width: 1
                RowLayout {
                    id: sysRow
                    anchors.centerIn: parent
                    spacing: 7
                    Icon {
                        name: root.ok ? "checkSmall" : "close"
                        size: 12
                        color: root.ok ? Theme.textMuted : Theme.danger
                    }
                    Text {
                        text: root.text
                        color: root.ok ? Theme.textMuted : Theme.danger
                        font.pixelSize: Theme.fontSm
                        elide: Text.ElideRight
                        Layout.maximumWidth: content.width - 60
                    }
                }
            }
        }
    }

    // plan：强调左边框 + 渐隐底色的计划卡片，含批准/修改动作，对齐 PlanReviewCard。
    Component {
        id: planCard
        Item {
            width: content.width
            implicitHeight: planCard2.implicitHeight
            Rectangle {
                id: planCard2
                x: root.contentIndent
                width: Math.min(parent.width - root.contentIndent, 760)
                implicitHeight: planCol.implicitHeight + 32
                radius: Theme.radius
                color: Theme.surface
                border.color: Theme.accentGhost
                border.width: 1
                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 3
                    topLeftRadius: Theme.radius
                    bottomLeftRadius: Theme.radius
                    color: Theme.accent
                }
                ColumnLayout {
                    id: planCol
                    anchors.fill: parent
                    anchors.margins: 16
                    anchors.leftMargin: 18
                    spacing: 12
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Icon { name: "list"; size: 14; color: Theme.accent }
                        Text {
                            text: "审查智能体的计划"
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontLg
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Pill {
                            text: root.context.length > 0 ? root.context : "计划"
                            bg: Theme.accentSoft
                            fg: Theme.accent
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.text
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontLg
                        wrapMode: Text.WordWrap
                        lineHeight: 1.45
                    }
                    RowLayout {
                        visible: root.context === "计划模式"
                        Layout.fillWidth: true
                        spacing: 10
                        Rectangle {
                            implicitWidth: approveRow.implicitWidth + 24
                            implicitHeight: 38
                            radius: Theme.radiusSm
                            color: approveMouse.containsMouse ? Theme.accentHover : Theme.accent
                            RowLayout {
                                id: approveRow
                                anchors.centerIn: parent
                                spacing: 6
                                Icon { name: "check"; size: 13; color: Theme.isDark && Theme.colorTheme === "black" ? "#101010" : "#FFFFFF" }
                                Text {
                                    text: "执行计划"
                                    color: Theme.isDark && Theme.colorTheme === "black" ? "#101010" : "#FFFFFF"
                                    font.pixelSize: Theme.fontMd
                                    font.bold: true
                                }
                            }
                            MouseArea {
                                id: approveMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.approveRequested()
                            }
                        }
                        Rectangle {
                            implicitWidth: reviseText.implicitWidth + 28
                            implicitHeight: 38
                            radius: Theme.radiusSm
                            color: reviseMouse.containsMouse ? Theme.surfaceHover : Theme.surfaceMuted
                            border.color: Theme.border
                            Text {
                                id: reviseText
                                anchors.centerIn: parent
                                text: "修改计划"
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontMd
                                font.bold: true
                            }
                            MouseArea {
                                id: reviseMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.reviseRequested()
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }
    }

    // artifact：产物卡片，缩进对齐正文。
    Component {
        id: artifactCard
        Item {
            width: content.width
            implicitHeight: artCard2.implicitHeight
            Rectangle {
                id: artCard2
                x: root.contentIndent
                width: Math.min(parent.width - root.contentIndent, 720)
                implicitHeight: artCol.implicitHeight + 20
                radius: Theme.radius
                color: Theme.surface
                border.color: Theme.border
                ColumnLayout {
                    id: artCol
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 5
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 7
                        Icon { name: "file"; size: 13; color: Theme.textMuted }
                        Text {
                            text: root.context.length > 0 ? root.context : "产物"
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSm
                            font.bold: true
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.text
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontMd
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}
