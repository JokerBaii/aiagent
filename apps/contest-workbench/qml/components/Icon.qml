import QtQuick
import QtQuick.Shapes
import ".."

Item {
    id: root

    required property string name
    property color color: Theme.textMuted
    property real size: 16
    property real strokeWidth: 1.5

    implicitWidth: size
    implicitHeight: size

    readonly property var iconData: ({
        "plus":        { paths: ["M8 3v10M3 8h10"] },
        "chevronLeft": { paths: ["M10 4L6 8L10 12"] },
        "chevronDown": { paths: ["M4 6l4 4 4-4"] },
        "chevronRight":{ paths: ["M6 4l4 4-4 4"] },
        "arrowRight":  { paths: ["M3 8h10M9 4l4 4-4 4"] },
        "search":      { paths: ["M7.5 7.5m-4.5 0a4.5 4.5 0 1 0 9 0a4.5 4.5 0 1 0 -9 0", "M11 11l3 3"] },
        "folder":      { paths: ["M2.5 4.5h4l1.2 1.5h5.8v6.5a1 1 0 0 1-1 1h-10a1 1 0 0 1-1-1v-7a1 1 0 0 1 1-1z"] },
        "folderPlus":  { paths: ["M2.5 4.5h4l1.2 1.5h5.8v6.5a1 1 0 0 1-1 1h-10a1 1 0 0 1-1-1v-7a1 1 0 0 1 1-1z", "M11 8v3M9.5 9.5h3"] },
        "settings":    { paths: ["M8 5.5a2.5 2.5 0 1 0 0 5a2.5 2.5 0 0 0 0-5", "M8 1v2M8 13v2M1 8h2M13 8h2M3.05 3.05l1.41 1.41M11.54 11.54l1.41 1.41M3.05 12.95l1.41-1.41M11.54 4.46l1.41-1.41"] },
        "preview":     { paths: ["M2 4h12v8H2zM5 14h6"] },
        "skills":      { paths: ["M8 1L1 4.5l7 3.5 7-3.5L8 1zM1 11.5l7 3.5 7-3.5M1 8l7 3.5L15 8"] },
        "close":       { paths: ["M4 4l8 8M12 4l-8 8"] },
        "check":       { paths: ["M3 8.5l3.2 3.2L13 5"] },
        "checkSmall":  { paths: ["M2.5 6l2.5 2.5 4.5-4.5"] },
        "sidebar":     { paths: ["M2 3h12v10H2zM6 3v10"] },
        "attach":      { paths: ["M14 8.5l-5.5 5.5a3.5 3.5 0 0 1-5-5l6-6a2.5 2.5 0 0 1 3.5 3.5l-6 6a1.5 1.5 0 0 1-2-2l5.5-5.5"] },
        "download":    { paths: ["M8 2v8M4.5 7L8 10.5L11.5 7M3 13h10"] },
        "code":        { paths: ["M5 4L1 8l4 4M11 4l4 4-4 4"] },
        "ask":         { paths: ["M8 8m-5.5 0a5.5 5.5 0 1 0 11 0a5.5 5.5 0 1 0 -11 0", "M6 6.5a2 2 0 0 1 3.5 1.5c0 1-1.5 1.5-1.5 1.5M8 12v.5"] },
        "plan":        { paths: ["M4 4h8M4 8h6M4 12h4"] },
        "bypass":      { paths: ["M8 2l1.5 4H14l-3.5 2.5L12 13 8 10l-4 3 1.5-4.5L2 6h4.5L8 2z"] },
        "rewind":      { paths: ["M2 7a5 5 0 0 1 9.33-2.5M12 7a5 5 0 0 1-9.33 2.5", "M11 2v3h-3", "M3 12V9h3"] },
        "think":       { paths: ["M8 6m-4 0a4 4 0 1 0 8 0a4 4 0 1 0 -8 0", "M5.5 9.5C5.5 11.5 6 13 8 13s2.5-1.5 2.5-3.5", "M6.5 14h3"] },
        "stop":        { fills: ["M4 4h8v8H4z"] },
        "file":        { paths: ["M7 1H3a1 1 0 0 0-1 1v8a1 1 0 0 0 1 1h6a1 1 0 0 0 1-1V4L7 1z", "M7 1v3h3"] },
        "terminal":    { paths: ["M1 2h10v8H1zM3.5 4.5L5 6l-1.5 1.5M6.5 7.5h2"] },
        "edit":        { paths: ["M8.5 1.5l2 2-6.5 6.5H2V8L8.5 1.5z", "M7 3l2 2"] },
        "toolStack":   { paths: ["M2 2h7v7H2zM3.5 3.5h7v7h-7z"] },
        "diff":        { paths: ["M11 3H6a2 2 0 0 0-2 2v6M5 13h5a2 2 0 0 0 2-2V5", "M2.5 3.5l1.5 1.5 1.5-1.5", "M13.5 12.5l-1.5-1.5-1.5 1.5"] },
        "list":        { paths: ["M3 4h10M3 8h8M3 12h5"] },
        "dot":         { fills: ["M8 8m-3 0a3 3 0 1 0 6 0a3 3 0 1 0 -6 0"] }
    })

    readonly property var current: iconData[name] !== undefined ? iconData[name] : ({})
    readonly property string strokePath: current.paths !== undefined ? current.paths.join(" ") : ""
    readonly property string fillPath: current.fills !== undefined ? current.fills.join(" ") : ""

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        transform: Scale { xScale: root.width / 16; yScale: root.height / 16 }

        ShapePath {
            strokeColor: root.strokePath.length > 0 ? root.color : "transparent"
            strokeWidth: root.strokeWidth
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.strokePath }
        }

        ShapePath {
            strokeColor: "transparent"
            strokeWidth: 0
            fillColor: root.fillPath.length > 0 ? root.color : "transparent"
            PathSvg { path: root.fillPath }
        }
    }
}
