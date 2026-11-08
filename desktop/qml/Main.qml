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
    minimumWidth: 820; minimumHeight: 600
    visible: !app.testMode
    color: Theme.background
    property int page: 0
    property bool diagnosticAnimating: false
    property string chosenGamePath: ""
    readonly property var titles: ["Every frame. Under control.","Make it unmistakably yours.","Sound that pulls you in.","Find your smooth.","Your games. Your rules.","Ready when you are."]
    readonly property var subtitles: ["Measure performance. Shape your play.","Every readout. Every detail. Your choice.","Your audio. A little more presence.","Frame pacing, borderless play and your game library.","Clear preferences for the games you trust.","A comfortable fit, wherever you play."]
    readonly property var colors: ["#7df2c6","#59cfff","#c2a3ff","#f7be6f"]
    Binding { target: Theme; property: "accent"; value: window.colors[app.options.accent||0] }
    Binding { target: Theme; property: "scale"; value: app.options.uiScale>0?app.options.uiScale/100:Screen.width<1200?.9:Screen.width>2400?1.1:1 }
    Binding { target: Theme; property: "motion"; value: app.options.motion&&(window.active||window.diagnosticAnimating)&&window.visibility!==Window.Minimized&&window.visible }
    function requestHook(){hookDialog.open()}
    function chooseGame(){gameFile.open()}
    function chooseApo(){apoFile.open()}
    function exportCapture(){captureFile.open()}
    function exportPresets(){savePresetFile.open()}
    function importPresets(){loadPresetFile.open()}
    function choosePage(index){page=index}
    onActiveChanged: app.setWindowActive(active)
    onClosing: {app.action("quit")}
    RowLayout {
        anchors.fill: parent; spacing: 0
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: Math.min(Theme.px(205),window.width*.22)
            color: "#0c151e"
            Rectangle { anchors.right: parent.right; height: parent.height; width: 1; color: Theme.border }
            ColumnLayout {
                anchors.fill: parent; anchors.margins: Theme.px(18); spacing: Theme.px(7)
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
                    Text { text: "FRAMORYX"; x: Theme.px(4); y: Theme.px(78); color: Theme.text; font.pixelSize: Theme.px(20); font.weight: Font.Bold; font.letterSpacing: .8 }
                    Text { text: "PLAY AT YOUR BEST"; x: Theme.px(5); y: Theme.px(109); color: Theme.muted; font.pixelSize: Theme.px(8); font.letterSpacing: 1.2 }
                }
                Repeater {
                    model: ["Overview","Overlay Studio","Audio","Games & pacing","Game rules","Settings & setup"]
                    FButton {
                        required property int index
                        required property string modelData
                        text: modelData
                        Layout.fillWidth: true
                        implicitHeight: Theme.px(46)
                        quiet: window.page!==index
                        onClicked: {window.page=index;if(index===2)app.action("audioOpen")}
                        background: Rectangle { color: window.page===index?"#1b3b35":parent.hovered?"#172934":"transparent"; radius: Theme.px(10); border.width: parent.visualFocus?2:0; border.color: Theme.accent; Rectangle { visible: window.page===index; x: 0; y: 10; width: 3; height: parent.height-20; radius: 2; color: Theme.accent } Behavior on color { ColorAnimation { duration: Theme.duration } } }
                    }
                }
                Item { Layout.fillHeight: true }
                FToggle { text: "Motion effects"; checked: app.options.motion; Layout.fillWidth: true; onToggled: app.setOption("motion",checked) }
                Text { text: "FRAMORYX 0.3.0 · "+app.platformName.toUpperCase(); color: Theme.muted; font.pixelSize: Theme.px(8); Layout.fillWidth: true; wrapMode: Text.Wrap; Layout.topMargin: Theme.px(12) }
                Text { text: app.platformName==="Windows"?"UNSIGNED BUILD":"NATIVE LINUX"; color: app.platformName==="Windows"?Theme.amber:Theme.accent; font.pixelSize: Theme.px(8); font.letterSpacing: 1 }
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
                    ColumnLayout { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; anchors.leftMargin: Theme.px(28); width: parent.width-Theme.px(50); spacing: Theme.px(10); Text { text: window.titles[window.page]; color: Theme.text; font.pixelSize: Theme.px(30); font.weight: Font.DemiBold; Layout.fillWidth: true; wrapMode: Text.Wrap } Text { text: window.subtitles[window.page]; color: Theme.muted; font.pixelSize: Theme.px(13); Layout.fillWidth: true; wrapMode: Text.Wrap } }
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
                        FButton { text: app.state.capture?"Stop capture":app.platformName==="Linux"?"Launch & capture":"Start capture"; primary: true; onClicked: app.action("capture") }
                    }
                }
                Loader {
                    id: content
                    Layout.fillWidth: true
                    Layout.preferredHeight: item?item.implicitHeight:0
                    sourceComponent: [overview,studio,audio,games,rules,settings][window.page]
                    onLoaded: {enter.restart();scroll.contentItem.contentY=0}
                    NumberAnimation { id: enter; target: content; property: "opacity"; from: Theme.motion?.25:1; to: 1; duration: Theme.duration; easing.type: Easing.OutCubic }
                }
                Text { text: app.state.notice||"Ready when you are."; color: Theme.muted; font.pixelSize: Theme.px(12); Layout.fillWidth: true; wrapMode: Text.Wrap; Layout.bottomMargin: Theme.px(32) }
            }
            contentHeight: column.implicitHeight+Theme.px(48)
        }
    }
    Component { id: overview; Overview { property var shell: window } }
    Component { id: studio; OverlayStudio { property var shell: window } }
    Component { id: audio; AudioPage { property var shell: window } }
    Component { id: games; GamesPage { shell: window } }
    Component { id: rules; RulesPage { shell: window } }
    Component { id: settings; SettingsPage { shell: window } }
    FileDialog { id: gameFile; title: "Choose a game executable"; onAccepted: {window.chosenGamePath=selectedFile;addGameDialog.open()} }
    FileDialog { id: apoFile; title: "Choose Equalizer APO config.txt"; nameFilters: ["APO configuration (config.txt)"]; onAccepted: app.action("apoConfig",{url:String(selectedFile)}) }
    FileDialog { id: captureFile; title: "Export rolling capture"; fileMode: FileDialog.SaveFile; defaultSuffix: "csv"; nameFilters: ["CSV capture (*.csv)"]; onAccepted: app.action("export",{url:String(selectedFile)}) }
    FileDialog { id: savePresetFile; title: "Export Framoryx presets"; fileMode: FileDialog.SaveFile; defaultSuffix: "json"; nameFilters: ["Framoryx presets (*.json)"]; onAccepted: app.exportProfiles(selectedFile) }
    FileDialog { id: loadPresetFile; title: "Import Framoryx presets"; nameFilters: ["Framoryx presets (*.json)"]; onAccepted: app.importProfiles(selectedFile) }
    Dialog {
        id: addGameDialog
        anchors.centerIn: parent
        width: Math.min(window.width-50,Theme.px(540))
        height: Math.min(window.height-60,Theme.px(295))
        modal: true
        title: "Add to your library"
        standardButtons: Dialog.Save|Dialog.Cancel
        background: Rectangle { color: Theme.surface; radius: Theme.px(15); border.color: Theme.border }
        contentItem: ColumnLayout { spacing: Theme.px(14); TextField { id: gameName; Layout.fillWidth: true; placeholderText: "Game name (optional)"; color: Theme.text; selectByMouse: true } TextField { id: gameArgs; Layout.fillWidth: true; placeholderText: "Launch arguments (optional)"; color: Theme.text; selectByMouse: true } Text { text: "Launch arguments are passed directly to the game. Framoryx does not run them through a shell."; color: Theme.muted; font.pixelSize: Theme.px(12); wrapMode: Text.Wrap; Layout.fillWidth: true } }
        onAccepted: {app.addGame(window.chosenGamePath,gameName.text,gameArgs.text);gameName.clear();gameArgs.clear()}
    }
    Dialog {
        id: hookDialog
        anchors.centerIn: parent
        width: Math.min(window.width-50,Theme.px(580))
        height: Math.min(window.height-60,Theme.px(450))
        modal: true
        title: "Enable experimental in-game features?"
        standardButtons: Dialog.Yes|Dialog.Cancel
        background: Rectangle { color: Theme.surface; radius: Theme.px(15); border.color: Theme.border }
        contentItem: ScrollView { clip: true; Text { width: parent.width; text: "This loads the Framoryx DX11/DX12 module into the selected game for an in-game overlay and frame limiting.\n\nGame hooks can cause crashes or anti-cheat bans. An allow list or clear scan does not establish game approval. Use only where permitted.\n\nKnown anti-cheat/system blocks remain enforced. Disable restores hook entry points; restart the game to remove the module fully.\n\nUse at your own risk. You are responsible for checking the game’s rules. Framoryx’s maintainers accept no responsibility for bans, suspensions or other consequences of its use.\n\nEnable for this selected game now?"; color: Theme.text; font.pixelSize: Theme.px(13); wrapMode: Text.Wrap } }
        onAccepted: app.action("enableHook",{confirmed:true})
    }
    Popup {
        id: toast
        property string message: ""
        property string severity: "info"
        x: window.width-width-Theme.px(24); y: window.height-height-Theme.px(24)
        width: Math.min(window.width-Theme.px(50),Theme.px(510))
        padding: Theme.px(18)
        background: Rectangle { color: "#20333e"; radius: Theme.px(12); border.color: toast.severity==="error"?Theme.red:toast.severity==="warning"?Theme.amber:Theme.accent }
        contentItem: Text { text: toast.message; color: Theme.text; font.pixelSize: Theme.px(13); wrapMode: Text.Wrap }
        Timer { id: toastTimer; interval: 5500; onTriggered: toast.close() }
    }
    Connections { target: app; function onToast(text,severity){toast.message=text;toast.severity=severity;toast.open();toastTimer.restart()} function onShowRequested(){window.show();window.raise();window.requestActivate()} }
}
