pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."

Rectangle {
    id: root

    required property var compiler
    property string currentTab: "files"
    property string activeArtifactPage: ""
    property string fileSearch: ""

    signal closeRequested()
    signal tabRequested(string tab)
    signal artifactRequested(string pageKey)

    readonly property var panelTabs: [
        { key: "files", label: "文件", icon: "file" },
        { key: "preview", label: "上下文", icon: "think" },
        { key: "artifacts", label: "检查结果", icon: "skills" }
    ]

    function fileName(path) {
        if (!path || path.length === 0)
            return "FocusZone"
        var normalized = String(path).replace(/\\/g, "/")
        var parts = normalized.split("/")
        return parts.length > 0 && parts[parts.length - 1].length > 0
                ? parts[parts.length - 1] : normalized
    }

    function artifactIcon(pageKey) {
        if (pageKey === "dashboard") return "preview"
        if (pageKey === "assets") return "folder"
        if (pageKey === "cpir") return "list"
        if (pageKey === "claims") return "check"
        if (pageKey === "consistency") return "diff"
        if (pageKey === "findings") return "bypass"
        if (pageKey === "tasks") return "plan"
        if (pageKey === "diff") return "diff"
        if (pageKey === "brain") return "think"
        if (pageKey === "report") return "download"
        return "file"
    }

    color: Theme.surfaceMuted
    clip: true

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: Theme.surfaceMuted
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 8
                Repeater {
                    model: root.panelTabs
                    delegate: Rectangle {
                        id: panelTabDelegate
                        required property var modelData

                        implicitWidth: tabRow.implicitWidth + 30
                        implicitHeight: 36
                        radius: Theme.radiusSm
                        color: root.currentTab === panelTabDelegate.modelData.key ? Theme.surfaceHover : "transparent"
                        RowLayout {
                            id: tabRow
                            anchors.centerIn: parent
                            spacing: 7
                            Icon { name: panelTabDelegate.modelData.icon; size: 14; color: root.currentTab === panelTabDelegate.modelData.key ? Theme.textPrimary : Theme.textMuted }
                            Text {
                                text: panelTabDelegate.modelData.label
                                color: root.currentTab === panelTabDelegate.modelData.key ? Theme.textPrimary : Theme.textMuted
                                font.pixelSize: Theme.fontMd
                                font.bold: root.currentTab === panelTabDelegate.modelData.key
                            }
                        }
                        ActionArea {
                            anchors.fill: parent
                            accessibleName: "切换到" + panelTabDelegate.modelData.label + "面板"
                            onClicked: root.tabRequested(panelTabDelegate.modelData.key)
                        }
                    }
                }
                Item { Layout.fillWidth: true }
                Rectangle {
                    Layout.preferredWidth: 30
                    Layout.preferredHeight: 30
                    radius: Theme.radiusSm
                    color: closePanelMouse.containsMouse ? Theme.surfaceHover : "transparent"
                    Icon { anchors.centerIn: parent; name: "close"; size: 14; color: Theme.textMuted }
                    ActionArea {
                        id: closePanelMouse
                        anchors.fill: parent
                        accessibleName: "关闭右侧面板"
                        onClicked: root.closeRequested()
                    }
                }
            }
            Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.border }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.currentTab === "files" ? 0
                          : root.currentTab === "preview" ? 1 : 2

            ColumnLayout {
                spacing: 12
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 62
                    color: Theme.surfaceMuted
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 20
                        spacing: 10
                        Icon { name: "folder"; size: 15; color: Theme.textMuted }
                        Text {
                            Layout.fillWidth: true
                            text: root.fileName(root.compiler.projectContext.originalRoot)
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontLg
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Pill {
                            text: root.compiler.projectContext.inputFileCount > 0
                                  ? root.compiler.projectContext.inputFileCount + " 已扫描"
                                  : "等待扫描"
                            bg: root.compiler.projectContext.inputFileCount > 0 ? Theme.successSoft : Theme.surface
                            fg: root.compiler.projectContext.inputFileCount > 0 ? Theme.success : Theme.textMuted
                        }
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                    Layout.preferredHeight: 42
                    radius: Theme.radius
                    color: Theme.surface
                    border.color: Theme.border
                    Icon {
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        name: "search"
                        size: 14
                        color: Theme.textMuted
                    }
                    TextField {
                        anchors.fill: parent
                        Accessible.name: "搜索项目文件"
                        leftPadding: 38
                        rightPadding: 12
                        placeholderText: "搜索文件..."
                        placeholderTextColor: Theme.textMuted
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontMd
                        selectByMouse: true
                        background: null
                        onTextChanged: root.fileSearch = text
                    }
                }
                ListView {
                    id: fileList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                    clip: true
                    spacing: 2
                    model: root.compiler.assets.filter(function(item) {
                        var query = root.fileSearch.trim().toLowerCase()
                        return query.length === 0
                                || String(item.path).toLowerCase().indexOf(query) >= 0
                                || String(item.role).toLowerCase().indexOf(query) >= 0
                                || String(item.format).toLowerCase().indexOf(query) >= 0
                    })
                    delegate: Rectangle {
                        id: fileDelegate
                        required property var modelData

                        width: fileList.width
                        implicitHeight: 34
                        radius: Theme.radiusSm
                        color: fileMouse.containsMouse ? Theme.surface : "transparent"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 8
                            Icon {
                                name: fileDelegate.modelData.roleCode === "SourceCode" ? "code"
                                     : fileDelegate.modelData.roleCode === "Archive" ? "folder"
                                     : "file"
                                size: 13
                                color: fileDelegate.modelData.risk && fileDelegate.modelData.risk.length > 0 ? Theme.warning : Theme.textTertiary
                            }
                            Text {
                                Layout.fillWidth: true
                                text: fileDelegate.modelData.path
                                color: fileDelegate.modelData.risk && fileDelegate.modelData.risk.length > 0 ? Theme.warning : Theme.textMuted
                                font.pixelSize: Theme.fontMd
                                elide: Text.ElideMiddle
                            }
                        }
                        ActionArea {
                            id: fileMouse
                            anchors.fill: parent
                            accessibleName: "预览文件：" + fileDelegate.modelData.path
                            onClicked: {
                                root.compiler.previewProjectFile(fileDelegate.modelData.path)
                                root.tabRequested("preview")
                            }
                        }
                    }
                    EmptyState {
                        anchors.fill: parent
                        visible: fileList.count === 0
                        text: root.compiler.projectContext.originalRoot ? "等待检查文件" : "未选择项目"
                        hint: "左侧添加真实项目文件夹、压缩包或文件后运行检查。"
                    }
                }
            }

            ScrollView {
                id: contextScroll
                clip: true
                contentWidth: availableWidth

                ColumnLayout {
                    width: contextScroll.availableWidth
                    spacing: 10

                    Rectangle {
                        Layout.leftMargin: 14
                        Layout.rightMargin: 14
                        Layout.topMargin: 12
                        Layout.fillWidth: true
                        implicitHeight: statusColumn.implicitHeight + 24
                        radius: Theme.radius
                        color: Theme.surface
                        border.color: Theme.border

                        ColumnLayout {
                            id: statusColumn
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                Rectangle {
                                    Layout.preferredWidth: 8
                                    Layout.preferredHeight: 8
                                    radius: 4
                                    color: root.compiler.agentRunning ? Theme.warning
                                           : root.compiler.hasAuditResult ? Theme.success
                                                                     : Theme.textTertiary
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: root.compiler.agentRunning ? "正在检查" : "当前状态"
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontMd
                                    font.bold: true
                                }
                                Pill {
                                    visible: root.compiler.hasAuditResult
                                    text: root.compiler.trustScore + " 分"
                                    bg: Theme.accentSoft
                                    fg: Theme.accent
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: root.compiler.status
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontSm
                                wrapMode: Text.WordWrap
                            }
                            RowLayout {
                                visible: root.compiler.hasAuditResult
                                spacing: 8
                                Pill {
                                    text: "必须处理 " + root.compiler.blockerCount
                                    bg: root.compiler.blockerCount > 0 ? Theme.dangerSoft : Theme.surfaceMuted
                                    fg: root.compiler.blockerCount > 0 ? Theme.danger : Theme.textMuted
                                }
                                Pill {
                                    text: "需关注 " + root.compiler.warningCount
                                    bg: root.compiler.warningCount > 0 ? Theme.warningSoft : Theme.surfaceMuted
                                    fg: root.compiler.warningCount > 0 ? Theme.warning : Theme.textMuted
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.leftMargin: 14
                        Layout.rightMargin: 14
                        Layout.fillWidth: true
                        visible: root.compiler.selectedFilePreview.content !== undefined
                                 || root.compiler.selectedFilePreview.error !== undefined
                        implicitHeight: previewColumn.implicitHeight + 24
                        radius: Theme.radius
                        color: Theme.surface
                        border.color: root.compiler.selectedFilePreview.error
                                      ? Theme.danger : Theme.accentGhost

                        ColumnLayout {
                            id: previewColumn
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                Icon { name: "file"; size: 14; color: Theme.accent }
                                Text {
                                    Layout.fillWidth: true
                                    text: root.compiler.selectedFilePreview.name || "文件预览"
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontMd
                                    font.bold: true
                                    elide: Text.ElideMiddle
                                }
                                Rectangle {
                                    Layout.preferredWidth: 26
                                    Layout.preferredHeight: 26
                                    radius: Theme.radiusSm
                                    color: closePreviewMouse.containsMouse
                                           ? Theme.surfaceHover : "transparent"
                                    Icon {
                                        anchors.centerIn: parent
                                        name: "close"
                                        size: 12
                                        color: Theme.textMuted
                                    }
                                    ActionArea {
                                        id: closePreviewMouse
                                        anchors.fill: parent
                                        accessibleName: "关闭文件预览"
                                        onClicked: root.compiler.clearSelectedFilePreview()
                                    }
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                visible: !root.compiler.selectedFilePreview.error
                                text: (root.compiler.selectedFilePreview.format || "未知格式")
                                      + " · " + (root.compiler.selectedFilePreview.status || "待检查")
                                      + (root.compiler.selectedFilePreview.risk
                                         ? " · 风险 " + root.compiler.selectedFilePreview.risk : "")
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontXs
                                wrapMode: Text.WordWrap
                            }
                            ScrollView {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 260
                                clip: true
                                contentWidth: availableWidth

                                TextArea {
                                    id: filePreviewText
                                    width: parent.width
                                    text: root.compiler.selectedFilePreview.error
                                          || root.compiler.selectedFilePreview.content || ""
                                    color: root.compiler.selectedFilePreview.error
                                           ? Theme.danger : Theme.textPrimary
                                    font.family: Theme.monoFamily
                                    font.pixelSize: Theme.fontSm
                                    wrapMode: TextArea.Wrap
                                    textFormat: Text.PlainText
                                    selectByMouse: true
                                    readOnly: true
                                    leftPadding: 0
                                    rightPadding: 8
                                    topPadding: 0
                                    bottomPadding: 0
                                    background: null
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                visible: root.compiler.selectedFilePreview.truncated === true
                                text: "预览已限长；完整内容不会一次性加载到界面。"
                                color: Theme.warning
                                font.pixelSize: Theme.fontXs
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    SectionTitle {
                        Layout.leftMargin: 14
                        Layout.rightMargin: 14
                        title: "检查进度"
                        subtitle: root.compiler.agentRunning
                                  ? "当前步骤会随运行实时更新。"
                                  : "每一步结果都会进入当前会话上下文。"
                    }
                    Repeater {
                        model: root.compiler.toolCards
                        delegate: Rectangle {
                            id: toolDelegate
                            required property var modelData

                            Layout.leftMargin: 14
                            Layout.rightMargin: 14
                            Layout.fillWidth: true
                            implicitHeight: toolColumn.implicitHeight + 18
                            radius: Theme.radius
                            color: Theme.surface
                            border.color: toolDelegate.modelData.status === "进行中"
                                          ? Theme.warning : Theme.border

                            ColumnLayout {
                                id: toolColumn
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 4
                                RowLayout {
                                    Layout.fillWidth: true
                                    Icon {
                                        name: toolDelegate.modelData.status === "完成" ? "checkSmall"
                                              : toolDelegate.modelData.status === "进行中" ? "think" : "toolStack"
                                        size: 13
                                        color: toolDelegate.modelData.status === "完成" ? Theme.success
                                               : toolDelegate.modelData.status === "进行中" ? Theme.warning
                                                                             : Theme.textTertiary
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: toolDelegate.modelData.name
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.fontMd
                                        font.bold: true
                                    }
                                    Pill {
                                        text: toolDelegate.modelData.status
                                        bg: toolDelegate.modelData.status === "完成" ? Theme.successSoft
                                            : toolDelegate.modelData.status === "进行中" ? Theme.warningSoft
                                                                          : Theme.surfaceMuted
                                        fg: toolDelegate.modelData.status === "完成" ? Theme.success
                                            : toolDelegate.modelData.status === "进行中" ? Theme.warning
                                                                          : Theme.textMuted
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: toolDelegate.modelData.detail
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSm
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                    Item { Layout.preferredHeight: 8 }
                }
            }

            ScrollView {
                id: artifactPanelScroll
                clip: true
                contentWidth: availableWidth

                ColumnLayout {
                    width: artifactPanelScroll.availableWidth
                    spacing: 10

                    SectionTitle {
                        Layout.leftMargin: 14
                        Layout.rightMargin: 14
                        Layout.topMargin: 14
                        title: "检查结果"
                        subtitle: "打开总览、证据、风险、任务、差分或报告。"
                    }
                    Repeater {
                        model: root.compiler.artifacts
                        delegate: Rectangle {
                            id: artifactDelegate
                            required property var modelData

                            Layout.leftMargin: 14
                            Layout.rightMargin: 14
                            Layout.fillWidth: true
                            implicitHeight: artifactColumn.implicitHeight + 22
                            radius: Theme.radius
                            color: artifactAction.containsMouse && artifactDelegate.modelData.available
                                   ? Theme.surfaceHover : Theme.surface
                            border.color: root.activeArtifactPage === artifactDelegate.modelData.pageKey
                                          ? Theme.accent : Theme.border
                            opacity: artifactDelegate.modelData.available ? 1 : 0.58

                            Behavior on color { ColorAnimation { duration: Theme.fast } }
                            Behavior on border.color { ColorAnimation { duration: Theme.fast } }

                            ColumnLayout {
                                id: artifactColumn
                                anchors.fill: parent
                                anchors.margins: 11
                                spacing: 6
                                RowLayout {
                                    Layout.fillWidth: true
                                    Icon {
                                        name: root.artifactIcon(artifactDelegate.modelData.pageKey)
                                        size: 14
                                        color: artifactDelegate.modelData.available ? Theme.accent : Theme.textTertiary
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: artifactDelegate.modelData.title
                                        color: artifactDelegate.modelData.available ? Theme.textPrimary : Theme.textMuted
                                        font.pixelSize: Theme.fontMd
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }
                                    Pill {
                                        text: artifactDelegate.modelData.kind
                                        bg: Theme.surfaceMuted
                                        fg: Theme.textMuted
                                    }
                                    Icon {
                                        visible: artifactDelegate.modelData.available
                                        name: "chevronRight"
                                        size: 11
                                        color: Theme.textTertiary
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: artifactDelegate.modelData.detail
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSm
                                    wrapMode: Text.WordWrap
                                }
                            }
                            ActionArea {
                                id: artifactAction
                                anchors.fill: parent
                                enabled: artifactDelegate.modelData.available
                                accessibleName: artifactDelegate.modelData.available
                                                ? "打开" + artifactDelegate.modelData.title
                                                : artifactDelegate.modelData.title + "，完成检查后可用"
                                onClicked: root.artifactRequested(artifactDelegate.modelData.pageKey)
                            }
                        }
                    }
                    Text {
                        Layout.leftMargin: 14
                        Layout.rightMargin: 14
                        Layout.fillWidth: true
                        text: "快捷键 Ctrl+Shift+A 可随时打开检查结果。"
                        color: Theme.textTertiary
                        font.pixelSize: Theme.fontXs
                        wrapMode: Text.WordWrap
                    }
                    Item { Layout.preferredHeight: 8 }
                }
            }

        }
    }
}
