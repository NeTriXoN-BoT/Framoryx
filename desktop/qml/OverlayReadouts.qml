import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import Framoryx.Hud 1.0
Rectangle {
        id: hud
        property bool sample: true
        property real density: Theme.scale
        function px(value){return Math.round(value*density)}
        readonly property bool rainbow: app.options.accent === 10
        readonly property bool glow: !!app.options.hudTextGlow
        readonly property bool valueColors: !!app.options.hudValueColors
        readonly property bool temperatureColors: !!app.options.hudTemperatureColors
        readonly property var containingWindow: Window.window
        readonly property int rainbowCycleMs: app.hudRainbowCycleMs
        property real rainbowPhase: 0
        Timer {
            interval: 33
            repeat: true
            triggeredOnStart: true
            running: hud.rainbow && hud.visible && !!hud.containingWindow &&
                     hud.containingWindow.visible && hud.containingWindow.visibility !== Window.Minimized
            onTriggered: hud.rainbowPhase = (Date.now() % hud.rainbowCycleMs) / hud.rainbowCycleMs
        }
        readonly property bool compact: app.options.style===1||app.options.style===3
        property var geometry: app.overlayLayout
        readonly property var labelOverrides: app.options.hudLabels || []
        readonly property string fontFamily: app.options.hudFontFamily || ""
        readonly property var telemetry: app.state
        readonly property bool preview: sample && !telemetry.live
        readonly property real totalRam: preview ? 16 : measured(telemetry.totalRam)
        readonly property var frameKeys: ["fps", "average", "low", "frameTime"]
        readonly property var previewFrames: [144, 141, 120, 6.94]
        readonly property var hardwareKeys: ["gpu", "vramPercent", "gpuTemp", "cpuTemp", "gpuPower", "gpuClock"]
        readonly property var hardwareUnits: ["%", "%", " °C", " °C", " W", " MHz"]
        readonly property var previewHardware: [68, 42, 67, 61, 145, 2100]
        function customLabel(index) { return labelOverrides[index] || "" }
        function measured(value) {
            return typeof value === "number" && isFinite(value) && value >= 0 ? value : -1
        }
        function numericReading(index) {
            if (index <= 3) {
                if (preview) return previewFrames[index]
                const value = measured(telemetry[frameKeys[index]])
                return telemetry.live && value > 0 ? value : -1
            }
            if (index === 5) return preview ? 28 : measured(telemetry.cpu)
            if (index === 6) return preview ? 11.2 : totalRam > 0 ? measured(telemetry.ram) : -1
            if (index === 7) return app.options.cap
            if (index >= 10)
                return preview ? previewHardware[index - 10] : measured(telemetry[hardwareKeys[index - 10]])
            return -1
        }
        function readingColor(index, value) {
            if (value < 0 || (!valueColors && !(temperatureColors && (index === 12 || index === 13))))
                return "transparent"
            return app.hudReadoutColor(index, value, valueColors, temperatureColors, totalRam)
        }
        component HudText: Text {
            id: label
            property color ink: Theme.text
            property bool valueTint: false
            readonly property bool useRainbow: hud.rainbow && !valueTint
            readonly property bool useEffects: useRainbow || hud.glow
            color: useEffects ? "transparent" : ink
            textFormat: Text.PlainText
            Binding {
                target: label
                property: "font.family"
                value: hud.fontFamily
                when: hud.fontFamily !== ""
            }
            Loader {
                anchors.fill: parent
                active: label.useEffects && label.visible && label.text !== ""
                sourceComponent: HudStyledText {
                    caption: label.text
                    font: label.font
                    elide: label.elide === Text.ElideRight
                    rightAligned: label.effectiveHorizontalAlignment === Text.AlignRight
                    ink: label.ink
                    rainbow: label.useRainbow
                    phase: label.useRainbow ? hud.rainbowPhase : 0
                    glow: hud.glow
                }
            }
        }
        implicitWidth: hud.px(geometry.width)
        implicitHeight: hud.px(geometry.height)
        width: implicitWidth; height: implicitHeight
        color: Qt.rgba(.045,.073,.1,app.options.opacity/100)
        border.width: app.options.opacity===0||app.options.style===3?0:1; border.color: "#284a4d"; radius: hud.px(compact?8:13)
        Rectangle { visible: app.options.opacity>0&&app.options.style!==3; x: hud.px(10); y: 0; height: 2; width: parent.width-hud.px(20); color: Theme.accent }
        function reading(index, value){
            if(index===0)return (value>=0?Math.round(value):"—")+" "+(customLabel(0)||"FPS");
            if(index===1||index===2)return (value>=0?Math.round(value):"—")+" FPS";
            if(index===3)return (value>=0?value.toFixed(2):"—")+" ms";
            if(index===5)return value>=0?Math.round(value)+"%":"—";
            if(index===6)return value>=0?value.toFixed(1)+" GB":"—";
            if(index===7)return value>0?value:"Uncapped";
            if(index===8)return telemetry.live?telemetry.renderer:hud.sample?(app.platformName==="Windows"?"DirectX 12":"Vulkan"):"Waiting";
            if(index>=10)return value>=0?value.toFixed(index===14?1:0)+hardwareUnits[index-10]:"—";
            return customLabel(9)||"FRAMORYX";
        }
        Repeater {
            model: hud.geometry.cells
            Item {
                id: cell
                required property var modelData
                readonly property real numericValue: hud.numericReading(modelData.index)
                readonly property color tint: hud.readingColor(modelData.index, numericValue)
                clip: true
                x: hud.px(modelData.x); y: hud.px(modelData.y); width: hud.px(modelData.width); height: hud.px(modelData.height)
                HudText {
                    visible: modelData.index===0||modelData.index===9
                    text: visible ? hud.reading(modelData.index, cell.numericValue) : ""
                    valueTint: cell.tint.a > 0
                    ink: valueTint ? cell.tint : modelData.index===0?Theme.accent:Theme.muted
                    font.pixelSize: hud.px(modelData.index===0?(hud.compact?28:34):10)
                    font.weight: modelData.index===0?Font.Bold:Font.DemiBold
                    width: parent.width
                    elide: hud.customLabel(modelData.index) ? Text.ElideRight : Text.ElideNone
                }
                HudText { visible: modelData.index!==0&&modelData.index!==4&&modelData.index!==9; text: hud.customLabel(modelData.index)||modelData.name; ink: Theme.muted; font.pixelSize: hud.px(modelData.column?10:hud.compact?11:12); width: modelData.column?parent.width:parent.width-hud.px(105); elide: Text.ElideRight }
                HudText {
                    visible: modelData.index!==0&&modelData.index!==4&&modelData.index!==9
                    text: visible ? hud.reading(modelData.index, cell.numericValue) : ""
                    valueTint: cell.tint.a > 0
                    ink: valueTint ? cell.tint : Theme.text
                    y: modelData.column?hud.px(19):0
                    x: modelData.column?0:parent.width-width
                    font.pixelSize: hud.px(modelData.column?17:hud.compact?12:13)
                    font.weight: Font.DemiBold
                }
                HudText {
                    id: graphHeading
                    visible: modelData.index===4 && text.length>0
                    text: modelData.index===4 ? hud.customLabel(4) : ""
                    ink: Theme.muted
                    width: parent.width
                    height: hud.px(14)
                    font.pixelSize: hud.px(10)
                    elide: Text.ElideRight
                }
                Loader {
                    active: modelData.index === 4
                    width: parent.width
                    y: graphHeading.visible ? graphHeading.height : 0
                    height: Math.max(1, parent.height-y-hud.px(10))
                    sourceComponent: FrameGraph { samples: hud.telemetry.live ? app.graph : hud.sample ? [7,6.9,9,7,7,8,7,6.8,7,7,12,6.9] : [] }
                }
            }
        }
}
