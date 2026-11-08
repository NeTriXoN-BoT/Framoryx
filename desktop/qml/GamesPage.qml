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
            Text { text: "Find your smooth."; color: Theme.text; font.pixelSize: Theme.font(24); font.weight: Font.DemiBold }
            Text { text: "Choose a frame rate your game can sustain. Zero means uncapped."; color: Theme.muted; font.pixelSize: Theme.font(12); Layout.fillWidth: true; wrapMode: Text.Wrap }
            RowLayout {
                Layout.fillWidth: true
                FButton { text: "−"; implicitWidth: Theme.px(38); onClicked: app.setOption("cap",app.options.cap<=10?0:app.options.cap-1) }
                TextField { id: limit; text: String(app.options.cap); color: Theme.accent; font.pixelSize: Theme.font(27); font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter; Layout.fillWidth: true; validator: IntValidator { bottom: 0; top: 1000 } selectByMouse: true; Accessible.name: "Frame limit"; onEditingFinished: if(acceptableInput)app.setOption("cap",Number(text)); background: Rectangle { color: Theme.raised; radius: Theme.px(10); border.color: limit.activeFocus?Theme.accent:Theme.border } }
                FButton { text: "+"; implicitWidth: Theme.px(38); onClicked: app.setOption("cap",app.options.cap<10?10:Math.min(1000,app.options.cap+1)) }
            }
            Flow { Layout.fillWidth: true; spacing: Theme.px(7); Repeater { model: [0,30,60,90,120,144,165,240]; FButton { required property int modelData; text: modelData===0?"Uncapped":String(modelData); primary: app.options.cap===modelData; onClicked: app.setOption("cap",modelData) } } }
            FButton { enabled: app.platformName!=="Windows"||app.state.inGameSupported!==false; text: app.platformName==="Windows"?(app.state.hookActive?"Update active limit":"Enable in-game limit"):(app.options.linuxOverlayMode===2?"Apply game-layer limit":app.options.gamescope?"Save Gamescope launch limit":"Choose a limiter method"); primary: true; onClicked: {if(app.platformName==="Windows")shell.requestHook();else if(app.options.linuxOverlayMode===2)app.notify("Saved. Connected games receive this limit; new layer launches use it too.");else if(app.options.gamescope)app.notify("Saved for the next Gamescope launch.");else {shell.page=1;app.notify("Choose the opt-in game layer in Overlay Studio, or enable Gamescope for your next launch.")}} }
            Text { text: app.state.compatibilityNote||""; visible: app.platformName==="Windows"; color: Theme.muted; font.pixelSize: Theme.font(11); Layout.fillWidth: true; wrapMode: Text.Wrap }
            FButton { visible: app.platformName==="Windows"; text: "Disable in-game hook"; enabled: app.state.hookActive; onClicked: app.action("disableHook") }
            Text { text: app.platformName==="Windows"?"Optional game hooks require explicit approval. Process blocks always take priority.":"MangoHud applies the limit to monitored Vulkan/OpenGL games. Gamescope can also control pacing at launch."; color: Theme.muted; font.pixelSize: Theme.font(11); Layout.fillWidth: true; wrapMode: Text.Wrap }
        }
        FCard {
            Layout.fillWidth: true
            Text { text: "A bigger way to play."; color: Theme.text; font.pixelSize: Theme.font(24); font.weight: Font.DemiBold }
            Text { text: app.platformName==="Linux"&&app.state.session==="Wayland"?"Wayland controls game sizing through your compositor. Enable Gamescope before launching for a controlled fullscreen session.":"Select a running game window, apply borderless fullscreen, and restore its original window mode when you are done."; color: Theme.muted; font.pixelSize: Theme.font(12); Layout.fillWidth: true; wrapMode: Text.Wrap }
            Flow { Layout.fillWidth: true; spacing: Theme.px(10); FButton { text: "Make borderless"; primary: true; onClicked: app.action("borderless") } FButton { text: "Restore window"; onClicked: app.action("restore") } }
            FToggle { text: "Auto borderless for allowed games"; checked: app.options.autoBorderless; onToggled: app.setOption("autoBorderless",checked) }
            FToggle { visible: app.platformName==="Linux"; text: "Launch games inside Gamescope"; checked: app.options.gamescope; onToggled: app.setOption("gamescope",checked) }
            Text { visible: app.platformName==="Linux"; text: "Gamescope and the in-game layer are alternative launch methods. Use the external overlay method with Gamescope."; color: Theme.muted; font.pixelSize: Theme.font(11); Layout.fillWidth: true; wrapMode: Text.Wrap }
            Text { text: "Automatic changes only affect games on your allow list. Original window state is restored when Framoryx closes."; color: Theme.muted; font.pixelSize: Theme.font(11); Layout.fillWidth: true; wrapMode: Text.Wrap }
            Text { text: "USE AT YOUR OWN RISK"; color: Theme.amber; font.pixelSize: Theme.font(11); font.weight: Font.DemiBold }
            Text { text: "Games with anti-cheat may restrict or penalize overlays and in-game hooks. You are responsible for checking and following each game's rules. Framoryx does not guarantee anti-cheat compatibility. Its maintainers accept no responsibility for bans, suspensions, lost access or other consequences arising from its use."; color: Theme.muted; font.pixelSize: Theme.font(11); Layout.fillWidth: true; wrapMode: Text.Wrap }
            FButton { text: "Edit game rules"; quiet: true; onClicked: shell.page=4 }
        }
    }
    FCard {
        Layout.fillWidth: true
        RowLayout { Layout.fillWidth: true; Text { text: "Beyond the game's cap"; color: Theme.text; font.pixelSize: Theme.font(21); font.weight: Font.DemiBold; Layout.fillWidth: true } Text { text: app.unlockSupport.supported?"SUPPORTED LAUNCH OPTION":"GAME-SPECIFIC SUPPORT"; color: app.unlockSupport.supported?Theme.accent:Theme.muted; font.pixelSize: Theme.font(10) } }
        Text { text: app.unlockSupport.supported?app.unlockSupport.name+" · "+app.unlockSupport.method:"Select a library game. Only games with a supported cap override can use this option; an internal hard cap cannot be removed by a general FPS limiter."; color: Theme.muted; font.pixelSize: Theme.font(12); Layout.fillWidth: true; wrapMode: Text.Wrap }
        FToggle { text: "Use game-supported FPS cap override"; enabled: app.unlockSupport.supported; checked: app.unlockSupport.enabled; onToggled: {if(checked)unlockDialog.open();else app.setGameUnlock(false)} }
        Text { text: "Applies on the next library launch. Higher FPS can affect physics, animations, timing, stability or power usage. V-sync, drivers and the game engine may impose other limits."; color: Theme.amber; font.pixelSize: Theme.font(11); Layout.fillWidth: true; wrapMode: Text.Wrap }
        Dialog {
            id: unlockDialog; anchors.centerIn: Overlay.overlay; width: Math.min(page.width,Theme.px(540)); height: Theme.px(305); modal: true; title: "Apply the game's FPS override?"; standardButtons: Dialog.Yes|Dialog.Cancel
            background: Rectangle { color: Theme.surface; radius: Theme.px(14); border.color: Theme.border }
            contentItem: Text { text: "Framoryx will add the game's supported launch command and set its own limiter to Uncapped. No game files are patched.\n\nHigher frame rates can break physics, animations or game timing. Test carefully and disable this option if problems occur.\n\nThis affects the next launch from your Framoryx library."; color: Theme.text; font.pixelSize: Theme.font(13); wrapMode: Text.Wrap }
            onAccepted: app.setGameUnlock(true,true)
        }
    }
    FCard {
        Layout.fillWidth: true
        RowLayout { Layout.fillWidth: true; Text { text: "Your game library"; color: Theme.text; font.pixelSize: Theme.font(22); font.weight: Font.DemiBold; Layout.fillWidth: true } FButton { text: "Add game"; primary: true; onClicked: shell.chooseGame() } }
        Text { visible: app.games.length===0; text: "Add an executable for quick launches with your Framoryx settings."; color: Theme.muted; font.pixelSize: Theme.font(13); Layout.fillWidth: true; wrapMode: Text.Wrap }
        Repeater { model: app.games; Rectangle {
            required property var modelData
            Layout.fillWidth: true; implicitHeight: row.implicitHeight+Theme.px(26); color: modelData.id===app.options.selectedGame?"#1b3736":Theme.raised; radius: Theme.px(12); border.color: modelData.id===app.options.selectedGame?Theme.accent:Theme.border
            RowLayout { id: row; x: Theme.px(14); y: Theme.px(13); width: parent.width-Theme.px(28); spacing: Theme.px(12); ColumnLayout { Layout.fillWidth: true; Text { text: modelData.name; color: Theme.text; font.pixelSize: Theme.font(16); font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight } Text { text: modelData.executable; color: Theme.muted; font.pixelSize: Theme.font(10); Layout.fillWidth: true; elide: Text.ElideMiddle } } FButton { text: "Select"; quiet: true; onClicked: app.selectGame(modelData.id) } FButton { text: "Launch"; primary: true; onClicked: {app.selectGame(modelData.id);app.action("launch")} } FButton { text: "Remove"; quiet: true; danger: true; onClicked: app.removeGame(modelData.id) } }
        } }
        RowLayout { visible: app.platformName==="Linux"; Layout.fillWidth: true; FButton { text: "Copy Steam launch options"; onClicked: app.action("copySteam") } Text { text: "Paste once into a Steam game's Properties → Launch Options."; color: Theme.muted; font.pixelSize: Theme.font(11); Layout.fillWidth: true; wrapMode: Text.Wrap } }
    }
}
