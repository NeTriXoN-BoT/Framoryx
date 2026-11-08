import QtQuick
import QtQuick.Layouts
FCard {
    id: metric
    property string label: ""
    property real value: 0
    property string suffix: ""
    property string note: ""
    property bool available: true
    property bool highlighted: false
    property int decimals: 0
    property real animatedValue: value
    Behavior on animatedValue { NumberAnimation { duration: Theme.motion ? 200 : 0; easing.type: Easing.OutCubic } }
    Text { text: metric.label.toUpperCase(); color: Theme.muted; font.pixelSize: Theme.font(10); font.letterSpacing: 1.1 }
    Text { text: metric.available ? metric.animatedValue.toFixed(metric.decimals)+metric.suffix : "—"; color: metric.highlighted ? Theme.accent : Theme.text; font.pixelSize: Theme.font(35); font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight }
    Text { text: metric.note; color: Theme.muted; font.pixelSize: Math.max(10,Theme.px(11)); Layout.fillWidth: true; wrapMode: Text.Wrap }
}
