import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id: studio
    spacing: Theme.px(18)
    GridLayout {
        Layout.fillWidth: true; columns: studio.width>Theme.px(900)?2:1
        columnSpacing: Theme.px(18); rowSpacing: Theme.px(18)
        FCard {
            Layout.fillWidth: true; Layout.preferredWidth: Theme.px(350)
            Text { text: "Make it yours."; color: Theme.text; font.pixelSize: Theme.px(22); font.weight: Font.DemiBold }
            Text { text: "DISPLAY METHOD"; color: Theme.muted; font.pixelSize: Theme.px(10) }
            FCombo {
                id: method
                Layout.fillWidth: true
                model: app.platformName==="Windows"?["Auto · external first","External desktop overlay","In-game · manual hook"]:["Auto · external first","External · X Present","MangoHud game layer · opt-in"]
                currentIndex: app.platformName==="Windows"?app.options.mode:app.options.linuxOverlayMode
                onActivated: {
                    if(app.platformName==="Windows"&&currentIndex===2&&app.state.inGameSupported===false){app.setOption("mode",1);app.notify("This rendering API uses the external overlay. Optional in-game limiting is available for DX11/DX12.");method.currentIndex=Qt.binding(function(){return app.options.mode});return}
                    if(app.platformName==="Linux"&&currentIndex===2)layerDialog.open();else app.setOption(app.platformName==="Windows"?"mode":"linuxOverlayMode",currentIndex)
                }
            }
            Text { text: app.platformName==="Windows"?"External monitoring does not load code into your game. In-game mode requires a separate, explicit hook approval.":"External capture uses X11 presentation events. Native Wayland and some renderers may not expose them; Framoryx explains when frames are unavailable."; color: Theme.muted; font.pixelSize: Theme.px(11); Layout.fillWidth: true; wrapMode: Text.Wrap }
            Dialog {
                id: layerDialog; anchors.centerIn: Overlay.overlay; width: Theme.px(520); height: Theme.px(300); modal: true; title: "Enable the MangoHud game layer?"; standardButtons: Dialog.Yes|Dialog.Cancel
                background: Rectangle { color: Theme.surface; radius: Theme.px(14); border.color: Theme.border }
                contentItem: Text { text: "MangoHud loads a graphics layer into games launched with monitoring. It enables the in-game overlay and frame limiting.\n\nGame and anti-cheat rules may prohibit third-party layers. Use at your own risk; Framoryx cannot guarantee approval or prevent bans. Known anti-cheat indicators prevent a monitored layer launch.\n\nEnable this method for your next monitored launch?"; color: Theme.text; font.pixelSize: Theme.px(13); wrapMode: Text.Wrap }
                onAccepted: {if(app.state.antiCheatStatus==="detected")app.notify("Known anti-cheat indicators are present. The external method remains selected.","warning");else app.setOption("linuxOverlayMode",2);method.currentIndex=Qt.binding(function(){return app.options.linuxOverlayMode})}
                onRejected: method.currentIndex=Qt.binding(function(){return app.options.linuxOverlayMode})
            }
            GridLayout {
                Layout.fillWidth: true; columns: 2; columnSpacing: Theme.px(12); rowSpacing: Theme.px(10)
                Text { text: "LAYOUT"; color: Theme.muted; font.pixelSize: Theme.px(10) }
                Text { text: "ACCENT"; color: Theme.muted; font.pixelSize: Theme.px(10) }
                FCombo { Layout.fillWidth: true; model: ["Glass","Compact","Telemetry bar","Minimal","Dashboard"]; currentIndex: app.options.style; onActivated: app.setOption("style",currentIndex) }
                FCombo { Layout.fillWidth: true; model: ["Mint","Ice","Violet","Amber","Rose","Cyan","Solar","Lime","Coral","Silver"]; currentIndex: app.options.accent; onActivated: app.setOption("accent",currentIndex) }
                Text { text: "OVERLAY SIZE"; color: Theme.muted; font.pixelSize: Theme.px(10) }
                Text { text: "BACKGROUND"; color: Theme.muted; font.pixelSize: Theme.px(10) }
                FCombo { Layout.fillWidth: true; property var sizes: [0,50,75,100,125,150,200,250,300]; model: ["Auto","50%","75%","100%","125%","150%","200%","250%","300%"]; currentIndex: Math.max(0,sizes.indexOf(app.options.overlayScale)); onActivated: app.setOption("overlayScale",sizes[currentIndex]) }
                RowLayout { Layout.fillWidth: true; FSlider { Layout.fillWidth: true; from: 0; to: 100; stepSize: 1; value: app.options.opacity; Accessible.name: "Overlay background opacity"; onMoved: app.setOption("opacity",Math.round(value)) } Text { text: app.options.opacity+"%"; color: Theme.text; font.pixelSize: Theme.px(12); Layout.preferredWidth: Theme.px(38) } }
            }
            Text { text: "TEXT SIZE · "+app.options.fontScale+"%"; color: Theme.muted; font.pixelSize: Theme.px(10) }
            FSlider { Layout.fillWidth: true; from: 80; to: 140; stepSize: 5; value: app.options.fontScale; Accessible.name: "Overlay text size"; onMoved: app.setOption("fontScale",Math.round(value)) }
            Text { text: "READOUTS · use the arrows to arrange"; color: Theme.muted; font.pixelSize: Theme.px(10); Layout.topMargin: Theme.px(8) }
            Repeater {
                model: app.readouts
                RowLayout {
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    FToggle { text: modelData.name; checked: modelData.enabled; Layout.fillWidth: true; onToggled: app.setReadout(modelData.index,checked) }
                    FButton { text: "↑"; implicitWidth: Theme.px(30); implicitHeight: Theme.px(28); quiet: true; enabled: index>0; hint: "Move "+modelData.name+" up"; onClicked: app.moveReadout(modelData.index,-1) }
                    FButton { text: "↓"; implicitWidth: Theme.px(30); implicitHeight: Theme.px(28); quiet: true; enabled: index<15; hint: "Move "+modelData.name+" down"; onClicked: app.moveReadout(modelData.index,1) }
                }
            }
            RowLayout { Layout.fillWidth: true; FButton { text: app.state.overlay?"Hide overlay":"Show overlay"; primary: true; Layout.fillWidth: true; onClicked: app.action("overlay") } FButton { text: "Reset"; onClicked: app.action("resetOverlay") } }
        }
        ColumnLayout {
            Layout.fillWidth: true; Layout.preferredWidth: Theme.px(650); Layout.alignment: Qt.AlignTop; spacing: Theme.px(18)
            FCard {
                Layout.fillWidth: true
                RowLayout { Layout.fillWidth: true; Text { text: "LIVE PREVIEW"; color: Theme.accent; font.pixelSize: Theme.px(11); font.weight: Font.DemiBold; Layout.fillWidth: true } Text { text: app.state.live?"Live game data":"Sample values · layout preview"; color: Theme.muted; font.pixelSize: Theme.px(10) } }
                OverlayPreview { Layout.fillWidth: true; Layout.preferredHeight: Theme.px(380) }
                Text { text: app.state.overlayHint||"Drag the preview to choose its position. Your layout is saved automatically."; color: Theme.muted; font.pixelSize: Theme.px(12); Layout.fillWidth: true; wrapMode: Text.Wrap }
                Flow { Layout.fillWidth: true; spacing: Theme.px(8); Repeater { model: [{name:"Top left",x:.03,y:.03},{name:"Top right",x:.97,y:.03},{name:"Bottom left",x:.03,y:.97},{name:"Bottom right",x:.97,y:.97}]; FButton { required property var modelData; text: modelData.name; quiet: true; onClicked: {app.setOption("hudX",modelData.x);app.setOption("hudY",modelData.y)} } } }
            }
            FCard {
                Layout.fillWidth: true
                Text { text: "Keep a favorite look"; color: Theme.text; font.pixelSize: Theme.px(18); font.weight: Font.DemiBold }
                RowLayout { Layout.fillWidth: true; TextField { id: presetName; Layout.fillWidth: true; placeholderText: "Name this preset"; color: Theme.text; font.pixelSize: Theme.px(13); selectByMouse: true; background: Rectangle { color: Theme.raised; radius: 8; border.color: presetName.activeFocus?Theme.accent:Theme.border } } FButton { text: "Save preset"; primary: true; enabled: presetName.text.trim().length>0; onClicked: {app.saveProfile(presetName.text);presetName.clear()} } }
                Repeater { model: app.profiles; RowLayout { required property var modelData; required property int index; Layout.fillWidth: true; Text { text: modelData.name; color: Theme.text; font.pixelSize: Theme.px(13); Layout.fillWidth: true; elide: Text.ElideRight } FButton { text: "Load"; quiet: true; onClicked: app.applyProfile(index) } FButton { text: "Remove"; quiet: true; danger: true; onClicked: app.removeProfile(index) } } }
                RowLayout { FButton { text: "Import presets"; onClicked: shell.importPresets() } FButton { text: "Export presets"; enabled: app.profiles.length>0; onClicked: shell.exportPresets() } }
            }
        }
    }
}
