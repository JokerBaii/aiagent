import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: root
    required property var compiler

    function value(name) {
        return compiler.auditDiff && compiler.auditDiff[name] !== undefined ? compiler.auditDiff[name] : "—"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        SectionTitle {
            title: "二次审计差分"
            subtitle: "对比两份 audit.json，量化补证进展"
        }

        Card {
            Layout.fillWidth: true
            padding: 14
            ColumnLayout {
                anchors.fill: parent
                spacing: 10
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Text {
                        Layout.preferredWidth: 88
                        text: "旧 audit"
                        color: Theme.textSecondary
                        font.pixelSize: 13
                    }
                    FieldInput {
                        Layout.fillWidth: true
                        text: root.compiler.oldAuditPath
                        placeholderText: "build/acceptance/audit.json"
                        onTextEdited: root.compiler.oldAuditPath = text
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Text {
                        Layout.preferredWidth: 88
                        text: "新 audit"
                        color: Theme.textSecondary
                        font.pixelSize: 13
                    }
                    FieldInput {
                        Layout.fillWidth: true
                        text: root.compiler.newAuditPath
                        placeholderText: "build/acceptance/software_audit.json"
                        onTextEdited: root.compiler.newAuditPath = text
                    }
                    PrimaryButton {
                        text: "生成差分"
                        onClicked: root.compiler.runDiff()
                    }
                }
            }
        }

        Card {
            Layout.fillWidth: true
            visible: root.value("summary") !== "—"
            color: Theme.accentSoft
            borderColor: Theme.border
            Text {
                anchors.fill: parent
                text: root.value("summary")
                color: Theme.textPrimary
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }
        }

        Card {
            Layout.fillWidth: true
            Layout.fillHeight: true
            padding: 0

            ListView {
                id: metricsList
                anchors.fill: parent
                anchors.margins: 12
                clip: true
                spacing: 6
                ScrollBar.vertical: ScrollBar {}

                header: Rectangle {
                    width: metricsList.width
                    height: 34
                    color: "transparent"
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        Text { Layout.preferredWidth: 130; text: "指标"; color: Theme.textMuted; font.pixelSize: 12; font.bold: true }
                        Text { Layout.preferredWidth: 90; text: "旧"; color: Theme.textMuted; font.pixelSize: 12; font.bold: true }
                        Text { Layout.preferredWidth: 90; text: "新"; color: Theme.textMuted; font.pixelSize: 12; font.bold: true }
                        Text { Layout.fillWidth: true; text: "说明"; color: Theme.textMuted; font.pixelSize: 12; font.bold: true }
                    }
                }

                model: [
                    ["可信评分", root.value("oldScore"), root.value("newScore"), "越高越好"],
                    ["可信债务", root.value("oldTrustDebt"), root.value("newTrustDebt"), "越低越好"],
                    ["必须处理", root.value("oldBlockers"), root.value("newBlockers"), "必须优先清零"],
                    ["需要关注", root.value("oldWarnings"), root.value("newWarnings"), "影响答辩风险"],
                    ["证据覆盖率", root.value("oldEvidenceCoverage") + "%", root.value("newEvidenceCoverage") + "%", "Supported + Partial/2"],
                    ["材料完整性", root.value("oldMaterialCompleteness"), root.value("newMaterialCompleteness"), "评分维度"],
                    ["一致性分", root.value("oldConsistencyScore"), root.value("newConsistencyScore"), "评分维度"],
                    ["补证任务", root.value("oldFixTaskCount"), root.value("newFixTaskCount"), "越少越接近提交"]
                ]

                delegate: Rectangle {
                    width: metricsList.width
                    height: 40
                    radius: Theme.radiusSm
                    color: index % 2 === 0 ? Theme.surfaceMuted : "transparent"
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        Text { Layout.preferredWidth: 130; text: modelData[0]; color: Theme.textPrimary; font.pixelSize: 13; font.bold: true }
                        Text { Layout.preferredWidth: 90; text: modelData[1]; color: Theme.textSecondary; font.pixelSize: 13 }
                        Text { Layout.preferredWidth: 90; text: modelData[2]; color: Theme.textPrimary; font.pixelSize: 13; font.bold: true }
                        Text { Layout.fillWidth: true; text: modelData[3]; color: Theme.textMuted; font.pixelSize: 12 }
                    }
                }
            }
        }
    }
}
