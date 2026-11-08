import QtQuick
Item {
    id: backdrop
    property bool moving: Theme.motion && visible
    clip: true
    Image {
        id: art
        source: "qrc:/art/artwork.png"
        anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
        width: Math.max(parent.width*.65,Theme.px(620)); height: parent.height*1.5
        fillMode: Image.PreserveAspectCrop
        opacity: .7
        asynchronous: true
        SequentialAnimation on scale { running: backdrop.moving; loops: Animation.Infinite; NumberAnimation { from: 1; to: 1.045; duration: 16000; easing.type: Easing.InOutSine } NumberAnimation { from: 1.045; to: 1; duration: 16000; easing.type: Easing.InOutSine } }
    }
    Rectangle { anchors.fill: parent; gradient: Gradient { orientation: Gradient.Horizontal; GradientStop { position: 0; color: "#14232c" } GradientStop { position: .43; color: "#e014232c" } GradientStop { position: 1; color: "#0014232c" } } }
    Repeater {
        model: 8
        Rectangle {
            required property int index
            width: Theme.px(2); height: width; radius: width/2
            color: Theme.accent; opacity: .35
            x: backdrop.width*(.5+index*.06); y: backdrop.height*(.18+(index%4)*.18)
            SequentialAnimation on opacity { running: backdrop.moving; loops: Animation.Infinite; PauseAnimation { duration: index*370 } NumberAnimation { to: .08; duration: 2200 } NumberAnimation { to: .5; duration: 2200 } }
            SequentialAnimation on y { running: backdrop.moving; loops: Animation.Infinite; NumberAnimation { from: backdrop.height*.8; to: backdrop.height*.12; duration: 12000+index*1700 } }
        }
    }
}
