import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Window
import "."
ApplicationWindow {
    id: window
    objectName: "FramoryxWindow"
    title: "Framoryx — Your game, your way"
    width: 1360; height: 900
    minimumWidth: 640; minimumHeight: 460
    visible: false
    color: Theme.background
    property int page: 0
    property real autoUiScale: 1
    property bool diagnosticAnimating: false
    property string chosenGamePath: ""
    readonly property var titles: ["Every frame. Under control.","Make it unmistakably yours.","Sound that pulls you in.","Find your smooth.","Your games. Your rules.","Ready when you are.","The next frame starts here."]
    readonly property var subtitles: ["Measure performance. Shape your play.","Every readout. Every detail. Your choice.","Your audio. A little more presence.","Frame pacing, borderless play and your game library.","Clear preferences for the games you trust.","A comfortable fit, wherever you play.","Every release. Every improvement. All in one place."]
    readonly property var colors: ["#7df2c6","#59cfff","#c2a3ff","#f7be6f","#ff8baf","#63dfdf","#f4e478","#a3e56d","#ff9d72","#d5e4f5"]
    Binding { target: Theme; property: "accent"; value: window.colors[app.options.accent] || window.colors[0] }
    Binding { target: Theme; property: "scale"; value: app.options.uiScale>0?app.options.uiScale/100:Math.max(.6,Math.min(window.autoUiScale,window.width/1100,window.height/760)) }
    Binding { target: Theme; property: "motion"; value: app.options.motion&&(window.active||window.diagnosticAnimating)&&window.visibility!==Window.Minimized&&window.visible }
    function requestInGameMode() {
        if (app.state.inGameSupported === false) {
            app.notify("This rendering API uses the external overlay. In-game features support DX11 and DX12.", "warning")
            return
        }
        if (app.options.hideHookWarning) {
            app.setOption("mode", 2)
            return
        }
        hookDialog.choosingMode = true
        hookDialog.open()
    }
    function requestHook() {
        if (!app.state.profilePid || !app.state.profileExe ||
                (app.options.selectedGame === "auto" && app.state.autoHudEligible === false)) {
            app.notify("Select a running game window before enabling the in-game hook.", "warning")
            return
        }
        if (app.state.inGameSupported === false) {
            app.notify("This renderer uses the external overlay. Optional Windows in-game limiting supports DX11/DX12.")
            return
        }
        if (app.state.antiCheatStatus === "detected") {
            app.notify("Known anti-cheat indicators are present. In-game hooks are blocked; use the external overlay.", "warning")
            return
        }
        hookDialog.choosingMode = false
        hookDialog.targetPid = app.state.profilePid
        hookDialog.targetExe = app.state.profileExe
        hookDialog.open()
    }
    function chooseGame(){gameFile.open()}
    function chooseApo(){apoFile.open()}
    function exportCapture(){captureFile.open()}
    function exportPresets(){savePresetFile.open()}
    function importPresets(){loadPresetFile.open()}
    function choosePage(index){page=index}
    onActiveChanged: app.setWindowActive(active)
    onClosing: function(close){if(!app.testMode&&app.options.closeToTray&&app.state.trayAvailable){close.accepted=false;app.action("hideToTray")}else app.action("quit")}
    onVisibilityChanged: function(visibility){if(visibility===Window.Minimized&&!app.testMode&&app.options.minimizeToTray&&app.state.trayAvailable)app.action("hideToTray")}
    property int warmedPages: 0
    Timer { interval: 220; repeat: true; running: window.warmedPages<7; onTriggered: window.warmedPages++ }
    RowLayout {
        anchors.fill: parent; spacing: 0
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: Math.min(Theme.px(205),window.width*.22)
            color: "#0c151e"
            Rectangle { anchors.right: parent.right; height: parent.height; width: 1; color: Theme.border }
            ScrollView {
                id: navigationScroll
                anchors.fill: parent; padding: Theme.px(18); clip: true
                contentWidth: availableWidth
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ColumnLayout {
                width: navigationScroll.availableWidth
                height: Math.max(implicitHeight,navigationScroll.availableHeight)
                spacing: Theme.px(7)
                Item {
                    Layout.fillWidth: true; Layout.preferredHeight: Theme.px(142)
                    Image {
                        id: mark
                        source: "qrc:/art/icon.png"
                        width: Theme.px(58); height: width
                        x: Theme.px(4); y: Theme.px(5)
                        mipmap: true; smooth: true
                        SequentialAnimation on scale { loops: Animation.Infinite; running: Theme.motion; NumberAnimation { from: 1; to: 1.045; duration: 3000; easing.type: Easing.InOutSine } NumberAnimation { from: 1.045; to: 1; duration: 3000; easing.type: Easing.InOutSine } }
                    }
                    Text { text: "FRAMORYX"; x: Theme.px(4); y: Theme.px(78); color: Theme.text; font.pixelSize: Theme.font(20); font.weight: Font.Bold; font.letterSpacing: .8 }
                    Text { text: "PLAY AT YOUR BEST"; x: Theme.px(5); y: Theme.px(109); color: Theme.muted; font.pixelSize: Theme.font(8); font.letterSpacing: 1.2 }
                }
                Repeater {
                    model: ["Overview","Overlay Studio","Audio","Games & pacing","Process control","Settings & setup","What’s new"]
                    FButton {
                        required property int index
                        required property string modelData
                        text: modelData
                        hint: modelData
                        objectName: "navigation-"+index
                        Layout.fillWidth: true
                        implicitHeight: Theme.px(46)
                        quiet: window.page!==index
                        onClicked: {window.page=index;if(index===2)app.action("audioOpen");if(index===4)app.refreshProcesses()}
                        background: Rectangle { color: window.page===index?Qt.rgba(Theme.accent.r,Theme.accent.g,Theme.accent.b,.14):parent.hovered?"#172934":"transparent"; radius: Theme.px(10); border.width: parent.visualFocus?2:0; border.color: Theme.accent; Rectangle { visible: window.page===index; x: 0; y: 10; width: 3; height: parent.height-20; radius: 2; color: Theme.accent } Behavior on color { ColorAnimation { duration: Theme.duration } } }
                    }
                }
                Item { Layout.fillHeight: true }
                FToggle { text: "Motion effects"; checked: app.options.motion; Layout.fillWidth: true; onToggled: app.setOption("motion",checked) }
                Text { text: "FRAMORYX "+Qt.application.version+" · "+app.platformName.toUpperCase(); color: Theme.muted; font.pixelSize: Theme.font(8); Layout.fillWidth: true; wrapMode: Text.Wrap; Layout.topMargin: Theme.px(12) }
                Text { text: app.platformName==="Windows"?"UNSIGNED BUILD":"NATIVE LINUX"; color: app.platformName==="Windows"?Theme.amber:Theme.accent; font.pixelSize: Theme.font(8); font.letterSpacing: 1 }
                }
            }
        }
        ScrollView {
            id: scroll
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true
            contentWidth: Math.max(availableWidth,Theme.px(610))
            ScrollBar.horizontal.policy: contentWidth>availableWidth?ScrollBar.AsNeeded:ScrollBar.AlwaysOff
            ColumnLayout {
                id: column
                width: scroll.contentWidth-Theme.px(48)
                x: Theme.px(24); y: Theme.px(24)
                spacing: Theme.px(20)
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: Theme.px(152); radius: Theme.px(17); color: "#14232c"; border.color: Theme.border
                    Backdrop { anchors.fill: parent; anchors.margins: 1 }
                    ColumnLayout { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; anchors.leftMargin: Theme.px(28); width: parent.width-Theme.px(50); spacing: Theme.px(10)
                        Text { text: "FRAMORYX // PERFORMANCE SYSTEM"; color: Theme.accent; font.family: "monospace"; font.pixelSize: Theme.font(9); font.letterSpacing: 1.4 }
                        Item { Layout.fillWidth: true; implicitHeight: headline.implicitHeight
                            Text { id: headline; width: parent.width-Theme.px(20); text: window.titles[window.page].toUpperCase(); color: Theme.text; font.family: app.platformName==="Windows"?"Consolas":"monospace"; font.pixelSize: Theme.font(25); font.weight: Font.DemiBold; font.letterSpacing: -.5; wrapMode: Text.Wrap }
                            Rectangle { x: Math.min(headline.contentWidth,headline.width)+Theme.px(7); y: headline.height-Theme.px(25); width: Theme.px(11); height: Theme.px(24); color: Theme.accent; opacity: 1
                                SequentialAnimation on opacity { running: Theme.motion&&visible; loops: Animation.Infinite; PauseAnimation { duration: 560 } NumberAnimation { to: 0; duration: 80 } PauseAnimation { duration: 360 } NumberAnimation { to: 1; duration: 80 } }
                            }
                        }
                        Text { text: window.subtitles[window.page]; color: Theme.muted; font.pixelSize: Theme.font(13); Layout.fillWidth: true; wrapMode: Text.Wrap }
                    }
                }
                FCard {
                    Layout.fillWidth: true
                    padding: Theme.px(16)
                    RowLayout {
                        Layout.fillWidth: true; spacing: Theme.px(12)
                        Rectangle { width: Theme.px(8); height: width; radius: width/2; color: app.state.live?Theme.accent:app.state.connected?Theme.amber:Theme.red }
                        FCombo {
                            id: gameSelect
                            Layout.fillWidth: true
                            property var entries: {let list=[{id:"auto",name:app.platformName==="Windows"?"Auto · follow the foreground game":"Choose a game or running window"}];for(let g of app.games)list.push({id:g.id,name:g.name});for(let w of (app.state.windows||[]))list.push({id:w.id,name:w.name});return list}
                            model: entries; textRole: "name"
                            currentIndex: {for(let i=0;i<entries.length;i++)if(entries[i].id===app.options.selectedGame)return i;return 0}
                            onActivated: app.selectGame(entries[currentIndex].id)
                        }
                        FButton { text: "Refresh"; visible: column.width>Theme.px(780); onClicked: app.action("refresh") }
                        FButton { text: app.state.capture?"Stop capture":"Start capture"; primary: true; onClicked: app.action("capture") }
                    }
                }
                FCard {
                    Layout.fillWidth: true; padding: Theme.px(13)
                    RowLayout { Layout.fillWidth: true
                        Rectangle { width: Theme.px(7); height: width; radius: width/2; color: app.state.antiCheatStatus==="detected"?Theme.amber:Theme.muted }
                        Text { text: app.state.antiCheatStatus==="detected"?"Anti-cheat indicators detected":app.state.antiCheatStatus==="no-known-indicator"?"No known anti-cheat indicators · approval unverified":"Anti-cheat status unknown · external overlay preferred"; color: Theme.muted; font.pixelSize: Theme.font(11); Layout.fillWidth: true; wrapMode: Text.Wrap }
                        FButton { text: "Details"; quiet: true; implicitHeight: Theme.px(30); onClicked: window.page=5 }
                    }
                }
                Item {
                    id: pageHost
                    Layout.fillWidth: true
                    implicitHeight: {const loader=pages.itemAt(window.page);return loader&&loader.item?loader.item.implicitHeight:Theme.px(300)}
                    Repeater {
                        id: pages
                        model: 7
                        Loader {
                            required property int index
                            width: pageHost.width
                            asynchronous: true
                            active: index===window.page||index<window.warmedPages
                            visible: index===window.page
                            sourceComponent: [overview,studio,audio,games,rules,settings,changes][index]
                        }
                    }
                    Text { anchors.centerIn: parent; visible: {const loader=pages.itemAt(window.page);return !loader||loader.status!==Loader.Ready} text: "Preparing your workspace…"; color: Theme.muted; font.pixelSize: Theme.font(13) }
                    Connections { target: window; function onPageChanged(){scroll.contentItem.contentY=0;pageEnter.restart()} }
                    NumberAnimation { id: pageEnter; target: pageHost; property: "opacity"; from: Theme.motion?.7:1; to: 1; duration: Theme.duration; easing.type: Easing.OutCubic }
                }
                Text { textFormat: Text.PlainText; text: app.state.notice||"Ready when you are."; color: Theme.muted; font.pixelSize: Theme.font(12); Layout.fillWidth: true; wrapMode: Text.Wrap; Layout.bottomMargin: Theme.px(32) }
            }
            contentHeight: column.implicitHeight+Theme.px(48)
        }
    }
    ExternalOverlay { }
    Component { id: overview; Overview { property var shell: window } }
    Component { id: studio; OverlayStudio { property var shell: window } }
    Component { id: audio; AudioPage { property var shell: window } }
    Component { id: games; GamesPage { shell: window } }
    Component { id: rules; RulesPage { shell: window } }
    Component { id: settings; SettingsPage { shell: window } }
    Component { id: changes; ChangesPage { shell: window } }
    FileDialog { id: gameFile; title: "Choose a game executable"; onAccepted: {window.chosenGamePath=selectedFile;addGameDialog.open()} }
    FileDialog { id: apoFile; title: "Choose Equalizer APO config.txt"; nameFilters: ["APO configuration (config.txt)"]; onAccepted: app.action("apoConfig",{url:String(selectedFile)}) }
    FileDialog { id: captureFile; title: "Export rolling capture"; fileMode: FileDialog.SaveFile; defaultSuffix: "csv"; nameFilters: ["CSV capture (*.csv)"]; onAccepted: app.action("export",{url:String(selectedFile)}) }
    FileDialog { id: savePresetFile; title: "Export Framoryx presets"; fileMode: FileDialog.SaveFile; defaultSuffix: "json"; nameFilters: ["Framoryx presets (*.json)"]; onAccepted: app.exportProfiles(selectedFile) }
    FileDialog { id: loadPresetFile; title: "Import Framoryx presets"; nameFilters: ["Framoryx presets (*.json)"]; onAccepted: app.importProfiles(selectedFile) }
    FDialog {
        id: addGameDialog
        objectName: "addGameDialog"
        title: "Add to your library"
        confirmText: "Add game"
        message: "Launch arguments are passed directly to the game executable."
        initialFocus: gameName
        formContent: [
            TextField {
                id: gameName
                Layout.fillWidth: true
                placeholderText: "Game name (optional)"
                color: Theme.text
                placeholderTextColor: Theme.muted
                selectByMouse: true
                padding: Theme.px(12)
                font.pixelSize: Theme.font(13)
                background: Rectangle {
                    color: Theme.raised
                    radius: Theme.px(9)
                    border.color: gameName.activeFocus ? Theme.accent : Theme.border
                }
            },
            TextField {
                id: gameArgs
                Layout.fillWidth: true
                placeholderText: "Launch arguments (optional)"
                color: Theme.text
                placeholderTextColor: Theme.muted
                selectByMouse: true
                padding: Theme.px(12)
                font.pixelSize: Theme.font(13)
                background: Rectangle {
                    color: Theme.raised
                    radius: Theme.px(9)
                    border.color: gameArgs.activeFocus ? Theme.accent : Theme.border
                }
            }
        ]
        onAccepted: {
            app.addGame(window.chosenGamePath, gameName.text, gameArgs.text)
            gameName.clear()
            gameArgs.clear()
        }
    }
    FDialog {
        id: hookDialog
        objectName: "hookWarningDialog"
        property bool choosingMode: false
        property double targetPid: 0
        property string targetExe: ""
        warning: !app.options.hideHookWarning
        offerRemember: warning
        title: choosingMode ? "Use the in-game overlay?" : "Enable experimental in-game features?"
        confirmText: choosingMode ? "Use in-game mode" : "Enable hook"
        message: (choosingMode
                  ? "In-game mode uses a DX11/DX12 hook for the overlay and frame limiter. You’ll approve the hook for a specific game separately."
                  : "This loads the Framoryx DX11/DX12 module into " + targetExe + " (PID " + targetPid + ") for an in-game overlay and frame limiting.")
                 + (warning ? "\n\nGame hooks can cause crashes or anti-cheat bans. An allow-list entry or clear scan does not establish approval. Use hooks only where the game permits them."
                   + "\n\nKnown anti-cheat and system blocks stay enforced. Disabling the hook restores its entry points. Restart the game to remove the module fully."
                   + "\n\nYou are responsible for checking the game’s rules. Framoryx’s maintainers accept no responsibility for bans, suspensions or other consequences of using the hook." : "")
        onAccepted: {
            if (offerRemember && rememberWarning)
                app.setOption("hideHookWarning", true)
            if (choosingMode)
                app.setOption("mode", 2)
            else
                app.action("enableHook", {confirmed: true, pid: targetPid, exe: targetExe})
        }
    }
    Popup {
        id: toast
        property string message: ""
        property string severity: "info"
        x: window.width-width-Theme.px(24); y: window.height-height-Theme.px(24)
        width: Math.min(window.width-Theme.px(50),Theme.px(510))
        padding: Theme.px(18)
        readonly property color tone: severity === "error" ? Theme.red : severity === "warning" ? Theme.amber : Theme.accent
        background: Rectangle { color: Theme.surface; radius: Theme.px(12); border.color: toast.tone }
        contentItem: RowLayout {
            spacing: Theme.px(10)
            Rectangle { Layout.alignment: Qt.AlignTop; Layout.topMargin: Theme.px(5); implicitWidth: Theme.px(8); implicitHeight: implicitWidth; radius: width / 2; color: toast.tone }
            Text { Layout.fillWidth: true; textFormat: Text.PlainText; text: toast.message; color: Theme.text; font.pixelSize: Theme.font(13); wrapMode: Text.Wrap }
        }
        Timer { id: toastTimer; interval: 5500; onTriggered: toast.close() }
    }
    Connections { target: app; function onToast(text,severity){toast.message=text;toast.severity=severity;toast.open();toastTimer.restart()} }
}
