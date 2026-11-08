import QtQuick
import QtQuick.Layouts
Rectangle {
    id: card
    default property alias content: body.data
    property real padding: Theme.px(22)
    implicitHeight: body.implicitHeight + padding * 2
    color: Theme.surface
    radius: Theme.px(16)
    border.color: Theme.border
    border.width: 1
    ColumnLayout {
        id: body
        x: card.padding; y: card.padding
        width: card.width - card.padding * 2
        spacing: Theme.px(14)
    }
}
