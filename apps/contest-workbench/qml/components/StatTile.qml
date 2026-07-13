import QtQuick
import QtQuick.Layouts
import ".."

Rectangle {
    id: root
    required property string label
    required property string value
    property string suffix: ""
    property string hint: ""
    property color accent: Theme.textPrimary
    property color tint: Theme.surface

    Layout.fillWidth: true
    Layout.preferredHeight: 112
    radius: Theme.radiusMd
    color: tint
    border.color: Theme.borderSubtle
    border.width: 1
    Behavior on color { ColorAnimation { duration: Theme.normal } }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 7

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Rectangle {
                Layout.preferredWidth: 8
                Layout.preferredHeight: 8
                radius: 4
                color: root.accent
            }
            Text {
                Layout.fillWidth: true
                text: root.label
                color: Theme.textSecondary
                font.pixelSize: Theme.fontMd
                font.weight: Font.DemiBold
            }
            Text {
                visible: root.hint.length > 0
                text: root.hint
                color: Theme.textMuted
                font.pixelSize: Theme.fontSm
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 5
            Text {
                text: root.value
                color: root.accent
                font.pixelSize: Math.max(30, Theme.uiFontSize + 16)
                font.bold: true
            }
            Text {
                visible: root.suffix.length > 0
                text: root.suffix
                color: Theme.textMuted
                font.pixelSize: Theme.fontMd
                Layout.alignment: Qt.AlignBottom
                Layout.bottomMargin: 5
            }
            Item { Layout.fillWidth: true }
        }
    }
}
