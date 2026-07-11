import QtQuick
import ".."

Item {
    id: root
    property string text: "暂无数据"
    property string hint: "运行审计后在此查看结果。"

    Column {
        anchors.centerIn: parent
        spacing: 6
        width: Math.min(root.width - 40, 360)

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.text
            color: Theme.textSecondary
            font.pixelSize: Theme.fontXl
            font.bold: true
        }
        Text {
            width: parent.width
            text: root.hint
            color: Theme.textMuted
            font.pixelSize: Theme.fontMd
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
    }
}
