import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id: page
    property var shell
    spacing: Theme.px(18)
    Text { text: "Your processes. Your choice."; color: Theme.text; font.pixelSize: Theme.font(25); font.weight: Font.DemiBold; Layout.fillWidth: true }
    Text { text: "Allow optional modifications for specific games. Block rules take priority, and system processes remain protected."; color: Theme.muted; font.pixelSize: Theme.font(13); Layout.fillWidth: true; wrapMode: Text.Wrap }
    FCard {
        Layout.fillWidth: true
        RowLayout {
            Layout.fillWidth: true
            Text { text: "Running processes"; color: Theme.text; font.pixelSize: Theme.font(21); font.weight: Font.DemiBold; Layout.fillWidth: true }
            Text { text: app.processes.length+" processes"; color: Theme.muted; font.pixelSize: Theme.font(11) }
            FButton { text: app.state.processBusy?"Refreshing…":"Refresh list"; enabled: !app.state.processBusy; onClicked: app.refreshProcesses() }
        }
        TextField { id: processSearch; Layout.fillWidth: true; placeholderText: "Find an executable or PID…"; color: Theme.text; selectByMouse: true; Accessible.name: "Search running processes"; onTextEdited: filterDelay.restart(); background: Rectangle { color: Theme.raised; radius: Theme.px(9); border.color: processSearch.activeFocus?Theme.accent:Theme.border } }
        Timer { id: filterDelay; interval: 120; onTriggered: processList.query=processSearch.text.toLowerCase() }
        ListView {
            id: processList
            property string query: ""
            Layout.fillWidth: true; Layout.preferredHeight: Theme.px(330)
            clip: true; reuseItems: true; spacing: Theme.px(6)
            model: {let result=[];for(let p of app.processes)if(!query.length||p.name.toLowerCase().indexOf(query)>=0||String(p.pid).indexOf(query)>=0)result.push(p);return result}
            ScrollBar.vertical: ScrollBar { }
            delegate: Rectangle {
                required property var modelData
                width: processList.width; height: Theme.px(60); radius: Theme.px(10); color: Theme.raised
                RowLayout {
                    anchors.fill: parent; anchors.margins: Theme.px(10); spacing: Theme.px(10)
                    ColumnLayout { Layout.fillWidth: true; spacing: 2
                        Text { textFormat: Text.PlainText; text: modelData.name; color: Theme.text; font.pixelSize: Theme.font(13); font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
                        Text { text: "PID "+modelData.pid+" · "+status; property string status: {const rules=app.options;return app.processRuleStatus(modelData.name)} color: status==="Blocked"||status==="Protected"?Theme.amber:Theme.muted; font.pixelSize: Theme.font(10) }
                    }
                    FButton { text: "Allow"; quiet: true; enabled: !modelData.protected; hint: "Add executable name to allow list; hooks remain manual"; onClicked: app.addProcessRule(modelData.name,false) }
                    FButton { text: "Block"; quiet: true; danger: true; onClicked: app.addProcessRule(modelData.name,true) }
                }
            }
            Text { anchors.centerIn: parent; visible: processList.count===0; text: app.state.processBusy?"Reading process names…":app.processes.length?"No matching processes.":"Refresh to list running processes."; color: Theme.muted; font.pixelSize: Theme.font(12) }
        }
        Text { text: "Rules use the executable name, so they also apply on its next launch. A block always takes priority over an allow entry. Refresh updates this snapshot."; color: Theme.muted; font.pixelSize: Theme.font(11); Layout.fillWidth: true; wrapMode: Text.Wrap }
    }
    GridLayout {
        Layout.fillWidth: true; columns: page.width>Theme.px(800)?2:1; columnSpacing: Theme.px(18); rowSpacing: Theme.px(18)
        Repeater {
            model: [{key:"allow",title:"Allow list",note:"Optional hooks and automatic borderless changes",accent:true},{key:"deny",title:"Block list",note:"Hidden targets · modifications prevented",accent:false}]
            FCard {
                required property var modelData
                Layout.fillWidth: true
                Text { text: modelData.title; color: modelData.accent?Theme.accent:Theme.amber; font.pixelSize: Theme.font(21); font.weight: Font.DemiBold }
                Text { text: modelData.note; color: Theme.muted; font.pixelSize: Theme.font(12); Layout.fillWidth: true; wrapMode: Text.Wrap }
                ScrollView {
                    Layout.fillWidth: true; Layout.preferredHeight: Theme.px(250)
                    TextArea {
                        id: rules
                        text: (app.options[modelData.key]||[]).join("\n")
                        color: Theme.text; selectByMouse: true; wrapMode: TextEdit.NoWrap
                        font.pixelSize: Theme.font(13)
                        placeholderText: app.platformName==="Windows"?"game.exe\nexample*.exe":"game-binary\nexample*"
                        background: Rectangle { color: "#0e1821"; border.color: rules.activeFocus?Theme.accent:Theme.border; radius: Theme.px(9) }
                        Accessible.name: modelData.title
                    }
                }
                FButton { text: "Save "+modelData.title.toLowerCase(); primary: modelData.accent; onClicked: {app.setOption(modelData.key,rules.text.split("\n"));app.notify("Rules saved. Invalid paths and control characters were excluded.")} }
                Text { text: "One executable name per line. Wildcards * and ? are supported. Use names rather than full paths."; color: Theme.muted; font.pixelSize: Theme.font(11); Layout.fillWidth: true; wrapMode: Text.Wrap }
            }
        }
    }
    Text { text: "Allow-list entries are your preferences. They do not establish approval from a game or anti-cheat vendor."; color: Theme.muted; font.pixelSize: Theme.font(12); Layout.fillWidth: true; wrapMode: Text.Wrap }
}
