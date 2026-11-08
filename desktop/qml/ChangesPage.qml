import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id: page
    property var shell
    spacing: Theme.px(18)
    FCard {
        Layout.fillWidth: true
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                Text { text: "Built frame by frame."; color: Theme.text; font.pixelSize: Theme.font(24); font.weight: Font.DemiBold }
                Text { text: "The full change history travels with your app. No internet connection is needed to read it."; color: Theme.muted; font.pixelSize: Theme.font(12); Layout.fillWidth: true; wrapMode: Text.Wrap }
            }
            FButton { text: "Latest download"; onClicked: app.action("release") }
        }
        TextField { id: search; Layout.fillWidth: true; placeholderText: "Search every release…"; color: Theme.text; selectByMouse: true; Accessible.name: "Search release notes"; background: Rectangle { color: Theme.raised; radius: Theme.px(9); border.color: search.activeFocus?Theme.accent:Theme.border } }
    }
    Repeater {
        model: app.changelog
        FCard {
            id: release
            required property var modelData
            required property int index
            property bool expanded: index===0
            readonly property bool matches: !search.text.length||(modelData.title+modelData.body).toLowerCase().indexOf(search.text.toLowerCase())>=0
            visible: matches
            Layout.fillWidth: true
            RowLayout {
                Layout.fillWidth: true
                Rectangle { width: Theme.px(9); height: width; radius: width/2; color: release.index===0?Theme.accent:Theme.muted }
                ColumnLayout {
                    Layout.fillWidth: true
                    Text { text: modelData.version+(release.index===0?" · CURRENT DEVELOPMENT":" · RELEASED"); color: release.index===0?Theme.accent:Theme.muted; font.family: "monospace"; font.pixelSize: Theme.font(10); font.letterSpacing: 1 }
                    Text { text: modelData.title; color: Theme.text; font.pixelSize: Theme.font(19); font.weight: Font.DemiBold; Layout.fillWidth: true; wrapMode: Text.Wrap }
                }
                FButton { text: release.expanded||search.text.length?"Collapse":"Read changes"; quiet: true; onClicked: {if(search.text.length)search.clear();release.expanded=!release.expanded} }
            }
            Text {
                Layout.fillWidth: true
                visible: release.expanded||search.text.length>0
                text: modelData.body
                textFormat: Text.MarkdownText
                wrapMode: Text.Wrap
                color: Theme.muted
                linkColor: Theme.accent
                font.pixelSize: Theme.font(13)
                lineHeight: 1.2
                onLinkActivated: function(link){if(String(link).indexOf("https://")===0)Qt.openUrlExternally(link);else Qt.openUrlExternally("https://github.com/NeTriXoN-BoT/framoryx/blob/main/docs/releases/"+link)}
            }
        }
    }
}
