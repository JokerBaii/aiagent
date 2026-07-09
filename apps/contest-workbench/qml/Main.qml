import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import ContestTrust
import "."
import "components"
import "pages"

ApplicationWindow {
    id: root
    width: 1440
    height: 900
    minimumWidth: 1100
    minimumHeight: 700
    visible: true
    title: "大学生项目材料审计平台"
    color: Theme.window
    font.family: Theme.fontFamily

    CompileController { id: compiler }

    Settings {
        id: persistedTheme
        category: "theme"
        property string appearance: "light"
        property string colorTheme: "black"
        property string backgroundTheme: "garden"
        property string fontPreset: "microsoft"
        property int uiFontSize: 18
    }

    Component.onCompleted: {
        Theme.appearance = persistedTheme.appearance
        Theme.colorTheme = persistedTheme.colorTheme
        Theme.backgroundTheme = persistedTheme.backgroundTheme
        Theme.fontPreset = persistedTheme.fontPreset
        Theme.uiFontSize = persistedTheme.uiFontSize
    }

    Connections {
        target: Theme
        function onAppearanceChanged() { persistedTheme.appearance = Theme.appearance }
        function onColorThemeChanged() { persistedTheme.colorTheme = Theme.colorTheme }
        function onBackgroundThemeChanged() { persistedTheme.backgroundTheme = Theme.backgroundTheme }
        function onFontPresetChanged() { persistedTheme.fontPreset = Theme.fontPreset }
        function onUiFontSizeChanged() { persistedTheme.uiFontSize = Theme.uiFontSize }
    }

    readonly property var panelTabs: [
        { key: "files", label: "文件", icon: "file" },
        { key: "preview", label: "上下文", icon: "think" }
    ]
    readonly property var colorChoices: [
        { key: "black", label: "Black", color: "#333333" },
        { key: "blue", label: "Blue", color: "#4E80F7" },
        { key: "orange", label: "Orange", color: "#C47252" },
        { key: "green", label: "Green", color: "#57A64B" }
    ]
    readonly property var backgroundChoices: [
        { key: "minimal", label: "纯白简约", color: "#FFFFFF" },
        { key: "vscode", label: "VS Code Dark", color: "#1E1E1E" },
        { key: "garden", label: "花园", color: "#FFF8EA" },
        { key: "sakura", label: "粉樱", color: "#FFF4F7" },
        { key: "lake", label: "湖蓝", color: "#F3FBF8" },
        { key: "dusk", label: "暮紫", color: "#F7F1FB" },
        { key: "ink", label: "墨纸", color: "#F8F5EC" }
    ]
    readonly property var fontChoices: [
        { key: "microsoft", label: "微软雅黑 UI" },
        { key: "system", label: "系统清晰字体" },
        { key: "sourceHan", label: "思源黑体 / Noto" },
        { key: "lxgw", label: "霞鹜文楷" },
        { key: "mono", label: "等宽字体" }
    ]
    readonly property var accessModeChoices: [
        { key: "ask", label: "Ask", hint: "常规问答与安全读取" },
        { key: "plan", label: "Plan", hint: "只生成计划，不执行工具" },
        { key: "code", label: "Code", hint: "写入受控工作区产物" },
        { key: "bypass", label: "Bypass", hint: "完全授权，仅确认需要时使用" }
    ]

    property bool settingsOpen: false
    property bool rightPanelOpen: true
    property string rightPanelTab: "files"
    property string taskSearch: ""
    property string fileSearch: ""

    function fileName(path) {
        if (!path || path.length === 0) return "FocusZone"
        var normalized = String(path).replace(/\\/g, "/")
        var parts = normalized.split("/")
        return parts.length > 0 && parts[parts.length - 1].length > 0 ? parts[parts.length - 1] : normalized
    }

    FolderDialog {
        id: folderDialog
        title: "添加项目文件夹"
        onAccepted: compiler.selectProject(selectedFolder)
    }

    FileDialog {
        id: materialFileDialog
        title: "添加项目材料或材料包"
        nameFilters: [
            "项目材料 (*.pdf *.docx *.doc *.pptx *.ppt *.xlsx *.xls *.md *.txt *.json *.cpp *.c *.h *.hpp *.py *.java *.js *.ts *.zip *.tar *.tgz *.gz)",
            "所有文件 (*)"
        ]
        onAccepted: compiler.selectProject(selectedFile)
    }

    Shortcut {
        sequence: StandardKey.Open
        onActivated: folderDialog.open()
    }

    Shortcut {
        sequence: "Ctrl+Shift+O"
        onActivated: materialFileDialog.open()
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 280
            Layout.fillHeight: true
            color: Theme.sidebar

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                anchors.topMargin: 14
                anchors.bottomMargin: 14
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 16
                    spacing: 7
                    Rectangle { width: 11; height: 11; radius: 6; color: "#FF5F57" }
                    Rectangle { width: 11; height: 11; radius: 6; color: "#FFBD2E" }
                    Rectangle { width: 11; height: 11; radius: 6; color: "#28C840" }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    AiAvatar { size: 30 }
                    Text {
                        text: "大学生项目材料审计平台"
                        color: Theme.sidebarTextActive
                        font.pixelSize: 16
                        font.bold: true
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    radius: Theme.radiusLg
                    color: newTaskMouse.containsMouse ? Theme.accentHover : Theme.accent
                    Behavior on color { ColorAnimation { duration: Theme.fast } }
                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 8
                        Icon { name: "plus"; size: 16; strokeWidth: 2; color: Theme.isDark && Theme.colorTheme === "black" ? "#101010" : "#FFFFFF" }
                        Text {
                            text: "新任务"
                            color: Theme.isDark && Theme.colorTheme === "black" ? "#101010" : "#FFFFFF"
                            font.pixelSize: Theme.fontLg
                            font.bold: true
                        }
                    }
                    MouseArea {
                        id: newTaskMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            compiler.newSession()
                            workspacePage.focusComposer("")
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    radius: Theme.radiusMd
                    color: addMouse.containsMouse ? Theme.surfaceHover : Theme.surfaceMuted
                    border.color: Theme.border
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 12
                        spacing: 9
                        Icon { name: "folderPlus"; size: 15; color: Theme.textMuted }
                        Text {
                            Layout.fillWidth: true
                            text: "添加项目"
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontMd
                            font.bold: true
                        }
                        Text {
                            text: Qt.platform.os === "osx" ? "⌘O" : "Ctrl+O"
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontXs
                        }
                    }
                    MouseArea {
                        id: addMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: folderDialog.open()
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    radius: Theme.radius
                    color: Theme.surface
                    border.color: Theme.border
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 9
                        Rectangle { width: 8; height: 8; radius: 4; color: compiler.agentRunning ? Theme.warning : Theme.success }
                        Text {
                            Layout.fillWidth: true
                            text: compiler.trustScore > 0 ? "可信评分 " + compiler.trustScore : "等待审计"
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontMd
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text {
                            text: compiler.agentRunning ? "审计中" : (compiler.trustScore > 0 ? "已完成" : "待开始")
                            color: Theme.textTertiary
                            font.pixelSize: Theme.fontXs
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
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
                        leftPadding: 38
                        rightPadding: 12
                        placeholderText: "搜索任务..."
                        placeholderTextColor: Theme.textMuted
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontMd
                        selectByMouse: true
                        background: null
                        onTextChanged: root.taskSearch = text
                    }
                }

                ListView {
                    id: sessionList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 3
                    model: compiler.sessionList.filter(function(item) {
                        var query = root.taskSearch.trim().toLowerCase()
                        return query.length === 0
                                || String(item.title).toLowerCase().indexOf(query) >= 0
                                || String(item.subtitle).toLowerCase().indexOf(query) >= 0
                    })
                    section.property: "active"
                    delegate: Rectangle {
                        width: sessionList.width
                        implicitHeight: 44
                        radius: Theme.radius
                        color: modelData.active ? Theme.accentSoft
                             : sessionMouse.containsMouse ? Theme.surfaceMuted
                             : "transparent"
                        border.color: modelData.active ? Theme.accentGhost : "transparent"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 8
                            Text {
                                Layout.fillWidth: true
                                text: modelData.title
                                color: modelData.active ? Theme.textPrimary : Theme.sidebarText
                                font.pixelSize: Theme.fontMd
                                font.bold: modelData.active
                                elide: Text.ElideRight
                            }
                            Text {
                                text: modelData.subtitle
                                color: Theme.textTertiary
                                font.pixelSize: Theme.fontXs
                                elide: Text.ElideRight
                            }
                        }
                        MouseArea {
                            id: sessionMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.rightPanelTab = "preview"
                                root.rightPanelOpen = true
                            }
                        }
                    }
                    EmptyState {
                        anchors.fill: parent
                        visible: sessionList.count === 0
                        text: "还没有任务"
                        hint: "选择文件夹或材料包后开始。"
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.border
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        radius: Theme.radius
                        color: packMouse.containsMouse ? Theme.surfaceHover : "transparent"
                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 8
                            Icon { name: "think"; size: 15; color: Theme.textMuted }
                            Text { text: "上下文"; color: Theme.sidebarText; font.pixelSize: Theme.fontSm; font.bold: true }
                        }
                        MouseArea {
                            id: packMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.rightPanelTab = "preview"
                                root.rightPanelOpen = true
                            }
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        radius: Theme.radius
                        color: settingsMouse.containsMouse ? Theme.surfaceHover : "transparent"
                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 8
                            Icon { name: "settings"; size: 15; color: Theme.textMuted }
                            Text { text: "设置"; color: Theme.sidebarText; font.pixelSize: Theme.fontSm; font.bold: true }
                        }
                        MouseArea {
                            id: settingsMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.settingsOpen = true
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            color: Theme.border
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                color: Theme.window
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 20
                    spacing: 14
                    Text {
                        text: compiler.trustScore > 0 ? "可信评分 " + compiler.trustScore : "项目材料审计"
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontXl
                        font.bold: true
                    }
                    Text {
                        text: root.fileName(compiler.projectContext.originalRoot)
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMd
                        elide: Text.ElideRight
                    }
                    Rectangle { width: 7; height: 7; radius: 4; color: Theme.success }
                    Text { text: "审计助手"; color: Theme.success; font.pixelSize: Theme.fontSm; font.bold: true }
                    Rectangle { width: 7; height: 7; radius: 4; color: Theme.borderStrong }
                    Text { text: "规则引擎"; color: Theme.textMuted; font.pixelSize: Theme.fontSm }
                    Item { Layout.fillWidth: true }
                    Rectangle {
                        width: 32
                        height: 32
                        radius: Theme.radiusSm
                        color: panelMouse.containsMouse ? Theme.surfaceMuted : "transparent"
                        Icon { anchors.centerIn: parent; name: "sidebar"; size: 16; color: root.rightPanelOpen ? Theme.accent : Theme.textMuted }
                        MouseArea {
                            id: panelMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.rightPanelOpen = !root.rightPanelOpen
                        }
                    }
                }
                Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.border }
            }

            SessionWorkspacePage {
                id: workspacePage
                Layout.fillWidth: true
                Layout.fillHeight: true
                compiler: compiler
                onAttachProjectRequested: materialFileDialog.open()
            }
        }

        Rectangle {
            Layout.preferredWidth: root.rightPanelOpen ? 360 : 0
            Layout.fillHeight: true
            color: Theme.surfaceMuted
            clip: true
            visible: root.rightPanelOpen

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
                                implicitWidth: tabRow.implicitWidth + 30
                                implicitHeight: 36
                                radius: Theme.radiusSm
                                color: root.rightPanelTab === modelData.key ? Theme.surfaceHover : "transparent"
                                RowLayout {
                                    id: tabRow
                                    anchors.centerIn: parent
                                    spacing: 7
                                    Icon { name: modelData.icon; size: 14; color: root.rightPanelTab === modelData.key ? Theme.textPrimary : Theme.textMuted }
                                    Text {
                                        text: modelData.label
                                        color: root.rightPanelTab === modelData.key ? Theme.textPrimary : Theme.textMuted
                                        font.pixelSize: Theme.fontMd
                                        font.bold: root.rightPanelTab === modelData.key
                                    }
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.rightPanelTab = modelData.key
                                }
                            }
                        }
                        Item { Layout.fillWidth: true }
                        Rectangle {
                            width: 30; height: 30; radius: Theme.radiusSm
                            color: closePanelMouse.containsMouse ? Theme.surfaceHover : "transparent"
                            Icon { anchors.centerIn: parent; name: "close"; size: 14; color: Theme.textMuted }
                            MouseArea {
                                id: closePanelMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.rightPanelOpen = false
                            }
                        }
                    }
                    Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.border }
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: root.rightPanelTab === "files" ? 0
                                  : 1

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
                                    text: root.fileName(compiler.projectContext.originalRoot)
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontLg
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                                Pill {
                                    text: compiler.projectContext.inputFileCount > 0
                                          ? compiler.projectContext.inputFileCount + " 已扫描"
                                          : "等待扫描"
                                    bg: compiler.projectContext.inputFileCount > 0 ? Theme.successSoft : Theme.surface
                                    fg: compiler.projectContext.inputFileCount > 0 ? Theme.success : Theme.textMuted
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
                            model: compiler.assets.filter(function(item) {
                                var query = root.fileSearch.trim().toLowerCase()
                                return query.length === 0
                                        || String(item.path).toLowerCase().indexOf(query) >= 0
                                        || String(item.role).toLowerCase().indexOf(query) >= 0
                                        || String(item.format).toLowerCase().indexOf(query) >= 0
                            })
                            delegate: Rectangle {
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
                                        name: modelData.roleCode === "SourceCode" ? "code"
                                             : modelData.roleCode === "Archive" ? "folder"
                                             : "file"
                                        size: 13
                                        color: modelData.risk && modelData.risk.length > 0 ? Theme.warning : Theme.textTertiary
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.path
                                        color: modelData.risk && modelData.risk.length > 0 ? Theme.warning : Theme.textMuted
                                        font.pixelSize: Theme.fontMd
                                        elide: Text.ElideMiddle
                                    }
                                }
                                MouseArea {
                                    id: fileMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        compiler.previewProjectFile(modelData.path)
                                        root.rightPanelTab = "preview"
                                    }
                                }
                            }
                            EmptyState {
                                anchors.fill: parent
                                visible: fileList.count === 0
                                text: compiler.projectContext.originalRoot ? "等待审计扫描文件" : "未选择项目"
                                hint: "左侧添加文件夹或材料包后运行审计。"
                            }
                        }
                    }

                    ScrollView {
                        id: artifactScroll
                        clip: true
                        contentWidth: availableWidth
                        ColumnLayout {
                            width: artifactScroll.availableWidth
                            spacing: 10
                            Rectangle {
                                Layout.leftMargin: 14
                                Layout.rightMargin: 14
                                Layout.fillWidth: true
                                visible: compiler.selectedFilePreview.content !== undefined
                                         || compiler.selectedFilePreview.error !== undefined
                                implicitHeight: previewColumn.implicitHeight + 24
                                radius: Theme.radius
                                color: Theme.surface
                                border.color: compiler.selectedFilePreview.error
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
                                            text: compiler.selectedFilePreview.name || "文件预览"
                                            color: Theme.textPrimary
                                            font.pixelSize: Theme.fontMd
                                            font.bold: true
                                            elide: Text.ElideMiddle
                                        }
                                        Rectangle {
                                            width: 26
                                            height: 26
                                            radius: Theme.radiusSm
                                            color: closePreviewMouse.containsMouse
                                                   ? Theme.surfaceHover : "transparent"
                                            Icon {
                                                anchors.centerIn: parent
                                                name: "close"
                                                size: 12
                                                color: Theme.textMuted
                                            }
                                            MouseArea {
                                                id: closePreviewMouse
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: compiler.clearSelectedFilePreview()
                                            }
                                        }
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        visible: !compiler.selectedFilePreview.error
                                        text: (compiler.selectedFilePreview.format || "未知格式")
                                              + " · " + (compiler.selectedFilePreview.status || "待检查")
                                              + (compiler.selectedFilePreview.risk
                                                 ? " · 风险 " + compiler.selectedFilePreview.risk : "")
                                        color: Theme.textMuted
                                        font.pixelSize: Theme.fontXs
                                        wrapMode: Text.WordWrap
                                    }
                                    TextArea {
                                        Layout.fillWidth: true
                                        text: compiler.selectedFilePreview.error
                                              || compiler.selectedFilePreview.content || ""
                                        color: compiler.selectedFilePreview.error
                                               ? Theme.danger : Theme.textPrimary
                                        font.family: Theme.monoFamily
                                        font.pixelSize: Theme.fontSm
                                        wrapMode: TextArea.Wrap
                                        textFormat: Text.PlainText
                                        selectByMouse: true
                                        readOnly: true
                                        leftPadding: 0
                                        rightPadding: 0
                                        topPadding: 0
                                        bottomPadding: 0
                                        background: null
                                    }
                                }
                            }
                            SectionTitle {
                                Layout.leftMargin: 14
                                title: "审计资料"
                                subtitle: "项目申报、成果证明和软著相关材料会自动提供给审计助手。"
                            }
                            Repeater {
                                model: compiler.artifacts
                                delegate: Rectangle {
                                    Layout.leftMargin: 14
                                    Layout.rightMargin: 14
                                    Layout.fillWidth: true
                                    implicitHeight: artifactCol.implicitHeight + 20
                                    radius: Theme.radius
                                    color: Theme.surface
                                    border.color: Theme.border
                                    ColumnLayout {
                                        id: artifactCol
                                        anchors.fill: parent
                                        anchors.margins: 12
                                        spacing: 6
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Icon { name: "file"; size: 13; color: Theme.textMuted }
                                            Text { Layout.fillWidth: true; text: modelData.title; color: Theme.textPrimary; font.pixelSize: Theme.fontMd; font.bold: true }
                                            Pill { text: modelData.kind; bg: Theme.surfaceMuted; fg: Theme.textMuted }
                                        }
                                        Text { Layout.fillWidth: true; text: modelData.detail; color: Theme.textMuted; font.pixelSize: Theme.fontSm; wrapMode: Text.WordWrap }
                                        RowLayout {
                                            spacing: 5
                                            Icon { name: "checkSmall"; size: 12; color: Theme.success }
                                            Text { text: "审计助手自动参考"; color: Theme.success; font.pixelSize: Theme.fontSm; font.bold: true }
                                        }
                                    }
                                }
                            }
                        }
                    }

                }
            }
        }
    }

    DropArea {
        anchors.fill: parent
        onDropped: function(drop) {
            if (drop.urls.length > 0) compiler.selectProject(drop.urls[0])
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#00000000"
        visible: root.settingsOpen
        z: 20
        Rectangle {
            anchors.fill: parent
            color: "#000000"
            opacity: root.settingsOpen ? 0.22 : 0
            MouseArea { anchors.fill: parent; onClicked: root.settingsOpen = false }
        }
        Rectangle {
            width: Math.min(520, parent.width - 80)
            height: parent.height
            anchors.right: parent.right
            color: Theme.surface
            border.color: Theme.border

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 22
                spacing: 18
                RowLayout {
                    Layout.fillWidth: true
                    Text { Layout.fillWidth: true; text: "设置"; color: Theme.textPrimary; font.pixelSize: Theme.fontTitle; font.bold: true }
                    Rectangle {
                        width: 32; height: 32; radius: Theme.radiusSm
                        color: closeSettingsMouse.containsMouse ? Theme.surfaceMuted : "transparent"
                        Icon { anchors.centerIn: parent; name: "close"; size: 15; color: Theme.textMuted }
                        MouseArea { id: closeSettingsMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.settingsOpen = false }
                    }
                }

                ScrollView {
                    id: settingsScroll
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentWidth: availableWidth

                    ColumnLayout {
                        width: settingsScroll.availableWidth
                        spacing: 18

                SectionTitle { title: "大模型审计助手" }
                FieldInput {
                    Layout.fillWidth: true
                    text: compiler.llmEndpoint
                    placeholderText: "https://api.deepseek.com/chat/completions"
                    onTextEdited: compiler.llmEndpoint = text
                }
                FieldInput {
                    Layout.fillWidth: true
                    text: compiler.llmModel
                    placeholderText: "deepseek-v4-flash"
                    onTextEdited: compiler.llmModel = text
                }
                FieldInput {
                    Layout.fillWidth: true
                    echoMode: TextInput.Password
                    text: compiler.llmApiKey
                    placeholderText: "DeepSeek / OpenAI-compatible API Key"
                    onActiveFocusChanged: if (activeFocus && text === "********") text = ""
                    onTextEdited: compiler.llmApiKey = text
                }
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: brainPermissionRow.implicitHeight + 18
                    radius: 12
                    color: compiler.llmApproved ? Theme.successSoft : Theme.warningSoft
                    border.color: compiler.llmApproved ? Theme.success : Theme.warning
                    RowLayout {
                        id: brainPermissionRow
                        anchors.fill: parent
                        anchors.margins: 9
                        spacing: 8
                        CheckBox {
                            checked: compiler.llmApproved
                            onToggled: compiler.llmApproved = checked
                        }
                        Text {
                            Layout.fillWidth: true
                            text: "配置 API Key 后，大模型会调用规则工具并解释项目材料审计结果。"
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSm
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                SectionTitle { title: "外观" }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Repeater {
                        model: [
                            { key: "light", label: "浅色" },
                            { key: "dark", label: "深色" }
                        ]
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 38
                            radius: 12
                            color: Theme.appearance === modelData.key ? Theme.accentSoft : Theme.surfaceMuted
                            border.color: Theme.appearance === modelData.key ? Theme.accent : Theme.border
                            Text { anchors.centerIn: parent; text: modelData.label; color: Theme.appearance === modelData.key ? Theme.accent : Theme.textMuted; font.pixelSize: Theme.fontMd; font.bold: true }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Theme.appearance = modelData.key }
                        }
                    }
                }

                SectionTitle { title: "强调色" }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 4
                    rowSpacing: 10
                    columnSpacing: 10
                    Repeater {
                        model: root.colorChoices
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 72
                            radius: 14
                            color: Theme.colorTheme === modelData.key ? Theme.accentSoft : Theme.surfaceMuted
                            border.color: Theme.colorTheme === modelData.key ? Theme.accent : Theme.border
                            Column {
                                anchors.centerIn: parent
                                spacing: 7
                                Rectangle { anchors.horizontalCenter: parent.horizontalCenter; width: 24; height: 24; radius: 12; color: modelData.color }
                                Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.label; color: Theme.textPrimary; font.pixelSize: Theme.fontSm; font.bold: true }
                            }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Theme.colorTheme = modelData.key }
                        }
                    }
                }

                SectionTitle { title: "背景风格" }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 3
                    rowSpacing: 10
                    columnSpacing: 10
                    Repeater {
                        model: root.backgroundChoices
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 78
                            radius: 14
                            color: Theme.backgroundTheme === modelData.key ? Theme.accentSoft : Theme.surfaceMuted
                            border.color: Theme.backgroundTheme === modelData.key ? Theme.accent : Theme.border
                            Column {
                                anchors.centerIn: parent
                                spacing: 8
                                Rectangle {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: 72
                                    height: 30
                                    radius: 8
                                    color: modelData.color
                                    border.color: Theme.border
                                    Rectangle { anchors.left: parent.left; width: 20; height: parent.height; radius: 8; color: Theme.backgroundTheme === "vscode" && modelData.key === "vscode" ? "#252526" : Qt.rgba(0, 0, 0, 0.06) }
                                    Rectangle { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 5; width: 12; height: 12; radius: 4; color: Theme.accent }
                                }
                                Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.label; color: Theme.textPrimary; font.pixelSize: Theme.fontSm; font.bold: true }
                            }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Theme.backgroundTheme = modelData.key }
                        }
                    }
                }

                SectionTitle { title: "字体" }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Rectangle {
                        width: 34; height: 34; radius: 10
                        color: Theme.surfaceMuted
                        Text { anchors.centerIn: parent; text: "−"; color: Theme.textPrimary; font.pixelSize: 18; font.bold: true }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Theme.uiFontSize = Math.max(10, Theme.uiFontSize - 1) }
                    }
                    Text { text: Theme.uiFontSize + "px"; color: Theme.textPrimary; font.pixelSize: Theme.fontMd; font.bold: true; horizontalAlignment: Text.AlignHCenter; Layout.preferredWidth: 54 }
                    Rectangle {
                        width: 34; height: 34; radius: 10
                        color: Theme.surfaceMuted
                        Text { anchors.centerIn: parent; text: "+"; color: Theme.textPrimary; font.pixelSize: 18; font.bold: true }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Theme.uiFontSize = Math.min(36, Theme.uiFontSize + 1) }
                    }
                }
                ComboBox {
                    Layout.fillWidth: true
                    model: root.fontChoices.map(function(item) { return item.label })
                    currentIndex: root.fontChoices.findIndex(function(item) { return item.key === Theme.fontPreset })
                    onActivated: function(index) { Theme.fontPreset = root.fontChoices[index].key }
                }

                SectionTitle { title: "权限边界" }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    rowSpacing: 10
                    columnSpacing: 10
                    Repeater {
                        model: root.accessModeChoices
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 74
                            radius: 14
                            color: compiler.accessMode === modelData.key
                                   ? (modelData.key === "bypass" ? Theme.warningSoft : Theme.accentSoft)
                                   : Theme.surfaceMuted
                            border.color: compiler.accessMode === modelData.key
                                          ? (modelData.key === "bypass" ? Theme.warning : Theme.accent)
                                          : Theme.border
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 3
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.label
                                    color: compiler.accessMode === modelData.key
                                           ? (modelData.key === "bypass" ? Theme.warning : Theme.accent)
                                           : Theme.textPrimary
                                    font.pixelSize: Theme.fontMd
                                    font.bold: true
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.hint
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontXs
                                    wrapMode: Text.WordWrap
                                }
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: compiler.accessMode = modelData.key
                            }
                        }
                    }
                }

                Repeater {
                    model: compiler.permissionCards
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: permCol.implicitHeight + 18
                        radius: 12
                        color: modelData.allowed ? Theme.successSoft : Theme.dangerSoft
                        border.color: Theme.border
                        ColumnLayout {
                            id: permCol
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4
                            RowLayout {
                                Layout.fillWidth: true
                                Text { Layout.fillWidth: true; text: modelData.name; color: Theme.textPrimary; font.pixelSize: Theme.fontMd; font.bold: true }
                                Text { text: modelData.status; color: modelData.allowed ? Theme.success : Theme.danger; font.pixelSize: Theme.fontSm; font.bold: true }
                            }
                            Text { Layout.fillWidth: true; text: modelData.detail; color: Theme.textMuted; font.pixelSize: Theme.fontSm; wrapMode: Text.WordWrap }
                        }
                    }
                }

                Item { Layout.preferredHeight: 16 }
                    }
                }
            }
        }
    }
}
