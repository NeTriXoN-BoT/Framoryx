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
            Text { text: "Find your smooth."; color: Theme.text; font.pixelSize: Theme.px(24); font.weight: Font.DemiBold }
            Text { text: "Choose a frame rate your game can sustain. Zero means uncapped."; color: Theme.muted; font.pixelSize: Theme.px(12); Layout.fillWidth: true; wrapMode: Text.Wrap }
            RowLayout {
                Layout.fillWidth: true
                FButton { text: "−"; implicitWidth: Theme.px(38); onClicked: app.setOption("cap",app.options.cap<=10?0:app.options.cap-1) }
                TextField { id: limit; text: String(app.options.cap); color: Theme.accent; font.pixelSize: Theme.px(27); font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter; Layout.fillWidth: true; validator: IntValidator { bottom: 0; top: 1000 } selectByMouse: true; Accessible.name: "Frame limit"; onEditingFinished: if(acceptableInput)app.setOption("cap",Number(text)); background: Rectangle { color: Theme.raised; radius: Theme.px(10); border.color: limit.activeFocus?Theme.accent:Theme.border } }
                FButton { text: "+"; implicitWidth: Theme.px(38); onClicked: app.setOption("cap",app.options.cap<10?10:Math.min(1000,app.options.cap+1)) }
            }
            Flow { Layout.fillWidth: true; spacing: Theme.px(7); Repeater { model: [0,30,60,90,120,144,165,240]; FButton { required property int modelData; text: modelData===0?"Uncapped":String(modelData); primary: app.options.cap===modelData; onClicked: app.setOption("cap",modelData) } } }
            FButton { text: app.platformName==="Windows"?(app.state.hookActive?"Update active limit":"Enable in-game limit"):"Save launch frame limit"; primary: true; onClicked: app.platformName==="Windows"?shell.requestHook():app.notify("Saved. The next monitored launch uses this frame limit.") }
            FButton { visible: app.platformName==="Windows"; text: "Disable in-game hook"; enabled: app.state.hookActive; onClicked: app.action("disableHook") }
            Text { text: app.platformName==="Windows"?"Optional game hooks require explicit approval. Process blocks always take priority.":"MangoHud applies the limit to monitored Vulkan/OpenGL games. Gamescope can also control pacing at launch."; color: Theme.muted; font.pixelSize: Theme.px(11); Layout.fillWidth: true; wrapMode: Text.Wrap }
        }
        FCard {
            Layout.fillWidth: true
            Text { text: "A bigger way to play."; color: Theme.text; font.pixelSize: Theme.px(24); font.weight: Font.DemiBold }
            Text { text: app.platformName==="Linux"&&app.state.session==="Wayland"?"Wayland controls game sizing through your compositor. Enable Gamescope before launching for a controlled fullscreen session.":"Select a running game window, apply borderless fullscreen, and restore its original window mode when you are done."; color: Theme.muted; font.pixelSize: Theme.px(12); Layout.fillWidth: true; wrapMode: Text.Wrap }
            Flow { Layout.fillWidth: true; spacing: Theme.px(10); FButton { text: "Make borderless"; primary: true; onClicked: app.action("borderless") } FButton { text: "Restore window"; onClicked: app.action("restore") } }
            FToggle { text: "Auto borderless for allowed games"; checked: app.options.autoBorderless; onToggled: app.setOption("autoBorderless",checked) }
            FToggle { visible: app.platformName==="Linux"; text: "Launch games inside Gamescope"; checked: app.options.gamescope; onToggled: app.setOption("gamescope",checked) }
            Text { text: "Automatic changes only affect games on your allow list. Original window state is restored when Framoryx closes."; color: Theme.muted; font.pixelSize: Theme.px(11); Layout.fillWidth: true; wrapMode: Text.Wrap }
            Text { text: "USE AT YOUR OWN RISK"; color: Theme.amber; font.pixelSize: Theme.px(11); font.weight: Font.DemiBold }
            Text { text: "Games with anti-cheat may restrict or penalize overlays and in-game hooks. You are responsible for checking and following each game's rules. Framoryx does not guarantee anti-cheat compatibility. Its maintainers accept no responsibility for bans, suspensions, lost access or other consequences arising from its use."; color: Theme.muted; font.pixelSize: Theme.px(11); Layout.fillWidth: true; wrapMode: Text.Wrap }
            FButton { text: "Edit game rules"; quiet: true; onClicked: shell.page=4 }
        }
    }
    FCard {
        Layout.fillWidth: true
        RowLayout { Layout.fillWidth: true; Text { text: "Your game library"; color: Theme.text; font.pixelSize: Theme.px(22); font.weight: Font.DemiBold; Layout.fillWidth: true } FButton { text: "Add game"; primary: true; onClicked: shell.chooseGame() } }
        Text { visible: app.games.length===0; text: "Add an executable for quick launches with your Framoryx settings."; color: Theme.muted; font.pixelSize: Theme.px(13); Layout.fillWidth: true; wrapMode: Text.Wrap }
        Repeater { model: app.games; Rectangle {
            required property var modelData
            Layout.fillWidth: true; implicitHeight: row.implicitHeight+Theme.px(26); color: modelData.id===app.options.selectedGame?"#1b3736":Theme.raised; radius: Theme.px(12); border.color: modelData.id===app.options.selectedGame?Theme.accent:Theme.border
            RowLayout { id: row; x: Theme.px(14); y: Theme.px(13); width: parent.width-Theme.px(28); spacing: Theme.px(12); ColumnLayout { Layout.fillWidth: true; Text { text: modelData.name; color: Theme.text; font.pixelSize: Theme.px(16); font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight } Text { text: modelData.executable; color: Theme.muted; font.pixelSize: Theme.px(10); Layout.fillWidth: true; elide: Text.ElideMiddle } } FButton { text: "Select"; quiet: true; onClicked: app.selectGame(modelData.id) } FButton { text: "Launch"; primary: true; onClicked: {app.selectGame(modelData.id);app.action("launch")} } FButton { text: "Remove"; quiet: true; danger: true; onClicked: app.removeGame(modelData.id) } }
        } }
        RowLayout { visible: app.platformName==="Linux"; Layout.fillWidth: true; FButton { text: "Copy Steam launch options"; onClicked: app.action("copySteam") } Text { text: "Paste once into a Steam game's Properties → Launch Options."; color: Theme.muted; font.pixelSize: Theme.px(11); Layout.fillWidth: true; wrapMode: Text.Wrap } }
    }
}
