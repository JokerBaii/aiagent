import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ContestTrust
import "."
import "components"
import "pages"

ApplicationWindow {
    id: root
    width: 1320
    height: 860
    minimumWidth: 1040
    minimumHeight: 680
    visible: true
    title: "Contest Trust Workbench"
    color: Theme.window

    CompileController {
        id: compiler
    }

    readonly property var navItems: [
        { icon: "◉", label: "会话工作区" },
        { icon: "■", label: "可信仪表盘" },
        { icon: "▤", label: "资产清单" },
        { icon: "◈", label: "项目画像" },
        { icon: "◎", label: "声明证据" },
        { icon: "≡", label: "一致性" },
        { icon: "⚠", label: "规则风险" },
        { icon: "✓", label: "补证任务" },
        { icon: "⇄", label: "二次审计差分" },
        { icon: "✨", label: "LLM Brain" },
        { icon: "⤓", label: "报告导出" }
    ]
    property int currentIndex: 0

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ---------- 侧边栏 ----------
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 248
            color: Theme.sidebar

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 4

                // 品牌区
                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 6
                    Layout.bottomMargin: 14
                    spacing: 10

                    Rectangle {
                        width: 34
                        height: 34
                        radius: 9
                        color: Theme.accent
                        Text {
                            anchors.centerIn: parent
                            text: "◉"
                            color: "#FFFFFF"
                            font.pixelSize: 18
                        }
                    }
                    ColumnLayout {
                        spacing: 0
                        Text {
                            text: "Contest Trust"
                            color: Theme.sidebarTextActive
                            font.pixelSize: 15
                            font.bold: true
                        }
                        Text {
                            text: "竞赛项目审计"
                            color: Theme.sidebarText
                            font.pixelSize: 11
                        }
                    }
                    Item { Layout.fillWidth: true }
                }

                Text {
                    Layout.leftMargin: 12
                    Layout.bottomMargin: 2
                    text: "审计流程"
                    color: Theme.textMuted
                    font.pixelSize: 11
                    font.bold: true
                }

                // 导航列表
                Repeater {
                    model: root.navItems
                    delegate: Rectangle {
                        id: navItem
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        radius: Theme.radiusSm
                        property bool active: root.currentIndex === index
                        property bool hovered: navMouse.containsMouse
                        color: active ? Theme.sidebarActive
                             : hovered ? Qt.rgba(1, 1, 1, 0.05)
                             : "transparent"
                        Behavior on color { ColorAnimation { duration: Theme.normal } }

                        // 选中项左侧竖条
                        Rectangle {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            width: 3
                            height: 18
                            radius: 1.5
                            color: Theme.accent
                            visible: navItem.active
                            Behavior on height { NumberAnimation { duration: Theme.normal; easing.type: Easing.OutCubic } }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 12
                            spacing: 11

                            Text {
                                text: modelData.icon
                                color: navItem.active ? Theme.accent : Theme.sidebarText
                                font.pixelSize: 14
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.label
                                color: navItem.active ? Theme.sidebarTextActive : Theme.sidebarText
                                font.pixelSize: 13
                                font.bold: navItem.active
                                elide: Text.ElideRight
                            }
                        }

                        MouseArea {
                            id: navMouse
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            onClicked: root.currentIndex = index
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                // 底部状态卡
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: statusCol.implicitHeight + 24
                    radius: Theme.radiusSm
                    color: Theme.sidebarActive

                    ColumnLayout {
                        id: statusCol
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 6

                        RowLayout {
                            spacing: 8
                            Rectangle {
                                id: statusDot
                                width: 8; height: 8; radius: 4
                                color: compiler.trustScore > 0 ? Theme.success : Theme.textMuted
                                opacity: compiler.trustScore > 0 ? 1 : 0.65
                                SequentialAnimation on opacity {
                                    loops: Animation.Infinite
                                    running: compiler.trustScore === 0
                                    NumberAnimation { to: 0.35; duration: 900; easing.type: Easing.InOutSine }
                                    NumberAnimation { to: 0.9; duration: 900; easing.type: Easing.InOutSine }
                                }
                            }
                            Text {
                                text: "运行状态"
                                color: Theme.sidebarTextActive
                                font.pixelSize: 12
                                font.bold: true
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: compiler.status
                            color: Theme.sidebarText
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }

        // ---------- 主内容区 ----------
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // 顶栏
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 68
                color: Theme.window

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 28
                    anchors.rightMargin: 28
                    spacing: 16

                    Text {
                        text: root.navItems[root.currentIndex].label
                        color: Theme.textPrimary
                        font.pixelSize: 20
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }

                    Pill {
                        visible: compiler.trustScore > 0
                        text: "评分 " + compiler.trustScore
                        bg: Theme.accentSoft
                        fg: Theme.accentActive
                    }
                    Pill {
                        visible: compiler.blockerCount > 0
                        text: "必须处理 " + compiler.blockerCount
                        bg: Theme.dangerSoft
                        fg: Theme.danger
                    }
                    Pill {
                        visible: compiler.warningCount > 0
                        text: "需要关注 " + compiler.warningCount
                        bg: Theme.warningSoft
                        fg: Theme.warning
                    }
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 1
                    color: Theme.border
                }
            }

            // 页面栈
            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.currentIndex
                Behavior on opacity { NumberAnimation { duration: Theme.fast } }

                SessionWorkspacePage { compiler: compiler }
                TrustDashboardPage { compiler: compiler }
                AssetInventoryPage { compiler: compiler }
                CPIRPage { compiler: compiler }
                ClaimEvidencePage { compiler: compiler }
                ConsistencyPage { compiler: compiler }
                FindingsPage { compiler: compiler }
                FixTaskPage { compiler: compiler }
                AuditDiffPage { compiler: compiler }
                BrainPage { compiler: compiler }
                ReportExportPage { compiler: compiler }
            }
        }
    }

    // 拖放导入
    DropArea {
        anchors.fill: parent
        onDropped: function(drop) {
            if (drop.urls.length > 0) {
                compiler.projectPath = drop.urls[0]
            }
        }
    }
}
