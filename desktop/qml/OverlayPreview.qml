import QtQuick
import QtQuick.Layouts
Rectangle {
    id: stage
    property bool sample: true
    property bool draggable: true
    color: "#0c1720"
    radius: Theme.px(10)
    clip: true
    implicitHeight: Theme.px(320)
    Repeater { model: 14; Rectangle { required property int index; x: index*stage.width/13; width: 1; height: stage.height; color: "#172934" } }
    Repeater { model: 9; Rectangle { required property int index; y: index*stage.height/8; height: 1; width: stage.width; color: "#172934" } }
    Text { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: Theme.px(12); text: stage.draggable ? "Drag to place · live sizing preview" : ""; color: Theme.muted; opacity: .7; font.pixelSize: Theme.px(10) }
    Rectangle {
        id: hud
        readonly property bool bar: app.options.style===2
        readonly property bool compact: app.options.style===1||app.options.style===3
        readonly property var geometry: app.overlayLayout
        property real fittedScale: Math.min(1,(stage.width-Theme.px(30))/implicitWidth,(stage.height-Theme.px(40))/implicitHeight)
        property real desiredScale: (app.options.overlayScale===0 ? 1 : app.options.overlayScale/100)*app.options.fontScale/100
        scale: Math.max(.25,Math.min(desiredScale,fittedScale))
        transformOrigin: Item.TopLeft
        implicitWidth: Theme.px(geometry.width)
        implicitHeight: Theme.px(geometry.height)
        width: implicitWidth; height: implicitHeight
        x: Math.max(0,Math.min(stage.width-width*scale,app.options.hudX*(stage.width-width*scale)))
        y: Math.max(0,Math.min(stage.height-height*scale,app.options.hudY*(stage.height-height*scale)))
        color: Qt.rgba(.045,.073,.1,app.options.opacity/100)
        border.width: app.options.opacity===0||app.options.style===3?0:1; border.color: "#284a4d"; radius: Theme.px(compact?8:13)
        Rectangle { visible: app.options.opacity>0&&app.options.style!==3; x: Theme.px(10); y: 0; height: 2; width: parent.width-Theme.px(20); color: Theme.accent }
        function reading(index){
            const live=app.state.live;
            if(index===0)return (live?Math.round(app.state.fps):stage.sample?144:"—")+" FPS";
            if(index===1)return (live?Math.round(app.state.average):stage.sample?141:"—")+" FPS";
            if(index===2)return (live?Math.round(app.state.low):stage.sample?120:"—")+" FPS";
            if(index===3)return (live?Number(app.state.frameTime).toFixed(2):stage.sample?"6.94":"—")+" ms";
            if(index===5)return Math.round(app.state.cpu||0)+"%";
            if(index===6)return Number(app.state.ram||0).toFixed(1)+" GB";
            if(index===7)return app.options.cap===0?"Uncapped":app.options.cap;
            if(index===8)return live?app.state.renderer:stage.sample?"Renderer":"Waiting";
            return "FRAMORYX";
        }
        Repeater {
            model: hud.geometry.cells
            Item {
                required property var modelData
                x: Theme.px(modelData.x); y: Theme.px(modelData.y); width: Theme.px(modelData.width); height: Theme.px(modelData.height)
                Text { visible: modelData.index===0||modelData.index===9; text: hud.reading(modelData.index); color: modelData.index===0?Theme.accent:Theme.muted; font.pixelSize: Theme.px(modelData.index===0?(hud.compact?28:34):10); font.weight: modelData.index===0?Font.Bold:Font.DemiBold }
                Text { visible: modelData.index!==0&&modelData.index!==4&&modelData.index!==9; text: modelData.name; color: Theme.muted; font.pixelSize: Theme.px(modelData.column?10:hud.compact?11:12); width: modelData.column?parent.width:parent.width-Theme.px(105); elide: Text.ElideRight }
                Text { visible: modelData.index!==0&&modelData.index!==4&&modelData.index!==9; text: hud.reading(modelData.index); color: Theme.text; y: modelData.column?Theme.px(19):0; x: modelData.column?0:parent.width-width; font.pixelSize: Theme.px(modelData.column?17:hud.compact?12:13); font.weight: Font.DemiBold }
                FrameGraph { visible: modelData.index===4; width: parent.width; height: Math.max(1,parent.height-Theme.px(10)); samples: app.state.live?app.graph:stage.sample?[7,6.9,9,7,7,8,7,6.8,7,7,12,6.9]:[] }
            }
        }
        DragHandler {
            id: placement
            target: null
            property point startScene: Qt.point(0,0)
            property point startPosition: Qt.point(0,0)
            enabled: stage.draggable
            onActiveChanged: if(active){startScene=centroid.scenePosition;startPosition=Qt.point(hud.x,hud.y)}
            onCentroidChanged: if(active){app.setOption("hudX",Math.max(0,Math.min(1,(startPosition.x+centroid.scenePosition.x-startScene.x)/Math.max(1,stage.width-hud.width*hud.scale))));app.setOption("hudY",Math.max(0,Math.min(1,(startPosition.y+centroid.scenePosition.y-startScene.y)/Math.max(1,stage.height-hud.height*hud.scale))))}
        }
    }
}
