import QtQuick
import QtQuick.Layouts
import ".."

Rectangle {
    id: root
    required property string label
    required property string value
    property color accent: Theme.textPrimary
    property color tint: Theme.surface

    Layout.fillWidth: true
    Layout.preferredHeight: 96
    radius: Theme.radius
    color: tint
    border.color: Theme.border
    border.width: 1
    Behavior on color { ColorAnimation { duration: Theme.normal } }

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        spacing: 4

        Text {
            text: root.value
            color: root.accent
            font.pixelSize: 30
            font.bold: true
        }
        Text {
            text: root.label
            color: Theme.textSecondary
            font.pixelSize: 13
        }
    }

    Rectangle {
        width: 4
        radius: 2
        color: root.accent
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: 14
    }
}
