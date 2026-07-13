pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: root

    required property var compiler
    property string pageKey: "dashboard"
    signal closeRequested()
    signal filePreviewRequested(string relativePath)

    readonly property var pageMetadata: ({
        "dashboard": ["检查结果总览", "先看必须处理的问题，再按建议完善材料"],
        "assets": ["材料资产清单", "识别每份文件的格式、用途与风险"],
        "cpir": ["项目基本信息", "检查系统从材料中理解到的项目内容是否正确"],
        "claims": ["成果与证明材料", "核对每项成果是否有相应材料支撑"],
        "consistency": ["材料内容是否矛盾", "查看不同文件中的名称、数据和时间是否一致"],
        "findings": ["发现的问题", "红色问题必须处理，黄色问题建议处理"],
        "tasks": ["下一步修改清单", "按照优先级逐项完善参赛材料"],
        "diff": ["修改前后对比", "重新检查修改后的材料，并确认问题是否减少"],
        "brain": ["智能辅助检查", "需要联网服务；本地规则检查无需配置即可使用"],
        "report": ["下载检查报告", "导出便于阅读和分享的检查结果"]
    })

    function pageIndex(key) {
        var keys = ["dashboard", "assets", "cpir", "claims", "consistency",
                    "findings", "tasks", "diff", "brain", "report"]
        var value = keys.indexOf(key)
        return value < 0 ? 0 : value
    }

    function metadata(index) {
        var value = pageMetadata[pageKey]
        return value === undefined ? pageMetadata.dashboard[index] : value[index]
    }

    onPageKeyChanged: pageTransition.restart()

    Rectangle {
        anchors.fill: parent
        color: Theme.window

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 76
                color: Theme.window

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 22
                    spacing: 12

                    Rectangle {
                        Layout.preferredWidth: 34
                        Layout.preferredHeight: 34
                        radius: Theme.radiusSm
                        color: backAction.containsMouse ? Theme.surfaceMuted : "transparent"

                        Icon {
                            anchors.centerIn: parent
                            name: "chevronLeft"
                            size: 15
                            color: Theme.textMuted
                        }
                        ActionArea {
                            id: backAction
                            anchors.fill: parent
                            accessibleName: "返回项目对话"
                            onClicked: root.closeRequested()
                        }
                        ToolTip.visible: backAction.containsMouse
                        ToolTip.text: "返回对话"
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            Layout.fillWidth: true
                            text: root.metadata(0)
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontXl
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: root.metadata(1)
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSm
                            elide: Text.ElideRight
                        }
                    }

                    Pill {
                        visible: root.compiler.hasAuditResult && root.width >= 600
                        text: root.compiler.trustScore + " 分"
                        bg: Theme.accentSoft
                        fg: Theme.accent
                    }
                    Pill {
                        visible: root.compiler.hasAuditResult && root.compiler.blockerCount > 0
                                 && root.width >= 760
                        text: root.compiler.blockerCount + " 个要处理"
                        bg: Theme.dangerSoft
                        fg: Theme.danger
                    }
                    Pill {
                        visible: root.compiler.hasAuditResult && root.compiler.warningCount > 0
                                 && root.width >= 760
                        text: root.compiler.warningCount + " 个建议"
                        bg: Theme.warningSoft
                        fg: Theme.warning
                    }
                    Rectangle {
                        Layout.preferredWidth: 8
                        Layout.preferredHeight: 8
                        visible: root.compiler.agentRunning
                        radius: 4
                        color: Theme.warning
                    }
                    Text {
                        id: headerStatus
                        Layout.maximumWidth: 210
                        visible: root.compiler.agentRunning && root.width >= 760
                        text: root.compiler.status
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSm
                        elide: Text.ElideRight
                        HoverHandler { id: headerStatusHover }
                        ToolTip.visible: headerStatusHover.hovered && headerStatus.truncated
                        ToolTip.text: root.compiler.status
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: Theme.border
                }
            }

            StackLayout {
                id: pageStack
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.pageIndex(root.pageKey)

                TrustDashboardPage { compiler: root.compiler }
                AssetInventoryPage {
                    compiler: root.compiler
                    onPreviewRequested: function(relativePath) {
                        root.filePreviewRequested(relativePath)
                    }
                }
                CPIRPage { compiler: root.compiler }
                ClaimEvidencePage { compiler: root.compiler }
                ConsistencyPage { compiler: root.compiler }
                FindingsPage { compiler: root.compiler }
                FixTaskPage { compiler: root.compiler }
                AuditDiffPage { compiler: root.compiler }
                BrainPage { compiler: root.compiler }
                ReportExportPage { compiler: root.compiler }
            }
        }
    }

    ParallelAnimation {
        id: pageTransition
        NumberAnimation {
            target: pageStack
            property: "opacity"
            from: 0.45
            to: 1
            duration: Theme.normal
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: pageStack
            property: "x"
            from: 12
            to: 0
            duration: Theme.normal
            easing.type: Easing.OutCubic
        }
    }
}
