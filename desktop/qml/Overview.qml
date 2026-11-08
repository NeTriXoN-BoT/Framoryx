import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id: page
    spacing: Theme.px(18)
    GridLayout {
        Layout.fillWidth: true
        columns: page.width>Theme.px(1000)?4:2
        columnSpacing: Theme.px(14); rowSpacing: Theme.px(14)
        Metric { Layout.fillWidth: true; label: "Live FPS"; value: app.state.fps||0; available: app.state.live; highlighted: true; note: app.state.live?"Frames arriving from your game":"Waiting for game frames" }
        Metric { Layout.fillWidth: true; label: "Average FPS"; value: app.state.average||0; available: app.state.average>0; note: "Rolling capture" }
        Metric { Layout.fillWidth: true; label: "1% low"; value: app.state.low||0; available: app.state.low>0; note: "The slowest frames" }
        Metric { Layout.fillWidth: true; label: "Frame time"; value: app.state.frameTime||0; available: app.state.live; suffix: " ms"; decimals: 2; note: "Lower and steadier feels smoother" }
    }
    GridLayout {
        Layout.fillWidth: true; columns: page.width>Theme.px(850)?2:1
        columnSpacing: Theme.px(18); rowSpacing: Theme.px(18)
        FCard {
            Layout.fillWidth: true
            Layout.preferredWidth: Theme.px(660)
            RowLayout { Layout.fillWidth: true; Text { text: "Frame pacing"; color: Theme.text; font.pixelSize: Theme.font(19); font.weight: Font.DemiBold; Layout.fillWidth: true } Text { text: app.state.renderer||"Waiting for renderer"; color: Theme.muted; font.pixelSize: Theme.font(11); elide: Text.ElideRight; Layout.maximumWidth: Theme.px(260) } }
            FrameGraph { Layout.fillWidth: true; Layout.preferredHeight: Theme.px(220); samples: app.graph }
            RowLayout { Layout.fillWidth: true; Text { text: "Earlier"; color: Theme.muted; font.pixelSize: Theme.font(10); Layout.fillWidth: true } Text { text: "Now"; color: Theme.muted; font.pixelSize: Theme.font(10) } }
            Text { visible: app.state.droppedFrameSamples>0; text: "Frame samples omitted: "+String(app.state.droppedFrameSamples||0)+". Statistics use the frames received."; color: Theme.amber; font.pixelSize: Theme.font(11); Layout.fillWidth: true; wrapMode: Text.Wrap }
        }
        FCard {
            Layout.fillWidth: true
            Layout.preferredWidth: Theme.px(300)
            Text { text: "System pulse"; color: Theme.text; font.pixelSize: Theme.font(19); font.weight: Font.DemiBold }
            Text { text: "PROCESSOR"; color: Theme.muted; font.pixelSize: Theme.font(10); font.letterSpacing: 1 }
            Text { text: Math.round(app.state.cpu||0)+"%"; color: Theme.accent; font.pixelSize: Theme.font(32); font.weight: Font.DemiBold }
            ProgressBar { Layout.fillWidth: true; value: (app.state.cpu||0)/100; implicitHeight: Theme.px(5); background: Rectangle { color: Theme.border; radius: 3 } contentItem: Item { Rectangle { width: parent.width*parent.parent.position; height: 5; radius: 3; color: Theme.accent } } }
            Item { Layout.preferredHeight: Theme.px(8) }
            Text { text: "MEMORY"; color: Theme.muted; font.pixelSize: Theme.font(10); font.letterSpacing: 1 }
            Text { text: Number(app.state.ram||0).toFixed(1)+" GB"; color: Theme.text; font.pixelSize: Theme.font(30); font.weight: Font.DemiBold }
            Text { text: "of "+Number(app.state.totalRam||0).toFixed(1)+" GB system RAM"; color: Theme.muted; font.pixelSize: Theme.font(12) }
        }
    }
    FCard {
        Layout.fillWidth: true
        RowLayout { Layout.fillWidth: true; Text { text: "Hardware, in view."; color: Theme.text; font.pixelSize: Theme.font(21); font.weight: Font.DemiBold; Layout.fillWidth: true } Text { text: "LIVE · 1 SECOND"; color: Theme.accent; font.family: "monospace"; font.pixelSize: Theme.font(9) } }
        Text { text: app.state.gpuName||"Checking available GPU sensors…"; color: Theme.muted; font.pixelSize: Theme.font(12); Layout.fillWidth: true; wrapMode: Text.Wrap }
        GridLayout {
            Layout.fillWidth: true; columns: page.width>Theme.px(1050)?4:2; columnSpacing: Theme.px(12); rowSpacing: Theme.px(12)
            Repeater {
                model: [{key:"gpu",name:"GPU usage",unit:"%",note:"Device-wide load"},{key:"vramPercent",name:"VRAM usage",unit:"%",note:"Dedicated memory"},{key:"gpuTemp",name:"GPU temperature",unit:" °C",note:"Driver temperature sensor"},{key:"cpuTemp",name:"CPU temperature",unit:" °C",note:"Hottest available CPU sensor"},{key:"gpuPower",name:"GPU power",unit:" W",note:"Reported device power"},{key:"gpuClock",name:"GPU clock",unit:" MHz",note:"Graphics clock"},{key:"gpuFan",name:"GPU fan",unit:"%",note:"Driver-reported fan speed"},{key:"cpuClock",name:"CPU clock",unit:" MHz",note:"Average exposed core clock"}]
                Metric {
                    required property var modelData
                    Layout.fillWidth: true
                    readonly property var reading: app.state[modelData.key]
                    label: modelData.name; value: typeof reading==="number"?reading:0; available: typeof reading==="number"&&isFinite(reading); suffix: modelData.unit
                    note: available?modelData.note:"Unavailable on this device"; highlighted: modelData.key==="gpu"
                }
            }
        }
        Text { text: typeof app.state.vramUsed==="number"&&typeof app.state.vramTotal==="number"?Number(app.state.vramUsed).toFixed(1)+" / "+Number(app.state.vramTotal).toFixed(1)+" GB dedicated GPU memory":"Dedicated GPU memory capacity is unavailable."; color: Theme.muted; font.pixelSize: Theme.font(11); Layout.fillWidth: true; wrapMode: Text.Wrap }
        Text { visible: typeof app.state.gpuHotspot==="number"&&isFinite(app.state.gpuHotspot); text: "GPU junction / hotspot · "+Number(app.state.gpuHotspot||0).toFixed(0)+" °C"; color: Theme.muted; font.pixelSize: Theme.font(11); Layout.fillWidth: true; wrapMode: Text.Wrap }
        Text { text: app.state.sensorSource||"Sensors are read in the background. Unsupported values remain unavailable."; color: Theme.muted; font.pixelSize: Theme.font(11); Layout.fillWidth: true; wrapMode: Text.Wrap }
        RowLayout { visible: app.platformName==="Windows"; Layout.fillWidth: true; FButton { text: "Temperature setup"; quiet: true; onClicked: app.action("sensorGuide") } Text { text: "CPU temperatures can use an existing Libre Hardware Monitor WMI provider. Framoryx does not install a sensor driver."; color: Theme.muted; font.pixelSize: Theme.font(11); Layout.fillWidth: true; wrapMode: Text.Wrap } }
    }
    FCard {
        Layout.fillWidth: true
        Flow {
            Layout.fillWidth: true; spacing: Theme.px(12)
            FButton { text: app.state.overlay?"Hide overlay":"Show overlay"; primary: true; onClicked: app.action("overlay") }
            FButton { text: "Customize overlay"; onClicked: shell.page=1 }
            FButton { text: "Export capture"; enabled: app.state.average>0; onClicked: shell.exportCapture() }
            FButton { text: "Open test scene"; onClicked: app.action("fixture") }
        }
        Text { Layout.fillWidth: true; text: app.platformName==="Linux"?"Select a running X11 game for external capture, or choose an explicit game layer in Overlay Studio.":"Choose a running game or leave Auto selected, then start capture. Your data stays local."; color: Theme.muted; font.pixelSize: Theme.font(12); wrapMode: Text.Wrap }
    }
}
