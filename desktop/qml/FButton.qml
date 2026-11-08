import QtQuick
import QtQuick.Controls
Button {
    id: control
    property bool primary: false
    property bool quiet: false
    property bool danger: false
    property string hint: ""
    implicitHeight: Theme.px(42)
    implicitWidth: Math.max(Theme.px(96), label.implicitWidth + Theme.px(30))
    padding: Theme.px(14)
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: text
    Accessible.description: hint
    contentItem: Text {
        id: label
        text: control.text
        color: !control.enabled ? Theme.muted : control.primary ? "#081916" : control.danger ? Theme.red : Theme.text
        font.pixelSize: Math.max(11,Theme.px(13))
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: Theme.px(10)
        color: control.primary ? Theme.accent : control.down ? "#263f4b" : control.hovered ? "#233746" : control.quiet ? "transparent" : Theme.raised
        opacity: control.enabled ? 1 : .52
        border.width: control.visualFocus ? 2 : control.primary || control.quiet ? 0 : 1
        border.color: control.visualFocus ? Theme.accent : Theme.border
        Behavior on color { ColorAnimation { duration: Theme.duration } }
    }
    scale: down ? .98 : 1
    Behavior on scale { NumberAnimation { duration: Theme.duration; easing.type: Easing.OutCubic } }
    ToolTip.visible: hovered && hint.length > 0
    ToolTip.delay: 650
    ToolTip.text: hint
}
