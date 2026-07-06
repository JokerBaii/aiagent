import QtQuick
import QtQuick.Controls
import ".."

TextField {
    id: control
    implicitHeight: 38
    color: Theme.textPrimary
    placeholderTextColor: Theme.textMuted
    selectByMouse: true
    leftPadding: 12
    rightPadding: 12

    background: Rectangle {
        radius: Theme.radiusSm
        color: Theme.surface
        border.color: control.activeFocus ? Theme.accent : Theme.borderStrong
        border.width: control.activeFocus ? 2 : 1
    }
}
