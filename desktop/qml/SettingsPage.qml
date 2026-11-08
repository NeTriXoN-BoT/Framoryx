import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id: page
    property var shell
    spacing: Theme.px(18)
    GridLayout {
        Layout.fillWidth: true; columns: page.width>Theme.px(850)?2:1; columnSpacing: Theme.px(18); rowSpacing: Theme.px(18)
        FCard {
            Layout.fillWidth: true
            Text { text: "Your comfort, first."; color: Theme.text; font.pixelSize: Theme.px(23); font.weight: Font.DemiBold }
            Text { text: "INTERFACE SIZE"; color: Theme.muted; font.pixelSize: Theme.px(10); font.letterSpacing: 1 }
            FCombo { Layout.fillWidth: true; property var sizes: [0,60,75,80,90,100,110,125,150,175]; model: ["Auto · follow your display","60%","75%","80%","90%","100%","110%","125%","150%","175%"]; currentIndex: Math.max(0,sizes.indexOf(app.options.uiScale)); onActivated: app.setOption("uiScale",sizes[currentIndex]) }
            Text { text: "System DPI stays automatic for sharp rendering. The layout adapts to window size, with scrolling when needed."; color: Theme.muted; font.pixelSize: Theme.px(12); Layout.fillWidth: true; wrapMode: Text.Wrap }
            FToggle { text: "Ambient artwork and motion effects"; checked: app.options.motion; onToggled: app.setOption("motion",checked) }
            Text { text: "Motion pauses when Framoryx is hidden, minimized or inactive. Disable it for a quieter workspace."; color: Theme.muted; font.pixelSize: Theme.px(12); Layout.fillWidth: true; wrapMode: Text.Wrap }
            Flow { Layout.fillWidth: true; spacing: Theme.px(8); FButton { text: "Create desktop shortcut"; onClicked: app.action("shortcut") } FButton { text: "Open configuration folder"; onClicked: app.action("openConfig") } }
        }
        FCard {
            Layout.fillWidth: true
            Text { text: "Built around your setup."; color: Theme.text; font.pixelSize: Theme.px(23); font.weight: Font.DemiBold }
            Text { text: "Framoryx 0.3.0 · "+app.platformName+(app.platformName==="Linux"?" · "+app.state.session:""); color: Theme.accent; font.pixelSize: Theme.px(13); Layout.fillWidth: true; wrapMode: Text.Wrap }
            Text { text: "Your game library, presets and measurements stay on this computer. Microphone testing is opt-in and stores its sample in memory."; color: Theme.muted; font.pixelSize: Theme.px(12); Layout.fillWidth: true; wrapMode: Text.Wrap }
            Text { text: app.platformName==="Windows"?"Unsigned Windows build · no publisher certificate. Verify the release checksum before sharing or using a download.":"Native Linux application · hardware and compositor support are listed in the release notes."; color: Theme.amber; font.pixelSize: Theme.px(11); Layout.fillWidth: true; wrapMode: Text.Wrap }
            RowLayout { FButton { text: "Quick guide"; onClicked: app.action("guide") } FButton { text: "Latest release"; onClicked: app.action("release") } }
        }
    }
    FCard {
        Layout.fillWidth: true
        RowLayout { Layout.fillWidth: true; Text { text: "Components and compatibility"; color: Theme.text; font.pixelSize: Theme.px(21); font.weight: Font.DemiBold; Layout.fillWidth: true } FButton { text: "Refresh status"; onClicked: app.action("refresh") } }
        Repeater { model: app.capabilities; Rectangle {
            required property var modelData
            Layout.fillWidth: true; implicitHeight: row.implicitHeight+Theme.px(24); color: Theme.raised; radius: Theme.px(11)
            RowLayout { id: row; x: Theme.px(14); y: Theme.px(12); width: parent.width-Theme.px(28); spacing: Theme.px(16); Rectangle { width: Theme.px(8); height: width; radius: width/2; color: modelData.ready?Theme.accent:Theme.amber; Layout.alignment: Qt.AlignTop; Layout.topMargin: 5 } ColumnLayout { Layout.fillWidth: true; Text { text: modelData.name; color: Theme.text; font.pixelSize: Theme.px(14); font.weight: Font.DemiBold; Layout.fillWidth: true; wrapMode: Text.Wrap } Text { text: modelData.detail; color: Theme.muted; font.pixelSize: Theme.px(12); Layout.fillWidth: true; wrapMode: Text.Wrap } } Text { text: modelData.ready?"Available":"Setup needed"; color: modelData.ready?Theme.accent:Theme.amber; font.pixelSize: Theme.px(11) } }
        } }
        Text { visible: app.platformName==="Linux"; text: app.state.setupCommand||""; color: Theme.muted; font.family: "monospace"; font.pixelSize: Theme.px(11); Layout.fillWidth: true; wrapMode: Text.Wrap }
        Text { text: app.platformName==="Windows"?"Audio setup: install Equalizer APO, enable it for your playback/capture devices in its Configurator, then return to apply effects. RNNoise requires a 48 kHz capture format.":"Audio setup: apply effects in Audio, then select Framoryx Microphone or Framoryx Output in your game or system settings. RNNoise runs at 48 kHz. The app does not replace your audio server or restart desktop services."; color: Theme.muted; font.pixelSize: Theme.px(12); Layout.fillWidth: true; wrapMode: Text.Wrap }
        Flow { Layout.fillWidth: true; spacing: Theme.px(8); FButton { visible: app.platformName==="Windows"; text: "Get Equalizer APO"; onClicked: app.action("getApo") } FButton { visible: app.platformName==="Windows"; text: "Open device configurator"; onClicked: app.action("apoConfigurator") } FButton { visible: app.platformName==="Windows"; text: "Choose APO config"; onClicked: shell.chooseApo() } FButton { text: "Open audio guide"; onClicked: app.action("soundSettings") } }
    }
    FCard {
        Layout.fillWidth: true
        Text { text: "Diagnostics"; color: Theme.text; font.pixelSize: Theme.px(19); font.weight: Font.DemiBold }
        Text { text: [app.state.engineDiagnostic,app.state.gameDiagnostic,app.state.micDiagnostic,app.state.outputDiagnostic].filter(function(x){return x&&String(x).trim().length}).join("\n")||"No component errors reported."; color: Theme.muted; font.family: "monospace"; font.pixelSize: Theme.px(11); Layout.fillWidth: true; wrapMode: Text.WrapAnywhere }
    }
}
