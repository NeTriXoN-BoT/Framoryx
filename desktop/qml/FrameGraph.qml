import QtQuick
import QtQuick.Shapes
Item {
    id: chart
    property var samples: []
    property color lineColor: Theme.accent
    property real upper: Math.max(25,Math.min(100,maximum()))
    function maximum(){let n=0;for(let x of samples)n=Math.max(n,Number(x));return n*1.1}
    function points(){let result=[];if(samples.length<2)return result;for(let i=0;i<samples.length;i++)result.push(Qt.point(i*(width-2)/(samples.length-1)+1,height-Math.min(1,Number(samples[i])/upper)*(height-12)-6));return result}
    Repeater { model: 4; Rectangle { required property int index; x: 0; y: (chart.height-1)*index/3; width: chart.width; height: 1; color: "#20313e" } }
    Shape {
        anchors.fill: parent
        antialiasing: true
        ShapePath { strokeColor: chart.lineColor; strokeWidth: Theme.px(1.7); fillColor: "transparent"; PathPolyline { path: chart.visible ? chart.points() : [] } }
    }
    Text { anchors.centerIn: parent; visible: chart.samples.length<2; text: "Your game's frame times will appear here"; color: Theme.muted; font.pixelSize: Theme.px(13) }
}
