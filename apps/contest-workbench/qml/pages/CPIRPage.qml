pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: root
    required property var compiler

    function isBlank(value) {
        return value === undefined || value === null || String(value).trim().length === 0
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 24
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: root.width - 48
            spacing: 16

            SectionTitle {
                title: "我们从材料里读到的项目"
                subtitle: "如果这里理解错了，优先检查对应的项目说明材料"
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 16
                rowSpacing: 16

                Repeater {
                    model: [
                        ["项目名称", root.compiler.cpir.projectName],
                        ["竞赛类型", root.compiler.cpir.competitionType],
                        ["赛道判断把握", root.compiler.cpir.competitionConfidence === undefined
                                             ? "" : Math.round(Number(root.compiler.cpir.competitionConfidence) * 100) + "%"],
                        ["为什么这样判断", root.compiler.cpir.competitionReason],
                        ["目标用户", root.compiler.cpir.targetUser],
                        ["要解决的问题", root.compiler.cpir.painPoint],
                        ["解决方案", root.compiler.cpir.solution],
                        ["产品或服务", root.compiler.cpir.productOrService],
                        ["技术路线", root.compiler.cpir.technicalRoute],
                        ["商业模式", root.compiler.cpir.businessModel],
                        ["市场分析", root.compiler.cpir.marketAnalysis],
                        ["竞品分析", root.compiler.cpir.competitorAnalysis],
                        ["财务预测", root.compiler.cpir.financialProjection],
                        ["团队结构", root.compiler.cpir.teamStructure],
                        ["当前成果", root.compiler.cpir.currentResults],
                        ["社会价值", root.compiler.cpir.socialValue],
                        ["待补充信息", root.compiler.cpir.missingFields],
                        ["需关注点", root.compiler.cpir.riskItems]
                    ]
                    delegate: Card {
                        id: cpirCard
                        required property var modelData

                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        padding: 14
                        hoverable: true
                        property bool missing: root.isBlank(cpirCard.modelData[1])
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 6
                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: cpirCard.modelData[0]
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontSm
                                    font.bold: true
                                }
                                Item { Layout.fillWidth: true }
                                Pill {
                                    visible: cpirCard.missing
                                    text: "缺失"
                                    bg: Theme.warningSoft
                                    fg: Theme.warning
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: cpirCard.missing ? "—" : cpirCard.modelData[1]
                                color: cpirCard.missing ? Theme.textMuted : Theme.textPrimary
                                font.pixelSize: Theme.fontLg
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
        }
    }
}
