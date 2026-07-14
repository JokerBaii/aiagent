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

    property bool settingsOpen: false
    property bool rightPanelOpen: false
    property string rightPanelTab: "files"
    property string artifactPageKey: ""
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
            if (compiler.agentRunning) {
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

        WorkbenchSidebar {
            Layout.preferredWidth: 280
            Layout.fillHeight: true
            compiler: compiler
            onNewSessionRequested: {
                compiler.newSession()
                workspacePage.focusComposer("")
            }
            onAddProjectRequested: folderDialog.open()
            onContextRequested: {
                root.rightPanelTab = "preview"
                root.rightPanelOpen = true
            }
            onSettingsRequested: root.settingsOpen = true
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

        WorkbenchRightPanel {
            Layout.preferredWidth: root.animatedRightPanelWidth
            Layout.fillHeight: true
            visible: root.animatedRightPanelWidth > 0.5
            compiler: compiler
            currentTab: root.rightPanelTab
            activeArtifactPage: root.artifactPageKey
            onCloseRequested: root.rightPanelOpen = false
            onTabRequested: function(tab) { root.rightPanelTab = tab }
            onArtifactRequested: function(pageKey) { root.openArtifact(pageKey, true) }
        }
    }

    SettingsDrawer {
        anchors.fill: parent
        compiler: compiler
        opened: root.settingsOpen
        onCloseRequested: root.settingsOpen = false
    }
}
