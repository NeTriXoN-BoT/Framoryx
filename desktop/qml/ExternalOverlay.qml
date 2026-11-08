import QtQuick
import QtQuick.Window
Window {
    id: overlay
    objectName: "FramoryxExternalOverlay"
    transientParent: null
    color: app.state.compositing===false?"#0c151e":"transparent"
    flags: Qt.Tool|Qt.FramelessWindowHint|Qt.WindowStaysOnTopHint|Qt.WindowTransparentForInput|Qt.WindowDoesNotAcceptFocus
    readonly property var display: app.state.overlayDisplay||({x:0,y:0,width:1920,height:1080,dpr:1})
    readonly property real desiredScale: (app.options.overlayScale>0?app.options.overlayScale/100:Math.max(.85,Math.min(2.5,display.height*display.dpr/1080)))*app.options.fontScale/100/display.dpr
    readonly property real fittedScale: Math.max(.1,Math.min(desiredScale,(display.width-30)/readouts.implicitWidth,(display.height-30)/readouts.implicitHeight))
    width: Math.ceil(readouts.implicitWidth*fittedScale)
    height: Math.ceil(readouts.implicitHeight*fittedScale)
    x: Math.round(display.x+app.options.hudX*Math.max(0,display.width-width))
    y: Math.round(display.y+app.options.hudY*Math.max(0,display.height-height))
    visible: app.platformName==="Linux"&&!app.testMode&&app.state.externalCapture&&app.state.capture&&app.state.overlay&&app.state.gameForeground&&app.options.fields!==0
    OverlayReadouts { id: readouts; sample: false; density: 1; scale: overlay.fittedScale; transformOrigin: Item.TopLeft }
}
