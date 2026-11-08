import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id: page
    property bool microphone: false
    spacing: Theme.px(18)
    RowLayout { FButton { text: "Output"; primary: !page.microphone; onClicked: page.microphone=false } FButton { text: "Microphone"; primary: page.microphone; onClicked: {page.microphone=true;app.action("audioOpen")} } Item { Layout.fillWidth: true } FButton { text: "Refresh devices"; onClicked: app.action("refresh") } }
    FCard {
        Layout.fillWidth: true
        visible: !page.microphone
        Text { text: "Feel every detail."; color: Theme.text; font.pixelSize: Theme.font(26); font.weight: Font.DemiBold }
        Text { textFormat: Text.PlainText; text: app.state.outputName||"Default playback device"; color: Theme.muted; font.pixelSize: Theme.font(13); Layout.fillWidth: true; wrapMode: Text.Wrap }
        RowLayout {
            Layout.fillWidth: true
            FSlider { objectName: "output-volume-slider"; Layout.fillWidth: true; from: 0; to: 200; stepSize: 1; value: app.volume; Accessible.name: "Output volume, zero to two hundred percent"; onMoved: app.setOption("volume",Math.round(value)) }
            Text { text: app.volume+"%"; color: app.volume>100?Theme.amber:Theme.accent; font.pixelSize: Theme.font(32); font.weight: Font.DemiBold; Layout.preferredWidth: Theme.px(104); horizontalAlignment: Text.AlignRight }
        }
        RowLayout { Layout.fillWidth: true; Text { text: "0%"; color: Theme.muted; font.pixelSize: Theme.font(10) } Item { Layout.fillWidth: true } Text { text: "100%"; color: Theme.muted; font.pixelSize: Theme.font(10) } Item { Layout.fillWidth: true } Text { text: "200%"; color: Theme.amber; font.pixelSize: Theme.font(10) } }
        Text { text: "Extra gain can clip loud audio. Start gently and reduce the level if it distorts."; color: Theme.muted; font.pixelSize: Theme.font(12); Layout.fillWidth: true; wrapMode: Text.Wrap }
    }
    GridLayout {
        Layout.fillWidth: true; visible: !page.microphone; columns: page.width>Theme.px(800)?2:1; columnSpacing: Theme.px(18); rowSpacing: Theme.px(18)
        FCard {
            Layout.fillWidth: true
            Text { text: "Bass, with balance."; color: Theme.text; font.pixelSize: Theme.font(20); font.weight: Font.DemiBold }
            Text { text: "A clean low shelf with automatic headroom compensation."; color: Theme.muted; font.pixelSize: Theme.font(12); Layout.fillWidth: true; wrapMode: Text.Wrap }
            RowLayout { Layout.fillWidth: true; FCombo { Layout.fillWidth: true; model: ["Flat · 0 dB","Subtle · +3 dB","Full · +6 dB","Deep · +9 dB"]; currentIndex: Math.round(app.options.bass/3); onActivated: app.setOption("bass",currentIndex*3) } Text { text: app.state.bassConfigured?"Configured":"Off"; color: app.state.bassConfigured?Theme.accent:Theme.muted; font.pixelSize: Theme.font(12) } }
            RowLayout { FButton { text: "Apply bass"; primary: true; onClicked: app.action("bassApply") } FButton { text: "Bypass"; onClicked: app.action("bassDisable") } }
            FButton { text: "Use Framoryx output"; visible: app.platformName==="Linux"; hint: "Make the processed output your default playback device"; onClicked: app.action("defaultOutput") }
        }
        FCard {
            Layout.fillWidth: true
            Text { text: "Ready to hear it?"; color: Theme.text; font.pixelSize: Theme.font(20); font.weight: Font.DemiBold }
            Text { text: app.platformName==="Windows"?"Windows volume works immediately. Bass, microphone processing and extra gain need Equalizer APO enabled for your devices.":"Volume works through PipeWire. Bass creates a Framoryx Output device; choose it to route playback through the effect."; color: Theme.muted; font.pixelSize: Theme.font(13); Layout.fillWidth: true; wrapMode: Text.Wrap }
            Flow { Layout.fillWidth: true; spacing: Theme.px(8); FButton { text: "Audio setup"; onClicked: shell.page=5 } FButton { text: app.platformName==="Windows"?"System sound settings":"Audio guide"; onClicked: app.action("soundSettings") } }
        }
    }
    FCard {
        Layout.fillWidth: true; visible: page.microphone
        Text { text: "Your voice, clearer."; color: Theme.text; font.pixelSize: Theme.font(26); font.weight: Font.DemiBold }
        FCombo {
            Layout.fillWidth: true
            model: app.devices; textRole: "name"
            currentIndex: {for(let i=0;i<app.devices.length;i++)if(app.devices[i].id===app.options.micId)return i;return 0}
            onActivated: if(currentIndex>=0&&currentIndex<app.devices.length)app.setOption("micId",app.devices[currentIndex].id)
        }
        Text { text: app.state.micConfigured?"Effects configured · verify your voice with a test below":"Choose your effects, then Apply. Microphone access begins only when you start a test."; color: app.state.micConfigured?Theme.accent:Theme.muted; font.pixelSize: Theme.font(12); Layout.fillWidth: true; wrapMode: Text.Wrap }
    }
    GridLayout {
        Layout.fillWidth: true; visible: page.microphone; columns: page.width>Theme.px(900)?2:1; columnSpacing: Theme.px(18); rowSpacing: Theme.px(18)
        FCard {
            Layout.fillWidth: true
            FToggle { text: "RNNoise suppression"; checked: app.options.noise; onToggled: app.setOption("noise",checked) }
            Text { text: "Reduce background noise while keeping your voice present. RNNoise uses a 48 kHz processing path."; color: Theme.muted; font.pixelSize: Theme.font(12); Layout.fillWidth: true; wrapMode: Text.Wrap }
            FCombo { Layout.fillWidth: true; model: ["Natural · 50% voice detection","Balanced · 85% voice detection","Focused · 95% voice detection"]; currentIndex: app.options.noisePreset; onActivated: app.setOption("noisePreset",currentIndex) }
            Text { text: app.state.noiseStatus||"Automatic updates are ready."; color: app.state.noiseBusy?Theme.amber:Theme.muted; font.pixelSize: Theme.font(11); Layout.fillWidth: true; wrapMode: Text.Wrap }
            FToggle { text: "Automatic verified plugin updates"; checked: app.options.autoNoise; onToggled: app.setOption("autoNoise",checked) }
            FButton { text: app.state.noiseBusy?"Preparing RNNoise…":"Check for updates"; enabled: !app.state.noiseBusy; onClicked: app.action("noiseUpdate") }
        }
        FCard {
            Layout.fillWidth: true
            FToggle { text: "Microphone equalizer"; checked: app.options.eq; onToggled: app.setOption("eq",checked) }
            FCombo { Layout.fillWidth: true; model: ["Flat","Clarity","Warmth","Presence","Bassy voice","Custom"]; currentIndex: app.options.voicePreset; onActivated: app.setOption("voicePreset",currentIndex) }
            Text { text: app.options.voicePreset===4?"Bassy voice adds warmer lows and reduces boxiness. Automatic headroom keeps the boost controlled.":"Shape your voice in three bands. Positive gains include automatic headroom reduction."; color: Theme.muted; font.pixelSize: Theme.font(12); Layout.fillWidth: true; wrapMode: Text.Wrap }
            Repeater {
                model: [{key:"low",label:"Low · 150 Hz"},{key:"mid",label:"Mid · 1.2 kHz"},{key:"high",label:"High · 6 kHz"}]
                ColumnLayout { required property var modelData; Layout.fillWidth: true; spacing: Theme.px(3); RowLayout { Layout.fillWidth: true; Text { text: modelData.label; color: Theme.muted; font.pixelSize: Theme.font(11); Layout.fillWidth: true } Text { text: (app.options[modelData.key]>0?"+":"")+app.options[modelData.key]+" dB"; color: Theme.text; font.pixelSize: Theme.font(12) } } FSlider { Layout.fillWidth: true; from: -12; to: 12; stepSize: 3; value: app.options[modelData.key]; Accessible.name: modelData.label; onMoved: app.setOption(modelData.key,Math.round(value)) } }
            }
        }
    }
    FCard {
        Layout.fillWidth: true; visible: page.microphone
        Flow { Layout.fillWidth: true; spacing: Theme.px(10); FButton { text: app.state.pendingMic?"Preparing mic effects…":"Apply microphone effects"; primary: true; onClicked: app.action("micApply") } FButton { text: "Bypass effects"; onClicked: app.action("micDisable") } FButton { visible: app.platformName==="Linux"; text: "Use Framoryx microphone"; onClicked: app.action("defaultMic") } }
        Text { text: app.state.audioStatus||""; color: Theme.muted; font.pixelSize: Theme.font(12); Layout.fillWidth: true; wrapMode: Text.Wrap }
    }
    FCard {
        Layout.fillWidth: true; visible: page.microphone
        Text { text: "Hear the difference."; color: Theme.text; font.pixelSize: Theme.font(20); font.weight: Font.DemiBold }
        Text { text: "Record five seconds, then play it back. The sample stays in memory and is never uploaded. On Linux, select Framoryx Microphone above to test the processed signal."; color: Theme.muted; font.pixelSize: Theme.font(12); Layout.fillWidth: true; wrapMode: Text.Wrap }
        RowLayout {
            Layout.fillWidth: true
            Rectangle { Layout.fillWidth: true; height: Theme.px(12); radius: 6; color: Theme.border; Rectangle { height: parent.height; radius: 6; width: parent.width*Math.min(1,app.state.inputLevel||0); color: app.state.inputLevel>.9?Theme.amber:Theme.accent; Behavior on width { NumberAnimation { duration: 45 } } } }
            Text { text: app.state.recording?"Recording…":app.state.playing?"Playing…":app.state.sampleReady?"Sample ready":"Mic idle"; color: Theme.muted; font.pixelSize: Theme.font(12); Layout.preferredWidth: Theme.px(110) }
        }
        Flow { Layout.fillWidth: true; spacing: Theme.px(10); FButton { text: "Record 5-second test"; primary: true; enabled: !app.state.recording&&!app.state.playing&&app.devices.length>0; onClicked: app.action("record") } FButton { text: "Play sample"; enabled: app.state.sampleReady&&!app.state.recording&&!app.state.playing; onClicked: app.action("playSample") } FButton { text: "Stop"; enabled: app.state.recording||app.state.playing; onClicked: app.action("stopSample") } }
    }
}
