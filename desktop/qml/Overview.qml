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
            RowLayout { Layout.fillWidth: true; Text { text: "Frame pacing"; color: Theme.text; font.pixelSize: Theme.px(19); font.weight: Font.DemiBold; Layout.fillWidth: true } Text { text: app.state.renderer||"Waiting for renderer"; color: Theme.muted; font.pixelSize: Theme.px(11); elide: Text.ElideRight; Layout.maximumWidth: Theme.px(260) } }
            FrameGraph { Layout.fillWidth: true; Layout.preferredHeight: Theme.px(220); samples: app.graph }
            RowLayout { Layout.fillWidth: true; Text { text: "Earlier"; color: Theme.muted; font.pixelSize: Theme.px(10); Layout.fillWidth: true } Text { text: "Now"; color: Theme.muted; font.pixelSize: Theme.px(10) } }
        }
        FCard {
            Layout.fillWidth: true
            Layout.preferredWidth: Theme.px(300)
            Text { text: "System pulse"; color: Theme.text; font.pixelSize: Theme.px(19); font.weight: Font.DemiBold }
            Text { text: "PROCESSOR"; color: Theme.muted; font.pixelSize: Theme.px(10); font.letterSpacing: 1 }
            Text { text: Math.round(app.state.cpu||0)+"%"; color: Theme.accent; font.pixelSize: Theme.px(32); font.weight: Font.DemiBold }
            ProgressBar { Layout.fillWidth: true; value: (app.state.cpu||0)/100; implicitHeight: Theme.px(5); background: Rectangle { color: Theme.border; radius: 3 } contentItem: Item { Rectangle { width: parent.width*parent.parent.position; height: 5; radius: 3; color: Theme.accent } } }
            Item { Layout.preferredHeight: Theme.px(8) }
            Text { text: "MEMORY"; color: Theme.muted; font.pixelSize: Theme.px(10); font.letterSpacing: 1 }
            Text { text: Number(app.state.ram||0).toFixed(1)+" GB"; color: Theme.text; font.pixelSize: Theme.px(30); font.weight: Font.DemiBold }
            Text { text: "of "+Number(app.state.totalRam||0).toFixed(1)+" GB system RAM"; color: Theme.muted; font.pixelSize: Theme.px(12) }
        }
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
        Text { Layout.fillWidth: true; text: app.platformName==="Linux"?"Launch a library game with MangoHud, or copy Framoryx's Steam launch options in Games.":"Choose a running game or leave Auto selected, then start capture. Your data stays local."; color: Theme.muted; font.pixelSize: Theme.px(12); wrapMode: Text.Wrap }
    }
}
