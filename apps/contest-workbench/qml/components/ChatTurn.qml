pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import ".."

Item {
    id: root
    required property string kind
    required property string role
    required property string text
    required property string context
    property string detail: ""
    property string target: ""
    property bool ok: true
    signal approveRequested()
    signal reviseRequested()
    signal artifactRequested(string pageKey)

    height: implicitHeight
    implicitHeight: Math.max(root.showAvatar ? avatar.height : 0, contentHeight)
    readonly property Item loadedContent: content.item as Item
    readonly property real contentHeight: loadedContent ? loadedContent.implicitHeight : 0

    readonly property bool isUser: kind === "user"
    readonly property bool isTool: kind === "tool"
    readonly property bool isPlan: kind === "plan"
    readonly property bool isSystem: kind === "system"
    readonly property bool isArtifact: kind === "artifact"
    readonly property bool showAvatar: !root.isUser && !root.isSystem && !root.isTool
                                      && !root.isPlan && !root.isArtifact
    readonly property int contentIndent: 44

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

    Component {
        id: systemLine
        Item {
            width: content.width
            implicitHeight: systemCard.implicitHeight
            Rectangle {
                id: systemCard
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(parent.width, 680)
                implicitHeight: sysRow.implicitHeight + 16
                radius: Theme.radius
                color: root.ok ? Theme.surfaceMuted : Theme.dangerSoft
                border.color: root.ok ? Theme.border : Theme.danger
                border.width: 1
                RowLayout {
                    id: sysRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 8
                    spacing: 8
                    Icon {
                        name: root.ok ? "checkSmall" : "close"
                        size: 12
                        color: root.ok ? Theme.textMuted : Theme.danger
                        Layout.alignment: Qt.AlignTop
                        Layout.topMargin: 3
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.text
                        color: root.ok ? Theme.textMuted : Theme.danger
                        font.pixelSize: Theme.fontSm
                        wrapMode: Text.Wrap
                        lineHeight: 1.35
                        textFormat: Text.PlainText
                    }
                }
            }
        }
    }

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
                            ActionArea {
                                id: approveMouse
                                anchors.fill: parent
                                accessibleName: "执行当前计划"
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
                            ActionArea {
                                id: reviseMouse
                                anchors.fill: parent
                                accessibleName: "修改当前计划"
                                onClicked: root.reviseRequested()
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }
    }

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
                color: artifactMouse.containsMouse && root.target.length > 0
                       ? Theme.surfaceHover : Theme.surface
                border.color: artifactMouse.containsMouse && root.target.length > 0
                              ? Theme.accentGhost : Theme.border
                Behavior on color { ColorAnimation { duration: Theme.fast } }
                Behavior on border.color { ColorAnimation { duration: Theme.fast } }
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
                ActionArea {
                    id: artifactMouse
                    anchors.fill: parent
                    enabled: root.target.length > 0
                    accessibleName: root.target.length > 0 ? "打开" + root.text : "审计产物"
                    onClicked: root.artifactRequested(root.target)
                }
            }
        }
    }
}
