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
    property bool ok: true

    height: implicitHeight
    implicitHeight: layout.implicitHeight

    readonly property bool isUser: kind === "user"
    readonly property bool isTool: kind === "tool"
    readonly property bool isPlan: kind === "plan"
    readonly property bool isSystem: kind === "system"
    readonly property bool isArtifact: kind === "artifact"

    RowLayout {
        id: layout
        width: parent.width
        spacing: 14
        layoutDirection: root.isUser ? Qt.RightToLeft : Qt.LeftToRight

        Rectangle {
            Layout.alignment: Qt.AlignTop
            visible: !root.isUser && !root.isSystem && !root.isTool
            width: 38
            height: 38
            radius: 12
            color: Theme.isDark ? "#F0F0F0" : "#000000"
            Text {
                anchors.centerIn: parent
                text: "</>"
                color: Theme.isDark ? "#000000" : "#FFFFFF"
                font.family: Theme.monoFamily
                font.pixelSize: 13
                font.bold: true
            }
        }

        Loader {
            id: content
            Layout.fillWidth: true
            sourceComponent: root.isUser ? userBubble
                           : root.isTool ? toolLine
                           : root.isPlan ? planCard
                           : root.isSystem ? systemLine
                           : root.isArtifact ? artifactCard
                           : assistantText
        }
    }

    Component {
        id: userBubble
        Item {
            width: content.width
            implicitHeight: bubble.implicitHeight
            Rectangle {
                id: bubble
                anchors.right: parent.right
                width: Math.min(parent.width * 0.72, Math.max(180, userText.implicitWidth + 34))
                implicitHeight: userText.implicitHeight + 22
                radius: 20
                color: Theme.userBubble
                Text {
                    id: userText
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    text: root.text
                    color: Theme.userBubbleText
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontLg
                    wrapMode: Text.WordWrap
                    lineHeight: 1.35
                }
            }
        }
    }

    Component {
        id: assistantText
        Column {
            width: content.width
            spacing: 12
            Text {
                width: parent.width
                text: root.text
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontXl
                wrapMode: Text.WordWrap
                lineHeight: 1.45
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
        RowLayout {
            width: content.width
            spacing: 10
            Text {
                text: "›"
                color: Theme.textMuted
                font.pixelSize: 22
                Layout.alignment: Qt.AlignTop
            }
            Text {
                Layout.fillWidth: true
                text: root.text + (root.detail.length > 0 ? "  " + root.detail : "")
                color: root.ok ? Theme.textMuted : Theme.danger
                font.pixelSize: Theme.fontMd
                wrapMode: Text.WordWrap
                lineHeight: 1.3
            }
            Text {
                text: root.ok ? "✓" : "!"
                color: root.ok ? Theme.success : Theme.danger
                font.pixelSize: Theme.fontMd
                font.bold: true
            }
        }
    }

    Component {
        id: systemLine
        RowLayout {
            width: content.width
            spacing: 8
            Text { text: "•"; color: root.ok ? Theme.textMuted : Theme.danger; font.pixelSize: Theme.fontLg }
            Text {
                Layout.fillWidth: true
                text: root.text
                color: root.ok ? Theme.textMuted : Theme.danger
                font.pixelSize: Theme.fontMd
                wrapMode: Text.WordWrap
            }
        }
    }

    Component {
        id: planCard
        Rectangle {
            width: Math.min(content.width, 820)
            implicitHeight: planCol.implicitHeight + 26
            radius: 16
            color: Theme.surface
            border.color: Theme.borderStrong
            border.width: 1
            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 3
                radius: 2
                color: Theme.accent
            }
            ColumnLayout {
                id: planCol
                anchors.fill: parent
                anchors.margins: 18
                spacing: 14
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "⌄  审查智能体的计划"
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontLg
                        font.bold: true
                    }
                    Pill {
                        text: root.context.length > 0 ? root.context : "计划"
                        bg: Theme.surfaceMuted
                        fg: Theme.textMuted
                    }
                }
                Text {
                    Layout.fillWidth: true
                    text: root.text
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontXl
                    font.bold: true
                    wrapMode: Text.WordWrap
                    lineHeight: 1.35
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Rectangle {
                        implicitWidth: approveText.implicitWidth + 28
                        implicitHeight: 40
                        radius: 12
                        color: Theme.accent
                        Text {
                            id: approveText
                            anchors.centerIn: parent
                            text: "✓  批准并执行"
                            color: Theme.isDark && Theme.colorTheme === "black" ? "#101010" : "#FFFFFF"
                            font.pixelSize: Theme.fontMd
                            font.bold: true
                        }
                    }
                    Rectangle {
                        implicitWidth: reviseText.implicitWidth + 28
                        implicitHeight: 40
                        radius: 12
                        color: Theme.surfaceMuted
                        border.color: Theme.border
                        Text {
                            id: reviseText
                            anchors.centerIn: parent
                            text: "告诉智能体如何修改"
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontMd
                            font.bold: true
                        }
                    }
                    Item { Layout.fillWidth: true }
                }
            }
        }
    }

    Component {
        id: artifactCard
        Rectangle {
            width: Math.min(content.width, 780)
            implicitHeight: artCol.implicitHeight + 20
            radius: 14
            color: Theme.surface
            border.color: Theme.border
            ColumnLayout {
                id: artCol
                anchors.fill: parent
                anchors.margins: 12
                spacing: 5
                Text {
                    text: root.context.length > 0 ? root.context : "产物"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSm
                    font.bold: true
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
