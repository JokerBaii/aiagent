import QtQuick
import QtQuick.Shapes
import ".."

Rectangle {
    id: root
    property real size: 32
    property real corner: size * 0.31

    width: size
    height: size
    radius: corner
    color: Theme.isDark ? "#FFFFFF" : "#000000"

    readonly property color shieldColor: Theme.isDark ? "#000000" : "#FFFFFF"
    readonly property color checkColor: Theme.colorTheme === "black"
                                        ? (Theme.isDark ? "#FFFFFF" : "#000000")
                                        : Theme.accent

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        transform: Scale { xScale: root.width / 171; yScale: root.height / 171 }

        ShapePath {
            strokeColor: "transparent"
            fillColor: root.shieldColor
            PathSvg { path: "M85.5 18L140 39V78C140 112 118 139 85.5 153C53 139 31 112 31 78V39Z" }
        }
        ShapePath {
            strokeColor: "transparent"
            fillColor: root.checkColor
            PathSvg { path: "M55 84L75 104L118 61L128 71L75 124L45 94Z" }
        }
    }
}
