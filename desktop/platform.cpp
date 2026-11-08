#include "platform.hpp"
#include "controller.hpp"
#include "unlock.hpp"
#include "lifecycle.hpp"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QScreen>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QDateTime>
#include <QClipboard>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <cmath>
#include <QtConcurrent>
#include <QFutureWatcher>
#ifndef Q_OS_WIN
#include "vendor/miniz/miniz.h"
#include <unistd.h>
#include <signal.h>
#include <sys/prctl.h>
#include "passive_linux.hpp"
#include "mango_client.hpp"
#include "noise_cache.hpp"
#else
class PassiveCapture {};
class MangoClient {};
#endif
namespace {
QString exe(const QString& name){return QStandardPaths::findExecutable(name);}
QString processStart(qint64 pid){QFile file(QString("/proc/%1/stat").arg(pid));if(!file.open(QIODevice::ReadOnly))return {};const auto bytes=file.read(4096);const int end=bytes.lastIndexOf(')');if(end<0)return {};const auto fields=bytes.mid(end+1).simplified().split(' ');return fields.size()>19?QString::fromLatin1(fields[19]):QString();}
QString shellQuote(QString value){return "'"+value.replace("'","'\\''")+"'";}
QByteArray json(const QVariantMap& value){return QJsonDocument(QJsonObject::fromVariantMap(value)).toJson(QJsonDocument::Compact);}
bool writeFile(const QString& path,const QByteArray& bytes){QSaveFile file(path);return file.open(QIODevice::WriteOnly)&&file.write(bytes)==bytes.size()&&file.commit();}
bool denied(const QVariantMap& options,const QString& name){
    static const QStringList immutable{"framoryx","framoryx.exe","framoryxengine.exe","gnome-shell","kwin_wayland","kwin_x11","Xorg","Xwayland","pipewire","wireplumber","systemd","steamwebhelper","explorer.exe","dwm.exe","svchost.exe","lsass.exe"};
    if(immutable.contains(name,Qt::CaseInsensitive)||fg::matches(fg::antiCheatNames,name.toStdString()))return true;
    for(const auto& pattern:options["deny"].toStringList())if(fg::wildcard(pattern.toStdString(),name.toStdString()))return true;
    return false;
}
bool allowed(const QVariantMap& options,const QString& name){if(denied(options,name))return false;for(const auto& pattern:options["allow"].toStringList())if(fg::wildcard(pattern.toStdString(),name.toStdString()))return true;return false;}
QVariantMap selectedWindow(const QVariantMap& options,const QVariantList& games,const QVariantList& windows,quint32 foreground){
    const auto selected=options["selectedGame"].toString();QString libraryFile;QVariantList matches;
    for(const auto& value:games){const auto game=value.toMap();if(game["id"]==selected)libraryFile=QFileInfo(game["executable"].toString()).fileName();}
    for(const auto& value:windows){const auto window=value.toMap();const auto process=window["exe"].toString();if(denied(options,process))continue;
        bool ok=false;const auto id=window["id"].toString().toUInt(&ok,16);if(!ok||!id)continue;
        if(window["id"]==selected)return window;
        if(!libraryFile.isEmpty()&&process==libraryFile)matches<<window;
        if(selected=="auto"&&id==foreground&&allowed(options,process))return window;
    }
    for(const auto& value:matches)if(value.toMap()["id"].toString().toUInt(nullptr,16)==foreground)return value.toMap();
    return matches.size()==1?matches.first().toMap():QVariantMap();
}
QProcessEnvironment hostEnvironment(){
    auto environment=QProcessEnvironment::systemEnvironment();
#ifndef Q_OS_WIN
    if(environment.contains("FRAMORYX_ORIGINAL_LD_LIBRARY_PATH")){
        const QString appdir=environment.value("APPDIR");const auto original=environment.value("FRAMORYX_ORIGINAL_LD_LIBRARY_PATH");environment.remove("FRAMORYX_ORIGINAL_LD_LIBRARY_PATH");
        if(original.isEmpty())environment.remove("LD_LIBRARY_PATH");else environment.insert("LD_LIBRARY_PATH",original);
        for(const auto& key:QStringList{"QT_PLUGIN_PATH","QML2_IMPORT_PATH","QML_IMPORT_PATH"})if(!appdir.isEmpty()&&environment.contains(key)){auto paths=environment.value(key).split(':',Qt::SkipEmptyParts);paths.removeIf([&](const QString& path){return path==appdir||path.startsWith(appdir+'/');});if(paths.isEmpty())environment.remove(key);else environment.insert(key,paths.join(':'));}
        for(const auto& key:QStringList{"APPDIR","APPIMAGE","ARGV0","OWD"})environment.remove(key);
    }
#endif
    return environment;
}
}
Platform::Platform(Controller* controller):QObject(controller),c(controller){
    m_optionTimer.setInterval(75);m_optionTimer.setSingleShot(true);connect(&m_optionTimer,&QTimer::timeout,this,&Platform::flushOptions);
    m_volumeTimer.setSingleShot(true);m_volumeTimer.setInterval(45);
    connect(&m_volumeTimer,&QTimer::timeout,this,&Platform::flushVolume);
    m_launchTimer.setInterval(100);connect(&m_launchTimer,&QTimer::timeout,this,[this]{
        if(m_closing||m_deferredLaunch.isEmpty()||m_deferredLaunch!=c->m_options["selectedGame"].toString()){m_launchTimer.stop();m_deferredLaunch.clear();c->setState("gameLaunchPending",false);return;}
        if(c->m_safetyBusy&&c->m_clock.elapsed()-m_deferredAt<5000)return;
        if(c->m_safetyBusy){c->setOption("linuxOverlayMode",1);c->notify("The compatibility check is taking longer. Launching with external monitoring and no game layer.","warning");}
        m_launchTimer.stop();m_deferredLaunch.clear();c->setState("gameLaunchPending",false);linuxLaunch();
    });
    for(auto* process:{&m_mic,&m_output}){
#ifndef Q_OS_WIN
        const auto parentPid=getpid();process->setChildProcessModifier([parentPid]{prctl(PR_SET_PDEATHSIG,SIGTERM);if(getppid()!=parentPid)_exit(125);});
#endif
        connect(process,&QProcess::started,this,[this,process]{if(m_expectedStops.contains(process)||m_pendingAudio.contains(process))process->terminate();});
        connect(process,&QProcess::readyReadStandardError,this,[this,process]{const QString key=process==&m_mic?"micDiagnostic":"outputDiagnostic";const auto error=(c->m_state[key].toString()+QString::fromUtf8(process->readAllStandardError())).right(16384);c->setState(key,error);});
        connect(process,&QProcess::errorOccurred,this,[this,process](QProcess::ProcessError error){if(error==QProcess::FailedToStart)c->setState(process==&m_mic?"micDiagnostic":"outputDiagnostic",process->errorString());});
        connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,process](int code,QProcess::ExitStatus){c->setState(process==&m_mic?"micConfigured":"bassConfigured",false);const bool expected=m_expectedStops.remove(process);if(code&&!m_closing&&!expected&&!m_pendingAudio.contains(process))c->notify("An audio filter stopped. Open Audio setup to inspect its status.","warning");if(m_pendingAudio.contains(process)&&!m_closing)QTimer::singleShot(0,this,[this,process]{startAudio(process);});});
    }
}
void Platform::flushVolume(){
    m_volumeTimer.stop();
#ifdef Q_OS_WIN
        send({{"action","volume"},{"value",c->m_options["volume"].toInt()}});
#else
        if(exe("wpctl").isEmpty()){c->notify("PipeWire volume controls are unavailable. Open Setup for the required components.","warning");return;}
        if(m_volumeWriting){m_volumePending=true;return;}
        m_volumeWriting=true;m_volumePending=false;
        const double volume=c->m_options["volume"].toInt()/100.;
        command(exe("wpctl"),{"set-volume","-l","2.0","@DEFAULT_AUDIO_SINK@",QString::number(volume,'f',2)},[this](int code,QByteArray){m_volumeWriting=false;if(code&&!m_closing)c->notify("The output volume could not be changed. Check your audio session.","warning");if(m_volumePending&&!m_closing)flushVolume();});
#endif
}
Platform::~Platform(){shutdown();}
void Platform::command(const QString& program,const QStringList& args,std::function<void(int,QByteArray)> complete,int timeout){
    if(program.isEmpty()){if(complete)complete(-1,{});return;}
    auto process=new QProcess(this);m_commands<<process;
    auto buffer=std::make_shared<QByteArray>();auto done=std::make_shared<bool>(false);
    auto finish=[this,process,buffer,done,complete](int code){if(*done)return;*done=true;if(process->state()!=QProcess::NotRunning)process->kill();if(complete)complete(code,*buffer);m_commands.removeAll(process);process->deleteLater();};
    connect(process,&QProcess::readyReadStandardOutput,this,[process,buffer,finish]{*buffer+=process->readAllStandardOutput();if(buffer->size()>8*1024*1024)finish(-2);});
    connect(process,&QProcess::readyReadStandardError,this,[process]{process->readAllStandardError();});
    connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[process,buffer,finish](int code,QProcess::ExitStatus status){*buffer+=process->readAllStandardOutput();finish(status==QProcess::NormalExit?code:-1);});
    connect(process,&QProcess::errorOccurred,this,[finish](QProcess::ProcessError error){if(error==QProcess::FailedToStart)finish(-1);});
    QTimer::singleShot(timeout,process,[finish]{finish(-3);});
    process->setProcessEnvironment(hostEnvironment());process->setProgram(program);process->setArguments(args);process->start();
}
void Platform::start(){
#ifdef Q_OS_WIN
    m_token=QString::number(QRandomGenerator::system()->generate64(),16)+QString::number(QRandomGenerator::system()->generate64(),16);
    const QString path=QCoreApplication::applicationDirPath()+"/FramoryxEngine.exe";
    if(!QFileInfo::exists(path)){c->notify("The Windows engine is missing. Extract the complete Framoryx folder.","error");refresh();return;}
    m_engine.setProcessChannelMode(QProcess::SeparateChannels);
    m_engine.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args){args->flags|=0x08000000;});
    connect(&m_engine,&QProcess::readyReadStandardOutput,this,[this]{
        m_incoming+=m_engine.readAllStandardOutput();if(m_incoming.size()>1024*1024){m_engine.kill();c->notify("The engine sent an invalid response.","error");return;}
        int end;while((end=m_incoming.indexOf('\n'))>=0){const auto line=m_incoming.left(end);m_incoming.remove(0,end+1);const auto doc=QJsonDocument::fromJson(line);if(doc.isObject())c->receive(doc.object());}
    });
    connect(&m_engine,&QProcess::readyReadStandardError,this,[this]{c->setState("engineDiagnostic",QString::fromUtf8(m_engine.readAllStandardError()).right(4096));});
    connect(&m_engine,&QProcess::started,this,[this]{applyOptions();flushOptions();select(c->m_options["selectedGame"].toString());if(c->m_options["autoCapture"].toBool())send({{"action","captureStart"}});});
    connect(&m_engine,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int,QProcess::ExitStatus){c->setState("connected",false);if(!m_closing)c->notify("The Windows engine closed. Restart Framoryx to reconnect.","error");});
    m_engine.start(path,{"--engine-server",m_token,"--settings-file",c->root()+"/engine.ini"});
#else
    m_passive=std::make_unique<PassiveCapture>(this);
    m_mango=std::make_unique<MangoClient>(this);
    connect(m_mango.get(),&MangoClient::frames,this,[this](const QList<double>& times,quint32 pid){if(m_monitoring&&!c->m_state["externalCapture"].toBool()&&gameLayerAllowed())c->sampleFrames(times,c->m_state["profileExe"].toString(),pid);});
    connect(m_mango.get(),&MangoClient::connectedToGame,this,[this](quint32 pid){c->setState("profilePid",pid);c->setState("gameRunning",true);const bool layer=gameLayerAllowed();if(layer){if(m_passive)m_passive->stop();c->setState("externalCapture",false);}m_mango->configure(c->m_options,c->m_state["overlay"].toBool()&&layer);c->setState("overlayHint","Connected to the game. Overlay and frame-limit changes apply live; frame telemetry stays in bounded memory.");});
    connect(m_mango.get(),&MangoClient::limitApplied,this,[this](int cap){c->setState("appliedCap",cap);});
    connect(m_mango.get(),&MangoClient::surfaceSize,this,[this](int width,int height){c->setState("gamePixelWidth",width);c->setState("gamePixelHeight",height);applyOptions("surface");});
    connect(m_mango.get(),&MangoClient::disconnectedFromGame,this,[this]{c->setState("gameRunning",false);});
    connect(m_mango.get(),&MangoClient::lostSamples,this,[this](quint64 count){c->setState("droppedFrameSamples",count);});
    c->setState("connected",true);linuxScan();
    m_scanTimer.setInterval(5000);connect(&m_scanTimer,&QTimer::timeout,this,&Platform::linuxScan);m_scanTimer.start();
    applyOptions();
#endif
    refresh();
#ifndef Q_OS_WIN
    if(c->m_options["autoCapture"].toBool()){m_monitoring=true;c->setState("externalCapture",c->m_options["linuxOverlayMode"].toInt()!=2);c->setState("capture",true);c->notify("Automatic capture is armed for monitored game launches. No game is launched automatically.");}
    if(c->m_options["linuxOverlayMode"].toInt()==2&&m_mango)m_mango->start(mangoPrefix(),m_monitoring&&gameLayerAllowed());
#endif
}
void Platform::send(const QJsonObject& input){
#ifdef Q_OS_WIN
    if(m_engine.state()!=QProcess::Running)return;
    auto request=input;request["token"]=m_token;m_engine.write(QJsonDocument(request).toJson(QJsonDocument::Compact)+'\n');
#else
    Q_UNUSED(input);
#endif
}
void Platform::refresh(){
    QVariantList list;
#ifdef Q_OS_WIN
    list<<QVariantMap{{"name","Windows monitoring engine"},{"ready",QFileInfo::exists(QCoreApplication::applicationDirPath()+"/FramoryxEngine.exe")},{"detail","PresentMon capture, borderless controls and optional DX11/DX12 hooks."}};
    list<<QVariantMap{{"name","Graphics API monitoring"},{"ready",true},{"detail","PresentMon ETW monitoring and an external overlay for supported DXGI, D3D9, Vulkan and OpenGL presentations. Exclusive fullscreen may require borderless mode."}};
    list<<QVariantMap{{"name","Optional Windows in-game HUD and limiter"},{"ready",true},{"detail","DX11/DX12 only. Manual approval is required; other rendering APIs use the external monitor and their own game limits."}};
    list<<QVariantMap{{"name","Audio processing"},{"ready",c->m_state["apoAvailable"].toBool()},{"detail","Extra gain, bass and mic effects use Equalizer APO device setup."}};
    send({{"action","refresh"}});
#else
    const bool wayland=qEnvironmentVariable("XDG_SESSION_TYPE")=="wayland";
    const bool compositing=wayland||x11::compositing();c->setState("compositing",compositing);
    list<<QVariantMap{{"name","External overlay transparency"},{"ready",compositing},{"detail",compositing?"Desktop compositing is available for a transparent external HUD.":"Desktop compositing is off. The external HUD uses an opaque surface; enable your desktop compositor for transparent backgrounds."}};
    list<<QVariantMap{{"name","Framoryx · Vulkan/OpenGL game runtime"},{"ready",!mangoExecutable().isEmpty()},{"detail","The bundled 64-bit MangoHud integration provides the custom HUD, live limits and local frame telemetry. Keep the complete package together. 32-bit in-game rendering is not bundled; use external monitoring for those games."}};
    list<<QVariantMap{{"name","Gamescope · fullscreen and pacing"},{"ready",!exe("gamescope").isEmpty()},{"detail","Optional game compositor for controlled fullscreen sizing and frame pacing."}};
    list<<QVariantMap{{"name","PipeWire · audio processing"},{"ready",!exe("pipewire").isEmpty()&&!exe("wpctl").isEmpty()},{"detail","Output gain, bass, microphone EQ and the Framoryx noise-suppressed input."}};
    list<<QVariantMap{{"name",wayland?"Wayland window controls":"X11 window controls"},{"ready",wayland?!exe("gamescope").isEmpty():!exe("wmctrl").isEmpty()},{"detail",wayland?"Use Gamescope when launching. Wayland restricts arbitrary changes to other windows.":"Window selection and reversible fullscreen changes through your window manager."}};
    c->setState("session",wayland?"Wayland":"X11");
    c->setState("setupCommand","Ubuntu/Debian: sudo apt install gamescope pipewire wireplumber wmctrl\nFedora: sudo dnf install gamescope pipewire wireplumber wmctrl\nArch: sudo pacman -S gamescope pipewire wireplumber wmctrl");
    linuxScan();
#endif
    c->m_capabilities=list;emit c->capabilitiesChanged();
}
void Platform::applyOptions(const QString& key){
    if(key=="volume"){if(!m_volumeTimer.isActive())m_volumeTimer.start();return;}
    m_pendingOptions.insert(key);
#ifdef Q_OS_WIN
    if(!m_optionTimer.isActive())m_optionTimer.start();
#else
    m_optionTimer.start(180);
#endif
}
void Platform::flushOptions(){
    m_optionTimer.stop();const auto keys=m_pendingOptions.values();m_pendingOptions.clear();const QString key=keys.size()==1?keys.first():QString();
    if(key=="volume"){if(!m_volumeTimer.isActive())m_volumeTimer.start();return;}
#ifdef Q_OS_WIN
    auto values=c->m_options;values["profileExe"]=c->m_activeGameKey;values["micDevice"]=QString::fromUtf8(QByteArray::fromBase64(c->m_options["micId"].toByteArray()));
    for(const auto& entry:c->m_devices)if(entry.toMap()["id"]==c->m_options["micId"])values["micName"]=entry.toMap()["name"];
    send({{"action","options"},{"key",key},{"keys",QJsonArray::fromStringList(keys)},{"options",QJsonObject::fromVariantMap(values)}});
#else
    if(m_mango){
        const bool blocked=denied(c->m_options,c->m_state["profileExe"].toString());const bool layer=gameLayerAllowed();
        if(layer){m_mango->start(mangoPrefix(),m_monitoring);if(m_mango->connected()){if(m_passive)m_passive->stop();c->setState("externalCapture",false);}else if(m_monitoring)attachPassive();}
        else {m_mango->setCollecting(false);c->setState("externalCapture",true);if(m_monitoring)attachPassive();}
        if(blocked){c->setState("live",false);c->setState("overlayHint","This game is blocked by your process rules. Its layer overlay, capture and frame limit are disabled.");}
        const bool visible=c->m_state["overlay"].toBool()&&layer;
        m_mango->configure(c->m_options,visible);
        m_mango->setLimit(layer?c->m_options["cap"].toInt():0,mangoFontPixels());
        if(!blocked&&m_mango->connected())c->setState("overlayHint","Overlay and frame-limit changes sent to the running game.");
    }
#endif
}
void Platform::select(const QString& id){
#ifdef Q_OS_WIN
    QJsonObject request{{"action","select"},{"id",id}};for(const auto& item:c->m_games)if(item.toMap()["id"]==id)request["exe"]=QFileInfo(item.toMap()["executable"].toString()).fileName();send(request);
#else
    if(m_passive)m_passive->stop();if(m_mango)m_mango->stop();c->m_state["gamePixelWidth"]=0;c->m_state["gamePixelHeight"]=0;c->m_livePid=0;c->m_store.clear();c->m_graph.clear();c->m_state["gameForeground"]=false;c->m_state["live"]=false;c->m_state["fps"]=0.;c->m_state["average"]=0.;c->m_state["low"]=0.;c->setState("selectedWindow",id);
    for(const auto& value:c->m_games){const auto game=value.toMap();if(game["id"].toString()==id){c->m_state["profileExe"]=QFileInfo(game["executable"].toString()).fileName();c->m_state["profilePid"]=0;c->setState("game",game["name"]);if(c->m_options["linuxOverlayMode"].toInt()==2){if(m_mango)m_mango->start(mangoPrefix(),m_monitoring&&gameLayerAllowed());}else if(m_monitoring)attachPassive();return;}}
    for(const auto& value:c->m_state["windows"].toList()){const auto window=value.toMap();if(window["id"].toString()==id)c->setState("game",window["name"]);}if(m_monitoring)attachPassive();
#endif
}
void Platform::poll(){
#ifndef Q_OS_WIN
    if(m_mango&&m_mango->connected())m_mango->metrics(c->m_state);
    if(c->m_state["externalCapture"].toBool()){
        if(m_monitoring&&m_passive&&m_passive->running()){
        const auto times=m_passive->takeFrames();if(!times.isEmpty())c->sampleFrames(times,m_passiveName,m_passivePid);
        c->m_state["gameForeground"]=m_passive->foreground()==m_passive->window();
        }
    }
    QFile stat("/proc/stat");if(stat.open(QIODevice::ReadOnly)){const auto parts=stat.readLine().simplified().split(' ');quint64 total=0,idle=0;for(int i=1;i<parts.size()&&i<=8;++i){const quint64 n=parts[i].toULongLong();total+=n;if(i==4||i==5)idle+=n;}if(m_oldCpuTotal&&total>m_oldCpuTotal)c->m_state["cpu"]=qBound(0.,100.*(1.-double(idle-m_oldCpuIdle)/double(total-m_oldCpuTotal)),100.);m_oldCpuTotal=total;m_oldCpuIdle=idle;}
    QFile memory("/proc/meminfo");if(memory.open(QIODevice::ReadOnly)){quint64 total=0,available=0;for(const auto& bytes:memory.read(1024*1024).split('\n')){const auto line=bytes.simplified().split(' ');if(line.size()<2)continue;if(line[0]=="MemTotal:")total=line[1].toULongLong();if(line[0]=="MemAvailable:")available=line[1].toULongLong();}if(total){c->m_state["ram"]=double(total-std::min(total,available))/1048576.;c->m_state["totalRam"]=double(total)/1048576.;}}
    emit c->stateChanged();
#endif
}
void Platform::linuxScan(){
#ifndef Q_OS_WIN
    if(m_refreshing)return;m_refreshing=true;
    if(!exe("wmctrl").isEmpty()&&qEnvironmentVariable("XDG_SESSION_TYPE")!="wayland")command(exe("wmctrl"),{"-lpGx"},[this](int code,QByteArray output){
        if(code)return;QVariantList windows;
        for(const auto& line:QString::fromUtf8(output).split('\n')){const auto cells=line.split(QRegularExpression("\\s+"),Qt::SkipEmptyParts);if(cells.size()<10)continue;const auto pid=cells[2].toUInt();QFile comm(QString("/proc/%1/comm").arg(pid));QString process=QFileInfo(QFileInfo(QString("/proc/%1/exe").arg(pid)).symLinkTarget()).fileName();if(process.isEmpty()||process.startsWith("wine"))if(comm.open(QIODevice::ReadOnly))process=QString::fromUtf8(comm.read(256)).trimmed();if(denied(c->m_options,process)||pid==QCoreApplication::applicationPid())continue;
            windows<<QVariantMap{{"id",cells[0]},{"pid",pid},{"name",cells.mid(9).join(' ')},{"exe",process},{"x",cells[3].toInt()},{"y",cells[4].toInt()},{"width",cells[5].toInt()},{"height",cells[6].toInt()}};}
        c->setState("windows",windows);
        if(m_monitoring&&(c->m_options["linuxOverlayMode"].toInt()!=2||!m_mango||!m_mango->connected()))attachPassive();
        if(c->m_options["autoBorderless"].toBool()){const auto target=selectedWindow(c->m_options,c->m_games,windows,m_passive?m_passive->foreground():0);if(!target.isEmpty()&&allowed(c->m_options,target["exe"].toString())&&!m_windowRestores.contains(target["id"].toString()))borderless(false);}
    });
    if(!exe("wpctl").isEmpty()&&!m_volumeWriting&&!m_volumeTimer.isActive())command(exe("wpctl"),{"get-volume","@DEFAULT_AUDIO_SINK@"},[this](int code,QByteArray output){if(code)return;const auto parts=output.simplified().split(' ');if(parts.size()>1){const int value=qBound(0,qRound(parts[1].toDouble()*100),200);if(!m_volumeWriting&&!m_volumePending&&!m_volumeTimer.isActive()&&c->m_clock.elapsed()>c->m_volumeUntil&&c->volume()!=value){c->m_state["volume"]=value;c->m_options["volume"]=value;emit c->volumeChanged();}c->m_state["muted"]=output.contains("MUTED");}});
    if(!exe("pw-dump").isEmpty())command(exe("pw-dump"),{},[this](int code,QByteArray output){
        m_refreshing=false;if(code)return;const auto array=QJsonDocument::fromJson(output).array();QVariantList nodes;
        for(const auto& item:array){const auto object=item.toObject();if(object["type"]!="PipeWire:Interface:Node")continue;const auto props=object["info"].toObject()["props"].toObject();const auto media=props["media.class"].toString();if(!media.startsWith("Audio/Source")&&!media.startsWith("Audio/Sink"))continue;nodes<<QVariantMap{{"id",object["id"].toInt()},{"name",props["node.name"].toString()},{"description",props["node.description"].toString()},{"media",media},{"rate",props["audio.rate"].toInt()}};}
        c->setState("audioNodes",nodes);
        bool micNode=false,outputNode=false,micDevice=false;
        for(const auto& value:nodes){const auto node=value.toMap();if(node["name"]=="framoryx_mic")micNode=true;if(node["name"]=="framoryx_output")outputNode=true;}
        for(const auto& device:QMediaDevices::audioInputs())if(device.description()=="Framoryx Microphone"||device.id()=="framoryx_mic")micDevice=true;
        const bool micReady=m_mic.state()==QProcess::Running&&micNode&&micDevice,outputReady=m_output.state()==QProcess::Running&&outputNode;
        c->setState("micConfigured",micReady);c->setState("bassConfigured",outputReady);
        if(micReady&&c->m_state["micApplying"].toBool()){c->setState("micApplying",false);c->setState("audioStatus","Framoryx Microphone is ready. Select it in your game's voice settings.");}
        if(outputReady&&c->m_state["bassApplying"].toBool()){c->setState("bassApplying",false);c->setState("audioStatus","Framoryx Output is ready. Choose it as playback output to hear bass enhancement.");}
    });else m_refreshing=false;
#endif
}
QString Platform::mangoPrefix()const{return "framoryx-"+QString::fromLatin1(QCryptographicHash::hash((c->root()+"/"+c->m_activeGameKey).toUtf8(),QCryptographicHash::Sha256).toHex().left(24))+"-";}
bool Platform::gameLayerAllowed()const{
    const auto name=c->m_state["profileExe"].toString();
    return c->m_options["linuxOverlayMode"].toInt()==2&&c->m_state["antiCheatStatus"]!="detected"&&!denied(c->m_options,name.isEmpty()?c->m_activeGameKey:name);
}
int Platform::mangoFontPixels()const{
    const auto& options=c->m_options;const auto screen=QGuiApplication::primaryScreen();
    const double width=c->m_state["gamePixelWidth"].toInt()>0?c->m_state["gamePixelWidth"].toInt():screen?screen->size().width()*screen->devicePixelRatio():1920;
    const double height=c->m_state["gamePixelHeight"].toInt()>0?c->m_state["gamePixelHeight"].toInt():screen?screen->size().height()*screen->devicePixelRatio():1080;
    const double requested=(options["overlayScale"].toInt()?options["overlayScale"].toInt()/100.:qBound(.85,height/1080.,2.5))*options["fontScale"].toInt()/100.;
    const auto layout=c->overlayLayout();const double scale=std::max(.1,std::min({requested,(width-30)/std::max(1.,layout["width"].toDouble()),(height-30)/std::max(1.,layout["height"].toDouble())}));
    return qBound(12,qRound(34*scale),200);
}
QString Platform::mangoConfiguration()const{
    const bool layer=gameLayerAllowed();
    return "legacy_layout=0,no_display=0,cpu_stats=0,gpu_stats=0,frame_timing=0,fps=0,fps_limit_method=late,font_size="+QString::number(mangoFontPixels())+",fps_limit="+QString::number(layer?c->m_options["cap"].toInt():0)+",control="+mangoPrefix()+"%p";
}
QString Platform::mangoExecutable()const{
    const QString bundled=QCoreApplication::applicationDirPath()+"/runtime/mangohud/bin/mangohud";
    const QDir runtime(QCoreApplication::applicationDirPath()+"/runtime/mangohud");
    if(QFileInfo(bundled).isExecutable()&&runtime.exists("lib/libMangoHud.so")&&runtime.exists("lib/libMangoHud_shim.so")&&runtime.exists("lib/libMangoHud_opengl.so")&&runtime.exists("layers/Framoryx.json"))return bundled;return {};
}
QString Platform::launchCommand()const{
#ifdef Q_OS_WIN
    return {};
#else
    if(c->m_options["linuxOverlayMode"].toInt()==2&&(c->m_activeGameKey.isEmpty()||mangoExecutable().isEmpty()))return {};
    if(!gameLayerAllowed())return (c->m_options["gamescope"].toBool()?"gamescope -b -r "+QString::number(c->m_options["cap"].toInt())+" -- ":QString())+"%command%";
    return "env FRAMORYX_HUD=1 MANGOHUD_CONFIG="+shellQuote(mangoConfiguration())+' '+shellQuote(mangoExecutable().isEmpty()?"mangohud":mangoExecutable())+" %command%";
#endif
}
bool Platform::attachPassive(bool explain){
#ifndef Q_OS_WIN
    if(!m_passive)return false;
    if(qEnvironmentVariable("XDG_SESSION_TYPE")=="wayland"){
        c->setState("overlayHint","Native Wayland restricts external overlay positioning and passive presentation capture. Use an X11 session, or explicitly select the MangoHud game layer where permitted.");if(explain)c->notify(c->m_state["overlayHint"].toString(),"warning");return false;
    }
    const auto target=selectedWindow(c->m_options,c->m_games,c->m_state["windows"].toList(),m_passive->foreground());
    if(target.isEmpty()){if(m_passive->running())m_passive->stop();c->setState("live",false);c->setState("gameForeground",false);if(explain)c->notify("Capture is armed. Select a running game window, or add your game to the allow list for automatic capture.");return false;}
    const quint32 id=target["id"].toString().toUInt(nullptr,16);
    if(m_passive->running()&&m_passive->window()==id)return true;
    QString error;if(!m_passive->start(id,target["pid"].toUInt(),&error)){if(explain)c->notify(error,"warning");c->setState("overlayHint",error);return false;}
    m_passiveName=target["exe"].toString();m_passivePid=target["pid"].toUInt();c->m_store.clear();c->m_frameTime=0;
    c->activateGameProfile(m_passiveName);
    c->m_state["profileExe"]=m_passiveName;c->m_state["profilePid"]=m_passivePid;c->m_state["externalCapture"]=true;c->m_state["renderer"]="X Present · external";
    const double dpr=QGuiApplication::primaryScreen()?QGuiApplication::primaryScreen()->devicePixelRatio():1.;
    auto screen=QGuiApplication::screenAt(QPoint(int(target["x"].toInt()/dpr),int(target["y"].toInt()/dpr)));if(!screen)screen=QGuiApplication::primaryScreen();
    if(screen){const auto geometry=screen->geometry();c->m_state["overlayDisplay"]=QVariantMap{{"x",geometry.x()},{"y",geometry.y()},{"width",geometry.width()},{"height",geometry.height()},{"dpr",screen->devicePixelRatio()}};}
    c->setState("overlayHint","External capture uses X Present events without loading code into the game. If this game's renderer does not expose events, FPS stays unavailable; an in-game layer is never enabled automatically.");return true;
#else
    Q_UNUSED(explain);return false;
#endif
}
void Platform::linuxLaunch(){
#ifndef Q_OS_WIN
    if(denied(c->m_options,c->m_state["profileExe"].toString())){c->notify("This game is blocked by your process rules.","warning");return;}
    if(m_mango&&m_mango->connected()){m_monitoring=true;m_mango->setCollecting(gameLayerAllowed());if(!gameLayerAllowed())attachPassive();c->setState("capture",true);c->notify("Your selected game is already running. Monitoring resumed without launching a duplicate.");return;}
    if(m_launchedPid&&m_lastLaunchId==c->m_options["selectedGame"].toString()&&!m_launchStart.isEmpty()&&processStart(m_launchedPid)==m_launchStart){m_monitoring=true;if(gameLayerAllowed()&&m_mango)m_mango->start(mangoPrefix());else attachPassive();c->setState("capture",true);c->notify("Monitoring resumed for the running game.");return;}
    QVariantMap game;for(const auto& value:c->m_games)if(value.toMap()["id"]==c->m_options["selectedGame"])game=value.toMap();
    const QFileInfo file(game["executable"].toString());if(game.isEmpty()||!file.isFile()||!file.isExecutable()){c->notify("Add a game executable to your library, then select it.","warning");return;}
    if(denied(c->m_options,file.fileName())){c->notify("This executable is blocked by your process rules.","warning");return;}
    const bool inProcess=gameLayerAllowed();
    if(inProcess&&c->m_options["gamescope"].toBool()){c->notify("Choose either the Framoryx game layer or Gamescope for this launch. Their combined rendering path is not supported yet.","warning");return;}
    if(inProcess){QFile binary(file.absoluteFilePath());if(binary.open(QIODevice::ReadOnly)&&binary.read(5)==QByteArray("\x7f" "ELF\x01",5)){c->notify("The bundled game layer supports 64-bit games. Use external monitoring for this 32-bit executable.","warning");return;}}
    if(inProcess&&mangoExecutable().isEmpty()){c->notify("The Framoryx game runtime is missing. Extract the complete package; an unmodified system MangoHud does not provide Framoryx controls.","warning");return;}
    if(c->m_options["gamescope"].toBool()&&exe("gamescope").isEmpty()){c->notify("Gamescope is unavailable. Disable its launch option or install it in Setup.","warning");return;}
    if(inProcess&&c->m_safetyBusy){const auto id=c->m_options["selectedGame"].toString();if(m_deferredLaunch!=id){m_deferredLaunch=id;m_deferredAt=c->m_clock.elapsed();c->notify("Checking compatibility before starting the selected game layer.");}c->setState("gameLaunchPending",true);m_launchTimer.start();return;}
    m_launchTimer.stop();m_deferredLaunch.clear();c->setState("gameLaunchPending",false);
    applyOptions();flushOptions();c->m_store.clear();c->m_frameTime=0;
    QProcessEnvironment env=hostEnvironment();env.insert("MANGOHUD_CONFIG",mangoConfiguration());env.insert("MANGOHUD_DLSYM","1");if(inProcess)env.insert("FRAMORYX_HUD","1");m_game.setProcessEnvironment(env);
    QStringList args;if(inProcess)args<<"--dlsym";if(c->m_options["gamescope"].toBool())args<<exe("gamescope")<<"-b"<<"-r"<<QString::number(c->m_options["cap"].toInt())<<"--";
    args<<file.absoluteFilePath();args+=unlock::arguments(QProcess::splitCommand(game["arguments"].toString()),game["unlock"].toBool()&&unlock::supported(file.absoluteFilePath()));
    m_game.setWorkingDirectory(file.absolutePath());QString program;if(inProcess)program=mangoExecutable();else {program=args.takeFirst();if(program.isEmpty())program=file.absoluteFilePath();}
    m_game.setProgram(program);m_game.setArguments(args);m_game.setStandardOutputFile(QProcess::nullDevice());m_game.setStandardErrorFile(QProcess::nullDevice());if(!m_game.startDetached(&m_launchedPid)){c->notify("The game could not start. Check its executable and launch arguments.","error");return;}m_lastLaunchId=c->m_options["selectedGame"].toString();m_launchStart=processStart(m_launchedPid);m_monitoring=true;c->m_state["profilePid"]=quint32(m_launchedPid);c->m_state["profileExe"]=file.fileName();c->m_state["profilePath"]=file.absoluteFilePath();c->m_state["gameRunning"]=true;c->m_state["capture"]=true;c->m_state["game"]=game["name"];c->m_state["externalCapture"]=!inProcess;c->m_state["renderer"]=inProcess?"MangoHud · Vulkan / OpenGL":"Waiting for X Present";c->m_state["launchTime"]=QDateTime::currentMSecsSinceEpoch();
    c->notify(inProcess?"Launching with your explicitly enabled MangoHud layer and frame limit.":"Game launched. External capture is armed; no monitoring module was added to the game.");
    if(file.absolutePath()==QCoreApplication::applicationDirPath()&&QStringList{"FramoryxScene","FramoryxVulkanScene","FramoryxPresentScene"}.contains(file.fileName())&&m_launchedPid&&!m_launchStart.isEmpty())m_fixtures[m_launchedPid]=m_launchStart;
    if(inProcess&&m_mango)m_mango->start(mangoPrefix());
#endif
}
void Platform::borderless(bool restore){
#ifndef Q_OS_WIN
    if(qEnvironmentVariable("XDG_SESSION_TYPE")=="wayland"){c->notify("For Wayland, enable Gamescope in the game launch options to control fullscreen sizing.");return;}
    const auto window=selectedWindow(c->m_options,c->m_games,c->m_state["windows"].toList(),m_passive?m_passive->foreground():0);const auto id=window["id"].toString();
    if(window.isEmpty()||!id.startsWith("0x")||denied(c->m_options,window["exe"].toString())){c->notify("Select an allowed running game window first.","warning");return;}
    if(restore){if(!m_windowRestores.contains(id)){c->notify("This window has no saved Framoryx state.");return;}const auto original=m_windowRestores[id];const bool restored=x11::fullscreen(original,original.fullscreen);m_windowRestores.remove(id);c->setState("borderlessActive",false);c->notify(restored?"Original window mode restored.":"The original window identity changed or restoration was refused. Refresh your game selection.",restored?"info":"warning");}
    else {const auto current=x11::inspect(id.toUInt(nullptr,16));if(!current.valid||current.pid!=window["pid"].toUInt()){c->notify("The game window identity changed. Refresh before applying borderless mode.","warning");return;}const bool applied=x11::fullscreen(current,true);if(applied){if(!m_windowRestores.contains(id))m_windowRestores[id]=current;c->setState("borderlessActive",true);}c->notify(applied?"Borderless fullscreen requested. Restore returns the original mode.":"The window manager refused fullscreen.",applied?"info":"warning");}
#else
    send({{"action",restore?"restore":"borderless"}});
#endif
}
void Platform::linuxAudio(bool microphone,bool verified){
#ifndef Q_OS_WIN
    if(exe("pipewire").isEmpty()){c->notify("PipeWire is required for audio effects. Open Setup for installation guidance.","warning");return;}
    const auto& o=c->m_options;const QString plugin=c->root()+"/rnnoise/librnnoise_ladspa.so";
    if(microphone&&o["noise"].toBool()&&!verified){c->setState("pendingMic",true);updateNoise(false);c->notify("Preparing RNNoise. Microphone effects will apply when the verified plugin is ready.");return;}
    QJsonArray nodes,links;QString previous,first;
    auto add=[&](QString name,QString label,QJsonObject controls,QString type="builtin"){
        QJsonObject node{{"type",type},{"name",name},{"label",label},{"control",controls}};if(type=="ladspa")node["plugin"]=plugin;
        const QString input=name+(type=="ladspa"?":Input":":In"),output=name+(type=="ladspa"?":Output":":Out");
        nodes.append(node);if(first.isEmpty())first=input;if(!previous.isEmpty())links.append(QJsonObject{{"output",previous},{"input",input}});previous=output;
    };
    if(microphone){
        if(o["noise"].toBool())add("denoise","noise_suppressor_mono",{{"VAD Threshold (%)",QList<int>{50,85,95}.value(o["noisePreset"].toInt(),85)},{"VAD Grace Period (ms)",200},{"Retroactive VAD Grace (ms)",0}},"ladspa");
        if(o["eq"].toBool()){
            add("low","bq_lowshelf",{{"Freq",150},{"Q",.707},{"Gain",o["low"].toInt()}});add("mid","bq_peaking",{{"Freq",1200},{"Q",.9},{"Gain",o["mid"].toInt()}});add("high","bq_highshelf",{{"Freq",6000},{"Q",.707},{"Gain",o["high"].toInt()}});
        }
    }else add("bass","bq_lowshelf",{{"Freq",90},{"Q",.707},{"Gain",o["bass"].toInt()}});
    if(nodes.isEmpty()){c->notify("Choose a microphone effect before applying.");return;}
    const int headroom=microphone?(o["eq"].toBool()?std::max({0,o["low"].toInt(),o["mid"].toInt(),o["high"].toInt()}):0):o["bass"].toInt();
    QJsonObject gain{{"type","builtin"},{"name","headroom"},{"label","mixer"},{"control",QJsonObject{{"Gain 1",std::pow(10.,-headroom/20.)}}}};nodes.append(gain);links.append(QJsonObject{{"output",previous},{"input","headroom:In 1"}});previous="headroom:Out";
    QJsonObject capture{{"node.name",microphone?"capture.framoryx_mic":"framoryx_output"},{"node.description",microphone?"Framoryx microphone input":"Framoryx Output"},{"audio.position",QJsonArray{microphone?"MONO":"FL",microphone?"MONO":"FR"}}};
    QJsonObject playback{{"node.name",microphone?"framoryx_mic":"playback.framoryx_output"},{"node.description",microphone?"Framoryx Microphone":"Framoryx output playback"}};
    if(microphone){capture["node.passive"]=true;capture["audio.rate"]=48000;capture["audio.channels"]=1;capture["audio.position"]=QJsonArray{"MONO"};playback["media.class"]="Audio/Source";playback["audio.rate"]=48000;playback["audio.channels"]=1;playback["audio.position"]=QJsonArray{"MONO"};
        const QString chosen=QString::fromUtf8(QByteArray::fromBase64(o["micId"].toByteArray()));QString target;QStringList descriptions;
        for(const auto& value:c->m_state["audioNodes"].toList()){const auto entry=value.toMap();if(!entry["media"].toString().startsWith("Audio/Source")||entry["name"]=="framoryx_mic")continue;if(entry["name"]==chosen)target=entry["name"].toString();if(!c->m_state["selectedMicName"].toString().isEmpty()&&entry["description"]==c->m_state["selectedMicName"])descriptions<<entry["name"].toString();}
        if(target.isEmpty()&&descriptions.size()==1)target=descriptions.first();
        if(target.isEmpty()){c->notify("The selected physical microphone is unavailable. Refresh devices and select its input before applying effects.","warning");return;}capture["target.object"]=target;capture["node.target"]=target;capture["node.dont-fallback"]=true;capture["node.dont-reconnect"]=true;playback["priority.session"]=0;
    }else{
        capture["media.class"]="Audio/Sink";playback["node.passive"]=true;
        const auto output=QMediaDevices::defaultAudioOutput();const auto chosen=QString::fromUtf8(output.id());QString target;
        for(const auto& value:c->m_state["audioNodes"].toList()){const auto entry=value.toMap();if(!entry["media"].toString().startsWith("Audio/Sink")||entry["name"]=="framoryx_output")continue;if(entry["name"]==chosen||entry["description"]==output.description())target=entry["name"].toString();if(target.isEmpty()&&entry["name"]==m_outputTarget)target=m_outputTarget;}
        if(target.isEmpty()){c->notify("A physical output could not be identified. Refresh audio devices before applying bass.","warning");return;}m_outputTarget=target;playback["target.object"]=target;playback["node.target"]=target;playback["node.dont-fallback"]=true;playback["node.dont-reconnect"]=true;capture["priority.session"]=0;
    }
    // PipeWire 0.3.48 resolves links while parsing, so nodes must precede links.
    // QJsonObject sorts keys alphabetically. A JSON string preserves graph order;
    // PipeWire's properties parser decodes it before the graph parser consumes it.
    const QByteArray graph="{\"nodes\":"+QJsonDocument(nodes).toJson(QJsonDocument::Compact)+",\"links\":"+QJsonDocument(links).toJson(QJsonDocument::Compact)+",\"inputs\":"+QJsonDocument(QJsonArray{first}).toJson(QJsonDocument::Compact)+",\"outputs\":"+QJsonDocument(QJsonArray{previous}).toJson(QJsonDocument::Compact)+'}';
    const QJsonObject args{{"node.description",microphone?"Framoryx Microphone":"Framoryx Output"},{"media.name",microphone?"Framoryx Microphone":"Framoryx Output"},{"audio.rate",48000},{"audio.channels",microphone?1:2},{"audio.position",microphone?QJsonArray{"MONO"}:QJsonArray{"FL","FR"}},{"filter.graph",QString::fromUtf8(graph)},{"capture.props",capture},{"playback.props",playback}};
    QJsonArray modules{QJsonObject{{"name","libpipewire-module-rt"},{"flags",QJsonArray{"ifexists","nofail"}}},QJsonObject{{"name","libpipewire-module-protocol-native"}},QJsonObject{{"name","libpipewire-module-client-node"}},QJsonObject{{"name","libpipewire-module-adapter"}},QJsonObject{{"name","libpipewire-module-filter-chain"},{"args",args}}};
    QJsonObject config{{"context.properties",QJsonObject{{"log.level",qEnvironmentVariable("GITHUB_ACTIONS")=="true"?3:1}}},{"context.spa-libs",QJsonObject{{"audio.convert.*","audioconvert/libspa-audioconvert"},{"support.*","support/libspa-support"}}},{"context.modules",modules}};
    const QString path=c->root()+(microphone?"/microphone.conf":"/output.conf");if(!writeFile(path,QJsonDocument(config).toJson())){c->notify("Audio configuration could not be saved.","error");return;}
    c->setState(microphone?"micApplying":"bassApplying",true);auto process=microphone?&m_mic:&m_output;m_pendingAudio[process]=path;++m_audioGeneration[process];
    if(process->state()==QProcess::NotRunning)startAudio(process);
    else {const auto oldPid=process->processId();process->terminate();QTimer::singleShot(800,this,[this,process,oldPid]{if(!m_closing&&process->state()!=QProcess::NotRunning&&process->processId()==oldPid)process->kill();});}

#endif
}
void Platform::startAudio(QProcess* process){
#ifndef Q_OS_WIN
    if(m_closing||!m_pendingAudio.contains(process))return;
    const auto path=m_pendingAudio.take(process);const auto generation=m_audioGeneration[process];const bool microphone=process==&m_mic;m_expectedStops.remove(process);
    process->setProcessEnvironment(hostEnvironment());process->start(exe("pipewire"),{"-c",path});
    QTimer::singleShot(1200,this,[this,process,generation,microphone]{if(generation!=m_audioGeneration[process]||m_closing)return;if(process->state()!=QProcess::Running){c->setState(microphone?"micApplying":"bassApplying",false);c->setState("audioStatus","The audio filter could not start. Open Settings for its diagnostic.");}linuxScan();});
    QTimer::singleShot(10000,this,[this,process,generation,microphone]{if(generation!=m_audioGeneration[process]||m_closing)return;if(c->m_state[microphone?"micApplying":"bassApplying"].toBool()){c->setState(microphone?"micApplying":"bassApplying",false);c->setState("audioStatus","The audio process started, but its virtual device is unavailable. Refresh audio and check your PipeWire session.");}});
#else
    Q_UNUSED(process);
#endif
}
void Platform::updateNoise(bool force){
#ifdef Q_OS_WIN
    send({{"action","noiseUpdate"},{"force",force}});
#else
    if(c->m_state["noiseBusy"].toBool())return;
    c->setState("noiseBusy",true);c->setState("noiseStatus","Verifying the installed RNNoise plugin…");
    auto watcher=new QFutureWatcher<QVariantMap>(this);const auto directory=c->root()+"/rnnoise";
    connect(watcher,&QFutureWatcher<QVariantMap>::finished,this,[this,watcher,force]{const auto cached=watcher->result();watcher->deleteLater();
        const auto age=QDateTime::currentSecsSinceEpoch()-cached["checkedAt"].toLongLong();
        if(cached["ok"].toBool()&&!force&&(!c->m_options["autoNoise"].toBool()||(age>=0&&age<6*60*60))){c->setState("noiseBusy",false);c->setState("noiseStatus","RNNoise "+cached["version"].toString()+" ready · verified official Linux plugin");if(c->m_state["pendingMic"].toBool()){c->setState("pendingMic",false);linuxAudio(true,true);}return;}
        beginNoiseDownload(cached);
    });watcher->setFuture(QtConcurrent::run(verifiedNoiseCache,directory));
#endif
}
void Platform::beginNoiseDownload(const QVariantMap& cached){
#ifndef Q_OS_WIN
    const QString base=c->root()+"/rnnoise";
    auto fail=[this,cached](const QString& text){if(m_closing)return;c->setState("noiseBusy",false);c->setState("noiseStatus",cached["ok"].toBool()?text+" Verified cached "+cached["version"].toString()+" remains ready.":text);if(cached["ok"].toBool()&&c->m_state["pendingMic"].toBool()){c->setState("pendingMic",false);linuxAudio(true,true);}else c->setState("pendingMic",false);};
    c->setState("noiseStatus","Checking the latest official Linux RNNoise release…");
    auto fetch=std::make_shared<std::function<void(QUrl,int,qint64,std::function<void(QByteArray)>)>>();
    const std::weak_ptr<std::function<void(QUrl,int,qint64,std::function<void(QByteArray)>)>> weakFetch=fetch;
    *fetch=[this,weakFetch,fail](QUrl url,int redirects,qint64 limit,std::function<void(QByteArray)> done){
        const auto fetch=weakFetch.lock();if(!fetch)return;
        const QStringList hosts{"api.github.com","github.com","release-assets.githubusercontent.com","objects.githubusercontent.com"};
        if(url.scheme()!="https"||!hosts.contains(url.host())||!url.userInfo().isEmpty()||redirects>5){fail("RNNoise update refused an unexpected download location.");return;}
        QNetworkRequest request(url);request.setHeader(QNetworkRequest::UserAgentHeader,"Framoryx/0.3");request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);request.setTransferTimeout(20000);
        auto reply=m_network.get(request);auto data=std::make_shared<QByteArray>();
        connect(reply,&QIODevice::readyRead,this,[reply,data,limit]{*data+=reply->readAll();if(data->size()>limit)reply->abort();});
        connect(reply,&QNetworkReply::finished,this,[this,reply,data,limit,redirects,done,fetch,fail]{
            *data+=reply->readAll();const auto redirect=reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();const int code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();const auto destination=reply->url().resolved(redirect);const auto error=reply->error();reply->deleteLater();
            if(code>=300&&code<400&&!redirect.isEmpty()){(*fetch)(destination,redirects+1,limit,done);return;}
            if(error!=QNetworkReply::NoError||code!=200||data->size()>limit){fail("RNNoise update could not finish. Check your connection.");return;}done(*data);
        });
    };
    (*fetch)(QUrl("https://api.github.com/repos/werman/noise-suppression-for-voice/releases/latest"),0,2*1024*1024,[this,fetch,base,cached,fail](QByteArray bytes){
        const auto release=QJsonDocument::fromJson(bytes).object();const auto version=release["tag_name"].toString();QJsonObject asset;
        for(const auto& value:release["assets"].toArray())if(value.toObject()["name"]=="linux-rnnoise.zip")asset=value.toObject();
        QString digest=asset["digest"].toString();if(version=="v1.10")digest="sha256:811390b6eb6e28dde023c70590c74d26e74ebb2e595bcf4b95af2341db160e99";
        if(asset.isEmpty()||!QRegularExpression("^sha256:[0-9a-f]{64}$").match(digest).hasMatch()||release["prerelease"].toBool()||release["draft"].toBool()){fail("The latest release could not be verified.");return;}
        if(cached["ok"].toBool()&&cached["version"]==version&&cached["archiveSha256"]==digest.mid(7)){auto updated=cached;updated["checkedAt"]=QDateTime::currentSecsSinceEpoch();writeFile(base+"/verified.json",json(updated));c->setState("noiseBusy",false);c->setState("noiseStatus","RNNoise "+version+" is current · verified official plugin");if(c->m_state["pendingMic"].toBool()){c->setState("pendingMic",false);linuxAudio(true,true);}return;}
        c->setState("noiseStatus","Downloading the latest verified Linux RNNoise plugin…");
        (*fetch)(QUrl(asset["browser_download_url"].toString()),0,64*1024*1024,[this,base,digest,version,fail](QByteArray archive){
            c->setState("noiseStatus","Preparing RNNoise in the background…");auto watcher=new QFutureWatcher<QVariantMap>(this);
            connect(watcher,&QFutureWatcher<QVariantMap>::finished,this,[this,watcher,fail]{const auto result=watcher->result();watcher->deleteLater();if(!result["ok"].toBool()){fail("Plugin verification failed; the existing audio configuration was kept.");return;}c->setState("noiseBusy",false);c->setState("noiseStatus","RNNoise "+result["version"].toString()+" ready · verified official Linux plugin");if(c->m_state["pendingMic"].toBool()){c->setState("pendingMic",false);linuxAudio(true,true);}});
            watcher->setFuture(QtConcurrent::run(installNoiseArchive,base,archive,digest.mid(7),version));
        });
    });
#else
    Q_UNUSED(cached);
#endif
}

void Platform::action(const QString& name,const QVariantMap& args){
    if(name=="telemetry"){
#ifdef Q_OS_WIN
        auto request=QJsonObject::fromVariantMap(args);request["action"]="telemetry";send(request);
#endif
        return;
    }
    if(name=="safetyFallback"){
#ifdef Q_OS_WIN
        send({{"action","safetyFallback"},{"enabled",args["enabled"].toBool()}});
#else
        const bool enabled=args["enabled"].toBool();c->setState("externalPreferred",enabled);
        if(enabled&&c->m_state["antiCheatStatus"]=="detected"){if(m_mango){m_mango->configure(c->m_options,false);m_mango->setLimit(0,mangoFontPixels());m_mango->setCollecting(false);}c->setOption("linuxOverlayMode",1);c->m_state["externalCapture"]=true;c->m_state["live"]=false;if(m_monitoring)attachPassive();c->setState("overlayHint","Known anti-cheat indicators were detected. The running layer has been disabled and external capture is preferred. Restart the game without the layer launch options to remove loaded code.");}
#endif
        return;
    }
    if(name=="copySteam"){if(c->m_options["linuxOverlayMode"].toInt()==2&&c->m_activeGameKey.isEmpty()){c->notify("Select your library game or its running window before copying its game-layer launch options.","warning");return;}const auto options=launchCommand();if(options.isEmpty()){c->notify("The bundled game runtime is unavailable. Extract the complete package before copying launch options.","warning");return;}QGuiApplication::clipboard()->setText(options);c->notify("Steam launch options copied. Paste them into the game's Properties → Launch Options.");return;}
    if(name=="noiseUpdate"){updateNoise(true);return;}
    if(name=="audioOpen"){if(c->m_options["autoNoise"].toBool())updateNoise(false);return;}
    if(name=="borderless"||name=="restore"){borderless(name=="restore");return;}
#ifdef Q_OS_WIN
    if(name=="launch"){for(const auto& entry:c->m_games){const auto game=entry.toMap();if(game["id"]!=c->m_options["selectedGame"])continue;const QFileInfo file(game["executable"].toString());if(!file.isFile()||denied(c->m_options,file.fileName())){c->notify("This game is unavailable or blocked by your process rules.","warning");return;}QProcess launch;launch.setProgram(file.absoluteFilePath());launch.setWorkingDirectory(file.absolutePath());launch.setArguments(unlock::arguments(QProcess::splitCommand(game["arguments"].toString()),game["unlock"].toBool()&&unlock::supported(file.absoluteFilePath())));if(!launch.startDetached())c->notify("The game could not be launched.","warning");else {c->notify("Game launched. Capture can follow its foreground window.");send({{"action","select"},{"id","auto"},{"exe",file.fileName()}});}return;}c->notify("Add and select a game in your library first.");return;}
    QJsonObject request=QJsonObject::fromVariantMap(args);if(name=="apoConfig")request["path"]=QUrl(args["url"].toString()).toLocalFile();request["action"]=name;send(request);
#else
    if(name=="capture"&&c->m_state["capture"].toBool()){m_monitoring=false;if(m_passive)m_passive->stop();if(m_mango)m_mango->setCollecting(false);c->setState("capture",false);c->setState("live",false);c->notify("Capture stopped. Your game and its in-game overlay keep running.");return;}
    if(name=="capture"){m_monitoring=true;const bool layer=gameLayerAllowed();c->setState("externalCapture",!layer);c->setState("capture",true);if(layer){if(m_mango)m_mango->start(mangoPrefix(),true);if(!m_mango||!m_mango->connected())attachPassive();c->notify("Capture armed. External capture is used when available while waiting for this game's enabled layer.");}else attachPassive(true);return;}
    if(name=="launch"){linuxLaunch();return;}
    if(name=="overlay"){c->setState("overlay",!c->m_state["overlay"].toBool());applyOptions("fields");c->notify(c->m_state["overlay"].toBool()?"Overlay enabled. Connected game layers update automatically.":"Overlay hidden. Connected game layers update automatically.");return;}
    if(name=="micApply"){linuxAudio(true);return;}
    if(name=="bassApply"){linuxAudio(false);return;}
    if(name=="micDisable"||name=="bassDisable"){auto process=name=="micDisable"?&m_mic:&m_output;m_pendingAudio.remove(process);++m_audioGeneration[process];m_expectedStops.insert(process);process->terminate();c->setState(name=="micDisable"?"micApplying":"bassApplying",false);c->setState(name=="micDisable"?"micConfigured":"bassConfigured",false);c->setState("pendingMic",false);c->notify("The selected Framoryx effects are disabled.");return;}
    if(name=="defaultMic"||name=="defaultOutput"){const QString target=name=="defaultMic"?"framoryx_mic":"framoryx_output";for(const auto& value:c->m_state["audioNodes"].toList()){const auto node=value.toMap();if(node["name"]==target){command(exe("wpctl"),{"set-default",node["id"].toString()},[this](int code,QByteArray){c->notify(code?"The default audio device could not be changed.":"Framoryx is now the default audio device.",code?"warning":"info");});return;}}c->notify("Apply this effect first, then choose its virtual device.");return;}
    if(name=="shortcut"){const QString folder=QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);if(folder.isEmpty()){c->notify("No desktop folder is configured. Framoryx is available in your app launcher.");return;}const QByteArray entry=lifecycle::desktopEntry(lifecycle::launchPath(),false);const QString path=folder+"/Framoryx.desktop";if(!entry.isEmpty()&&writeFile(path,entry)){QFile::setPermissions(path,QFile::permissions(path)|QFile::ExeOwner);c->notify("Desktop shortcut created. Your desktop may ask you to trust the launcher once.");}else c->notify("The desktop shortcut could not be created.","warning");return;}
    if(name=="fixtureClose"){stopFixtures();return;}
    if(name=="fixture"){const int mode=c->m_options["linuxOverlayMode"].toInt(),cap=c->m_options["cap"].toInt();const QString scene=QCoreApplication::applicationDirPath()+(mode==2?(args["api"]=="vulkan"?"/FramoryxVulkanScene":"/FramoryxScene"):"/FramoryxPresentScene");c->addGame(QUrl::fromLocalFile(scene),"Framoryx test scene","");c->setOption("linuxOverlayMode",mode);c->setOption("cap",cap);linuxLaunch();if(m_launchedPid&&!m_launchStart.isEmpty())m_fixtures[m_launchedPid]=m_launchStart;return;}
    if(name=="soundSettings"){QDesktopServices::openUrl(QUrl("https://github.com/NeTriXoN-BoT/Framoryx/blob/main/docs/AUDIO.md"));return;}
    c->notify("This control is unavailable in the current desktop session.","warning");
#endif
}
void Platform::stopFixtures(){
#ifndef Q_OS_WIN
    const auto folder=QCoreApplication::applicationDirPath();
    for(auto item=m_fixtures.begin();item!=m_fixtures.end();++item){if(item.value().isEmpty()||processStart(item.key())!=item.value())continue;const auto file=QFileInfo(QString("/proc/%1/exe").arg(item.key())).symLinkTarget();if(file==folder+"/FramoryxScene"||file==folder+"/FramoryxVulkanScene"||file==folder+"/FramoryxPresentScene")::kill(pid_t(item.key()),SIGTERM);}
    m_fixtures.clear();
#endif
}
void Platform::shutdown(){
    if(m_closing)return;if(m_volumeTimer.isActive())flushVolume();if(m_optionTimer.isActive())flushOptions();m_closing=true;c->setState("pendingMic",false);for(auto reply:m_network.findChildren<QNetworkReply*>()){reply->disconnect(this);reply->abort();}for(auto child:children())child->disconnect(this);m_scanTimer.stop();m_volumeTimer.stop();m_optionTimer.stop();m_launchTimer.stop();
#ifdef Q_OS_WIN
    send({{"action","quit"}});m_engine.closeWriteChannel();if(!m_engine.waitForFinished(2500)){m_engine.terminate();if(!m_engine.waitForFinished(500))m_engine.kill();}
#else
    for(const auto& original:m_windowRestores)x11::fullscreen(original,original.fullscreen);
    for(auto process:{&m_mic,&m_output}){process->terminate();if(!process->waitForFinished(1000)){process->kill();process->waitForFinished(250);}}
    if(m_passive)m_passive->stop();
    if(m_mango)m_mango->stop();
    stopFixtures();
    const auto commands=m_commands;for(auto process:commands){if(process->state()!=QProcess::NotRunning&&!process->waitForFinished(250)){process->kill();process->waitForFinished(100);}}
    // A user's game keeps running when the control application closes.
    m_game.disconnect(this);
#endif
}
