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

    readonly property var pageMetadata: ({
        "dashboard": ["可信评分总览", "评分、必须处理项与扣分依据"],
        "assets": ["材料资产清单", "识别每份文件的格式、用途与风险"],
        "cpir": ["项目画像", "从材料中归纳的竞赛类型和关键信息"],
        "claims": ["声明与证据", "核对每项成果声明的支撑材料"],
        "consistency": ["材料一致性", "查看跨文件冲突和修复建议"],
        "findings": ["规则风险", "按规则定位必须处理和需要关注的问题"],
        "tasks": ["补证与修复任务", "按优先级推进材料完善"],
        "diff": ["实际变更与二次审计", "查看真实补丁并比较修复前后的审计数据"],
        "brain": ["智能体运行记录", "查看混合研判、运行结果与工具轨迹"],
        "report": ["审计报告导出", "导出可阅读报告或结构化数据包"]
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
                Layout.preferredHeight: 70
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
                            accessibleName: "返回审计对话"
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
                        visible: root.compiler.hasAuditResult
                        text: "评分 " + root.compiler.trustScore
                        bg: Theme.accentSoft
                        fg: Theme.accent
                    }
                    Pill {
                        visible: root.compiler.hasAuditResult && root.compiler.blockerCount > 0
                        text: "必须处理 " + root.compiler.blockerCount
                        bg: Theme.dangerSoft
                        fg: Theme.danger
                    }
                    Pill {
                        visible: root.compiler.hasAuditResult && root.compiler.warningCount > 0
                        text: "需关注 " + root.compiler.warningCount
                        bg: Theme.warningSoft
                        fg: Theme.warning
                    }
                    Rectangle {
                        Layout.preferredWidth: 8
                        Layout.preferredHeight: 8
                        radius: 4
                        color: root.compiler.agentRunning ? Theme.warning
                                                           : root.compiler.hasAuditResult
                                                             ? Theme.success : Theme.textTertiary
                    }
                    Text {
                        Layout.maximumWidth: 210
                        text: root.compiler.status
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSm
                        elide: Text.ElideRight
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
                AssetInventoryPage { compiler: root.compiler }
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
