import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id: page
    property var shell
    spacing: Theme.px(18)
    Text { text: "Your games. Your rules."; color: Theme.text; font.pixelSize: Theme.px(25); font.weight: Font.DemiBold; Layout.fillWidth: true }
    Text { text: "Allow optional modifications for specific games. Block rules take priority, and system processes remain protected."; color: Theme.muted; font.pixelSize: Theme.px(13); Layout.fillWidth: true; wrapMode: Text.Wrap }
    GridLayout {
        Layout.fillWidth: true; columns: page.width>Theme.px(800)?2:1; columnSpacing: Theme.px(18); rowSpacing: Theme.px(18)
        Repeater {
            model: [{key:"allow",title:"Allow list",note:"Optional hooks and automatic borderless changes",accent:true},{key:"deny",title:"Block list",note:"Hidden targets · modifications prevented",accent:false}]
            FCard {
                required property var modelData
                Layout.fillWidth: true
                Text { text: modelData.title; color: modelData.accent?Theme.accent:Theme.amber; font.pixelSize: Theme.px(21); font.weight: Font.DemiBold }
                Text { text: modelData.note; color: Theme.muted; font.pixelSize: Theme.px(12); Layout.fillWidth: true; wrapMode: Text.Wrap }
                ScrollView {
                    Layout.fillWidth: true; Layout.preferredHeight: Theme.px(250)
                    TextArea {
                        id: rules
                        text: (app.options[modelData.key]||[]).join("\n")
                        color: Theme.text; selectByMouse: true; wrapMode: TextEdit.NoWrap
                        font.pixelSize: Theme.px(13)
                        placeholderText: app.platformName==="Windows"?"game.exe\nexample*.exe":"game-binary\nexample*"
                        background: Rectangle { color: "#0e1821"; border.color: rules.activeFocus?Theme.accent:Theme.border; radius: Theme.px(9) }
                        Accessible.name: modelData.title
                    }
                }
                FButton { text: "Save "+modelData.title.toLowerCase(); primary: modelData.accent; onClicked: {app.setOption(modelData.key,rules.text.split("\n"));app.notify("Rules saved. Invalid paths and control characters were excluded.")} }
                Text { text: "One executable name per line. Wildcards * and ? are supported. Use names rather than full paths."; color: Theme.muted; font.pixelSize: Theme.px(11); Layout.fillWidth: true; wrapMode: Text.Wrap }
            }
        }
    }
    Text { text: "Allow-list entries are your preferences. They do not establish approval from a game or anti-cheat vendor."; color: Theme.muted; font.pixelSize: Theme.px(12); Layout.fillWidth: true; wrapMode: Text.Wrap }
}
