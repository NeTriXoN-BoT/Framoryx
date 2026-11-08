import QtQuick
import QtQuick.Controls
Slider {
    id: control
    implicitHeight: Theme.px(34)
    implicitWidth: Theme.px(220)
    padding: Theme.px(8)
    live: true
    // Keep hit geometry constant while dragging; only the visual focus ring grows.
    Accessible.name: "Level"
    background: Rectangle {
        x: control.leftPadding; y: control.topPadding+(control.availableHeight-height)/2
        width: control.availableWidth; height: Theme.px(4); radius: height/2
        color: Theme.border
        Rectangle { width: control.visualPosition*parent.width; height: parent.height; radius: parent.radius; color: Theme.accent }
    }
    handle: Rectangle {
        x: control.leftPadding + control.visualPosition*(control.availableWidth-width)
        y: control.topPadding+(control.availableHeight-height)/2
        width: Theme.px(16); height: width; radius: width/2
        color: Theme.accent
        border.width: control.visualFocus ? 2 : 0; border.color: Theme.text
        Rectangle { anchors.centerIn: parent; width: parent.width+Theme.px(10); height: width; radius: width/2; color: "transparent"; border.color: Theme.accent; opacity: control.pressed?.3:0 }
    }
}
