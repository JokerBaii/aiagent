pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: root
    required property var compiler
    property bool dropActive: false
    property bool autoFollowConversation: true
    signal attachProjectRequested()
    signal artifactRequested(string pageKey)

    function sendComposerMessage() {
        var text = composer.text.trim()
        if (text.length === 0)
            return
        composer.text = ""
        compiler.submitMessage(text)
    }

    function focusComposer(prefix) {
        composer.text = prefix || ""
        composer.focusInput()
    }

    function focusComposerInput() {
        composer.focusInput()
    }

    function nearConversationEnd() {
        return historyList.contentHeight <= historyList.height
                || historyList.contentY + historyList.height
                   >= historyList.contentHeight - 72
    }

    function followConversationIfNeeded() {
        if (!autoFollowConversation)
            return
        Qt.callLater(function() { historyList.positionViewAtEnd() })
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
                anchors.leftMargin: Math.max(48, (parent.width - 760) / 2)
                anchors.rightMargin: Math.max(48, (parent.width - 760) / 2)
                anchors.topMargin: 34
                anchors.bottomMargin: 188
                clip: true
                spacing: 22
                model: root.compiler.sessionHistory
                boundsBehavior: Flickable.StopAtBounds
                onMovementStarted: root.autoFollowConversation = false
                onMovementEnded: root.autoFollowConversation = root.nearConversationEnd()
                onContentHeightChanged: root.followConversationIfNeeded()
                ScrollBar.vertical: ScrollBar {
                    width: 8
                    policy: ScrollBar.AsNeeded
                }
                onCountChanged: root.followConversationIfNeeded()

                add: Transition {
                    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.normal }
                    NumberAnimation { property: "x"; from: 12; to: 0; duration: Theme.normal; easing.type: Easing.OutCubic }
                }

                footer: ThinkingIndicator {
                    width: historyList.width
                    running: root.compiler.agentRunning || root.compiler.advisoryRunning
                    action: root.compiler.currentAgentAction
                    progress: root.compiler.agentProgress
                    onStopRequested: root.compiler.cancelCurrentJob()
                }

                delegate: ChatTurn {
                    required property var modelData

                    width: historyList.width
                    kind: modelData.kind || "assistant"
                    role: modelData.role || ""
                    text: modelData.text || ""
                    context: modelData.context || ""
                    detail: modelData.detail === undefined ? "" : modelData.detail
                    target: modelData.target === undefined ? "" : modelData.target
                    ok: modelData.ok === undefined ? true : modelData.ok
                    onApproveRequested: root.compiler.approvePendingPlan()
                    onReviseRequested: root.focusComposer("请修改刚才的计划：")
                    onArtifactRequested: function(pageKey) { root.artifactRequested(pageKey) }
                }

                EmptyState {
                    anchors.fill: parent
                    visible: historyList.count === 0
                    text: "拖入项目开始缺点评审"
                    hint: "支持竞赛、大创、课程与毕业设计，以及论文、专利、软著等成果材料。"
                }
            }

            Rectangle {
                id: composerDock
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 150
                color: Theme.window

                Composer {
                    id: composer
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: Math.max(48, (parent.width - 760) / 2)
                    anchors.rightMargin: Math.max(48, (parent.width - 760) / 2)
                    anchors.bottomMargin: 20
                    busy: root.compiler.agentRunning || root.compiler.advisoryRunning
                    currentModel: root.compiler.llmModel
                    onSubmit: root.sendComposerMessage()
                    onCommand: function(value) { root.compiler.submitMessage(value) }
                    onAttachRequested: root.attachProjectRequested()
                    onAuditRequested: root.compiler.submitMessage("/audit")
                    onPlanRequested: root.focusComposer("/plan ")
                    onRewindRequested: root.compiler.rewindLastTurn()
                }
            }

            Rectangle {
                anchors.right: composerDock.right
                anchors.rightMargin: Math.max(48, (parent.width - 760) / 2)
                anchors.bottom: composerDock.top
                anchors.bottomMargin: 8
                width: jumpBottomText.implicitWidth + 26
                height: 34
                radius: 17
                visible: !root.autoFollowConversation && historyList.count > 0
                color: jumpBottomMouse.containsMouse ? Theme.surfaceHover : Theme.surface
                border.color: Theme.border
                opacity: visible ? 1 : 0

                Behavior on opacity { NumberAnimation { duration: Theme.fast } }

                Text {
                    id: jumpBottomText
                    anchors.centerIn: parent
                    text: "回到最新消息"
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSm
                    font.bold: true
                }
                ActionArea {
                    id: jumpBottomMouse
                    anchors.fill: parent
                    accessibleName: "回到最新消息"
                    onClicked: {
                        root.autoFollowConversation = true
                        historyList.positionViewAtEnd()
                    }
                }
            }

            Connections {
                target: root.compiler
                function onAgentStateChanged() { root.followConversationIfNeeded() }
                function onSessionChanged() { root.followConversationIfNeeded() }
            }
        }
    }

    DropArea {
        anchors.fill: parent
        onEntered: root.dropActive = true
        onExited: root.dropActive = false
        onDropped: function(drop) {
            root.dropActive = false
            if (drop.urls.length > 0)
                root.compiler.selectProject(drop.urls[0])
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: root.dropActive
        color: Qt.rgba(0, 0, 0, Theme.isDark ? 0.28 : 0.16)
        border.color: Theme.accent
        border.width: 2
        z: 10
        Text {
            anchors.centerIn: parent
            text: "释放后开始评审"
            color: Theme.accent
            font.pixelSize: Theme.fontTitle
            font.bold: true
        }
    }

    component ThinkingIndicator: Item {
        id: indicator
        property bool running: false
        property string action: ""
        property int progress: 0
        property int wordIndex: 0
        property string displayText: ""
        property int phase: 0
        signal stopRequested()
        readonly property var words: [
            "思考中",
            "计算中",
            "核对材料",
            "读取项目文件",
            "检查证据",
            "整理报告"
        ]
        readonly property bool cycling: action.length === 0 || action === "思考中"
        readonly property string activeWord: cycling ? words[wordIndex] : action
        readonly property string visibleText: cycling ? displayText : activeWord

        width: parent ? parent.width : implicitWidth
        implicitHeight: 48
        height: running ? implicitHeight : 0
        visible: height > 0
        clip: true

        function resetTyping() {
            displayText = ""
            phase = 0
        }

        onRunningChanged: {
            resetTyping()
        }
        onActionChanged: {
            if (running)
                resetTyping()
        }

        Timer {
            id: typingTimer
            interval: indicator.phase === 1 ? 1400 : (indicator.phase === 2 ? 44 : 76)
            running: indicator.running && indicator.cycling
            repeat: true
            onTriggered: {
                if (indicator.phase === 0) {
                    if (indicator.displayText.length < indicator.activeWord.length) {
                        indicator.displayText = indicator.activeWord.slice(0, indicator.displayText.length + 1)
                    } else {
                        indicator.phase = 1
                    }
                    return
                }
                if (indicator.phase === 1) {
                    indicator.phase = 2
                    return
                }
                if (indicator.displayText.length > 0) {
                    indicator.displayText = indicator.displayText.slice(0, indicator.displayText.length - 1)
                    return
                }
                indicator.wordIndex = (indicator.wordIndex + 1) % indicator.words.length
                indicator.phase = 0
            }
        }

        Behavior on height { NumberAnimation { duration: Theme.normal; easing.type: Easing.OutCubic } }

        RowLayout {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            Text {
                text: "/"
                color: Theme.accent
                font.pixelSize: Theme.fontXl
                font.bold: true
                Layout.alignment: Qt.AlignVCenter

                SequentialAnimation on opacity {
                    running: indicator.running
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.42; duration: 520; easing.type: Easing.InOutQuad }
                    NumberAnimation { to: 1.0; duration: 520; easing.type: Easing.InOutQuad }
                }
            }

            RowLayout {
                spacing: 6
                Layout.alignment: Qt.AlignVCenter

                Text {
                    text: indicator.visibleText.length > 0 ? indicator.visibleText : indicator.activeWord
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontMd
                    verticalAlignment: Text.AlignVCenter
                }

                Row {
                    spacing: 3
                    Layout.alignment: Qt.AlignVCenter
                    Repeater {
                        model: 3
                        Rectangle {
                            id: dotDelegate
                            required property int index

                            width: 3
                            height: 3
                            radius: 2
                            color: Theme.textTertiary
                            opacity: 0.28
                            anchors.verticalCenter: parent.verticalCenter

                            SequentialAnimation on opacity {
                                running: indicator.running
                                loops: Animation.Infinite
                                PauseAnimation { duration: dotDelegate.index * 120 }
                                NumberAnimation { to: 1.0; duration: 220; easing.type: Easing.InOutQuad }
                                NumberAnimation { to: 0.28; duration: 360; easing.type: Easing.InOutQuad }
                                PauseAnimation { duration: 240 }
                            }
                        }
                    }
                }

                Text {
                    visible: indicator.progress > 0 && indicator.progress < 100
                    text: "(" + indicator.progress + "%)"
                    color: Theme.textTertiary
                    font.pixelSize: Theme.fontXs
                    Layout.alignment: Qt.AlignVCenter
                }
            }

            Rectangle {
                Layout.preferredWidth: 54
                Layout.preferredHeight: 28
                radius: Theme.radiusSm
                color: stopMouse.containsMouse ? Theme.surfaceHover : Theme.surface
                border.color: Theme.border

                Text {
                    anchors.centerIn: parent
                    text: "停止"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSm
                    font.bold: true
                }
                ActionArea {
                    id: stopMouse
                    anchors.fill: parent
                    accessibleName: "停止当前任务"
                    onClicked: indicator.stopRequested()
                }
            }
        }
    }
}
