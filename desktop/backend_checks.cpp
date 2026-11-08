#include "backend_checks.hpp"
#include "controller.hpp"
#include "lifecycle.hpp"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QWindow>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTimer>
#include <algorithm>

namespace {
class Checks : public QObject {
    Controller* c;
    QString destination;
    bool* qmlFailure;
    QTimer tick,heartbeat;
    QElapsedTimer total,phase,beat;
    QJsonArray records;
    int stage=0;
    bool entered=false,finished=false;
    qint64 maximumGap=0;
    double maximumInput=0;
    void record(const QString& name,bool passed){records.append(QJsonObject{{"name",name},{"passed",passed},{"atMs",total.elapsed()}});}
    void next(){++stage;entered=false;phase.restart();}
    void complete(){
        if(finished)return;finished=true;tick.stop();heartbeat.stop();
        record("QML reports no errors",!*qmlFailure);record("Frontend event loop remains responsive",maximumGap<500);
        bool passed=true;for(const auto& value:records)passed=passed&&value.toObject()["passed"].toBool();
        QDir().mkpath(destination);QSaveFile file(destination+"/backend-result.json");
        const QJsonObject result{{"passed",passed},{"platform",c->platformName()},{"elapsedMs",total.elapsed()},{"maximumEventLoopGapMs",maximumGap},{"maximumInputLevel",maximumInput},{"checks",records}};
        if(!file.open(QIODevice::WriteOnly)||file.write(QJsonDocument(result).toJson())<0||!file.commit())passed=false;
        QCoreApplication::exit(passed?0:1);
    }
    void await(bool condition,const QString& name,int timeout=15000){if(condition){record(name,true);next();}else if(phase.elapsed()>timeout){record(name,false);complete();}}
    void step(){
        if(total.elapsed()>210000){record("Bounded integration timeout",false);complete();return;}
        maximumInput=std::max(maximumInput,c->state()["inputLevel"].toDouble());
        const bool first=!entered;entered=true;const auto state=c->state();
        switch(stage){
        case 0:await(state["connected"].toBool(),"Native backend connects");break;
        case 1:
            if(first){
#ifdef Q_OS_WIN
                c->action("captureStart");
#endif
                c->action("fixture");
            }
            await(state["live"].toBool()&&state["fps"].toDouble()>10&&!c->graph().isEmpty(),"Real presentation events reach the frontend",25000);break;
        case 2:
            if(first){c->setOption("style",4);c->setOption("fields",65535);c->setOption("opacity",0);c->setOption("hudX",.85);c->setOption("hudY",.05);if(!state["overlay"].toBool())c->action("overlay");}
#ifdef Q_OS_WIN
            await(state["overlay"].toBool()&&state["live"].toBool(),"Transparent customized external HUD remains live");
#else
            {bool exposed=false;for(auto window:QGuiApplication::allWindows())if(window->objectName()=="FramoryxExternalOverlay")exposed=window->isVisible()&&window->isExposed();await(exposed&&state["live"].toBool(),"External HUD works while the control window is hidden");}
#endif
            break;
        case 3:
            if(first){
#ifndef Q_OS_WIN
                for(const auto& value:state["windows"].toList())if(value.toMap()["pid"]==state["profilePid"]){c->selectGame(value.toMap()["id"].toString());break;}
#endif
                c->action("borderless");
            }
            await(state["borderlessActive"].toBool(),"Borderless command reaches the selected game");break;
        case 4:
            if(first)c->action("restore");
            await(!state["borderlessActive"].toBool()&&phase.elapsed()>500,"Window restoration completes");break;
#ifdef Q_OS_WIN
        case 5:
            if(first){c->setOption("cap",30);c->action("enableHook",{{"confirmed",true}});}
            await(state["hookActive"].toBool()&&state["fps"].toDouble()>26&&state["fps"].toDouble()<34,"Confirmed frontend limiter request produces 30 FPS",15000);break;
        case 6:
            if(first)c->action("disableHook");await(!state["hookActive"].toBool()&&phase.elapsed()>500,"Frontend hook disable completes");break;
        case 7:
            if(first)c->action("capture");await(!state["capture"].toBool(),"Capture stops through the frontend");break;
        default:complete();break;
#else
        case 5:
            if(first)c->action("bassApply");await(state["bassConfigured"].toBool(),"PipeWire output filter starts",20000);break;
        case 6:
            if(first){c->setOption("noise",false);c->setOption("voicePreset",4);c->action("micApply");}
            await(state["micConfigured"].toBool(),"Physical microphone routes through the bassy EQ",20000);break;
        case 7:
            if(first)c->action("record");
            await(state["sampleReady"].toBool()&&!state["recording"].toBool()&&state["recordingDevice"]=="Framoryx Microphone"&&maximumInput>0,"Processed microphone records a nonempty live signal",12000);break;
        case 8:
            if(first)c->action("playSample");
            await(!state["playing"].toBool()&&state["playbackFinished"].toBool()&&phase.elapsed()>5500,"Recorded sample playback finishes",12000);break;
        case 9:
            if(first){c->setOption("noise",true);c->action("micApply");}
            await(!state["micApplying"].toBool()&&!state["pendingMic"].toBool()&&!state["noiseBusy"].toBool()&&state["micConfigured"].toBool()&&state["noiseStatus"].toString().contains("verified",Qt::CaseInsensitive),"Official RNNoise is verified and the live microphone chain restarts",90000);break;
        case 10:
            if(first){c->action("micDisable");c->action("bassDisable");}
            await(!state["micConfigured"].toBool()&&!state["bassConfigured"].toBool(),"Microphone and output bypass release their filters");break;
        case 11:
            if(first)c->action("capture");await(!state["capture"].toBool(),"Passive capture stops");break;
        case 12:
            if(first){c->setOption("startWithSystem",true);record("Sign-in entry is registered",lifecycle::startupEnabled());c->setOption("startWithSystem",false);record("Sign-in entry can be removed",!lifecycle::startupEnabled());next();}break;
        default:complete();break;
#endif
        }
    }
public:
    Checks(Controller* controller,QString output,bool* errors):c(controller),destination(std::move(output)),qmlFailure(errors){
        total.start();phase.start();beat.start();heartbeat.setInterval(10);heartbeat.setTimerType(Qt::PreciseTimer);connect(&heartbeat,&QTimer::timeout,this,[this]{maximumGap=std::max(maximumGap,beat.restart());});heartbeat.start();tick.setInterval(100);connect(&tick,&QTimer::timeout,this,[this]{step();});tick.start();
    }
};
}
int runBackendChecks(Controller* controller,const QString& destination,bool* qmlFailure){
    if(qEnvironmentVariable("GITHUB_ACTIONS")!="true")return 2;
    Checks checks(controller,destination,qmlFailure);return QCoreApplication::exec();
}
