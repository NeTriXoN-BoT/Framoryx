pragma Singleton
import QtQuick
QtObject {
    property real scale: 1
    property bool motion: true
    property color accent: "#7df2c6"
    readonly property color background: "#0b1118"
    readonly property color surface: "#131e29"
    readonly property color raised: "#1a2a37"
    readonly property color border: "#2a3c4b"
    readonly property color text: "#edf5fa"
    readonly property color muted: "#96aabb"
    readonly property color amber: "#f7be6f"
    readonly property color red: "#ff8b96"
    readonly property int duration: motion ? 170 : 0
    function px(value) { return Math.round(value * scale) }
    function font(value) { return Math.max(10,Math.round(value * scale)) }
}
