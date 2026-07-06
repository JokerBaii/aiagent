import QtQuick
import QtQuick.Controls
import ".."

Button {
    id: control
    implicitHeight: 38
    implicitWidth: Math.max(112, label.implicitWidth + 34)
    padding: 0

    contentItem: Text {
        id: label
        text: control.text
        color: "#FFFFFF"
        font.pixelSize: 14
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radiusSm
        color: !control.enabled ? Theme.borderStrong
             : control.down ? Theme.accentActive
             : control.hovered ? Theme.accentHover
             : Theme.accent
        scale: control.down ? 0.985 : control.hovered ? 1.015 : 1
        Behavior on color { ColorAnimation { duration: Theme.fast } }
        Behavior on scale { NumberAnimation { duration: Theme.fast; easing.type: Easing.OutCubic } }
    }
}
