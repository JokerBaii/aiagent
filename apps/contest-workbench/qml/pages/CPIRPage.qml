import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: root
    required property var compiler

    ScrollView {
        anchors.fill: parent
        anchors.margins: 24
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: root.width - 48
            spacing: 16

            SectionTitle {
                title: "项目画像"
                subtitle: "从材料中整理出的项目关键信息"
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
                        ["类型置信度", root.compiler.cpir.competitionConfidence],
                        ["判断理由", root.compiler.cpir.competitionReason],
                        ["目标用户", root.compiler.cpir.targetUser],
                        ["痛点", root.compiler.cpir.painPoint],
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
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        padding: 14
                        hoverable: true
                        property bool missing: !modelData[1] || modelData[1] === ""
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 6
                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: modelData[0]
                                    color: Theme.textSecondary
                                    font.pixelSize: 12
                                    font.bold: true
                                }
                                Item { Layout.fillWidth: true }
                                Pill {
                                    visible: parent.parent.parent.missing
                                    text: "缺失"
                                    bg: Theme.warningSoft
                                    fg: Theme.warning
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: parent.parent.missing ? "—" : modelData[1]
                                color: parent.parent.missing ? Theme.textMuted : Theme.textPrimary
                                font.pixelSize: 14
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
        }
    }
}
