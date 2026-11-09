#include "backend_checks.hpp"
#include "controller.hpp"
#include "lifecycle.hpp"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QWindow>
#include <QScreen>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QProcess>
#include <QTimer>
#include <algorithm>
#ifndef Q_OS_WIN
#include "x11_window.hpp"
#endif

namespace {
class Checks : public QObject {
    Controller* c;
    QString destination;
    QString scope;
    bool* qmlFailure;
    QTimer tick,heartbeat;
    QElapsedTimer total,phase,beat;
    QJsonArray records;
    int stage=0;
    bool entered=false,finished=false;
    qint64 maximumGap=0;
    double maximumInput=0;
    void record(const QString& name,bool passed){records.append(QJsonObject{{"name",name},{"passed",passed},{"atMs",total.elapsed()}});}
    void next(){
#ifndef Q_OS_WIN
        if(stage==0&&scope=="wayland")stage=13;
        else if(stage==0&&scope=="audio")stage=5;
        else if(stage==4&&scope=="graphics")stage=13;
        else if(stage==10&&scope=="audio")stage=12;
        else if(stage==12&&scope=="audio")stage=24;
        else ++stage;
#else
        ++stage;
#endif
        entered=false;phase.restart();
    }
    void screenshot(const QString& name){if(scope=="wayland")return;QDir().mkpath(destination);const auto screen=QGuiApplication::primaryScreen();const auto picture=screen?screen->grabWindow(0):QPixmap();record("Capture "+name,!picture.isNull()&&picture.save(destination+'/'+name));}
    void complete(){
        if(finished)return;finished=true;tick.stop();heartbeat.stop();
        record("QML reports no errors",!*qmlFailure);record("Frontend event loop remains responsive",maximumGap<500);
        bool passed=true;for(const auto& value:records)passed=passed&&value.toObject()["passed"].toBool();
        QDir().mkpath(destination);QSaveFile file(destination+"/backend-result.json");
        const auto state=c->state();
        QJsonArray microphones;for(const auto& value:c->devices())microphones.append(value.toMap()["name"].toString());
        if(!passed){for(const auto& name:{"output.conf","microphone.conf"})QFile::copy(c->root()+'/'+name,destination+'/'+name);}
        const QJsonObject result{{"passed",passed},{"platform",c->platformName()},{"displayBackend",QGuiApplication::platformName()},{"gameHudFrames",state["gameHudFrames"].toLongLong()},{"scope",scope},{"elapsedMs",total.elapsed()},{"maximumEventLoopGapMs",maximumGap},{"maximumInputLevel",maximumInput},{"checks",records},{"notice",state["notice"].toString()},{"audioStatus",state["audioStatus"].toString()},{"microphones",microphones},{"nodes",QJsonArray::fromVariantList(state["audioNodes"].toList())},{"micDiagnostic",state["micDiagnostic"].toString()},{"outputDiagnostic",state["outputDiagnostic"].toString()},{"noiseStatus",state["noiseStatus"].toString()}};
        auto diagnostic=result;diagnostic["fps"]=state["fps"].toDouble();diagnostic["appliedCap"]=state["appliedCap"].toInt();diagnostic["renderer"]=state["renderer"].toString();diagnostic["sampleBytes"]=state["sampleBytes"].toLongLong();diagnostic["sampleReady"]=state["sampleReady"].toBool();
#ifndef Q_OS_WIN
        if(!passed){QProcess dump;dump.start("pw-dump");if(dump.waitForFinished(1500)){const auto bytes=dump.readAllStandardOutput();if(bytes.size()<=8*1024*1024){QSaveFile graph(destination+"/audio-graph.json");if(graph.open(QIODevice::WriteOnly)){graph.write(bytes);graph.commit();}}}else {dump.kill();dump.waitForFinished(200);}}
#endif
        if(!file.open(QIODevice::WriteOnly)||file.write(QJsonDocument(diagnostic).toJson())<0||!file.commit())passed=false;
        QCoreApplication::exit(passed?0:1);
    }
    void await(bool condition,const QString& name,int timeout=15000){if(condition){record(name,true);next();}else if(phase.elapsed()>timeout){record(name,false);complete();}}
    void step(){
        if(total.elapsed()>210000){record("Bounded integration timeout",false);complete();return;}
        maximumInput=std::max(maximumInput,c->state()["inputLevel"].toDouble());
        const bool first=!entered;entered=true;const auto state=c->state();
        // QVulkanWindow uses FIFO; the private software Wayland compositor
        // presents this scene at about 34 FPS even without any layer. Exercise
        // two achievable caps there; X11 and OpenGL retain the 30/60 checks.
        const int vulkanLow=scope=="wayland"?20:30,vulkanHigh=scope=="wayland"?30:60;
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
#ifndef Q_OS_WIN
            if(first){c->setOption("allow",QStringList{});c->selectGame("auto");}
#endif
            if(first){c->setOption("style",4);c->setOption("fields",65535);c->setOption("opacity",0);c->setOption("hudX",.85);c->setOption("hudY",.05);if(!state["overlay"].toBool())c->action("overlay");}
#ifdef Q_OS_WIN
            await(state["overlay"].toBool()&&state["live"].toBool(),"Transparent customized external HUD remains live");
#else
            {bool exposed=false;for(auto window:QGuiApplication::allWindows())if(window->objectName()=="FramoryxExternalOverlay")exposed=window->isVisible()&&window->isExposed();await(exposed&&state["live"].toBool()&&c->options()["selectedGame"]=="auto"&&c->options()["allow"].toStringList().isEmpty(),"Auto displays an unlisted foreground game's external HUD after receiving X Present frames");}
#endif
            break;
        case 3:
            if(first){
#ifndef Q_OS_WIN
                for(const auto& value:state["windows"].toList())if(value.toMap()["pid"]==state["profilePid"]){c->selectGame(value.toMap()["id"].toString());break;}
#endif
                c->action("borderless");
            }
#ifdef Q_OS_WIN
            await(state["borderlessActive"].toBool(),"Borderless command reaches the selected game");
#else
            await(state["borderlessActive"].toBool()&&x11::inspect(c->options()["selectedGame"].toString().toUInt(nullptr,16)).fullscreen,"Window manager confirms the selected game is fullscreen");
#endif
            break;
        case 4:
            if(first)c->action("restore");
#ifdef Q_OS_WIN
            await(!state["borderlessActive"].toBool()&&phase.elapsed()>500,"Window restoration completes");
#else
            await(!state["borderlessActive"].toBool()&&!x11::inspect(c->options()["selectedGame"].toString().toUInt(nullptr,16)).fullscreen&&phase.elapsed()>500,"Window manager confirms fullscreen is restored");
#endif
            break;
#ifdef Q_OS_WIN
        case 5:
            if(first){
                const auto pid=state["profilePid"].toUInt();
                const auto executable=state["profileExe"].toString();
                c->setOption("cap",30);
                c->action("enableHook",{{"confirmed",true},{"pid",pid+1},{"exe",executable}});
                record("Hook confirmation for a different process is refused",
                       !c->state()["hookActive"].toBool()&&c->state()["notice"].toString().startsWith("The game changed."));
                c->action("enableHook",{{"confirmed",true},{"pid",pid},{"exe",executable}});
            }
            await(state["hookActive"].toBool()&&state["fps"].toDouble()>26&&state["fps"].toDouble()<34,"Confirmed frontend limiter request produces 30 FPS",15000);break;
        case 6:
            if(first)c->action("disableHook");await(!state["hookActive"].toBool()&&phase.elapsed()>500,"Frontend hook disable completes");break;
        case 7:
            if(first)c->action("capture");await(!state["capture"].toBool(),"Capture stops through the frontend");break;
        case 8:
            if(first){c->setOption("startWithSystem",true);record("Windows sign-in entry is registered",lifecycle::startupEnabled());c->setOption("startWithSystem",false);record("Windows sign-in entry can be removed",!lifecycle::startupEnabled());next();}break;
        default:complete();break;
#else
        case 5:
            if(first){c->action("bassDisable");c->action("bassApply");}await(state["bassConfigured"].toBool(),"PipeWire output filter starts after an already-disabled bypass",20000);break;
        case 6:
            if(first){c->action("micDisable");c->setOption("noise",false);c->setOption("eq",true);c->setOption("voicePreset",4);c->action("micApply");}
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
        case 13:
            if(first){c->action("fixtureClose");if(!state["overlay"].toBool())c->action("overlay");c->setOption("linuxOverlayMode",2);c->setOption("cap",30);c->action("fixture");}
            await(state["live"].toBool()&&!state["externalCapture"].toBool()&&state["fps"].toDouble()>26&&state["fps"].toDouble()<34&&phase.elapsed()>2000,"OpenGL game layer streams real frames at 30 FPS",20000);if(stage==14){record("Game rendering resolution reaches automatic HUD sizing",state["gamePixelWidth"].toInt()>0&&state["gamePixelHeight"].toInt()>0);record("OpenGL HUD generates visible geometry",state["gameHudFrames"].toLongLong()>0);}break;
        case 14:
            if(first){screenshot("hud-opengl-glass.png");c->setOption("cap",60);c->setOption("style",4);c->setOption("fields",65535);c->setOption("opacity",0);}
            await(state["fps"].toDouble()>54&&state["fps"].toDouble()<66&&phase.elapsed()>2500,"OpenGL limiter updates to 60 FPS without relaunch",15000);break;
        case 15:
            if(first){screenshot("hud-opengl-dashboard-transparent.png");c->action("capture");}await(!state["capture"].toBool(),"Game-layer capture stops without closing the game");break;
        case 16:
            if(first)c->action("capture");await(state["capture"].toBool()&&state["live"].toBool()&&phase.elapsed()>1500,"Game-layer telemetry reconnects to the running game");break;
        case 17:
            if(first){c->action("capture");c->action("fixtureClose");c->setOption("linuxOverlayMode",2);c->setOption("cap",vulkanLow);QFile::remove(c->root()+"/fixture-result.json");c->action("fixture",{{"api","vulkan"},{"seconds",35},{"report",true},{"launcher",true}});}
            await(state["live"].toBool()&&!state["externalCapture"].toBool()&&state["fps"].toDouble()>vulkanLow*.9&&state["fps"].toDouble()<vulkanLow*1.1&&phase.elapsed()>2000,QString("Vulkan game layer streams real frames at %1 FPS").arg(vulkanLow),20000);if(stage==18){record("Vulkan HUD generates visible geometry",state["gameHudFrames"].toLongLong()>0);record("Launcher capture resolves the actual game process identity",state["profileExe"]=="FramoryxVulkanScene");}break;
        case 18:
            if(first){screenshot("hud-vulkan-glass.png");c->setOption("cap",vulkanHigh);c->setOption("style",2);c->setOption("fields",65535);c->setOption("accent",8);c->setOption("opacity",85);}
            await(state["fps"].toDouble()>vulkanHigh*.9&&state["fps"].toDouble()<vulkanHigh*1.1&&state["appliedCap"].toInt()==vulkanHigh&&phase.elapsed()>2500,QString("Vulkan limiter updates to %1 FPS without relaunch").arg(vulkanHigh),15000);break;
        case 19:
            if(first){screenshot("hud-vulkan-telemetry.png");c->action("capture");}await(!state["capture"].toBool(),"Vulkan telemetry stops cleanly");break;
        case 20:
            if(first){c->action("capture");c->setOption("deny",QStringList{"FramoryxVulkanScene"});}await(!state["live"].toBool()&&state["appliedCap"].toInt()==0,"Blocking a running game stops its layer capture and limit");break;
        case 21:
            if(first){c->action("capture");c->action("capture");}await(!state["live"].toBool()&&state["appliedCap"].toInt()==0&&phase.elapsed()>1200,"Restarting capture does not override a process block");break;
        case 22:
            if(first)c->setOption("deny",QStringList{});await(state["live"].toBool()&&state["appliedCap"].toInt()==vulkanHigh,"Removing a process block restores the explicitly selected method");break;
        case 23:
            {QFile file(c->root()+"/fixture-result.json");QJsonObject report;if(!state["gameRunning"].toBool()&&file.open(QIODevice::ReadOnly))report=QJsonDocument::fromJson(file.read(16384)).object();await(!state["gameRunning"].toBool()&&!state["live"].toBool()&&report["valid"].toBool()&&report["frames"].toInt()>0&&report["exitCode"].toInt(-1)==0,"Normal game shutdown releases its surface and clears telemetry",45000);}break;
        default:complete();break;
#endif
        }
    }
public:
    Checks(Controller* controller,QString output,bool* errors,QString testScope):c(controller),destination(std::move(output)),scope(std::move(testScope)),qmlFailure(errors){
        total.start();phase.start();beat.start();heartbeat.setInterval(10);heartbeat.setTimerType(Qt::PreciseTimer);connect(&heartbeat,&QTimer::timeout,this,[this]{maximumGap=std::max(maximumGap,beat.restart());});heartbeat.start();tick.setInterval(100);connect(&tick,&QTimer::timeout,this,[this]{step();});tick.start();
    }
};
}
int runBackendChecks(Controller* controller,const QString& destination,bool* qmlFailure,const QString& scope){
    if(qEnvironmentVariable("GITHUB_ACTIONS")!="true")return 2;
    if(scope!="all"&&scope!="audio"&&scope!="graphics"&&scope!="wayland")return 2;
    if(scope=="wayland"&&!QGuiApplication::platformName().startsWith("wayland"))return 2;
    Checks checks(controller,destination,qmlFailure,scope);return QCoreApplication::exec();
}
