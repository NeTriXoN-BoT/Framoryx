import QtQuick
import QtQuick.Controls
Switch {
    id: control
    implicitHeight: Math.max(Theme.px(32),contentItem.implicitHeight+8)
    implicitWidth: indicator.width + Theme.px(12) + contentItem.implicitWidth
    spacing: Theme.px(12)
    padding: 0
    hoverEnabled: true
    Accessible.name: text
    indicator: Rectangle {
        implicitWidth: Theme.px(36); implicitHeight: Theme.px(21)
        x: 0; y: (control.height-height)/2
        radius: height/2
        color: control.checked ? "#2e7966" : "#304554"
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.accent
        Rectangle {
            width: Theme.px(15); height: width; radius: width/2
            x: control.checked ? parent.width-width-Theme.px(3) : Theme.px(3)
            y: (parent.height-height)/2
            color: control.checked ? Theme.accent : Theme.muted
            Behavior on x { NumberAnimation { duration: Theme.duration; easing.type: Easing.OutCubic } }
            Behavior on color { ColorAnimation { duration: Theme.duration } }
        }
    }
    contentItem: Text { text: control.text; color: control.enabled ? Theme.text : Theme.muted; font.pixelSize: Math.max(11,Theme.px(13)); verticalAlignment: Text.AlignVCenter; leftPadding: control.indicator.width+control.spacing; wrapMode: Text.Wrap }
}
