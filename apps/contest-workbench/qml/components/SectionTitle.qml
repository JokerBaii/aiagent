import QtQuick
import ".."

Column {
    id: root
    property string title: ""
    property string subtitle: ""
    spacing: 2

    Text {
        text: root.title
        color: Theme.textPrimary
        font.pixelSize: 16
        font.bold: true
    }
    Text {
        visible: root.subtitle.length > 0
        text: root.subtitle
        color: Theme.textSecondary
        font.pixelSize: 13
    }
}
