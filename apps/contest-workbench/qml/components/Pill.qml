import QtQuick
import ".."

Rectangle {
    id: root
    property string text: ""
    property color bg: Theme.surfaceMuted
    property color fg: Theme.textSecondary

    implicitWidth: label.implicitWidth + 20
    implicitHeight: 22
    radius: height / 2
    color: bg
    Behavior on color { ColorAnimation { duration: Theme.normal } }

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.fg
        font.pixelSize: 12
        font.bold: true
    }
}
