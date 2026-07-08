import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: root
    required property var compiler
    signal openReport(int index)

    function sendComposerMessage() {
        var text = composer.text.trim()
        if (text.length === 0)
            return
        compiler.submitMessage(text)
        composer.text = ""
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.window

            ListView {
                id: historyList
                anchors.fill: parent
                anchors.leftMargin: 72
                anchors.rightMargin: 72
                anchors.topMargin: 38
                anchors.bottomMargin: 190
                clip: true
                spacing: 20
                model: root.compiler.sessionHistory
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {
                    width: 8
                    policy: ScrollBar.AsNeeded
                }
                onCountChanged: Qt.callLater(positionViewAtEnd)

                header: Item {
                    width: historyList.width
                    height: root.compiler.agentRunning ? 58 : 0
                    visible: root.compiler.agentRunning
                    Row {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 10
                        Text {
                            text: "/"
                            color: Theme.accent
                            font.pixelSize: 24
                            font.bold: true
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: root.compiler.currentAgentAction.length > 0
                                  ? root.compiler.currentAgentAction + "... (" + root.compiler.agentProgress + "%)"
                                  : "思考中..."
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontXl
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }

                delegate: ChatTurn {
                    width: historyList.width
                    kind: modelData.kind
                    role: modelData.role
                    text: modelData.text
                    context: modelData.context
                    detail: modelData.detail === undefined ? "" : modelData.detail
                    ok: modelData.ok === undefined ? true : modelData.ok
                }

                EmptyState {
                    anchors.fill: parent
                    visible: historyList.count === 0
                    text: "开始一次可信审计对话"
                    hint: "左侧添加竞赛项目后，可直接输入 /audit，或用 /plan 先让智能体拟定步骤。"
                }
            }

            Rectangle {
                id: composerDock
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 168
                color: Theme.window

                Composer {
                    id: composer
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: 72
                    anchors.rightMargin: 72
                    anchors.bottomMargin: 26
                    busy: root.compiler.agentRunning
                    accessMode: root.compiler.accessMode
                    onSubmit: root.sendComposerMessage()
                    onCommand: function(value) { root.compiler.submitMessage(value) }
                    onModeChange: function(value) { root.compiler.submitMessage("/" + value) }
                }
            }
        }
    }
}
