import QtQuick
import QtQuick.Controls
import ".."

Button {
    id: control
    implicitHeight: 38
    implicitWidth: Math.max(96, label.implicitWidth + 30)
    padding: 0

    contentItem: Text {
        id: label
        text: control.text
        color: control.enabled ? Theme.textPrimary : Theme.textMuted
        font.pixelSize: 14
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radiusSm
        color: control.down ? Theme.surfaceHover
             : control.hovered ? Theme.surfaceMuted
             : Theme.surface
        border.color: Theme.borderStrong
        border.width: 1
        scale: control.down ? 0.985 : control.hovered ? 1.01 : 1
        Behavior on color { ColorAnimation { duration: Theme.fast } }
        Behavior on scale { NumberAnimation { duration: Theme.fast; easing.type: Easing.OutCubic } }
    }
}
