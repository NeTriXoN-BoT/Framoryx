import QtQuick
import QtQuick.Layouts
Rectangle {
        id: hud
        property bool sample: true
        property real density: Theme.scale
        function px(value){return Math.round(value*density)}
        readonly property bool bar: app.options.style===2
        readonly property bool compact: app.options.style===1||app.options.style===3
        readonly property var geometry: app.overlayLayout
        implicitWidth: hud.px(geometry.width)
        implicitHeight: hud.px(geometry.height)
        width: implicitWidth; height: implicitHeight
        color: Qt.rgba(.045,.073,.1,app.options.opacity/100)
        border.width: app.options.opacity===0||app.options.style===3?0:1; border.color: "#284a4d"; radius: hud.px(compact?8:13)
        Rectangle { visible: app.options.opacity>0&&app.options.style!==3; x: hud.px(10); y: 0; height: 2; width: parent.width-hud.px(20); color: Theme.accent }
        function reading(index){
            const live=app.state.live;
            if(index===0)return (live?Math.round(app.state.fps):hud.sample?144:"—")+" FPS";
            if(index===1)return (live?Math.round(app.state.average):hud.sample?141:"—")+" FPS";
            if(index===2)return (live?Math.round(app.state.low):hud.sample?120:"—")+" FPS";
            if(index===3)return (live?Number(app.state.frameTime).toFixed(2):hud.sample?"6.94":"—")+" ms";
            if(index===5)return (hud.sample&&!live?28:Math.round(app.state.cpu||0))+"%";
            if(index===6)return (hud.sample&&!live?"11.2":Number(app.state.ram||0).toFixed(1))+" GB";
            if(index===7)return app.options.cap===0?"Uncapped":app.options.cap;
            if(index===8)return live?app.state.renderer:hud.sample?(app.platformName==="Windows"?"DirectX 12":"Vulkan"):"Waiting";
            if(index>=10){const keys=["gpu","vramPercent","gpuTemp","cpuTemp","gpuPower","gpuClock"];const units=["%","%"," °C"," °C"," W"," MHz"];const values=[68,42,67,61,145,2100];const value=hud.sample&&!live?values[index-10]:app.state[keys[index-10]];return typeof value==="number"&&isFinite(value)?Number(value).toFixed(index===14?1:0)+units[index-10]:"—";}
            return "FRAMORYX";
        }
        Repeater {
            model: hud.geometry.cells
            Item {
                required property var modelData
                clip: true
                x: hud.px(modelData.x); y: hud.px(modelData.y); width: hud.px(modelData.width); height: hud.px(modelData.height)
                Text { visible: modelData.index===0||modelData.index===9; text: hud.reading(modelData.index); color: modelData.index===0?Theme.accent:Theme.muted; font.pixelSize: hud.px(modelData.index===0?(hud.compact?28:34):10); font.weight: modelData.index===0?Font.Bold:Font.DemiBold }
                Text { visible: modelData.index!==0&&modelData.index!==4&&modelData.index!==9; text: modelData.name; color: Theme.muted; font.pixelSize: hud.px(modelData.column?10:hud.compact?11:12); width: modelData.column?parent.width:parent.width-hud.px(105); elide: Text.ElideRight }
                Text { visible: modelData.index!==0&&modelData.index!==4&&modelData.index!==9; text: hud.reading(modelData.index); color: Theme.text; y: modelData.column?hud.px(19):0; x: modelData.column?0:parent.width-width; font.pixelSize: hud.px(modelData.column?17:hud.compact?12:13); font.weight: Font.DemiBold }
                FrameGraph { visible: modelData.index===4; width: parent.width; height: Math.max(1,parent.height-hud.px(10)); samples: app.state.live?app.graph:hud.sample?[7,6.9,9,7,7,8,7,6.8,7,7,12,6.9]:[] }
            }
        }
}
