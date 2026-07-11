import QtQuick
import ".."

MouseArea {
    id: control

    property string accessibleName: ""

    hoverEnabled: true
    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    activeFocusOnTab: enabled && visible

    Accessible.role: Accessible.Button
    Accessible.name: accessibleName
    Accessible.focusable: activeFocusOnTab

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                || event.key === Qt.Key_Space) {
            control.clicked(null)
            event.accepted = true
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: Math.max(0, Math.min(width, height) / 5)
        color: "transparent"
        border.width: control.activeFocus ? 2 : 0
        border.color: Theme.accent
        visible: control.activeFocus
    }
}
