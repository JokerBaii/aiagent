import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: root
    required property var compiler

    function sendComposerMessage() {
        var text = composer.text.trim()
        if (text.length === 0)
            return
        compiler.submitMessage(text)
        composer.text = ""
    }

    function startAudit() {
        root.compiler.runAudit()
    }

    function statusStyle(status) {
        if (status === "完成")
            return { bg: Theme.successSoft, fg: Theme.success, border: Theme.border }
        if (status === "进行中")
            return { bg: Theme.accentSoft, fg: Theme.accentActive, border: Theme.accent }
        if (status === "可导出" || status === "可追问")
            return { bg: Theme.surfaceMuted, fg: Theme.textPrimary, border: Theme.border }
        return { bg: Theme.warningSoft, fg: Theme.warning, border: Theme.border }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 20

        // ============ 左：对话 + composer ============
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 16

            // 项目导入行
            Card {
                Layout.fillWidth: true
                padding: 14
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        FieldInput {
                            id: projectInput
                            Layout.fillWidth: true
                            text: root.compiler.projectPath
                            placeholderText: "拖拽项目目录或压缩包到窗口，或在此输入项目路径"
                            onTextChanged: root.compiler.projectPath = text
                        }
                        PrimaryButton {
                            enabled: !root.compiler.agentRunning
                            text: root.compiler.agentRunning ? "审计中" : "开始审计"
                            onClicked: root.startAudit()
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: root.compiler.agentRunning
                        spacing: 6
                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                Layout.fillWidth: true
                                text: root.compiler.currentAgentAction
                                color: Theme.textSecondary
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }
                            Text {
                                text: root.compiler.agentProgress + "%"
                                color: Theme.textMuted
                                font.pixelSize: 12
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 5
                            radius: 2
                            color: Theme.surfaceMuted
                            Rectangle {
                                height: parent.height
                                radius: 2
                                color: Theme.accent
                                width: parent.width * root.compiler.agentProgress / 100
                                Behavior on width {
                                    NumberAnimation { duration: Theme.normal; easing.type: Easing.OutCubic }
                                }
                            }
                        }
                    }
                }
            }

            // 对话流
            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                padding: 0

                ListView {
                    id: historyList
                    anchors.fill: parent
                    anchors.margins: 16
                    clip: true
                    spacing: 14
                    model: root.compiler.sessionHistory
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}
                    add: Transition {
                        NumberAnimation { properties: "opacity"; from: 0; to: 1; duration: Theme.normal }
                        NumberAnimation { properties: "y"; from: 12; duration: Theme.normal; easing.type: Easing.OutCubic }
                    }
                    displaced: Transition {
                        NumberAnimation { properties: "y"; duration: Theme.normal; easing.type: Easing.OutCubic }
                    }

                    delegate: RowLayout {
                        id: turn
                        width: historyList.width
                        spacing: 12
                        readonly property bool isUser: modelData.role === "用户"

                        // 头像
                        Rectangle {
                            Layout.alignment: Qt.AlignTop
                            width: 30; height: 30; radius: 9
                            color: turn.isUser ? Theme.surfaceMuted : Theme.accent
                            border.color: turn.isUser ? Theme.borderStrong : Theme.accent
                            border.width: 1
                            Text {
                                anchors.centerIn: parent
                                text: turn.isUser ? "你" : "AI"
                                color: turn.isUser ? Theme.textSecondary : "#FFFFFF"
                                font.pixelSize: 12
                                font.bold: true
                            }
                        }

                        // 内容
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Text {
                                    text: turn.isUser ? "你" : "竞赛审计智能体"
                                    color: Theme.textPrimary
                                    font.pixelSize: 13
                                    font.bold: true
                                }
                                Item { Layout.fillWidth: true }
                                Text {
                                    text: modelData.context
                                    color: Theme.textMuted
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    Layout.maximumWidth: 220
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: modelData.text
                                color: Theme.textPrimary
                                font.pixelSize: 14
                                wrapMode: Text.WordWrap
                                lineHeight: 1.35
                            }
                        }
                    }

                    // 空状态
                    EmptyState {
                        anchors.fill: parent
                        visible: historyList.count === 0
                        text: "开始一次可信审计对话"
                        hint: "导入竞赛项目并开始审计后，可在此追问风险、证据缺口与补证优先级。"
                    }
                }
            }

            // composer
            Card {
                Layout.fillWidth: true
                padding: 12
                RowLayout {
                    anchors.fill: parent
                    spacing: 10
                    FieldInput {
                        id: composer
                        Layout.fillWidth: true
                        placeholderText: "询问当前项目风险、证据缺口或补证优先级…"
                        onAccepted: root.sendComposerMessage()
                    }
                    PrimaryButton {
                        text: "发送"
                        onClicked: root.sendComposerMessage()
                    }
                }
            }
        }

        // ============ 右：上下文侧栏 ============
        ScrollView {
            Layout.preferredWidth: 380
            Layout.fillHeight: true
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: 364
                spacing: 14

                // 项目上下文
                Card {
                    Layout.fillWidth: true
                    hoverable: true
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8
                        SectionTitle { title: "项目状态" }
                        Repeater {
                            model: [
                                ["当前材料", root.compiler.projectContext.originalRoot],
                                ["处理状态", root.compiler.projectContext.unpackStatus === "WAITING" ? "等待审计" : "已建立工作副本"],
                                ["材料数量", root.compiler.projectContext.inputFileCount > 0 ? root.compiler.projectContext.inputFileCount + " 个文件" : "等待扫描"],
                                ["会话", root.compiler.projectContext.sessionId === "未创建" ? "尚未开始" : "已创建"]
                            ]
                            delegate: RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Text {
                                    Layout.preferredWidth: 74
                                    text: modelData[0]
                                    color: Theme.textMuted
                                    font.pixelSize: 12
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: (modelData[1] === undefined || modelData[1] === "") ? "—" : modelData[1]
                                    color: Theme.textPrimary
                                    font.pixelSize: 12
                                    wrapMode: Text.WrapAnywhere
                                }
                            }
                        }
                    }
                }

                // 顾问摘要
                Card {
                    Layout.fillWidth: true
                    visible: root.compiler.advisorSummary.length > 0
                    color: Theme.accentSoft
                    borderColor: Theme.border
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 6
                        SectionTitle { title: "顾问摘要" }
                        Text {
                            Layout.fillWidth: true
                            text: root.compiler.advisorSummary
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                // 受控工具
                SectionTitle {
                    Layout.leftMargin: 4
                    title: "审计步骤"
                }
                Repeater {
                    model: root.compiler.toolCards
                    delegate: Card {
                        Layout.fillWidth: true
                        padding: 12
                        hoverable: true
                        borderColor: root.statusStyle(modelData.status).border
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 4
                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: modelData.name
                                    color: Theme.textPrimary
                                    font.pixelSize: 13
                                    font.bold: true
                                }
                                Item { Layout.fillWidth: true }
                                Pill {
                                    text: modelData.status
                                    bg: root.statusStyle(modelData.status).bg
                                    fg: root.statusStyle(modelData.status).fg
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.detail
                                color: Theme.textSecondary
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }

                // 权限
                SectionTitle {
                    Layout.leftMargin: 4
                    title: "安全边界"
                }
                Repeater {
                    model: root.compiler.permissionCards
                    delegate: Card {
                        Layout.fillWidth: true
                        padding: 12
                        color: modelData.allowed ? Theme.successSoft : Theme.dangerSoft
                        borderColor: Theme.border
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 4
                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: modelData.name
                                    color: Theme.textPrimary
                                    font.pixelSize: 13
                                    font.bold: true
                                }
                                Item { Layout.fillWidth: true }
                                Text {
                                    text: modelData.status
                                    color: modelData.allowed ? Theme.success : Theme.danger
                                    font.pixelSize: 12
                                    font.bold: true
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.detail
                                color: Theme.textSecondary
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }

                // artifact
                SectionTitle {
                    Layout.leftMargin: 4
                    visible: root.compiler.artifacts.length > 0
                    title: "产物"
                }
                Repeater {
                    model: root.compiler.artifacts
                    delegate: Card {
                        Layout.fillWidth: true
                        padding: 12
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 4
                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: modelData.title
                                    color: Theme.textPrimary
                                    font.pixelSize: 13
                                    font.bold: true
                                }
                                Item { Layout.fillWidth: true }
                                Pill { text: modelData.kind }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.detail
                                color: Theme.textSecondary
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
        }
    }
}
