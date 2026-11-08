#include "platform.hpp"
#include "controller.hpp"
#include "unlock.hpp"
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
#ifndef Q_OS_WIN
#include "vendor/miniz/miniz.h"
#include <fcntl.h>
#include <unistd.h>
#include <linux/falloc.h>
#endif
namespace {
QString exe(const QString& name){return QStandardPaths::findExecutable(name);}
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
}
Platform::Platform(Controller* controller):QObject(controller),c(controller){
    m_optionTimer.setInterval(75);m_optionTimer.setSingleShot(true);connect(&m_optionTimer,&QTimer::timeout,this,&Platform::flushOptions);
    m_volumeTimer.setSingleShot(true);m_volumeTimer.setInterval(45);
    connect(&m_volumeTimer,&QTimer::timeout,this,[this]{
#ifdef Q_OS_WIN
        send({{"action","volume"},{"value",c->m_options["volume"].toInt()}});
#else
        if(exe("wpctl").isEmpty()){c->notify("PipeWire volume controls are unavailable. Open Setup for the required components.","warning");return;}
        const double volume=c->m_options["volume"].toInt()/100.;
        command(exe("wpctl"),{"set-volume","-l","2.0","@DEFAULT_AUDIO_SINK@",QString::number(volume,'f',2)},[this](int code,QByteArray){if(code)c->notify("The output volume could not be changed. Check your audio session.","warning");});
#endif
    });
    for(auto* process:{&m_mic,&m_output}){
        connect(process,&QProcess::readyReadStandardError,this,[this,process]{auto error=process->readAllStandardError();if(error.size()>4096)error=error.right(4096);c->setState(process==&m_mic?"micDiagnostic":"outputDiagnostic",QString::fromUtf8(error));});
        connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,process](int code,QProcess::ExitStatus){c->setState(process==&m_mic?"micConfigured":"bassConfigured",false);if(code&&!m_closing)c->notify("An audio filter stopped. Open Audio setup to inspect its status.","warning");});
    }
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
    process->setProgram(program);process->setArguments(args);process->start();
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
    connect(&m_engine,&QProcess::started,this,[this]{applyOptions();flushOptions();if(c->m_options["autoCapture"].toBool())send({{"action","captureStart"}});});
    connect(&m_engine,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int,QProcess::ExitStatus){c->setState("connected",false);if(!m_closing)c->notify("The Windows engine closed. Restart Framoryx to reconnect.","error");});
    m_engine.start(path,{"--engine-server",m_token,"--settings-file",c->root()+"/engine.ini"});
#else
    c->setState("connected",true);linuxScan();
    m_scanTimer.setInterval(5000);connect(&m_scanTimer,&QTimer::timeout,this,&Platform::linuxScan);m_scanTimer.start();
    connect(&m_game,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus status){c->setState("gameRunning",false);c->setState("capture",false);if(status==QProcess::CrashExit||code)c->notify("The game exited. Its launch diagnostics are available in Compatibility.","warning");});
    connect(&m_game,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){if(error==QProcess::FailedToStart)c->notify("The game could not start. Check its executable and launch arguments.","error");});
    connect(&m_game,&QProcess::readyReadStandardError,this,[this]{const auto text=QString::fromUtf8(m_game.readAllStandardError()).right(8192);c->setState("gameDiagnostic",text);});
    connect(&m_game,&QProcess::readyReadStandardOutput,this,[this]{m_game.readAllStandardOutput();});
    applyOptions();
#endif
    refresh();
#ifndef Q_OS_WIN
    if(c->m_options["autoCapture"].toBool()){m_monitoring=true;c->setState("capture",true);c->notify("Automatic capture is armed for monitored game launches. No game is launched automatically.");}
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
    list<<QVariantMap{{"name","Audio processing"},{"ready",c->m_state["apoAvailable"].toBool()},{"detail","Extra gain, bass and mic effects use Equalizer APO device setup."}};
    send({{"action","refresh"}});
#else
    const bool wayland=qEnvironmentVariable("XDG_SESSION_TYPE")=="wayland";
    list<<QVariantMap{{"name","MangoHud · FPS and in-game overlay"},{"ready",!exe("mangohud").isEmpty()},{"detail","Native Vulkan/OpenGL and compatible Proton games. Applied when you launch a game."}};
    list<<QVariantMap{{"name","Gamescope · fullscreen and pacing"},{"ready",!exe("gamescope").isEmpty()},{"detail","Optional game compositor for controlled fullscreen sizing and frame pacing."}};
    list<<QVariantMap{{"name","PipeWire · audio processing"},{"ready",!exe("pipewire").isEmpty()&&!exe("wpctl").isEmpty()},{"detail","Output gain, bass, microphone EQ and the Framoryx noise-suppressed input."}};
    list<<QVariantMap{{"name",wayland?"Wayland window controls":"X11 window controls"},{"ready",wayland?!exe("gamescope").isEmpty():!exe("wmctrl").isEmpty()},{"detail",wayland?"Use Gamescope when launching. Wayland restricts arbitrary changes to other windows.":"Window selection and reversible fullscreen changes through your window manager."}};
    c->setState("session",wayland?"Wayland":"X11");
    c->setState("setupCommand","Ubuntu/Debian: sudo apt install mangohud gamescope pipewire wireplumber wmctrl\nFedora: sudo dnf install mangohud gamescope pipewire wireplumber wmctrl\nArch: sudo pacman -S mangohud gamescope pipewire wireplumber wmctrl");
#endif
    c->m_capabilities=list;emit c->capabilitiesChanged();
}
void Platform::applyOptions(const QString& key){
    if(key=="volume"){if(!m_volumeTimer.isActive())m_volumeTimer.start();return;}
    m_pendingOptions.insert(key);if(!m_optionTimer.isActive())m_optionTimer.start();
}
void Platform::flushOptions(){
    m_optionTimer.stop();const auto keys=m_pendingOptions.values();m_pendingOptions.clear();const QString key=keys.size()==1?keys.first():QString();
    if(key=="volume"){if(!m_volumeTimer.isActive())m_volumeTimer.start();return;}
#ifdef Q_OS_WIN
    auto values=c->m_options;values["micDevice"]=QString::fromUtf8(QByteArray::fromBase64(c->m_options["micId"].toByteArray()));
    for(const auto& entry:c->m_devices)if(entry.toMap()["id"]==c->m_options["micId"])values["micName"]=entry.toMap()["name"];
    send({{"action","options"},{"key",key},{"keys",QJsonArray::fromStringList(keys)},{"options",QJsonObject::fromVariantMap(values)}});
#else
    QDir().mkpath(c->root()+"/logs");
    const auto& o=c->m_options;const int fields=o["fields"].toInt(),style=o["style"].toInt();
    const QStringList colors{"7df2c6","59cfff","c2a3ff","f7be6f","ff8baf","63dfdf","f4e478","a3e56d","ff9d72","d5e4f5"};
    const double scale=o["overlayScale"].toInt()?o["overlayScale"].toInt()/100.:qBound(.85,QGuiApplication::primaryScreen()?QGuiApplication::primaryScreen()->size().height()/1080.:1.,2.5);
    QByteArray config="legacy_layout=0\nbackground_color=0c131a\ntext_color=e9f2f6\nround_corners=12\n";
    config+="background_alpha="+QByteArray::number(o["opacity"].toInt()/100.,'f',2)+"\n";
    config+="font_size="+QByteArray::number(qRound((style==1?18:22)*scale*o["fontScale"].toInt()/100.))+"\n";
    config+="fps_color="+colors.value(o["accent"].toInt()).toLatin1()+"\nframetime_color="+colors.value(o["accent"].toInt()).toLatin1()+"\n";
    config+="fps_limit="+QByteArray::number(o["cap"].toInt())+"\nfps_limit_method=late\n";
    config+="position=top-left\noffset_x="+QByteArray::number(qRound(o["hudX"].toDouble()*std::max(1,QGuiApplication::primaryScreen()?QGuiApplication::primaryScreen()->size().width()-260:1000)))+"\noffset_y="+QByteArray::number(qRound(o["hudY"].toDouble()*std::max(1,QGuiApplication::primaryScreen()?QGuiApplication::primaryScreen()->size().height()-220:800)))+"\n";
    config+="autostart_log=1\nlog_interval=0\noutput_folder="+QFile::encodeName(c->root()+"/logs")+"\n";
    if(!c->m_state["overlay"].toBool()||!fields)config+="no_display\n";
    if(style==2)config+="horizontal\ntable_columns=10\n";
    if(style==3)config+="round_corners=0\nfont_size="+QByteArray::number(qRound(18*scale*o["fontScale"].toInt()/100.))+"\n";
    if(style==4)config+="table_columns=4\nwidth=390\n";
    const QStringList values{"fps","fps_metrics=avg","fps_metrics=0.01","frametime","frame_timing","cpu_stats","ram","show_fps_limit","engine_version","custom_text=FRAMORYX"};
    bool metricsWritten=false;
    for(const auto& index:o["order"].toList()){const int i=index.toInt();if(!(fields&(1<<i)))continue;if(i==1||i==2){if(!metricsWritten){QStringList metrics;if(fields&2)metrics<<"avg";if(fields&4)metrics<<"0.01";config+="fps_metrics="+metrics.join(',').toUtf8()+'\n';metricsWritten=true;}}else config+=values.value(i).toUtf8()+'\n';}
    if(!writeFile(c->root()+"/MangoHud.conf",config))c->notify("The overlay configuration could not be written.","warning");
    if(key.isEmpty()||key=="cap"||key=="style"||key=="fields"||key=="opacity"||key=="overlayScale"||key=="accent"||key=="order"||key=="hudX"||key=="hudY"||key=="fontScale"){
        if(c->m_state["gameRunning"].toBool())c->setState("overlayHint","Saved. Use MangoHud's reload shortcut (Shift+F4) in the game, or relaunch for all changes.");
    }
#endif
}
void Platform::select(const QString& id){
#ifdef Q_OS_WIN
    send({{"action","select"},{"id",id}});
#else
    c->setState("selectedWindow",id);
    for(const auto& value:c->m_games){const auto game=value.toMap();if(game["id"].toString()==id){c->setState("game",game["name"]);return;}}
    for(const auto& value:c->m_state["windows"].toList()){const auto window=value.toMap();if(window["id"].toString()==id)c->setState("game",window["name"]);}
#endif
}
void Platform::poll(){
#ifndef Q_OS_WIN
    readLog();
    QFile stat("/proc/stat");if(stat.open(QIODevice::ReadOnly)){const auto parts=stat.readLine().simplified().split(' ');quint64 total=0,idle=0;for(int i=1;i<parts.size()&&i<=8;++i){const quint64 n=parts[i].toULongLong();total+=n;if(i==4||i==5)idle+=n;}if(m_oldCpuTotal&&total>m_oldCpuTotal)c->m_state["cpu"]=qBound(0.,100.*(1.-double(idle-m_oldCpuIdle)/double(total-m_oldCpuTotal)),100.);m_oldCpuTotal=total;m_oldCpuIdle=idle;}
    QFile memory("/proc/meminfo");if(memory.open(QIODevice::ReadOnly)){quint64 total=0,available=0;while(!memory.atEnd()){const auto line=memory.readLine().simplified().split(' ');if(line.size()<2)continue;if(line[0]=="MemTotal:")total=line[1].toULongLong();if(line[0]=="MemAvailable:")available=line[1].toULongLong();}c->m_state["ram"]=double(total-std::min(total,available))/1048576.;c->m_state["totalRam"]=double(total)/1048576.;}
    emit c->stateChanged();
#endif
}
void Platform::linuxScan(){
#ifndef Q_OS_WIN
    if(m_refreshing)return;m_refreshing=true;
    if(!exe("wmctrl").isEmpty()&&qEnvironmentVariable("XDG_SESSION_TYPE")!="wayland")command(exe("wmctrl"),{"-lpGx"},[this](int code,QByteArray output){
        if(code)return;QVariantList windows;
        for(const auto& line:QString::fromUtf8(output).split('\n')){const auto cells=line.split(QRegularExpression("\\s+"),Qt::SkipEmptyParts);if(cells.size()<10)continue;const auto pid=cells[2].toUInt();QFile comm(QString("/proc/%1/comm").arg(pid));QString process;if(comm.open(QIODevice::ReadOnly))process=QString::fromUtf8(comm.read(256)).trimmed();if(denied(c->m_options,process)||pid==QCoreApplication::applicationPid())continue;
            windows<<QVariantMap{{"id",cells[0]},{"pid",pid},{"name",cells.mid(9).join(' ')},{"exe",process},{"x",cells[3].toInt()},{"y",cells[4].toInt()},{"width",cells[5].toInt()},{"height",cells[6].toInt()}};}
        c->setState("windows",windows);
        if(c->m_options["autoBorderless"].toBool())for(const auto& value:windows){const auto window=value.toMap();if(window["id"]==c->m_options["selectedGame"]&&allowed(c->m_options,window["exe"].toString())&&!m_windowRestores.contains(window["id"].toString()))borderless(false);}
    });
    if(!exe("wpctl").isEmpty())command(exe("wpctl"),{"get-volume","@DEFAULT_AUDIO_SINK@"},[this](int code,QByteArray output){if(code)return;const auto parts=output.simplified().split(' ');if(parts.size()>1){const int value=qBound(0,qRound(parts[1].toDouble()*100),200);if(!m_volumeTimer.isActive()&&c->m_clock.elapsed()>c->m_volumeUntil){c->m_state["volume"]=value;c->m_options["volume"]=value;emit c->optionsChanged();}c->m_state["muted"]=output.contains("MUTED");}});
    if(!exe("pw-dump").isEmpty())command(exe("pw-dump"),{},[this](int code,QByteArray output){
        m_refreshing=false;if(code)return;const auto array=QJsonDocument::fromJson(output).array();QVariantList nodes;
        for(const auto& item:array){const auto object=item.toObject();if(object["type"]!="PipeWire:Interface:Node")continue;const auto props=object["info"].toObject()["props"].toObject();const auto media=props["media.class"].toString();if(media!="Audio/Source"&&media!="Audio/Sink")continue;nodes<<QVariantMap{{"id",object["id"].toInt()},{"name",props["node.name"].toString()},{"description",props["node.description"].toString()},{"media",media},{"rate",props["audio.rate"].toInt()}};}
        c->setState("audioNodes",nodes);
    });else m_refreshing=false;
#endif
}
QString Platform::launchCommand()const{
#ifdef Q_OS_WIN
    return {};
#else
    return "env MANGOHUD_CONFIGFILE="+shellQuote(c->root()+"/MangoHud.conf")+" mangohud "+(c->m_options["gamescope"].toBool()?"gamescope -b -r "+QString::number(c->m_options["cap"].toInt())+" -- ":QString())+"%command%";
#endif
}
void Platform::linuxLaunch(){
#ifndef Q_OS_WIN
    if(m_launchedPid&&QFileInfo::exists(QString("/proc/%1").arg(m_launchedPid))){m_monitoring=true;c->setState("capture",true);c->notify("Monitoring resumed for the running game.");return;}
    QVariantMap game;for(const auto& value:c->m_games)if(value.toMap()["id"]==c->m_options["selectedGame"])game=value.toMap();
    const QFileInfo file(game["executable"].toString());if(game.isEmpty()||!file.isFile()||!file.isExecutable()){c->notify("Add a game executable to your library, then select it.","warning");return;}
    if(denied(c->m_options,file.fileName())){c->notify("This executable is blocked by your process rules.","warning");return;}
    if(exe("mangohud").isEmpty()){c->notify("Install MangoHud to launch with monitoring and the in-game overlay. Setup shows the required packages.","warning");return;}
    if(c->m_options["gamescope"].toBool()&&exe("gamescope").isEmpty()){c->notify("Gamescope is unavailable. Disable its launch option or install it in Setup.","warning");return;}
    applyOptions();flushOptions();m_logFile.clear();m_logOffset=0;m_logPending.clear();m_logHeaders.clear();c->m_store.clear();c->m_frameTime=0;
    QProcessEnvironment env=QProcessEnvironment::systemEnvironment();env.insert("MANGOHUD_CONFIGFILE",c->root()+"/MangoHud.conf");env.insert("MANGOHUD_DLSYM","1");m_game.setProcessEnvironment(env);
    QStringList args{"--dlsym"};if(c->m_options["gamescope"].toBool())args<<exe("gamescope")<<"-b"<<"-r"<<QString::number(c->m_options["cap"].toInt())<<"--";
    args<<file.absoluteFilePath();args+=unlock::arguments(QProcess::splitCommand(game["arguments"].toString()),game["unlock"].toBool()&&unlock::supported(file.absoluteFilePath()));
    m_game.setWorkingDirectory(file.absolutePath());m_game.setProgram(exe("mangohud"));m_game.setArguments(args);m_game.setStandardOutputFile(QProcess::nullDevice());m_game.setStandardErrorFile(c->root()+"/game-launch.log");if(!m_game.startDetached(&m_launchedPid)){c->notify("The game could not start. Check its executable and launch arguments.","error");return;}m_monitoring=true;c->m_state["profilePid"]=quint32(m_launchedPid);c->m_state["profileExe"]=file.fileName();c->m_state["profilePath"]=file.absoluteFilePath();c->m_state["gameRunning"]=true;c->m_state["capture"]=true;c->m_state["game"]=game["name"];c->m_state["renderer"]="MangoHud · Vulkan / OpenGL";c->m_state["launchTime"]=QDateTime::currentMSecsSinceEpoch();
    c->notify("Launching with your overlay and frame-limit settings.");
#endif
}
void Platform::readLog(){
#ifndef Q_OS_WIN
    if(!m_monitoring)return;
    const auto files=QDir(c->root()+"/logs").entryInfoList({"*.csv"},QDir::Files,QDir::Time);
    if(files.isEmpty())return;
    const auto selected=files.first();if(selected.isSymLink())return;
    if(m_logFile!=selected.absoluteFilePath()){m_logFile=selected.absoluteFilePath();m_logOffset=0;m_logHeaders.clear();m_logPending.clear();}
    QFile file(m_logFile);if(!file.open(QIODevice::ReadWrite))return;if(file.size()<m_logOffset){m_logOffset=0;m_logHeaders.clear();m_logPending.clear();}
    file.seek(m_logOffset);const auto bytes=file.read(1024*1024);m_logOffset+=bytes.size();m_logPending+=bytes;if(m_logPending.size()>2*1024*1024)m_logPending.clear();
    QList<double> times;int end;
    while((end=m_logPending.indexOf('\n'))>=0){const auto line=m_logPending.left(end).trimmed();m_logPending.remove(0,end+1);const auto cells=QString::fromUtf8(line).split(',');if(cells.contains("fps")&&cells.contains("frametime")){m_logHeaders=cells;continue;}const int index=m_logHeaders.indexOf("frametime");if(index<0||index>=cells.size())continue;bool valid=false;const double ms=cells[index].toDouble(&valid);if(valid&&std::isfinite(ms)&&ms>0&&ms<60000)times<<ms;}
    if(!times.isEmpty()){c->m_state["capture"]=true;c->sampleFrames(times,c->m_state["game"].toString(),quint32(m_launchedPid?m_launchedPid:1));}
    // Keep only a small physical tail: telemetry history lives in the bounded Store.
    if(m_logOffset>8*1024*1024){const qint64 amount=((m_logOffset-4*1024*1024)/(1024*1024))*(1024*1024);if(amount>0)fallocate(file.handle(),FALLOC_FL_PUNCH_HOLE|FALLOC_FL_KEEP_SIZE,0,amount);}
    for(int i=3;i<files.size();++i)if(!files[i].isSymLink())QFile::remove(files[i].absoluteFilePath());
#endif
}
void Platform::borderless(bool restore){
#ifndef Q_OS_WIN
    if(qEnvironmentVariable("XDG_SESSION_TYPE")=="wayland"){c->notify("For Wayland, enable Gamescope in the game launch options to control fullscreen sizing.");return;}
    const QString id=c->m_options["selectedGame"].toString();QVariantMap window;for(const auto& value:c->m_state["windows"].toList())if(value.toMap()["id"]==id)window=value.toMap();
    if(window.isEmpty()||!id.startsWith("0x")||denied(c->m_options,window["exe"].toString())){c->notify("Select an allowed running game window first.","warning");return;}
    if(restore){if(!m_windowRestores.contains(id)){c->notify("This window has no saved Framoryx state.");return;}command(exe("wmctrl"),{"-ir",id,"-b","remove,fullscreen"},[this,id](int code,QByteArray){if(!code)m_windowRestores.remove(id);c->notify(code?"The window manager could not restore this window.":"Original window mode restored.",code?"warning":"info");});}
    else {if(!m_windowRestores.contains(id))m_windowRestores[id]=window;command(exe("wmctrl"),{"-ir",id,"-b","add,fullscreen"},[this](int code,QByteArray){c->notify(code?"The window manager refused fullscreen.":"Borderless fullscreen applied. Restore returns the original mode.",code?"warning":"info");});}
#else
    send({{"action",restore?"restore":"borderless"}});
#endif
}
void Platform::linuxAudio(bool microphone){
#ifndef Q_OS_WIN
    if(exe("pipewire").isEmpty()){c->notify("PipeWire is required for audio effects. Open Setup for installation guidance.","warning");return;}
    const auto& o=c->m_options;const QString plugin=c->root()+"/rnnoise/librnnoise_ladspa.so";
    if(microphone&&o["noise"].toBool()&&!QFileInfo::exists(plugin)){c->setState("pendingMic",true);updateNoise(false);c->notify("Preparing RNNoise. Microphone effects will apply when the verified plugin is ready.");return;}
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
    QJsonObject gain{{"type","builtin"},{"name","headroom"},{"label","linear"},{"control",QJsonObject{{"Mult",std::pow(10.,-headroom/20.)},{"Add",0.}}}};nodes.append(gain);links.append(QJsonObject{{"output",previous},{"input","headroom:In"}});previous="headroom:Out";
    QJsonObject capture{{"node.name",microphone?"capture.framoryx_mic":"framoryx_output"},{"node.description",microphone?"Framoryx microphone input":"Framoryx Output"},{"audio.position",QJsonArray{microphone?"MONO":"FL",microphone?"MONO":"FR"}}};
    QJsonObject playback{{"node.name",microphone?"framoryx_mic":"playback.framoryx_output"},{"node.description",microphone?"Framoryx Microphone":"Framoryx output playback"}};
    if(microphone){capture["node.passive"]=true;capture["audio.rate"]=48000;capture["audio.channels"]=1;capture["audio.position"]=QJsonArray{"MONO"};playback["media.class"]="Audio/Source";playback["audio.rate"]=48000;playback["audio.channels"]=1;playback["audio.position"]=QJsonArray{"MONO"};
        const QString chosen=QString::fromUtf8(QByteArray::fromBase64(o["micId"].toByteArray()));for(const auto& value:c->m_state["audioNodes"].toList()){const auto entry=value.toMap();if(entry["media"]=="Audio/Source"&&entry["name"]!="framoryx_mic"&&(entry["name"]==chosen||entry["description"]==c->m_state["selectedMicName"]))capture["target.object"]=entry["name"].toString();}
    }else{capture["media.class"]="Audio/Sink";playback["node.passive"]=true;}
    const QJsonObject graph{{"nodes",nodes},{"links",links},{"inputs",QJsonArray{first}},{"outputs",QJsonArray{previous}}};
    const QJsonObject args{{"node.description",microphone?"Framoryx Microphone":"Framoryx Output"},{"filter.graph",graph},{"capture.props",capture},{"playback.props",playback}};
    QJsonArray modules{QJsonObject{{"name","libpipewire-module-rt"},{"flags",QJsonArray{"ifexists","nofail"}}},QJsonObject{{"name","libpipewire-module-protocol-native"}},QJsonObject{{"name","libpipewire-module-client-node"}},QJsonObject{{"name","libpipewire-module-adapter"}},QJsonObject{{"name","libpipewire-module-filter-chain"},{"args",args}}};
    QJsonObject config{{"context.properties",QJsonObject{{"log.level",1}}},{"context.spa-libs",QJsonObject{{"audio.convert.*","audioconvert/libspa-audioconvert"},{"support.*","support/libspa-support"}}},{"context.modules",modules}};
    const QString path=c->root()+(microphone?"/microphone.conf":"/output.conf");if(!writeFile(path,QJsonDocument(config).toJson())){c->notify("Audio configuration could not be saved.","error");return;}
    auto process=microphone?&m_mic:&m_output;if(process->state()!=QProcess::NotRunning){process->terminate();if(!process->waitForFinished(800))process->kill();}
    process->start(exe("pipewire"),{"-c",path});
    QTimer::singleShot(1200,this,[this,process,microphone]{const bool running=process->state()==QProcess::Running;c->setState(microphone?"micConfigured":"bassConfigured",running);c->setState("audioStatus",running?(microphone?"Framoryx Microphone is ready. Select it in your game's voice settings.":"Framoryx Output is ready. Choose it as playback output to hear bass enhancement."):"The audio filter could not start. Open Compatibility for its diagnostic.");linuxScan();});
#endif
}
void Platform::updateNoise(bool force){
#ifdef Q_OS_WIN
    send({{"action","noiseUpdate"},{"force",force}});
#else
    if(c->m_state["noiseBusy"].toBool())return;
    const QString base=c->root()+"/rnnoise",plugin=base+"/librnnoise_ladspa.so";
    const auto verified=[&](){QFile f(plugin);if(!f.open(QIODevice::ReadOnly)||f.size()>32*1024*1024)return false;return QCryptographicHash::hash(f.readAll(),QCryptographicHash::Sha256).toHex()=="ba2b420c76cd2f1e7684430c96fef8c533041d5f67b94060f8eeda6af1701f97";};
    if(!force&&verified()){c->setState("noiseStatus","RNNoise v1.10 ready · verified official Linux plugin");if(c->m_state["pendingMic"].toBool()){c->setState("pendingMic",false);linuxAudio(true);}return;}
    c->setState("noiseBusy",true);c->setState("noiseStatus","Checking the latest official Linux RNNoise release…");
    auto fetch=std::make_shared<std::function<void(QUrl,int,qint64,std::function<void(QByteArray)>)>>();
    const std::weak_ptr<std::function<void(QUrl,int,qint64,std::function<void(QByteArray)>)>> weakFetch=fetch;
    *fetch=[this,weakFetch](QUrl url,int redirects,qint64 limit,std::function<void(QByteArray)> done){
        const auto fetch=weakFetch.lock();if(!fetch)return;
        const QStringList hosts{"api.github.com","github.com","release-assets.githubusercontent.com","objects.githubusercontent.com"};
        auto fail=[this](const QString& text){c->setState("noiseBusy",false);c->setState("pendingMic",false);c->setState("noiseStatus",text);c->notify(text,"warning");};
        if(url.scheme()!="https"||!hosts.contains(url.host())||!url.userInfo().isEmpty()||redirects>5){fail("RNNoise update refused an unexpected download location. The existing plugin was kept.");return;}
        QNetworkRequest request(url);request.setHeader(QNetworkRequest::UserAgentHeader,"Framoryx/0.3");request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);request.setTransferTimeout(20000);
        auto reply=m_network.get(request);auto data=std::make_shared<QByteArray>();
        connect(reply,&QIODevice::readyRead,this,[reply,data,limit]{*data+=reply->readAll();if(data->size()>limit)reply->abort();});
        connect(reply,&QNetworkReply::finished,this,[this,reply,data,limit,redirects,done,fetch,fail]{
            *data+=reply->readAll();const auto redirect=reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();const int code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();const auto destination=reply->url().resolved(redirect);const auto error=reply->error();reply->deleteLater();
            if(code>=300&&code<400&&!redirect.isEmpty()){(*fetch)(destination,redirects+1,limit,done);return;}
            if(error!=QNetworkReply::NoError||code!=200||data->size()>limit){fail("RNNoise update could not finish. Check your connection; the existing plugin was kept.");return;}
            done(*data);
        });
    };
    (*fetch)(QUrl("https://api.github.com/repos/werman/noise-suppression-for-voice/releases/latest"),0,2*1024*1024,[this,fetch,base,plugin](QByteArray bytes){
        const auto release=QJsonDocument::fromJson(bytes).object();const auto version=release["tag_name"].toString();QJsonObject asset;
        for(const auto& value:release["assets"].toArray())if(value.toObject()["name"]=="linux-rnnoise.zip")asset=value.toObject();
        QString digest=asset["digest"].toString();if(version=="v1.10")digest="sha256:811390b6eb6e28dde023c70590c74d26e74ebb2e595bcf4b95af2341db160e99";
        if(asset.isEmpty()||!digest.startsWith("sha256:")||digest.size()!=71||release["prerelease"].toBool()||release["draft"].toBool()){c->setState("noiseBusy",false);c->setState("pendingMic",false);c->setState("noiseStatus","The latest release could not be verified. Your installed plugin is unchanged.");return;}
        c->setState("noiseStatus","Downloading and verifying official Linux RNNoise…");
        (*fetch)(QUrl(asset["browser_download_url"].toString()),0,64*1024*1024,[this,base,plugin,digest,version](QByteArray archive){
            auto fail=[this]{c->setState("noiseBusy",false);c->setState("pendingMic",false);c->setState("noiseStatus","Plugin verification failed. The existing audio configuration was kept.");};
            if(QCryptographicHash::hash(archive,QCryptographicHash::Sha256).toHex()!=digest.mid(7).toLatin1()){fail();return;}
            mz_zip_archive zip{};if(!mz_zip_reader_init_mem(&zip,archive.constData(),archive.size(),0)){fail();return;}
            QByteArray binary;
            for(mz_uint i=0;i<mz_zip_reader_get_num_files(&zip);++i){mz_zip_archive_file_stat stat{};if(!mz_zip_reader_file_stat(&zip,i,&stat))continue;const QString name=QString::fromUtf8(stat.m_filename);if(!name.endsWith("/ladspa/librnnoise_ladspa.so")||stat.m_uncomp_size>32*1024*1024||stat.m_uncomp_size<64)continue;binary.resize(qsizetype(stat.m_uncomp_size));if(!mz_zip_reader_extract_to_mem(&zip,i,binary.data(),binary.size(),0))binary.clear();break;}
            mz_zip_reader_end(&zip);
            if(binary.size()<64||binary.left(4)!=QByteArray("\x7f" "ELF",4)||quint8(binary[4])!=2||quint8(binary[18])!=62){fail();return;}
            if(version=="v1.10"&&QCryptographicHash::hash(binary,QCryptographicHash::Sha256).toHex()!="ba2b420c76cd2f1e7684430c96fef8c533041d5f67b94060f8eeda6af1701f97"){fail();return;}
            QDir().mkpath(base);if(!writeFile(plugin,binary)){fail();return;}
            const QVariantMap record{{"version",version},{"sha256",QString::fromLatin1(QCryptographicHash::hash(binary,QCryptographicHash::Sha256).toHex())}};writeFile(base+"/verified.json",json(record));
            c->setState("noiseBusy",false);c->setState("noiseStatus","RNNoise "+version+" ready · verified official Linux plugin");if(c->m_state["pendingMic"].toBool()){c->setState("pendingMic",false);linuxAudio(true);}
        });
    });
#endif
}
void Platform::action(const QString& name,const QVariantMap& args){
    if(name=="safetyFallback"){
#ifdef Q_OS_WIN
        send({{"action","safetyFallback"},{"enabled",args["enabled"].toBool()}});
#else
        c->setState("externalPreferred",args["enabled"].toBool());
#endif
        return;
    }
    if(name=="copySteam"){QGuiApplication::clipboard()->setText(launchCommand());c->notify("Steam launch options copied. Paste them into the game's Properties → Launch Options.");return;}
    if(name=="noiseUpdate"){updateNoise(true);return;}
    if(name=="audioOpen"){if(c->m_options["autoNoise"].toBool())updateNoise(false);return;}
    if(name=="borderless"||name=="restore"){borderless(name=="restore");return;}
#ifdef Q_OS_WIN
    if(name=="launch"){for(const auto& entry:c->m_games){const auto game=entry.toMap();if(game["id"]!=c->m_options["selectedGame"])continue;const QFileInfo file(game["executable"].toString());if(!file.isFile()||denied(c->m_options,file.fileName())){c->notify("This game is unavailable or blocked by your process rules.","warning");return;}QProcess launch;launch.setProgram(file.absoluteFilePath());launch.setWorkingDirectory(file.absolutePath());launch.setArguments(unlock::arguments(QProcess::splitCommand(game["arguments"].toString()),game["unlock"].toBool()&&unlock::supported(file.absoluteFilePath())));if(!launch.startDetached())c->notify("The game could not be launched.","warning");else {c->notify("Game launched. Capture can follow its foreground window.");send({{"action","select"},{"id","auto"}});}return;}c->notify("Add and select a game in your library first.");return;}
    QJsonObject request=QJsonObject::fromVariantMap(args);if(name=="apoConfig")request["path"]=QUrl(args["url"].toString()).toLocalFile();request["action"]=name;send(request);
#else
    if(name=="capture"&&c->m_state["capture"].toBool()){m_monitoring=false;c->setState("capture",false);c->setState("live",false);c->notify("Capture stopped. Your game and its in-game overlay keep running.");return;}
    if(name=="launch"||name=="capture"){linuxLaunch();return;}
    if(name=="overlay"){c->setState("overlay",!c->m_state["overlay"].toBool());applyOptions("fields");c->notify(c->m_state["overlay"].toBool()?"Overlay enabled for monitored launches. Reload it with Shift+F4 in a running game.":"Overlay hidden for monitored launches. Reload it with Shift+F4 in a running game.");return;}
    if(name=="micApply"){linuxAudio(true);return;}
    if(name=="bassApply"){linuxAudio(false);return;}
    if(name=="micDisable"||name=="bassDisable"){auto process=name=="micDisable"?&m_mic:&m_output;process->terminate();c->setState(name=="micDisable"?"micConfigured":"bassConfigured",false);c->setState("pendingMic",false);c->notify("The selected Framoryx effects are disabled.");return;}
    if(name=="defaultMic"||name=="defaultOutput"){const QString target=name=="defaultMic"?"framoryx_mic":"framoryx_output";for(const auto& value:c->m_state["audioNodes"].toList()){const auto node=value.toMap();if(node["name"]==target){command(exe("wpctl"),{"set-default",node["id"].toString()},[this](int code,QByteArray){c->notify(code?"The default audio device could not be changed.":"Framoryx is now the default audio device.",code?"warning":"info");});return;}}c->notify("Apply this effect first, then choose its virtual device.");return;}
    if(name=="shortcut"){const QString folder=QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);if(folder.isEmpty()){c->notify("No desktop folder is configured. Framoryx is available in your app launcher.");return;}const QString program=QCoreApplication::applicationFilePath();QByteArray entry="[Desktop Entry]\nType=Application\nName=Framoryx\nExec=\""+QFile::encodeName(program).replace("\"","\\\"")+"\"\nIcon=io.framoryx.Framoryx\nTerminal=false\nCategories=Game;Utility;\n";const QString path=folder+"/Framoryx.desktop";if(writeFile(path,entry)){QFile::setPermissions(path,QFile::permissions(path)|QFile::ExeOwner);c->notify("Desktop shortcut created. Your desktop may ask you to trust the launcher once.");}else c->notify("The desktop shortcut could not be created.","warning");return;}
    if(name=="fixture"){const QString scene=QCoreApplication::applicationDirPath()+"/FramoryxScene";c->addGame(QUrl::fromLocalFile(scene),"Framoryx test scene","");linuxLaunch();return;}
    if(name=="soundSettings"){QDesktopServices::openUrl(QUrl("https://github.com/NeTriXoN-BoT/Framoryx/blob/main/docs/AUDIO.md"));return;}
    c->notify("This control is unavailable in the current desktop session.","warning");
#endif
}
void Platform::shutdown(){
    if(m_closing)return;m_closing=true;m_scanTimer.stop();m_volumeTimer.stop();m_optionTimer.stop();
#ifdef Q_OS_WIN
    send({{"action","quit"}});m_engine.closeWriteChannel();if(!m_engine.waitForFinished(2500)){m_engine.terminate();if(!m_engine.waitForFinished(500))m_engine.kill();}
#else
    for(const auto& id:m_windowRestores.keys())command(exe("wmctrl"),{"-ir",id,"-b","remove,fullscreen"});
    for(auto process:{&m_mic,&m_output}){process->terminate();if(!process->waitForFinished(1000))process->kill();}
    // A user's game keeps running when the control application closes.
    m_game.disconnect(this);
#endif
}
