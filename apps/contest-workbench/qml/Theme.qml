pragma Singleton
import QtQuick

QtObject {
    id: theme

    property string appearance: "light"
    property string colorTheme: "black"
    property string backgroundTheme: "garden"
    property string fontPreset: "microsoft"
    property int uiFontSize: 16

    readonly property bool isDark: appearance === "dark" || backgroundTheme === "vscode"

    readonly property color baseAccent: {
        if (colorTheme === "blue") return isDark ? "#6B9AFF" : "#4E80F7"
        if (colorTheme === "orange") return isDark ? "#D4856A" : "#C47252"
        if (colorTheme === "green") return isDark ? "#6DBF62" : "#57A64B"
        return isDark ? "#D0D0D0" : "#333333"
    }

    readonly property color window: {
        if (backgroundTheme === "vscode") return "#1E1E1E"
        if (backgroundTheme === "garden" && !isDark) return "#FAF9F6"
        if (backgroundTheme === "sakura" && !isDark) return "#FFF4F7"
        if (backgroundTheme === "lake" && !isDark) return "#F3FBF8"
        if (backgroundTheme === "dusk" && !isDark) return "#F7F1FB"
        if (backgroundTheme === "ink" && !isDark) return "#F8F5EC"
        return isDark ? "#0A0A0A" : "#FFFFFF"
    }
    readonly property color surface: {
        if (backgroundTheme === "garden" && !isDark) return "#FFFFFF"
        if (backgroundTheme === "sakura" && !isDark) return "#FFF8FA"
        if (backgroundTheme === "lake" && !isDark) return "#F8FCFA"
        if (backgroundTheme === "dusk" && !isDark) return "#FBF7FC"
        if (backgroundTheme === "ink" && !isDark) return "#FBF8F0"
        return isDark ? "#1C1C1E" : "#FFFFFF"
    }
    readonly property color surfaceMuted: {
        if (backgroundTheme === "vscode") return "#252526"
        if (backgroundTheme === "garden" && !isDark) return "#F3F1ED"
        if (backgroundTheme === "sakura" && !isDark) return "#FCEAF0"
        if (backgroundTheme === "lake" && !isDark) return "#E8F5F1"
        if (backgroundTheme === "dusk" && !isDark) return "#EFE7F4"
        if (backgroundTheme === "ink" && !isDark) return "#ECE8DD"
        return isDark ? "#2C2C2E" : "#F0F0F2"
    }
    readonly property color surfaceHover: {
        if (backgroundTheme === "garden" && !isDark) return "#ECE9E3"
        if (backgroundTheme === "sakura" && !isDark) return "#ECBECC"
        return isDark ? "#343436" : "#E8E8EA"
    }
    readonly property color sidebar: {
        if (backgroundTheme === "vscode") return "#252526"
        if (backgroundTheme === "garden" && !isDark) return "#F3F0EA"
        if (backgroundTheme === "sakura" && !isDark) return "#FFF0F4"
        if (backgroundTheme === "lake" && !isDark) return "#ECF8F4"
        if (backgroundTheme === "dusk" && !isDark) return "#F3EBF7"
        if (backgroundTheme === "ink" && !isDark) return "#F0EBDD"
        return isDark ? "#1C1C1E" : "#F5F5F7"
    }
    readonly property color sidebarActive: {
        if (backgroundTheme === "garden" && !isDark) return Qt.rgba(0.20, 0.20, 0.20, 0.08)
        return isDark ? "#2C2C2E" : "#E8E8EA"
    }
    readonly property color sidebarText: {
        if (backgroundTheme === "garden" && !isDark) return "#756C64"
        return isDark ? "#A3A3A3" : "#777777"
    }
    readonly property color sidebarTextActive: {
        if (backgroundTheme === "garden" && !isDark) return "#29241F"
        return isDark ? "#F0F0F0" : "#1A1A1A"
    }
    readonly property color input: backgroundTheme === "garden" && !isDark ? "#FFFFFF" : (isDark ? "#1C1C1E" : "#FFFFFF")
    readonly property color userBubble: backgroundTheme === "garden" && !isDark ? "#D9857A" : (isDark ? "#D0D0D0" : "#333333")
    readonly property color userBubbleText: isDark ? "#101010" : "#FFFFFF"

    readonly property color accent: baseAccent
    readonly property color accentHover: colorTheme === "black"
                                         ? (isDark ? "#FFFFFF" : "#1A1A1A")
                                         : Qt.darker(baseAccent, isDark ? 1.08 : 1.12)
    readonly property color accentActive: colorTheme === "black"
                                          ? (isDark ? "#FFFFFF" : "#111111")
                                          : Qt.darker(baseAccent, 1.18)
    readonly property color accentText: isDark && colorTheme === "black" ? "#101010" : "#FFFFFF"
    readonly property color accentSoft: Qt.rgba(baseAccent.r, baseAccent.g, baseAccent.b, isDark ? 0.16 : 0.10)
    readonly property color accentGhost: Qt.rgba(baseAccent.r, baseAccent.g, baseAccent.b, isDark ? 0.24 : 0.16)

    readonly property color textPrimary: {
        if (backgroundTheme === "garden" && !isDark) return "#29241F"
        return isDark ? "#F0F0F0" : "#1A1A1A"
    }
    readonly property color textSecondary: {
        if (backgroundTheme === "garden" && !isDark && colorTheme === "black") return "#625A52"
        return colorTheme === "black" ? (isDark ? "#B0B0B0" : "#555555") : baseAccent
    }
    readonly property color textMuted: backgroundTheme === "garden" && !isDark ? "#80766D" : (isDark ? "#8C8C8C" : "#888888")
    readonly property color textTertiary: backgroundTheme === "garden" && !isDark ? "#A0968C" : (isDark ? "#737373" : "#A0A0A0")

    readonly property color border: backgroundTheme === "garden" && !isDark ? "#E5DED5" : (isDark ? "#2C2C2E" : "#EBEBF0")
    readonly property color borderStrong: backgroundTheme === "garden" && !isDark ? "#D9CFC4" : (isDark ? "#38383A" : "#E5E5EA")
    readonly property color borderSubtle: backgroundTheme === "garden" && !isDark ? "#EEE9E3" : (isDark ? "#242426" : "#EFEFF3")

    readonly property color success: "#10B981"
    readonly property color successSoft: isDark ? "#123D32" : "#DDF7EF"
    readonly property color warning: "#F59E0B"
    readonly property color warningSoft: isDark ? "#44310E" : "#FFF6DF"
    readonly property color danger: "#EF4444"
    readonly property color dangerSoft: isDark ? "#4A1F24" : "#FFF0F1"

    readonly property string fontFamily: {
        if (fontPreset === "mono")
            return Qt.platform.os === "linux" ? "Cascadia Mono NF" : "Cascadia Mono"
        if (fontPreset === "sourceHan") return "Noto Sans CJK SC"
        if (fontPreset === "lxgw") return "LXGW WenKai"
        if (fontPreset === "system") {
            if (Qt.platform.os === "windows") return "Segoe UI"
            if (Qt.platform.os === "osx") return "Helvetica Neue"
            return "Noto Sans"
        }
        if (Qt.platform.os === "windows") return "Microsoft YaHei UI"
        if (Qt.platform.os === "osx") return "PingFang SC"
        return "Noto Sans CJK SC"
    }
    readonly property string monoFamily: Qt.platform.os === "linux"
                                         ? "Cascadia Mono NF" : "Cascadia Mono"

    readonly property int radius: 12
    readonly property int radiusSm: 8
    readonly property int radiusMd: 16
    readonly property int radiusLg: 20
    readonly property int gap: 16
    readonly property int fast: 120
    readonly property int normal: 190
    readonly property int slow: 320

    readonly property int fontXs: Math.max(10, uiFontSize - 7)
    readonly property int fontSm: Math.max(11, uiFontSize - 6)
    readonly property int fontMd: Math.max(12, uiFontSize - 5)
    readonly property int fontLg: Math.max(13, uiFontSize - 4)
    readonly property int fontXl: Math.max(15, uiFontSize - 2)
    readonly property int fontTitle: Math.max(20, uiFontSize + 4)
}
