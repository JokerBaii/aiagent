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
    title: "Contest Trust Workbench"
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

    readonly property var reportItems: [
        { icon: "▦", label: "可信仪表盘" },
        { icon: "⌘", label: "资产清单" },
        { icon: "◈", label: "项目画像" },
        { icon: "◎", label: "声明证据" },
        { icon: "≡", label: "一致性" },
        { icon: "!", label: "规则风险" },
        { icon: "✓", label: "补证任务" },
        { icon: "⇄", label: "二次审计差分" },
        { icon: "AI", label: "LLM Brain" },
        { icon: "⇩", label: "报告导出" }
    ]
    readonly property var panelTabs: [
        { key: "files", label: "文件", icon: "▦" },
        { key: "preview", label: "预览", icon: "◌" },
        { key: "skills", label: "技能", icon: "◇" }
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

    property int reportIndex: 0
    property bool reportOpen: false
    property bool settingsOpen: false
    property bool rightPanelOpen: true
    property string rightPanelTab: "files"

    function openReport(index) {
        root.reportIndex = index
        root.reportOpen = true
    }

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
                    Text {
                        text: "TOKEN"
                        color: Theme.sidebarTextActive
                        font.pixelSize: 18
                        font.bold: true
                        font.letterSpacing: 0
                    }
                    Text {
                        text: "/"
                        color: Theme.accent
                        font.pixelSize: 18
                        font.bold: true
                    }
                    Text {
                        text: "CODE"
                        color: Theme.sidebarTextActive
                        font.pixelSize: 18
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "‹"
                        color: Theme.sidebarText
                        font.pixelSize: 24
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 52
                    radius: 26
                    color: newTaskMouse.containsMouse ? Theme.accentHover : Theme.accent
                    Behavior on color { ColorAnimation { duration: Theme.fast } }
                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 10
                        Text { text: "+"; color: Theme.isDark && Theme.colorTheme === "black" ? "#101010" : "#FFFFFF"; font.pixelSize: 20; font.bold: true }
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
                        onClicked: compiler.newSession()
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    radius: 16
                    color: addMouse.containsMouse ? Theme.surfaceHover : Theme.surfaceMuted
                    border.color: Theme.border
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 12
                        spacing: 9
                        Text { text: "▣"; color: Theme.textMuted; font.pixelSize: 14 }
                        Text {
                            Layout.fillWidth: true
                            text: "添加项目"
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontMd
                            font.bold: true
                        }
                        Text { text: "⌘O"; color: Theme.textMuted; font.pixelSize: Theme.fontXs }
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
                    radius: 16
                    color: Theme.surface
                    border.color: Theme.border
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 9
                        Rectangle { width: 9; height: 9; radius: 5; color: compiler.agentRunning ? Theme.warning : Theme.success }
                        Text {
                            Layout.fillWidth: true
                            text: compiler.trustScore > 0 ? "Score " + compiler.trustScore : "DeepSeek"
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontMd
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text {
                            text: compiler.agentRunning ? "running" : (compiler.trustScore > 0 ? "completed" : "ready")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontXs
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    radius: 16
                    color: Theme.surface
                    border.color: Theme.border
                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        text: "⌕  搜索任务..."
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMd
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                ListView {
                    id: sessionList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 3
                    model: compiler.sessionList
                    section.property: "active"
                    delegate: Rectangle {
                        width: sessionList.width
                        implicitHeight: 46
                        radius: 18
                        color: modelData.active ? Theme.sidebarActive
                             : sessionMouse.containsMouse ? Qt.rgba(0, 0, 0, Theme.isDark ? 0.16 : 0.04)
                             : "transparent"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 12
                            spacing: 8
                            Text {
                                Layout.fillWidth: true
                                text: modelData.title
                                color: modelData.active ? Theme.sidebarTextActive : Theme.sidebarText
                                font.pixelSize: Theme.fontMd
                                font.bold: modelData.active
                                elide: Text.ElideRight
                            }
                            Text {
                                text: modelData.subtitle
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontXs
                                elide: Text.ElideRight
                            }
                        }
                        MouseArea {
                            id: sessionMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
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
                        radius: 14
                        color: packMouse.containsMouse ? Theme.surfaceHover : "transparent"
                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 8
                            Text { text: "▤"; color: Theme.textMuted; font.pixelSize: 15 }
                            Text { text: "预览"; color: Theme.sidebarText; font.pixelSize: Theme.fontSm; font.bold: true }
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
                        radius: 14
                        color: skillsMouse.containsMouse ? Theme.surfaceHover : "transparent"
                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 8
                            Text { text: "◇"; color: Theme.textMuted; font.pixelSize: 15 }
                            Text { text: "技能"; color: Theme.sidebarText; font.pixelSize: Theme.fontSm; font.bold: true }
                        }
                        MouseArea {
                            id: skillsMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.rightPanelTab = "skills"
                                root.rightPanelOpen = true
                            }
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        radius: 14
                        color: settingsMouse.containsMouse ? Theme.surfaceHover : "transparent"
                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 8
                            Text { text: "⚙"; color: Theme.textMuted; font.pixelSize: 15 }
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
                Layout.preferredHeight: 64
                color: Theme.window
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    spacing: 16
                    Text {
                        text: compiler.trustScore > 0 ? "Trust " + compiler.trustScore : "DeepseekV4Flash"
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
                    Rectangle { width: 8; height: 8; radius: 4; color: Theme.success }
                    Text { text: "Agent"; color: Theme.success; font.pixelSize: Theme.fontSm; font.bold: true }
                    Rectangle { width: 8; height: 8; radius: 4; color: Theme.borderStrong }
                    Text { text: "CLI"; color: Theme.textMuted; font.pixelSize: Theme.fontSm }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "⇩"
                        color: Theme.textMuted
                        font.pixelSize: 20
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.openReport(9)
                        }
                    }
                    Rectangle {
                        width: 32
                        height: 32
                        radius: 10
                        color: panelMouse.containsMouse ? Theme.surfaceMuted : "transparent"
                        Text { anchors.centerIn: parent; text: root.rightPanelOpen ? "▥" : "▤"; color: Theme.textMuted; font.pixelSize: 18 }
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
                Layout.fillWidth: true
                Layout.fillHeight: true
                compiler: compiler
                onOpenReport: function(index) { root.openReport(index) }
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
                    Layout.preferredHeight: 64
                    color: Theme.surfaceMuted
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        spacing: 8
                        Repeater {
                            model: root.panelTabs
                            delegate: Rectangle {
                                implicitWidth: tabText.implicitWidth + 34
                                implicitHeight: 38
                                radius: 12
                                color: root.rightPanelTab === modelData.key ? Theme.surfaceHover : "transparent"
                                Row {
                                    anchors.centerIn: parent
                                    spacing: 7
                                    Text { text: modelData.icon; color: Theme.textMuted; font.pixelSize: 13 }
                                    Text {
                                        id: tabText
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
                        Text {
                            text: "×"
                            color: Theme.textMuted
                            font.pixelSize: 20
                            MouseArea {
                                anchors.fill: parent
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
                                  : root.rightPanelTab === "skills" ? 1
                                  : 2

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
                                Text { text: "▱"; color: Theme.textMuted; font.pixelSize: 15 }
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
                            radius: 14
                            color: Theme.surface
                            border.color: Theme.border
                            Text {
                                anchors.fill: parent
                                anchors.leftMargin: 14
                                anchors.rightMargin: 14
                                text: "⌕  搜索文件..."
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontMd
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                        ListView {
                            id: fileList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.leftMargin: 14
                            Layout.rightMargin: 14
                            clip: true
                            spacing: 4
                            model: compiler.assets
                            delegate: Rectangle {
                                width: fileList.width
                                implicitHeight: 36
                                radius: 10
                                color: fileMouse.containsMouse ? Theme.surface : "transparent"
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    spacing: 8
                                    Text {
                                        text: modelData.roleCode === "SourceCode" ? "◇"
                                             : modelData.roleCode === "Archive" ? "▣"
                                             : modelData.auditable ? "▤" : "◌"
                                        color: modelData.risk && modelData.risk.length > 0 ? Theme.warning : Theme.textMuted
                                        font.pixelSize: 14
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.path
                                        color: modelData.risk && modelData.risk.length > 0 ? Theme.warning : Theme.textMuted
                                        font.pixelSize: Theme.fontMd
                                        elide: Text.ElideMiddle
                                    }
                                }
                                MouseArea { id: fileMouse; anchors.fill: parent; hoverEnabled: true }
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
                        clip: true
                        ColumnLayout {
                            width: parent.width
                            spacing: 14
                            anchors.margins: 14
                            SectionTitle { Layout.leftMargin: 14; title: "审计技能" }
                            Repeater {
                                model: compiler.toolCards
                                delegate: Rectangle {
                                    Layout.leftMargin: 14
                                    Layout.rightMargin: 14
                                    Layout.fillWidth: true
                                    implicitHeight: skillCol.implicitHeight + 18
                                    radius: 14
                                    color: Theme.surface
                                    border.color: Theme.border
                                    ColumnLayout {
                                        id: skillCol
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 4
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.name
                                                color: Theme.textPrimary
                                                font.pixelSize: Theme.fontMd
                                                font.bold: true
                                            }
                                            Pill {
                                                text: modelData.status
                                                bg: modelData.status === "完成" ? Theme.successSoft
                                                    : modelData.status === "进行中" ? Theme.accentSoft
                                                    : Theme.surfaceMuted
                                                fg: modelData.status === "完成" ? Theme.success
                                                    : modelData.status === "进行中" ? Theme.accent
                                                    : Theme.textMuted
                                            }
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.detail
                                            color: Theme.textMuted
                                            font.pixelSize: Theme.fontSm
                                            wrapMode: Text.WordWrap
                                        }
                                    }
                                }
                            }
                            SectionTitle { Layout.leftMargin: 14; title: "权限边界" }
                            Repeater {
                                model: compiler.permissionCards
                                delegate: Rectangle {
                                    Layout.leftMargin: 14
                                    Layout.rightMargin: 14
                                    Layout.fillWidth: true
                                    implicitHeight: permCol.implicitHeight + 18
                                    radius: 14
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
                        }
                    }

                    ScrollView {
                        clip: true
                        ColumnLayout {
                            width: parent.width
                            spacing: 10
                            anchors.margins: 14
                            SectionTitle { Layout.leftMargin: 14; title: "产物" }
                            Repeater {
                                model: compiler.artifacts
                                delegate: Rectangle {
                                    Layout.leftMargin: 14
                                    Layout.rightMargin: 14
                                    Layout.fillWidth: true
                                    implicitHeight: artifactCol.implicitHeight + 20
                                    radius: 14
                                    color: Theme.surface
                                    border.color: Theme.border
                                    ColumnLayout {
                                        id: artifactCol
                                        anchors.fill: parent
                                        anchors.margins: 12
                                        spacing: 6
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Text { Layout.fillWidth: true; text: modelData.title; color: Theme.textPrimary; font.pixelSize: Theme.fontMd; font.bold: true }
                                            Pill { text: modelData.kind; bg: Theme.surfaceMuted; fg: Theme.textMuted }
                                        }
                                        Text { Layout.fillWidth: true; text: modelData.detail; color: Theme.textMuted; font.pixelSize: Theme.fontSm; wrapMode: Text.WordWrap }
                                        Text { text: "打开报告 →"; color: Theme.accent; font.pixelSize: Theme.fontSm; font.bold: true }
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.openReport(modelData.title.indexOf("数据包") >= 0 || modelData.title.indexOf("Markdown") >= 0 ? 9
                                                              : modelData.title.indexOf("修复") >= 0 ? 6
                                                              : modelData.title.indexOf("差分") >= 0 ? 7
                                                              : modelData.title.indexOf("智能体") >= 0 ? 8 : 0)
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
                    Text { Layout.fillWidth: true; text: "主题配置"; color: Theme.textPrimary; font.pixelSize: Theme.fontTitle; font.bold: true }
                    Rectangle {
                        width: 32; height: 32; radius: 10
                        color: closeSettingsMouse.containsMouse ? Theme.surfaceMuted : "transparent"
                        Text { anchors.centerIn: parent; text: "×"; color: Theme.textMuted; font.pixelSize: 20 }
                        MouseArea { id: closeSettingsMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.settingsOpen = false }
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
                Item { Layout.fillHeight: true }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#00000000"
        visible: root.reportOpen
        z: 15
        Rectangle {
            anchors.fill: parent
            color: "#000000"
            opacity: 0.25
            MouseArea { anchors.fill: parent; onClicked: root.reportOpen = false }
        }
        Rectangle {
            id: drawer
            width: Math.min(parent.width - 120, 1040)
            height: parent.height
            anchors.right: parent.right
            color: Theme.surface
            border.color: Theme.border

            ColumnLayout {
                anchors.fill: parent
                spacing: 0
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    color: Theme.surface
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 20
                        spacing: 12
                        Text { text: "审计报告"; color: Theme.textPrimary; font.pixelSize: Theme.fontXl; font.bold: true }
                        Item { Layout.fillWidth: true }
                        Rectangle {
                            width: 32; height: 32; radius: 10
                            color: closeReportMouse.containsMouse ? Theme.surfaceMuted : "transparent"
                            Text { anchors.centerIn: parent; text: "×"; color: Theme.textMuted; font.pixelSize: 20 }
                            MouseArea { id: closeReportMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.reportOpen = false }
                        }
                    }
                    Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.border }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 0
                    Rectangle {
                        Layout.preferredWidth: 200
                        Layout.fillHeight: true
                        color: Theme.surfaceMuted
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4
                            Repeater {
                                model: root.reportItems
                                delegate: Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 36
                                    radius: 12
                                    color: root.reportIndex === index ? Theme.surface : "transparent"
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 10
                                        anchors.rightMargin: 8
                                        spacing: 9
                                        Text { text: modelData.icon; color: root.reportIndex === index ? Theme.accent : Theme.textMuted; font.pixelSize: Theme.fontSm; font.bold: true }
                                        Text { Layout.fillWidth: true; text: modelData.label; color: root.reportIndex === index ? Theme.textPrimary : Theme.textMuted; font.pixelSize: Theme.fontSm; font.bold: root.reportIndex === index; elide: Text.ElideRight }
                                    }
                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.reportIndex = index }
                                }
                            }
                            Item { Layout.fillHeight: true }
                        }
                    }
                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: root.reportIndex
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
        }
    }
}
