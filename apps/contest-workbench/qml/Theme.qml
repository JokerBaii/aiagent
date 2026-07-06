pragma Singleton
import QtQuick

QtObject {
    // 背景与表面
    readonly property color window: "#FAF9F5"
    readonly property color surface: "#FFFFFF"
    readonly property color surfaceMuted: "#F2F0E9"
    readonly property color surfaceHover: "#F0EEE6"

    // 侧边栏
    readonly property color sidebar: "#262624"
    readonly property color sidebarActive: "#37352F"
    readonly property color sidebarText: "#B7B4A9"
    readonly property color sidebarTextActive: "#F5F4EF"

    // 强调色（赤陶橙）
    readonly property color accent: "#C96442"
    readonly property color accentHover: "#B4553A"
    readonly property color accentActive: "#9E4A32"
    readonly property color accentSoft: "#F6E7E0"

    // 文本
    readonly property color textPrimary: "#25231D"
    readonly property color textSecondary: "#6B6960"
    readonly property color textMuted: "#9C9990"

    // 边框
    readonly property color border: "#E7E3D8"
    readonly property color borderStrong: "#D8D3C4"

    // 语义色
    readonly property color success: "#4F7A5B"
    readonly property color successSoft: "#E7F0E8"
    readonly property color warning: "#A9761F"
    readonly property color warningSoft: "#F6ECD6"
    readonly property color danger: "#B4453C"
    readonly property color dangerSoft: "#F6E2DE"

    // 尺寸
    readonly property int radius: 12
    readonly property int radiusSm: 8
    readonly property int radiusLg: 16
    readonly property int gap: 16
    readonly property int fast: 120
    readonly property int normal: 190
    readonly property int slow: 320
}
