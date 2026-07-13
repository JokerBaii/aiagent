pragma ComponentBehavior: Bound

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
    x: Math.max(0, (Screen.width - width) / 2)
    y: Math.max(0, (Screen.height - height) / 2)
    minimumWidth: 1100
    minimumHeight: 700
    visible: true
    title: "大学生项目审计与完善平台"
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
        property int uiFontSize: 16
        property int designVersion: 0
    }

    Component.onCompleted: {
        if (persistedTheme.designVersion < 2) {
            if (persistedTheme.uiFontSize === 18)
                persistedTheme.uiFontSize = 16
            persistedTheme.designVersion = 2
        }
        Theme.appearance = persistedTheme.appearance
        Theme.colorTheme = persistedTheme.colorTheme
        Theme.backgroundTheme = persistedTheme.backgroundTheme
        Theme.fontPreset = persistedTheme.fontPreset
        Theme.uiFontSize = Math.max(12, Math.min(24, persistedTheme.uiFontSize))
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
        { key: "preview", label: "上下文", icon: "think" },
        { key: "artifacts", label: "检查结果", icon: "skills" }
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
        { key: "garden", label: "暖白", color: "#FAF9F6" },
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
        { key: "ask", label: "仅查看", hint: "回答问题和读取已导入的材料" },
        { key: "plan", label: "先列计划", hint: "只说明准备怎么做，不修改文件" },
        { key: "code", label: "允许生成修改稿", hint: "只在安全副本中创建文件，不改原项目" },
        { key: "bypass", label: "扩展读取", hint: "允许读取你另外指定的位置，仍不改原项目" }
    ]

    property bool settingsOpen: false
    property bool rightPanelOpen: false
    property string rightPanelTab: "files"
    property string artifactPageKey: ""
    property string taskSearch: ""
    property string fileSearch: ""
    property real animatedRightPanelWidth: rightPanelOpen ? 360 : 0

    Behavior on animatedRightPanelWidth {
        NumberAnimation { duration: Theme.normal; easing.type: Easing.OutCubic }
    }

    function fileName(path) {
        if (!path || path.length === 0) return "FocusZone"
        var normalized = String(path).replace(/\\/g, "/")
        var parts = normalized.split("/")
        return parts.length > 0 && parts[parts.length - 1].length > 0 ? parts[parts.length - 1] : normalized
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

    function openArtifact(pageKey, available) {
        if (!available || !pageKey)
            return
        artifactPageKey = pageKey
        rightPanelOpen = false
    }

    function closeArtifact() {
        artifactPageKey = ""
        rightPanelTab = "artifacts"
        rightPanelOpen = true
        workspacePage.focusComposerInput()
    }

    FolderDialog {
        id: folderDialog
        title: "添加项目文件夹"
        onAccepted: compiler.selectProject(selectedFolder)
    }

    FileDialog {
        id: materialFileDialog
        title: "添加项目文件或项目压缩包"
        nameFilters: [
            "文档与数据 (*.pdf *.docx *.pptx *.xlsx *.doc *.ppt *.xls *.rtf *.odt *.odp *.ods *.md *.markdown *.txt *.rst *.adoc *.json *.jsonl *.yaml *.yml *.csv *.tsv *.xml *.html *.htm *.toml *.ini *.cfg *.conf *.sql)",
            "源码与配置 (*.c *.cc *.cpp *.cxx *.h *.hh *.hpp *.hxx *.py *.js *.mjs *.cjs *.ts *.tsx *.jsx *.vue *.svelte *.qml *.java *.kt *.kts *.go *.rs *.swift *.cs *.php *.rb *.lua *.r *.scala *.dart *.sh *.bash *.zsh *.ps1 *.bat *.cmd *.cmake *.gradle)",
            "材料包与代码包 (*.zip *.tar *.tar.gz *.tar.bz2 *.tar.xz *.tar.zst *.tgz *.gz *.bz2 *.xz *.zst *.7z *.rar *.cab *.cpio *.iso *.ar *.deb *.rpm *.apk *.jar *.war *.ear *.whl)",
            "图片、音视频与模型成果 (*.png *.jpg *.jpeg *.gif *.webp *.svg *.bmp *.tiff *.tif *.ico *.avif *.heic *.mp4 *.mov *.avi *.mkv *.webm *.mpeg *.mpg *.m4v *.wmv *.flv *.mp3 *.wav *.flac *.ogg *.m4a *.aac *.opus *.wma *.onnx *.pt *.pth *.pkl *.joblib *.safetensors *.tflite *.pb *.glb *.gltf *.fbx *.obj *.stl *.dae *.3ds *.blend)",
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

    Shortcut {
        sequence: "Ctrl+Shift+A"
        onActivated: {
            root.rightPanelTab = "artifacts"
            root.rightPanelOpen = true
        }
    }

    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (compiler.agentRunning || compiler.advisoryRunning) {
                compiler.cancelCurrentJob()
            } else if (root.settingsOpen) {
                root.settingsOpen = false
            } else if (root.artifactPageKey.length > 0) {
                root.closeArtifact()
            } else if (compiler.selectedFilePreview.content !== undefined
                       || compiler.selectedFilePreview.error !== undefined) {
                compiler.clearSelectedFilePreview()
            } else if (root.rightPanelOpen) {
                root.rightPanelOpen = false
            }
        }
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
                    spacing: 8
                    AiAvatar { size: 30 }
                    Text {
                        text: "项目审计"
                        color: Theme.sidebarTextActive
                        font.pixelSize: Theme.fontXl
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
                    ActionArea {
                        id: newTaskMouse
                        anchors.fill: parent
                        accessibleName: "开始新任务"
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
                    ActionArea {
                        id: addMouse
                        anchors.fill: parent
                        accessibleName: "添加项目文件夹"
                        onClicked: folderDialog.open()
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
                        Accessible.name: "搜索任务"
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
                        id: sessionDelegate
                        required property var modelData

                        width: sessionList.width
                        implicitHeight: 44
                        radius: Theme.radius
                        color: sessionDelegate.modelData.active ? Theme.accentSoft
                             : sessionMouse.containsMouse ? Theme.surfaceMuted
                             : "transparent"
                        border.color: sessionDelegate.modelData.active ? Theme.accentGhost : "transparent"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 8
                            Text {
                                Layout.fillWidth: true
                                text: sessionDelegate.modelData.title
                                color: sessionDelegate.modelData.active ? Theme.textPrimary : Theme.sidebarText
                                font.pixelSize: Theme.fontMd
                                font.bold: sessionDelegate.modelData.active
                                elide: Text.ElideRight
                            }
                            Text {
                                text: sessionDelegate.modelData.subtitle
                                color: Theme.textTertiary
                                font.pixelSize: Theme.fontXs
                                elide: Text.ElideRight
                            }
                        }
                        ActionArea {
                            id: sessionMouse
                            anchors.fill: parent
                            accessibleName: "打开任务上下文：" + sessionDelegate.modelData.title
                            onClicked: {
                                compiler.activateSession(sessionDelegate.modelData.sessionId)
                            }
                        }
                    }
                    EmptyState {
                        anchors.fill: parent
                        visible: sessionList.count === 0
                        text: "还没有任务"
                        hint: "选择完整项目文件夹、项目压缩包或单个项目文件后开始。"
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
                        ActionArea {
                            id: packMouse
                            anchors.fill: parent
                            accessibleName: "打开上下文面板"
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
                        ActionArea {
                            id: settingsMouse
                            anchors.fill: parent
                            accessibleName: "打开设置"
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
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1
                        Text {
                            Layout.fillWidth: true
                            text: compiler.projectPath.length > 0
                                  ? root.fileName(compiler.projectPath) : "新任务"
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontLg
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: compiler.agentRunning
                                  ? (compiler.currentAgentAction || "正在处理")
                                  : compiler.status
                            color: compiler.agentRunning ? Theme.warning : Theme.textMuted
                            font.pixelSize: Theme.fontSm
                            elide: Text.ElideRight
                        }
                    }
                    Pill {
                        visible: compiler.hasAuditResult
                        text: compiler.trustScore + " 分"
                        bg: compiler.trustScore >= 80 ? Theme.successSoft
                            : compiler.trustScore >= 60 ? Theme.warningSoft : Theme.dangerSoft
                        fg: compiler.trustScore >= 80 ? Theme.success
                            : compiler.trustScore >= 60 ? Theme.warning : Theme.danger
                    }
                    Rectangle {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        radius: Theme.radiusSm
                        color: panelMouse.containsMouse ? Theme.surfaceMuted : "transparent"
                        Icon { anchors.centerIn: parent; name: "sidebar"; size: 16; color: root.rightPanelOpen ? Theme.accent : Theme.textMuted }
                        ActionArea {
                            id: panelMouse
                            anchors.fill: parent
                            accessibleName: root.rightPanelOpen ? "关闭右侧面板" : "打开右侧面板"
                            onClicked: root.rightPanelOpen = !root.rightPanelOpen
                        }
                    }
                }
                Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.border }
            }

            Item {
                id: workspaceHost
                Layout.fillWidth: true
                Layout.fillHeight: true
                readonly property bool splitArtifacts: root.artifactPageKey.length > 0
                                                       && width >= 1250
                                                       && (root.artifactPageKey !== "brain"
                                                           || width >= 1450)

                SessionWorkspacePage {
                    id: workspacePage
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: root.artifactPageKey.length === 0 ? parent.width
                           : workspaceHost.splitArtifacts ? parent.width * 0.38
                                                          : parent.width
                    compiler: compiler
                    enabled: root.artifactPageKey.length === 0 || workspaceHost.splitArtifacts
                    opacity: enabled ? 1 : 0
                    onAttachProjectRequested: materialFileDialog.open()
                    onArtifactRequested: function(pageKey) { root.openArtifact(pageKey, true) }

                    Behavior on opacity {
                        NumberAnimation { duration: Theme.normal; easing.type: Easing.OutCubic }
                    }
                    Behavior on width {
                        NumberAnimation { duration: Theme.normal; easing.type: Easing.OutCubic }
                    }
                }

                ArtifactDetailPage {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: workspaceHost.splitArtifacts ? parent.width * 0.62 : parent.width
                    compiler: compiler
                    pageKey: root.artifactPageKey.length > 0 ? root.artifactPageKey : "dashboard"
                    enabled: root.artifactPageKey.length > 0
                    visible: opacity > 0
                    opacity: enabled ? 1 : 0
                    onCloseRequested: root.closeArtifact()
                    onFilePreviewRequested: function(relativePath) {
                        compiler.previewProjectFile(relativePath)
                        root.rightPanelTab = "preview"
                        root.rightPanelOpen = true
                    }

                    Behavior on opacity { NumberAnimation { duration: Theme.normal } }
                    Behavior on width {
                        NumberAnimation { duration: Theme.normal; easing.type: Easing.OutCubic }
                    }
                }

                Rectangle {
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    x: workspacePage.width
                    width: 1
                    visible: workspaceHost.splitArtifacts
                    color: Theme.border
                    opacity: visible ? 1 : 0

                    Behavior on opacity { NumberAnimation { duration: Theme.fast } }
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: root.animatedRightPanelWidth
            Layout.fillHeight: true
            color: Theme.surfaceMuted
            clip: true
            visible: root.animatedRightPanelWidth > 0.5

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
                                color: root.rightPanelTab === panelTabDelegate.modelData.key ? Theme.surfaceHover : "transparent"
                                RowLayout {
                                    id: tabRow
                                    anchors.centerIn: parent
                                    spacing: 7
                                    Icon { name: panelTabDelegate.modelData.icon; size: 14; color: root.rightPanelTab === panelTabDelegate.modelData.key ? Theme.textPrimary : Theme.textMuted }
                                    Text {
                                        text: panelTabDelegate.modelData.label
                                        color: root.rightPanelTab === panelTabDelegate.modelData.key ? Theme.textPrimary : Theme.textMuted
                                        font.pixelSize: Theme.fontMd
                                        font.bold: root.rightPanelTab === panelTabDelegate.modelData.key
                                    }
                                }
                                ActionArea {
                                    anchors.fill: parent
                                    accessibleName: "切换到" + panelTabDelegate.modelData.label + "面板"
                                    onClicked: root.rightPanelTab = panelTabDelegate.modelData.key
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
                                  : root.rightPanelTab === "preview" ? 1 : 2

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
                            model: compiler.assets.filter(function(item) {
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
                                        compiler.previewProjectFile(fileDelegate.modelData.path)
                                        root.rightPanelTab = "preview"
                                    }
                                }
                            }
                            EmptyState {
                                anchors.fill: parent
                                visible: fileList.count === 0
                                text: compiler.projectContext.originalRoot ? "等待检查文件" : "未选择项目"
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
                                            color: compiler.agentRunning ? Theme.warning
                                                   : compiler.hasAuditResult ? Theme.success
                                                                             : Theme.textTertiary
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: compiler.agentRunning ? "正在检查" : "当前状态"
                                            color: Theme.textPrimary
                                            font.pixelSize: Theme.fontMd
                                            font.bold: true
                                        }
                                        Pill {
                                            visible: compiler.hasAuditResult
                                            text: compiler.trustScore + " 分"
                                            bg: Theme.accentSoft
                                            fg: Theme.accent
                                        }
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: compiler.status
                                        color: Theme.textMuted
                                        font.pixelSize: Theme.fontSm
                                        wrapMode: Text.WordWrap
                                    }
                                    RowLayout {
                                        visible: compiler.hasAuditResult
                                        spacing: 8
                                        Pill {
                                            text: "必须处理 " + compiler.blockerCount
                                            bg: compiler.blockerCount > 0 ? Theme.dangerSoft : Theme.surfaceMuted
                                            fg: compiler.blockerCount > 0 ? Theme.danger : Theme.textMuted
                                        }
                                        Pill {
                                            text: "需关注 " + compiler.warningCount
                                            bg: compiler.warningCount > 0 ? Theme.warningSoft : Theme.surfaceMuted
                                            fg: compiler.warningCount > 0 ? Theme.warning : Theme.textMuted
                                        }
                                    }
                                }
                            }

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
                                    ScrollView {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 260
                                        clip: true
                                        contentWidth: availableWidth

                                        TextArea {
                                            id: filePreviewText
                                            width: parent.width
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
                                            rightPadding: 8
                                            topPadding: 0
                                            bottomPadding: 0
                                            background: null
                                        }
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        visible: compiler.selectedFilePreview.truncated === true
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
                                subtitle: compiler.agentRunning
                                          ? "当前步骤会随运行实时更新。"
                                          : "每一步结果都会进入当前会话上下文。"
                            }
                            Repeater {
                                model: compiler.toolCards
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
                                model: compiler.artifacts
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
                                    border.color: root.artifactPageKey === artifactDelegate.modelData.pageKey
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
                                        onClicked: root.openArtifact(artifactDelegate.modelData.pageKey,
                                                                     artifactDelegate.modelData.available)
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
    }

    Rectangle {
        id: settingsLayer
        anchors.fill: parent
        color: "#00000000"
        enabled: root.settingsOpen
        visible: opacity > 0
        opacity: root.settingsOpen ? 1 : 0
        z: 20

        Behavior on opacity { NumberAnimation { duration: Theme.normal } }

        Rectangle {
            anchors.fill: parent
            color: "#000000"
            opacity: 0.22
            ActionArea {
                anchors.fill: parent
                accessibleName: "关闭设置"
                onClicked: root.settingsOpen = false
            }
        }
        Rectangle {
            id: settingsDrawer
            width: Math.min(520, parent.width - 80)
            height: parent.height
            x: root.settingsOpen ? parent.width - width : parent.width
            color: Theme.surface
            border.color: Theme.border

            Behavior on x {
                NumberAnimation { duration: Theme.normal; easing.type: Easing.OutCubic }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 22
                spacing: 18
                RowLayout {
                    Layout.fillWidth: true
                    Text { Layout.fillWidth: true; text: "设置"; color: Theme.textPrimary; font.pixelSize: Theme.fontTitle; font.bold: true }
                    Rectangle {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        radius: Theme.radiusSm
                        color: closeSettingsMouse.containsMouse ? Theme.surfaceMuted : "transparent"
                        Icon { anchors.centerIn: parent; name: "close"; size: 15; color: Theme.textMuted }
                        ActionArea {
                            id: closeSettingsMouse
                            anchors.fill: parent
                            accessibleName: "关闭设置"
                            onClicked: root.settingsOpen = false
                        }
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

                SectionTitle { title: "智能辅助服务（可选）"; subtitle: "不配置也能使用完整的本地规则检查" }
                FieldInput {
                    Layout.fillWidth: true
                    text: compiler.llmEndpoint
                    placeholderText: "https://服务地址/v1/chat/completions 或 /v1/messages"
                    onTextEdited: compiler.llmEndpoint = text
                }
                FieldInput {
                    Layout.fillWidth: true
                    text: compiler.llmModel
                    placeholderText: "输入该服务支持的模型 ID"
                    onTextEdited: compiler.llmModel = text
                }
                FieldInput {
                    Layout.fillWidth: true
                    echoMode: TextInput.Password
                    text: compiler.llmApiKey
                    placeholderText: "访问密钥（仅在本次运行中使用）"
                    onActiveFocusChanged: if (activeFocus && text === "********") text = ""
                    onTextEdited: compiler.llmApiKey = text
                }
                PrimaryButton {
                    Layout.fillWidth: true
                    enabled: !compiler.llmModelsLoading
                    text: compiler.llmModelsLoading ? "正在获取可用模型…" : "按当前凭证获取模型"
                    onClicked: compiler.refreshLlmModels()
                }
                ComboBox {
                    Layout.fillWidth: true
                    visible: compiler.llmAvailableModels.length > 0
                    model: compiler.llmAvailableModels
                    currentIndex: compiler.llmAvailableModels.indexOf(compiler.llmModel)
                    onActivated: compiler.llmModel = currentText
                }
                Text {
                    Layout.fillWidth: true
                    visible: compiler.llmModelsStatus.length > 0
                    text: compiler.llmModelsStatus
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontXs
                    wrapMode: Text.WordWrap
                }
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: brainConnectionText.implicitHeight + 18
                    radius: 12
                    color: compiler.llmConfigured ? Theme.successSoft : Theme.warningSoft
                    border.color: compiler.llmConfigured ? Theme.success : Theme.warning
                    Text {
                        id: brainConnectionText
                        anchors.fill: parent
                        anchors.margins: 9
                        text: compiler.llmConfigured
                              ? "配置有效，项目任务将默认联网使用该模型；原始材料不会被修改，最终分数仍由本地规则计算。"
                              : "填写有效的 HTTPS 服务地址、模型 ID 和访问密钥后，将自动启用联网模型。"
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSm
                        wrapMode: Text.WordWrap
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
                            id: appearanceDelegate
                            required property var modelData

                            Layout.fillWidth: true
                            Layout.preferredHeight: 38
                            radius: 12
                            color: Theme.appearance === appearanceDelegate.modelData.key ? Theme.accentSoft : Theme.surfaceMuted
                            border.color: Theme.appearance === appearanceDelegate.modelData.key ? Theme.accent : Theme.border
                            Text { anchors.centerIn: parent; text: appearanceDelegate.modelData.label; color: Theme.appearance === appearanceDelegate.modelData.key ? Theme.accent : Theme.textMuted; font.pixelSize: Theme.fontMd; font.bold: true }
                            ActionArea {
                                anchors.fill: parent
                                accessibleName: "切换为" + appearanceDelegate.modelData.label
                                onClicked: Theme.appearance = appearanceDelegate.modelData.key
                            }
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
                            id: colorDelegate
                            required property var modelData

                            Layout.fillWidth: true
                            Layout.preferredHeight: 72
                            radius: 14
                            color: Theme.colorTheme === colorDelegate.modelData.key ? Theme.accentSoft : Theme.surfaceMuted
                            border.color: Theme.colorTheme === colorDelegate.modelData.key ? Theme.accent : Theme.border
                            Column {
                                anchors.centerIn: parent
                                spacing: 7
                                Rectangle { anchors.horizontalCenter: parent.horizontalCenter; width: 24; height: 24; radius: 12; color: colorDelegate.modelData.color }
                                Text { anchors.horizontalCenter: parent.horizontalCenter; text: colorDelegate.modelData.label; color: Theme.textPrimary; font.pixelSize: Theme.fontSm; font.bold: true }
                            }
                            ActionArea {
                                anchors.fill: parent
                                accessibleName: "选择强调色 " + colorDelegate.modelData.label
                                onClicked: Theme.colorTheme = colorDelegate.modelData.key
                            }
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
                            id: backgroundDelegate
                            required property var modelData

                            Layout.fillWidth: true
                            Layout.preferredHeight: 78
                            radius: 14
                            color: Theme.backgroundTheme === backgroundDelegate.modelData.key ? Theme.accentSoft : Theme.surfaceMuted
                            border.color: Theme.backgroundTheme === backgroundDelegate.modelData.key ? Theme.accent : Theme.border
                            Column {
                                anchors.centerIn: parent
                                spacing: 8
                                Rectangle {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: 72
                                    height: 30
                                    radius: 8
                                    color: backgroundDelegate.modelData.color
                                    border.color: Theme.border
                                    Rectangle { anchors.left: parent.left; width: 20; height: parent.height; radius: 8; color: Theme.backgroundTheme === "vscode" && backgroundDelegate.modelData.key === "vscode" ? "#252526" : Qt.rgba(0, 0, 0, 0.06) }
                                    Rectangle { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 5; width: 12; height: 12; radius: 4; color: Theme.accent }
                                }
                                Text { anchors.horizontalCenter: parent.horizontalCenter; text: backgroundDelegate.modelData.label; color: Theme.textPrimary; font.pixelSize: Theme.fontSm; font.bold: true }
                            }
                            ActionArea {
                                anchors.fill: parent
                                accessibleName: "选择背景 " + backgroundDelegate.modelData.label
                                onClicked: Theme.backgroundTheme = backgroundDelegate.modelData.key
                            }
                        }
                    }
                }

                SectionTitle { title: "字体" }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Rectangle {
                        Layout.preferredWidth: 34
                        Layout.preferredHeight: 34
                        radius: 10
                        color: Theme.surfaceMuted
                        Text { anchors.centerIn: parent; text: "−"; color: Theme.textPrimary; font.pixelSize: 18; font.bold: true }
                        ActionArea {
                            anchors.fill: parent
                            accessibleName: "减小界面字体"
                            onClicked: Theme.uiFontSize = Math.max(12, Theme.uiFontSize - 1)
                        }
                    }
                    Text { text: Theme.uiFontSize + "px"; color: Theme.textPrimary; font.pixelSize: Theme.fontMd; font.bold: true; horizontalAlignment: Text.AlignHCenter; Layout.preferredWidth: 54 }
                    Rectangle {
                        Layout.preferredWidth: 34
                        Layout.preferredHeight: 34
                        radius: 10
                        color: Theme.surfaceMuted
                        Text { anchors.centerIn: parent; text: "+"; color: Theme.textPrimary; font.pixelSize: 18; font.bold: true }
                        ActionArea {
                            anchors.fill: parent
                            accessibleName: "增大界面字体"
                            onClicked: Theme.uiFontSize = Math.min(24, Theme.uiFontSize + 1)
                        }
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
                            id: accessDelegate
                            required property var modelData

                            Layout.fillWidth: true
                            Layout.preferredHeight: 74
                            radius: 14
                            color: compiler.accessMode === accessDelegate.modelData.key
                                   ? (accessDelegate.modelData.key === "bypass" ? Theme.warningSoft : Theme.accentSoft)
                                   : Theme.surfaceMuted
                            border.color: compiler.accessMode === accessDelegate.modelData.key
                                          ? (accessDelegate.modelData.key === "bypass" ? Theme.warning : Theme.accent)
                                          : Theme.border
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 3
                                Text {
                                    Layout.fillWidth: true
                                    text: accessDelegate.modelData.label
                                    color: compiler.accessMode === accessDelegate.modelData.key
                                           ? (accessDelegate.modelData.key === "bypass" ? Theme.warning : Theme.accent)
                                           : Theme.textPrimary
                                    font.pixelSize: Theme.fontMd
                                    font.bold: true
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: accessDelegate.modelData.hint
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontXs
                                    wrapMode: Text.WordWrap
                                }
                            }
                            ActionArea {
                                anchors.fill: parent
                                accessibleName: "切换权限模式到 " + accessDelegate.modelData.label
                                onClicked: compiler.accessMode = accessDelegate.modelData.key
                            }
                        }
                    }
                }

                Repeater {
                    model: compiler.permissionCards
                    delegate: Rectangle {
                        id: permissionDelegate
                        required property var modelData

                        Layout.fillWidth: true
                        implicitHeight: permCol.implicitHeight + 18
                        radius: 12
                        color: permissionDelegate.modelData.allowed ? Theme.successSoft : Theme.dangerSoft
                        border.color: Theme.border
                        ColumnLayout {
                            id: permCol
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4
                            RowLayout {
                                Layout.fillWidth: true
                                Text { Layout.fillWidth: true; text: permissionDelegate.modelData.name; color: Theme.textPrimary; font.pixelSize: Theme.fontMd; font.bold: true }
                                Text { text: permissionDelegate.modelData.status; color: permissionDelegate.modelData.allowed ? Theme.success : Theme.danger; font.pixelSize: Theme.fontSm; font.bold: true }
                            }
                            Text { Layout.fillWidth: true; text: permissionDelegate.modelData.detail; color: Theme.textMuted; font.pixelSize: Theme.fontSm; wrapMode: Text.WordWrap }
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
