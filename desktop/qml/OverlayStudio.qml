import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id: studio
    spacing: Theme.px(18)
    readonly property var positionPresets: [
        {name: "Top left", x: 0, y: 0},
        {name: "Top center", x: .5, y: 0},
        {name: "Top right", x: 1, y: 0},
        {name: "Bottom left", x: 0, y: 1},
        {name: "Bottom center", x: .5, y: 1},
        {name: "Bottom right", x: 1, y: 1}
    ]
    readonly property int activePosition: positionPresets.findIndex(preset =>
        Math.abs(app.options.hudX - preset.x) < .003 &&
        Math.abs(app.options.hudY - preset.y) < .003)
    property bool editingPosition: false
    readonly property bool customPosition: activePosition < 0 || editingPosition
    GridLayout {
        Layout.fillWidth: true; columns: studio.width>Theme.px(900)?2:1
        columnSpacing: Theme.px(18); rowSpacing: Theme.px(18)
        FCard {
            Layout.fillWidth: true; Layout.preferredWidth: Theme.px(350)
            Text { text: "Make it yours."; color: Theme.text; font.pixelSize: Theme.font(22); font.weight: Font.DemiBold }
            Text { text: "DISPLAY METHOD"; color: Theme.muted; font.pixelSize: Theme.font(10) }
            FCombo {
                id: method
                Layout.fillWidth: true
                model: app.platformName==="Windows"?["Auto · external first","External desktop overlay","In-game · manual hook"]:["Auto · external first","External · X Present","MangoHud game layer · opt-in"]
                currentIndex: app.platformName==="Windows"?app.options.mode:app.options.linuxOverlayMode
                onActivated: function(index) {
                    const windows = app.platformName === "Windows"
                    const key = windows ? "mode" : "linuxOverlayMode"
                    const warn = windows ? index === 0 || index === 2 : index === 2
                    if (!warn) {
                        app.setOption(key, index)
                        return
                    }
                    method.currentIndex = Qt.binding(function() { return app.options[key] })
                    if (windows) {
                        if (index !== app.options[key])
                            studio.shell.requestHookDisplayMode(index)
                    } else {
                        layerDialog.open()
                    }
                }
            }
            Text { text: app.platformName==="Windows"?"Auto starts with external monitoring. Attaching a hook requires separate approval for the selected game.":app.state.session==="Wayland"?"Wayland: select a library game, explicitly enable its Vulkan/OpenGL layer where permitted, then launch. External capture and desktop HUD positioning are unavailable.":"External capture uses X11 presentation events. Some renderers may not expose them; Framoryx explains when frames are unavailable."; color: Theme.muted; font.pixelSize: Theme.font(11); Layout.fillWidth: true; wrapMode: Text.Wrap }
            FDialog {
                id: layerDialog
                objectName: "layerWarningDialog"
                title: "Enable the MangoHud game layer?"
                confirmText: "Enable layer"
                warning: true
                message: "MangoHud loads a graphics layer into games launched with monitoring. It enables the in-game overlay and frame limiting."
                         + "\n\nGame and anti-cheat rules may prohibit third-party layers. Use at your own risk. Framoryx cannot guarantee approval or prevent bans. Known anti-cheat indicators prevent a monitored layer launch."
                         + "\n\nThis setting applies to your next monitored launch."
                onAccepted: {
                    if (app.state.antiCheatStatus === "detected")
                        app.notify("Known anti-cheat indicators are present. The external method remains selected.", "warning")
                    else
                        app.setOption("linuxOverlayMode", 2)
                }
            }
            GridLayout {
                Layout.fillWidth: true; columns: 2; columnSpacing: Theme.px(12); rowSpacing: Theme.px(10)
                Text { text: "LAYOUT"; color: Theme.muted; font.pixelSize: Theme.font(10) }
                Text { text: "ACCENT"; color: Theme.muted; font.pixelSize: Theme.font(10) }
                FCombo { Layout.fillWidth: true; model: ["Glass","Compact","Telemetry bar","Minimal","Dashboard"]; currentIndex: app.options.style; onActivated: app.setOption("style",currentIndex) }
                FCombo { Layout.fillWidth: true; model: ["Mint","Ice","Violet","Amber","Rose","Cyan","Solar","Lime","Coral","Silver","Rainbow"]; currentIndex: app.options.accent; onActivated: app.setOption("accent",currentIndex) }
                Text { text: "OVERLAY SIZE"; color: Theme.muted; font.pixelSize: Theme.font(10) }
                Text { text: "BACKGROUND"; color: Theme.muted; font.pixelSize: Theme.font(10) }
                FCombo { Layout.fillWidth: true; property var sizes: [0,50,75,100,125,150,200,250,300]; model: ["Auto","50%","75%","100%","125%","150%","200%","250%","300%"]; currentIndex: Math.max(0,sizes.indexOf(app.options.overlayScale)); onActivated: app.setOption("overlayScale",sizes[currentIndex]) }
                RowLayout { Layout.fillWidth: true; FSlider { Layout.fillWidth: true; from: 0; to: 100; stepSize: 1; value: app.options.opacity; Accessible.name: "Overlay background opacity"; onMoved: app.setOption("opacity",Math.round(value)) } Text { text: app.options.opacity+"%"; color: Theme.text; font.pixelSize: Theme.font(12); Layout.preferredWidth: Theme.px(38) } }
            }
            Text { text: "TEXT SIZE · "+app.options.fontScale+"%"; color: Theme.muted; font.pixelSize: Theme.font(10) }
            FSlider { Layout.fillWidth: true; from: 80; to: 140; stepSize: 5; value: app.options.fontScale; Accessible.name: "Overlay text size"; onMoved: app.setOption("fontScale",Math.round(value)) }
            FToggle {
                Layout.fillWidth: true
                text: "Color readouts by value"
                checked: !!app.options.hudValueColors
                onToggled: app.setOption("hudValueColors", checked)
            }
            FToggle {
                Layout.fillWidth: true
                text: "Color temperatures by range"
                enabled: !app.options.hudValueColors
                checked: !!app.options.hudTemperatureColors
                onToggled: app.setOption("hudTemperatureColors", checked)
            }
            Text {
                Layout.fillWidth: true
                text: "Value colors cover FPS, frame time, the FPS limit target, CPU/GPU use, RAM/VRAM use and temperatures. Turn value colors off to use temperature-only colors."
                color: Theme.muted
                font.pixelSize: Theme.font(11)
                wrapMode: Text.Wrap
            }
            FToggle {
                Layout.fillWidth: true
                text: "Neon text glow"
                checked: !!app.options.hudTextGlow
                onToggled: app.setOption("hudTextGlow", checked)
            }
            Text {
                Layout.fillWidth: true
                text: "Rainbow animates across HUD text. Value-colored readings keep their range color. Glow follows the text color. Games can save their own accent."
                color: Theme.muted
                font.pixelSize: Theme.font(11)
                wrapMode: Text.Wrap
            }
            Text { text: "HUD FONT"; color: Theme.muted; font.pixelSize: Theme.font(10) }
            FCombo {
                readonly property var families: Array.from(app.hudFontFamilies || [])
                Layout.fillWidth: true
                Accessible.name: "HUD font family"
                model: ["Default HUD font"].concat(families)
                currentIndex: Math.max(0, families.indexOf(app.options.hudFontFamily) + 1)
                onActivated: app.setOption("hudFontFamily", currentIndex > 0 ? families[currentIndex - 1] : "")
            }
            Text {
                visible: app.platformName === "Linux"
                text: "The in-game font applies on the next monitored launch. Refresh copied Steam launch options after changing it."
                color: Theme.muted
                font.pixelSize: Theme.font(11)
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }
            Text { text: "READOUT LABEL"; color: Theme.muted; font.pixelSize: Theme.font(10) }
            FCombo {
                id: labelReadout
                Layout.fillWidth: true
                Accessible.name: "Readout to rename"
                model: Array.from(app.readouts).sort((a, b) => a.index - b.index).map(item => item.name)
            }
            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: labelEditor
                    readonly property string profileName: app.state.profileName || ""
                    readonly property string savedLabel: (app.options.hudLabels || [])[labelReadout.currentIndex] || ""
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredHeight: Theme.px(40)
                    Accessible.name: "Custom readout label"
                    maximumLength: 40
                    placeholderText: labelReadout.currentIndex === 0 ? "FPS" :
                        labelReadout.currentIndex === 4 ? "No heading" :
                        labelReadout.currentIndex === 9 ? "FRAMORYX" : labelReadout.currentText
                    color: Theme.text
                    placeholderTextColor: Theme.muted
                    font.pixelSize: Theme.font(13)
                    selectByMouse: true
                    background: Rectangle { color: Theme.raised; radius: Theme.px(8); border.color: labelEditor.activeFocus ? Theme.accent : Theme.border }
                    Component.onCompleted: text = savedLabel
                    onSavedLabelChanged: if (!activeFocus) text = savedLabel
                    onProfileNameChanged: text = savedLabel
                    onEditingFinished: text = savedLabel
                    onTextEdited: app.setHudLabel(labelReadout.currentIndex, text)
                }
                FButton {
                    text: "Reset"
                    enabled: labelEditor.savedLabel !== ""
                    onClicked: {
                        app.setHudLabel(labelReadout.currentIndex, "")
                        labelEditor.text = labelEditor.savedLabel
                    }
                }
            }
            Text {
                text: "Labels save as you type. Blank uses the default. A custom FPS label changes the suffix, and a custom frame graph label adds a heading."
                color: Theme.muted
                font.pixelSize: Theme.font(11)
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }
            Text { text: "READOUTS · use the arrows to arrange"; color: Theme.muted; font.pixelSize: Theme.font(10); Layout.topMargin: Theme.px(8) }
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
                RowLayout { Layout.fillWidth: true; Text { text: "LIVE PREVIEW"; color: Theme.accent; font.pixelSize: Theme.font(11); font.weight: Font.DemiBold; Layout.fillWidth: true } Text { text: app.state.live?"Live game data":"Sample values · layout preview"; color: Theme.muted; font.pixelSize: Theme.font(10) } }
                OverlayPreview { Layout.fillWidth: true; Layout.preferredHeight: Theme.px(380) }
                Text { text: app.state.overlayHint||"Drag the preview to choose its position. Your layout is saved automatically."; color: Theme.muted; font.pixelSize: Theme.font(12); Layout.fillWidth: true; wrapMode: Text.Wrap }
                GridLayout {
                    columns: 3
                    uniformCellWidths: true
                    Layout.fillWidth: true
                    Layout.maximumWidth: Theme.px(420)
                    Layout.alignment: Qt.AlignLeft
                    columnSpacing: Theme.px(8)
                    rowSpacing: Theme.px(6)
                    Repeater {
                        model: studio.positionPresets
                        FButton {
                            required property int index
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            leftPadding: Theme.px(5)
                            rightPadding: leftPadding
                            text: modelData.name
                            primary: !studio.customPosition && index === studio.activePosition
                            onClicked: {
                                app.setOption("hudX", modelData.x)
                                app.setOption("hudY", modelData.y)
                                studio.editingPosition = false
                            }
                        }
                    }
                }
                FButton {
                    text: "Custom"
                    Accessible.name: "Custom HUD position"
                    Layout.alignment: Qt.AlignLeft
                    primary: studio.customPosition
                    onClicked: studio.editingPosition = true
                }
                ColumnLayout {
                    visible: studio.customPosition
                    Layout.fillWidth: true
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "X"; color: Theme.muted; font.pixelSize: Theme.font(12) }
                        FSlider { Layout.fillWidth: true; from: 0; to: 1; stepSize: .01; value: app.options.hudX; Accessible.name: "HUD horizontal position"; onMoved: { studio.editingPosition = true; app.setOption("hudX", value) } }
                        Text { text: Math.round(app.options.hudX * 100) + "%"; color: Theme.text; font.pixelSize: Theme.font(12) }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "Y"; color: Theme.muted; font.pixelSize: Theme.font(12) }
                        FSlider { Layout.fillWidth: true; from: 0; to: 1; stepSize: .01; value: app.options.hudY; Accessible.name: "HUD vertical position"; onMoved: { studio.editingPosition = true; app.setOption("hudY", value) } }
                        Text { text: Math.round(app.options.hudY * 100) + "%"; color: Theme.text; font.pixelSize: Theme.font(12) }
                    }
                }
            }
            FCard {
                Layout.fillWidth: true
                Text { text: "Keep a favorite look"; color: Theme.text; font.pixelSize: Theme.font(18); font.weight: Font.DemiBold }
                RowLayout { Layout.fillWidth: true; TextField { id: presetName; Layout.fillWidth: true; placeholderText: "Name this preset"; color: Theme.text; font.pixelSize: Theme.font(13); selectByMouse: true; background: Rectangle { color: Theme.raised; radius: 8; border.color: presetName.activeFocus?Theme.accent:Theme.border } } FButton { text: "Save preset"; primary: true; enabled: presetName.text.trim().length>0; onClicked: {app.saveProfile(presetName.text);presetName.clear()} } }
                Repeater { model: app.profiles; RowLayout { required property var modelData; required property int index; Layout.fillWidth: true; Text { textFormat: Text.PlainText; text: modelData.name; color: Theme.text; font.pixelSize: Theme.font(13); Layout.fillWidth: true; elide: Text.ElideRight } FButton { text: "Load"; quiet: true; onClicked: app.applyProfile(index) } FButton { text: "Remove"; quiet: true; danger: true; onClicked: app.removeProfile(index) } } }
                RowLayout { FButton { text: "Import presets"; onClicked: shell.importPresets() } FButton { text: "Export presets"; enabled: app.profiles.length>0; onClicked: shell.exportPresets() } }
            }
        }
    }
}
