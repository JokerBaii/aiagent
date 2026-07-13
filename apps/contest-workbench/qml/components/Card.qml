import QtQuick
import ".."

Item {
    id: root
    default property alias content: body.data
    property int padding: 18
    property color color: Theme.surface
    property color borderColor: Theme.borderSubtle
    property int radius: Theme.radius
    property bool hoverable: false

    readonly property Item contentChild: body.children.length > 0 ? body.children[0] : null

    implicitWidth: (contentChild ? contentChild.implicitWidth : 0) + padding * 2
    implicitHeight: (contentChild ? contentChild.implicitHeight : 0) + padding * 2

    Rectangle {
        id: panel
        anchors.fill: parent
        radius: root.radius
        color: root.color
        border.color: root.borderColor
        border.width: 1
        scale: root.hoverable && hover.containsMouse ? 1.006 : 1
        Behavior on scale { NumberAnimation { duration: Theme.fast; easing.type: Easing.OutCubic } }
        Behavior on color { ColorAnimation { duration: Theme.normal } }
        Behavior on border.color { ColorAnimation { duration: Theme.normal } }
    }

    Item {
        id: body
        anchors.fill: parent
        anchors.margins: root.padding
    }

    MouseArea {
        id: hover
        anchors.fill: parent
        enabled: root.hoverable
        hoverEnabled: root.hoverable
        acceptedButtons: Qt.NoButton
    }
}
