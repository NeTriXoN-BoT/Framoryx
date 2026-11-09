import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    property string message: ""
    property string confirmText: "Continue"
    property bool warning: false
    property bool offerRemember: false
    readonly property bool rememberWarning: remember.checked
    property Item initialFocus: cancelButton
    property alias formContent: form.data

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape
    width: Math.min(Theme.px(590), parent ? parent.width - Theme.px(28) : Theme.px(590))
    height: Math.min(implicitHeight, parent ? parent.height - Theme.px(28) : implicitHeight)
    padding: Theme.px(22)
    spacing: 0
    onAboutToShow: {
        remember.checked = false
        bodyScroll.contentItem.contentY = 0
    }
    onOpened: initialFocus.forceActiveFocus(Qt.PopupFocusReason)

    Overlay.modal: Rectangle { color: "#99070d14" }
    background: Rectangle {
        color: Theme.surface
        radius: Theme.px(16)
        border.color: Theme.border
    }
    header: ColumnLayout {
        spacing: Theme.px(5)
        Text {
            visible: dialog.warning
            Layout.fillWidth: true
            Layout.topMargin: Theme.px(20)
            Layout.leftMargin: dialog.padding
            Layout.rightMargin: dialog.padding
            text: "BEFORE YOU CONTINUE"
            color: Theme.amber
            font.pixelSize: Theme.font(10)
            font.weight: Font.DemiBold
            font.letterSpacing: 1
        }
        Text {
            objectName: "framoryxDialogTitle"
            Layout.fillWidth: true
            Layout.topMargin: dialog.warning ? 0 : Theme.px(20)
            Layout.leftMargin: dialog.padding
            Layout.rightMargin: dialog.padding
            text: dialog.title
            textFormat: Text.PlainText
            color: Theme.text
            font.pixelSize: Theme.font(22)
            font.weight: Font.DemiBold
            wrapMode: Text.Wrap
            Accessible.role: Accessible.Heading
        }
    }
    contentItem: ScrollView {
        id: bodyScroll
        objectName: "framoryxDialogScroll"
        clip: true
        padding: 0
        rightPadding: Theme.px(10)
        contentWidth: availableWidth
        contentHeight: form.implicitHeight
        implicitHeight: form.implicitHeight
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            contentItem: Rectangle {
                implicitWidth: Theme.px(4)
                radius: width / 2
                color: Theme.muted
            }
            background: Item {}
        }
        ColumnLayout {
            id: form
            width: bodyScroll.availableWidth
            spacing: Theme.px(14)
            Text {
                objectName: "framoryxDialogBody"
                visible: dialog.message.length > 0
                Layout.fillWidth: true
                text: dialog.message
                textFormat: Text.PlainText
                color: Theme.text
                font.pixelSize: Theme.font(13)
                wrapMode: Text.Wrap
                lineHeight: 1.15
            }
        }
    }
    footer: ColumnLayout {
        spacing: Theme.px(14)
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: dialog.padding
            Layout.rightMargin: dialog.padding
            implicitHeight: 1
            color: Theme.border
        }
        CheckBox {
            id: remember
            objectName: "framoryxDialogRemember"
            visible: dialog.offerRemember
            Layout.fillWidth: true
            Layout.leftMargin: dialog.padding
            Layout.rightMargin: dialog.padding
            padding: 0
            spacing: Theme.px(9)
            text: "Don’t show this warning again"
            Accessible.name: text
            indicator: Rectangle {
                implicitWidth: Theme.px(18)
                implicitHeight: implicitWidth
                y: (remember.height - height) / 2
                radius: Theme.px(4)
                color: remember.checked ? Theme.accent : Theme.raised
                border.width: remember.visualFocus ? 2 : 1
                border.color: remember.checked || remember.visualFocus ? Theme.accent : Theme.border
                Text {
                    anchors.centerIn: parent
                    visible: remember.checked
                    text: "✓"
                    color: "#081916"
                    font.pixelSize: Theme.font(13)
                    font.weight: Font.Bold
                }
            }
            contentItem: Text {
                leftPadding: remember.indicator.width + remember.spacing
                text: remember.text
                color: Theme.text
                font.pixelSize: Theme.font(13)
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.Wrap
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: dialog.padding
            Layout.rightMargin: dialog.padding
            Layout.bottomMargin: Theme.px(18)
            spacing: Theme.px(8)
            Item { Layout.fillWidth: true }
            FButton {
                id: cancelButton
                objectName: "framoryxDialogCancel"
                text: "Cancel"
                onClicked: dialog.reject()
            }
            FButton {
                objectName: "framoryxDialogConfirm"
                text: dialog.confirmText
                primary: true
                onClicked: dialog.accept()
            }
        }
    }
}
