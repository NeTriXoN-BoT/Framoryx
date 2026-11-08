import QtQuick
import QtQuick.Controls
ComboBox {
    id: control
    implicitHeight: Theme.px(40)
    implicitWidth: Theme.px(190)
    hoverEnabled: true
    font.pixelSize: Math.max(11,Theme.px(13))
    leftPadding: Theme.px(13); rightPadding: Theme.px(34)
    contentItem: Text {
        text: control.displayText
        color: control.enabled ? Theme.text : Theme.muted
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle { color: control.hovered ? "#223643" : Theme.raised; radius: Theme.px(9); border.width: control.visualFocus ? 2 : 1; border.color: control.visualFocus ? Theme.accent : Theme.border }
    indicator: Text { text: "⌄"; color: Theme.muted; font.pixelSize: Theme.px(18); x: control.width - width - Theme.px(13); y: (control.height-height)/2 }
    delegate: ItemDelegate {
        required property int index
        required property var modelData
        width: control.width
        height: Theme.px(38)
        highlighted: control.highlightedIndex === index
        contentItem: Text { text: control.textRole.length ? modelData[control.textRole] : modelData; color: highlighted ? Theme.accent : Theme.text; font: control.font; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { color: highlighted ? "#26443f" : Theme.raised; radius: Theme.px(6) }
    }
    popup: Popup {
        y: control.height + 5
        width: control.width
        padding: 5
        implicitHeight: Math.min(contentItem.implicitHeight + 10, Theme.px(320))
        contentItem: ListView { clip: true; implicitHeight: contentHeight; model: control.popup.visible ? control.delegateModel : null; currentIndex: control.highlightedIndex; ScrollIndicator.vertical: ScrollIndicator {} }
        background: Rectangle { color: Theme.raised; border.color: Theme.border; radius: Theme.px(11) }
    }
}
