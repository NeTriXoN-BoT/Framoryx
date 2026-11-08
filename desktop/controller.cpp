#include "controller.hpp"
#include "audio_monitor.hpp"
#include "platform.hpp"
#include "lifecycle.hpp"
#include "process_catalog.hpp"
#include "game_safety.hpp"
#include "telemetry.hpp"
#ifndef Q_OS_WIN
#include "noise_cache.hpp"
#include <QCryptographicHash>
#endif
#include "unlock.hpp"
#include "presentation.hpp"
#include <QtConcurrent>
#include <QFutureWatcher>
#include "export.hpp"
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUuid>
#include <QUrl>
#include <QTextStream>
#include <QRegularExpression>
#include <QSet>
#include <fstream>
#include <cmath>

namespace {
const QStringList labels{"Live FPS","Average FPS","1% low","Frame time","Frame graph","System CPU","System RAM","FPS limit","Renderer","Framoryx label","GPU usage","VRAM usage","GPU temperature","CPU temperature","GPU power","GPU clock"};
const QVariantList defaultOrder{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
const QStringList visualKeys{"fields","order","style","opacity","overlayScale","fontScale","hudX","hudY","accent"};
const QStringList gameKeys=visualKeys+QStringList{"cap","mode","linuxOverlayMode"};
QVariantMap pick(const QVariantMap& source,const QStringList& keys){QVariantMap result;for(const auto& key:keys)if(source.contains(key))result[key]=source[key];return result;}
QString gameKey(QString name){name=name.replace('\\','/');name=QFileInfo(name).fileName();if(name.size()>160||name.contains(QRegularExpression("[\\x00-\\x1f]")))return {};
#ifdef Q_OS_WIN
    name=name.toLower();
#endif
    return name;
}
QVariantMap defaults(){return {{"motion",true},{"startWithSystem",false},{"startHidden",false},{"minimizeToTray",false},{"closeToTray",false},{"autoCapture",false},{"uiScale",0},{"overlayScale",0},{"opacity",90},{"style",0},{"accent",0},{"fields",1023},{"order",defaultOrder},{"hudX",.03},{"hudY",.03},{"fontScale",100},{"cap",60},{"mode",0},{"volume",100},{"bass",3},{"noise",false},{"eq",false},{"low",6},{"mid",-3},{"high",0},{"voicePreset",4},{"noisePreset",1},{"autoNoise",true},{"micId",""},{"selectedGame","auto"},{"autoBorderless",false},{"gamescope",false},{"linuxOverlayMode",0},{"allow",QStringList{"FramoryxTest.exe","FramoryxScene","FramoryxPresentScene"}},{"deny",QStringList{"RiotClientServices.exe"}}};}
bool boundedJson(const QString& path,QJsonObject* object){
    const QFileInfo info(path);
    if(!info.isFile() || info.isSymLink() || info.size()>1024*1024)return false;
    QFile file(path);if(!file.open(QIODevice::ReadOnly))return false;
    const auto data=file.read(1024*1024+1);if(data.size()>1024*1024)return false;
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(data,&error);
    if(error.error!=QJsonParseError::NoError || !doc.isObject())return false;
    *object=doc.object();return true;
}
QStringList cleanRules(const QVariant& value){
    QStringList out;
    auto values=value.typeId()==QMetaType::QString?value.toString().split('\n'):value.toStringList();
    for(auto name:values){name=name.trimmed();if(name.isEmpty())continue;
        if(out.size()>=128 || name.size()>160 || name.contains(QRegularExpression("[\\x00-\\x1f/\\\\:,\"]")))continue;
        if(!out.contains(name,Qt::CaseInsensitive))out<<name;
    }return out;
}
}
Controller::Controller(QString root,bool testing,QObject* parent):QObject(parent),m_root(std::move(root)),m_test(testing){
    QDir().mkpath(m_root);m_options=defaults();m_baseOverlay=pick(m_options,gameKeys);
    m_state={{"connected",false},{"capture",false},{"live",false},{"overlay",false},{"fps",0.},{"average",0.},{"low",0.},{"frameTime",0.},{"cpu",0.},{"ram",0.},{"totalRam",0.},{"renderer","Waiting for frames"},{"game","Choose a game"},{"notice","Ready when you are."},{"audioStatus","Choose effects, then Apply."},{"noiseStatus","Automatic updates are ready."},{"noiseBusy",false},{"inputLevel",0.},{"outputLevel",0.},{"recording",false},{"playing",false},{"sampleReady",false},{"volume",100},{"micConfigured",false},{"bassConfigured",false},{"boostConfigured",false},{"hookActive",false},{"pending",false},{"session",qEnvironmentVariable("XDG_SESSION_TYPE","unknown")}};
    load();if(!m_test)m_options["startWithSystem"]=lifecycle::startupEnabled();m_clock.start();
    m_audio=std::make_unique<AudioMonitor>(this);
    m_platform=std::make_unique<Platform>(this);
    m_telemetry=std::make_unique<Telemetry>(this);
    connect(m_telemetry.get(),&Telemetry::sampled,this,[this](const QVariantMap& values){for(auto it=values.begin();it!=values.end();++it)m_state[it.key()]=it.value();emit stateChanged();m_platform->action("telemetry",values);});
    m_saveTimer.setSingleShot(true);m_saveTimer.setInterval(220);
    connect(&m_saveTimer,&QTimer::timeout,this,[this]{save();});
    connect(&m_poll,&QTimer::timeout,this,&Controller::poll);
    connect(&m_media,&QMediaDevices::audioInputsChanged,this,&Controller::refreshDevices);
    connect(&m_media,&QMediaDevices::audioOutputsChanged,this,&Controller::refreshDevices);
    m_state["antiCheatStatus"]="unknown";m_state["antiCheatDetail"]="Select a game to check for known indicators. Inspection cannot establish anti-cheat approval.";
    m_safetyTimer.setInterval(10000);connect(&m_safetyTimer,&QTimer::timeout,this,[this]{assessCurrentGame(true);});
    if(!m_test){refreshDevices();m_platform->start();m_poll.start(250);m_safetyTimer.start();m_telemetry->start();}
}
Controller::~Controller(){m_poll.stop();m_safetyTimer.stop();m_telemetry->disconnect(this);m_audio->stop();m_platform->shutdown();save();}
QString Controller::platformName()const{
#ifdef Q_OS_WIN
    return "Windows";
#else
    return "Linux";
#endif
}
QVariantMap Controller::cleanOptions(const QVariantMap& input)const{
    auto out=defaults();
    const QMap<QString,QPair<int,int>> ranges{{"uiScale",{60,175}},{"overlayScale",{50,300}},{"opacity",{0,100}},{"style",{0,4}},{"accent",{0,9}},{"fontScale",{80,140}},{"cap",{0,1000}},{"mode",{0,2}},{"linuxOverlayMode",{0,2}},{"volume",{0,200}},{"bass",{0,9}},{"low",{-12,12}},{"mid",{-12,12}},{"high",{-12,12}},{"voicePreset",{0,5}},{"noisePreset",{0,2}}};
    for(auto it=input.begin();it!=input.end();++it){
        if(!out.contains(it.key()))continue;
        if(ranges.contains(it.key())){
            bool ok=false;const double n=it->toDouble(&ok);if(!ok||!std::isfinite(n))continue;
            const auto range=ranges[it.key()];int value=qBound(range.first,int(qBound(-10000.,n,10000.)),range.second);
            if((it.key()=="uiScale"||it.key()=="overlayScale")&&n==0)value=0;
            if(it.key()=="cap" && value>0 && value<10)value=10;
            out[it.key()]=value;
        }else if(it.key()=="hudX"||it.key()=="hudY"){
            bool ok=false;const double n=it->toDouble(&ok);if(ok&&std::isfinite(n))out[it.key()]=qBound(0.,n,1.);
        }else if(it.key()=="fields")out[it.key()]=it->toUInt()&65535u;
        else if(it.key()=="order"){
            const auto list=it->toList();QSet<int> seen;QVariantList order;
            for(const auto& item:list){const int n=item.toInt();if(n>=0&&n<16&&!seen.contains(n)){seen.insert(n);order<<n;}}
            for(int n=0;n<16;++n)if(!seen.contains(n))order<<n;
            out[it.key()]=order;
        }else if(it.key()=="allow"||it.key()=="deny")out[it.key()]=cleanRules(*it);
        else if(it.key()=="micId"||it.key()=="selectedGame")out[it.key()]=it->toString().left(4096);
        else out[it.key()]=it->toBool();
    }return out;
}
void Controller::load(){
    QJsonObject document;
    if(!boundedJson(m_root+"/settings.json",&document))return;
    if(document.value("schema").toInt()!=3)return;
    m_options=cleanOptions(document.value("options").toObject().toVariantMap());
    m_baseOverlay=pick(cleanOptions(document.contains("baseOverlay")?document["baseOverlay"].toObject().toVariantMap():m_options),gameKeys);
    const auto savedGames=document["gameOptions"].toObject();
    for(auto item=savedGames.begin();item!=savedGames.end()&&m_gameOptions.size()<256;++item)if(!item.key().isEmpty()&&gameKey(item.key())==item.key()&&item.value().isObject())m_gameOptions[item.key()]=pick(cleanOptions(item.value().toObject().toVariantMap()),gameKeys);
    m_activeGameKey=gameKey(document["activeGameKey"].toString());
    auto cleanList=[&](const QJsonArray& list,bool games){
        QVariantList out;
        for(const auto& value:list){if(out.size()>=128||!value.isObject())break;
            const auto object=value.toObject();const auto name=object.value("name").toString().trimmed().left(80);if(name.isEmpty())continue;
            if(games){const auto exe=object.value("executable").toString();if(exe.size()>4096||!QDir::isAbsolutePath(exe))continue;
                out<<QVariantMap{{"id",object.value("id").toString().left(80)},{"name",name},{"executable",exe},{"arguments",object.value("arguments").toString().left(4096)},{"unlock",object.value("unlock").toBool()&&unlock::supported(exe)}};
            }else out<<QVariantMap{{"name",name},{"options",pick(cleanOptions(object.value("options").toObject().toVariantMap()),visualKeys)}};
        }return out;
    };
    m_games=cleanList(document.value("games").toArray(),true);
    m_profiles=cleanList(document.value("profiles").toArray(),false);
}
bool Controller::save(){
    QSaveFile file(m_root+"/settings.json");
    const QJsonObject object{{"schema",3},{"activeGameKey",m_activeGameKey},{"baseOverlay",QJsonObject::fromVariantMap(m_baseOverlay)},{"gameOptions",QJsonObject::fromVariantMap(m_gameOptions)},{"options",QJsonObject::fromVariantMap(m_options)},{"games",QJsonArray::fromVariantList(m_games)},{"profiles",QJsonArray::fromVariantList(m_profiles)}};
    const auto bytes=QJsonDocument(object).toJson();if(bytes.size()>1024*1024){notify("Preferences exceed the storage limit. Remove some library entries or presets.","warning");return false;}
    if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit()){
        notify("Preferences could not be saved. Check the configuration folder.","warning");return false;
    }return true;
}
void Controller::scheduleSave(){m_saveTimer.start();}
void Controller::setOption(const QString& key,const QVariant& value){
    if(!m_options.contains(key))return;
    auto requested=m_options;requested[key]=value;
    auto next=cleanOptions(requested);if(next==m_options)return;
    if(key=="startWithSystem"&&!m_test){QString error;if(!lifecycle::setStartup(next[key].toBool(),&error)){notify(error,"warning");emit optionsChanged();return;}}
    m_options=std::move(next);
    if(key=="low"||key=="mid"||key=="high"){
        const int low=m_options["low"].toInt(),mid=m_options["mid"].toInt(),high=m_options["high"].toInt();
        const QList<QList<int>> presets{{0,0,0},{-3,0,3},{3,0,-3},{0,-3,6},{6,-3,0}};
        int chosen=5;for(int i=0;i<presets.size();++i)if(presets[i]==QList<int>{low,mid,high})chosen=i;
        m_options["voicePreset"]=chosen;
    }
    if(key=="voicePreset"){
        const QList<QList<int>> presets{{0,0,0},{-3,0,3},{3,0,-3},{0,-3,6},{6,-3,0}};
        const int i=m_options[key].toInt();if(i<5){m_options["low"]=presets[i][0];m_options["mid"]=presets[i][1];m_options["high"]=presets[i][2];m_options["eq"]=true;}
    }
    if(key=="micId"){m_state["selectedMicName"]="";for(const auto& item:m_devices)if(item.toMap()["id"]==m_options["micId"])m_state["selectedMicName"]=item.toMap()["name"];}
    if(key=="volume"){m_volumeUntil=m_clock.elapsed()+700;setState("volume",m_options["volume"]);}
    if(key=="micId"||key=="noise"||key=="eq"||key=="noisePreset"||key=="voicePreset"||key=="low"||key=="mid"||key=="high")setState("pendingMic",false);
    if(gameKeys.contains(key))rememberOverlay();
    emit optionsChanged();if(visualKeys.contains(key))emit overlayChanged();scheduleSave();
    if(!m_test&&!QStringList{"startWithSystem","startHidden","minimizeToTray","closeToTray","autoCapture","motion","uiScale"}.contains(key))m_platform->applyOptions(key);
}
QVariantList Controller::readouts()const{
    QVariantList out;const int fields=m_options["fields"].toInt();
    for(const auto& value:m_options["order"].toList()){
        const int i=value.toInt();out<<QVariantMap{{"index",i},{"name",labels.value(i)},{"enabled",bool(fields&(1<<i))}};
    }return out;
}
QVariantMap Controller::overlayLayout()const{
    fg::HudOptions options;options.fields=m_options["fields"].toUInt();options.style=m_options["style"].toInt();options.order=0;
    const auto order=m_options["order"].toList();for(int i=0;i<order.size()&&i<16;++i)options.order|=uint64_t(order[i].toInt())<<(4*i);
    const auto layout=fg::hudLayout(options);QVariantList cells;
    for(size_t i=0;i<layout.count;++i){const auto& cell=layout.cells[i];int index=0;while(index<16&&cell.field!=(1u<<index))++index;cells<<QVariantMap{{"index",index},{"name",labels.value(index)},{"x",cell.x},{"y",cell.y},{"width",cell.width},{"height",cell.height},{"column",cell.column}};}
    return {{"width",layout.width},{"height",layout.height},{"cells",cells}};
}
void Controller::setReadout(int index,bool visible){if(index<0||index>=16)return;int fields=m_options["fields"].toInt();setOption("fields",visible?fields|(1<<index):fields&~(1<<index));}
void Controller::moveReadout(int index,int direction){auto order=m_options["order"].toList();const int at=order.indexOf(index),to=at+(direction<0?-1:1);if(at<0||to<0||to>=order.size())return;order.swapItemsAt(at,to);setOption("order",order);}
void Controller::rememberOverlay(){
    if(m_activeGameKey.isEmpty())m_baseOverlay=pick(m_options,gameKeys);
    else {if(!m_gameOptions.contains(m_activeGameKey)&&m_gameOptions.size()>=256)m_gameOptions.erase(m_gameOptions.begin());m_gameOptions[m_activeGameKey]=pick(m_options,gameKeys);}
}
void Controller::activateGameProfile(const QString& executable){
    const auto key=gameKey(executable);if(key==m_activeGameKey)return;rememberOverlay();m_activeGameKey=key;
    const auto values=key.isEmpty()||!m_gameOptions.contains(key)?m_baseOverlay:m_gameOptions[key].toMap();
    auto next=m_options;for(const auto& name:gameKeys)if(values.contains(name))next[name]=values[name];m_options=cleanOptions(next);
    m_state["profileName"]=key.isEmpty()?"Default layout":key;emit optionsChanged();emit overlayChanged();emit stateChanged();scheduleSave();if(!m_test&&m_platform)m_platform->applyOptions();
}
void Controller::selectGame(const QString& id){
    QString executable;if(id=="auto")executable=m_state["profileExe"].toString();
    for(const auto& item:m_games)if(item.toMap()["id"]==id)executable=item.toMap()["executable"].toString();
    for(const auto& item:m_state["windows"].toList())if(item.toMap()["id"]==id)executable=item.toMap()["exe"].toString();
    activateGameProfile(executable);setOption("selectedGame",id);if(!m_test){m_platform->select(id);assessCurrentGame(true);}
}
void Controller::addGame(const QUrl& url,const QString& label,const QString& arguments){
    const QString path=QFileInfo(url.toLocalFile()).canonicalFilePath();
    const QFileInfo info(path);
    if(!url.isLocalFile()||path.isEmpty()||!info.isFile()||arguments.size()>4096){notify("Choose a local game executable and valid arguments.","warning");return;}
    if(m_games.size()>=128){notify("The game library is full.","warning");return;}
    for(const auto& entry:m_games)if(entry.toMap()["executable"].toString()==path){selectGame(entry.toMap()["id"].toString());notify("That game is already in your library.");return;}
    const QString name=label.trimmed().isEmpty()?info.completeBaseName():label.trimmed().left(80);
    const QString id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_games<<QVariantMap{{"id",id},{"name",name},{"executable",path},{"arguments",arguments}};
    emit gamesChanged();selectGame(id);scheduleSave();notify("Game added. Launch it from your library when ready.");
}
QVariantList Controller::changelog()const{
    QVariantList result;
    for(const auto& version:QStringList{"0.3.0","0.2.3","0.2.2","0.2.1","0.2.0"}){
        QFile file(":/releases/v"+version+".md");if(!file.open(QIODevice::ReadOnly))continue;
        const QString text=QString::fromUtf8(file.readAll());const auto newline=text.indexOf('\n');const QString title=text.left(newline).remove(0,2);
        result<<QVariantMap{{"version",version},{"title",title},{"body",text.mid(newline+1).trimmed()}};
    }
    return result;
}
QString Controller::processRuleStatus(const QString& name)const{
    const auto native=name.toStdString();fg::Rules rules;rules.deny.clear();
    if(rules.blocked(native))return "Protected";
    for(const auto& item:m_processes)if(item.toMap()["name"]==name&&item.toMap()["protected"].toBool())return "Protected";
    for(const auto& pattern:m_options["deny"].toStringList())if(fg::wildcard(pattern.toStdString(),native))return "Blocked";
    for(const auto& pattern:m_options["allow"].toStringList())if(fg::wildcard(pattern.toStdString(),native))return "Allowed";
    return "Default";
}
void Controller::assessCurrentGame(bool force){
    if(m_test)return;
    QString path;quint32 pid=0;const auto selected=m_options["selectedGame"].toString();
    for(const auto& item:m_games){const auto game=item.toMap();if(game["id"]==selected){path=game["executable"].toString();if(QFileInfo(path).fileName().compare(m_state["profileExe"].toString(),Qt::CaseInsensitive)==0)pid=m_state["profilePid"].toUInt();}}
    for(const auto& item:m_state["windows"].toList()){const auto window=item.toMap();if(window["id"]==selected){pid=window["pid"].toUInt();break;}}
    if(selected=="auto"){pid=m_state["profilePid"].toUInt();path=m_state["profilePath"].toString();}
    const auto key=path+":"+QString::number(pid);if(!force&&key==m_safetyKey)return;
    if(key!=m_safetyKey){m_manualRiskKey.clear();m_state["antiCheatStatus"]="unknown";m_state["antiCheatDetail"]="Checking known per-game and system-wide indicators…";m_platform->action("safetyFallback",{{"enabled",true}});}
    if(m_safetyBusy){m_safetyKey=key;return;}
    m_safetyKey=key;m_safetyBusy=true;
    auto watcher=new QFutureWatcher<QVariantMap>(this);
    connect(watcher,&QFutureWatcher<QVariantMap>::finished,this,[this,watcher,key]{
        const auto result=watcher->result();watcher->deleteLater();m_safetyBusy=false;
        if(key!=m_safetyKey){assessCurrentGame(true);return;}
        m_state["antiCheatStatus"]=result["status"];m_state["antiCheatDetail"]=result["detail"];
        const bool fallback=result["status"]!="no-known-indicator"&&m_manualRiskKey!=key+":"+result["status"].toString();
        if(result["status"]=="detected")m_manualRiskKey.clear();
        m_platform->action("safetyFallback",{{"enabled",result["status"]=="detected"||fallback}});emit stateChanged();
    });
    watcher->setFuture(QtConcurrent::run(assessGame,path,pid));emit stateChanged();
}
void Controller::refreshProcesses(){
    if(m_test||m_scanningProcesses)return;
    m_scanningProcesses=true;setState("processBusy",true);
    auto watcher=new QFutureWatcher<QVariantList>(this);
    connect(watcher,&QFutureWatcher<QVariantList>::finished,this,[this,watcher]{m_processes=watcher->result();watcher->deleteLater();m_scanningProcesses=false;setState("processBusy",false);emit processesChanged();});
    watcher->setFuture(QtConcurrent::run(&runningProcesses));
}
void Controller::addProcessRule(const QString& name,bool block){
    // Only names from the current snapshot can be added with this action.
    bool found=false,protectedProcess=false;
    for(const auto& item:m_processes){const auto entry=item.toMap();if(entry["name"]==name){found=true;protectedProcess=entry["protected"].toBool();break;}}
    if(!found||cleanRules(QStringList{name})!=QStringList{name}){notify("Refresh the process list and select a valid executable name.","warning");return;}
    if(!block&&protectedProcess){notify("System and known anti-cheat processes remain protected.","warning");return;}
    auto rules=m_options[block?"deny":"allow"].toStringList();if(rules.contains(name,Qt::CaseInsensitive)){notify("That executable is already listed.");return;}rules<<name;
    setOption(block?"deny":"allow",rules);notify(name+(block?" added to the block list.":" added to the allow list. Hooks still require separate approval."));
}
QVariantMap Controller::unlockSupport()const{
    for(const auto& item:m_games){const auto game=item.toMap();if(game["id"]==m_options["selectedGame"])return {{"supported",unlock::supported(game["executable"].toString())},{"enabled",game["unlock"].toBool()},{"name",game["name"]},{"method","Source engine · +fps_max 0"}};}
    return {{"supported",false},{"enabled",false},{"name","Select a library game"}};
}
void Controller::setGameUnlock(bool enabled,bool confirmed){
    for(int i=0;i<m_games.size();++i){auto game=m_games[i].toMap();if(game["id"]!=m_options["selectedGame"])continue;
        if(!unlock::supported(game["executable"].toString())||(enabled&&!confirmed)){notify("No confirmed, supported unlock method was selected.","warning");return;}
        game["unlock"]=enabled;m_games[i]=game;if(enabled)setOption("cap",0);emit gamesChanged();emit optionsChanged();scheduleSave();
        notify(enabled?"Game-supported FPS cap override saved for the next library launch. Physics and timing can change at high frame rates.":"Game FPS cap override disabled for future launches.");return;
    }
    notify("Select a supported game from your library first.","warning");
}
void Controller::removeGame(const QString& id){for(int i=0;i<m_games.size();++i)if(m_games[i].toMap()["id"].toString()==id){m_games.removeAt(i);break;}if(m_options["selectedGame"]==id)selectGame("auto");emit gamesChanged();scheduleSave();}
void Controller::saveProfile(const QString& name){const auto clean=name.trimmed().left(80);if(clean.isEmpty())return;for(int i=0;i<m_profiles.size();++i)if(m_profiles[i].toMap()["name"].toString()==clean){m_profiles[i]=QVariantMap{{"name",clean},{"options",pick(m_options,visualKeys)}};emit profilesChanged();scheduleSave();notify("Preset updated.");return;}if(m_profiles.size()>=128){notify("Remove a preset before adding another.","warning");return;}m_profiles<<QVariantMap{{"name",clean},{"options",pick(m_options,visualKeys)}};emit profilesChanged();scheduleSave();notify("Preset saved.");}
void Controller::applyProfile(int index){if(index<0||index>=m_profiles.size())return;auto next=m_options;const auto saved=m_profiles[index].toMap()["options"].toMap();for(const auto& key:visualKeys)if(saved.contains(key))next[key]=saved[key];m_options=cleanOptions(next);rememberOverlay();emit optionsChanged();emit overlayChanged();scheduleSave();if(!m_test)m_platform->applyOptions();notify("Overlay preset loaded. Game limits, audio and startup preferences are preserved.");}
void Controller::removeProfile(int index){if(index<0||index>=m_profiles.size())return;m_profiles.removeAt(index);emit profilesChanged();scheduleSave();}
bool Controller::exportProfiles(const QUrl& url){if(!url.isLocalFile())return false;QSaveFile file(url.toLocalFile());QVariantList exported;for(const auto& value:m_profiles){auto preset=value.toMap();auto options=preset["options"].toMap();options.remove("micId");options.remove("selectedGame");for(const auto& key:QStringList{"startWithSystem","startHidden","minimizeToTray","closeToTray","autoCapture"})options.remove(key);preset["options"]=options;exported<<preset;}QJsonObject document{{"format","Framoryx presets"},{"version",1},{"presets",QJsonArray::fromVariantList(exported)}};if(!file.open(QIODevice::WriteOnly)||file.write(QJsonDocument(document).toJson())<0||!file.commit()){notify("Presets could not be exported.","warning");return false;}notify("Presets exported. Device identifiers and game paths are excluded from the export.");return true;}
bool Controller::importProfiles(const QUrl& url){QJsonObject object;if(!url.isLocalFile()||!boundedJson(url.toLocalFile(),&object)||object["format"]!="Framoryx presets"||object["version"].toInt()!=1){notify("This is not a supported Framoryx preset file.","warning");return false;}int count=0;for(const auto& value:object["presets"].toArray()){if(m_profiles.size()>=128)break;if(!value.isObject())continue;const auto entry=value.toObject();const auto name=entry["name"].toString().trimmed().left(80);if(name.isEmpty())continue;auto options=pick(cleanOptions(entry["options"].toObject().toVariantMap()),visualKeys);m_profiles<<QVariantMap{{"name",name},{"options",options}};++count;}emit profilesChanged();scheduleSave();notify(QString("Imported %1 presets. Nothing was launched or applied to audio devices.").arg(count));return count>0;}
void Controller::notify(const QString& text,const QString& severity){m_state["notice"]=text;emit stateChanged();emit toast(text,severity);}
void Controller::setState(const QString& key,const QVariant& value){if(m_state.value(key)==value)return;m_state[key]=value;emit stateChanged();}
void Controller::setWindowActive(bool active){m_active=active;}
void Controller::refreshDevices(){
    QVariantList out;for(const auto& device:QMediaDevices::audioInputs())out<<QVariantMap{{"id",QString::fromLatin1(device.id().toBase64())},{"name",device.description()},{"rate",device.preferredFormat().sampleRate()},{"channels",device.preferredFormat().channelCount()}};
    m_devices=out;for(const auto& item:m_devices)if(item.toMap()["id"]==m_options["micId"])m_state["selectedMicName"]=item.toMap()["name"];emit devicesChanged();
    if(m_options["micId"].toString().isEmpty()&&!out.isEmpty())setOption("micId",QString::fromLatin1(QMediaDevices::defaultAudioInput().id().toBase64()));
}
void Controller::refreshCapabilities(){if(!m_test)m_platform->refresh();}
void Controller::receive(const QJsonObject& incoming){
    auto next=incoming.toVariantMap();
    if(next.contains("profileExe")){const auto executable=next["profileExe"].toString();bool follows=m_options["selectedGame"]=="auto";for(const auto& item:m_games)if(item.toMap()["id"]==m_options["selectedGame"]&&gameKey(item.toMap()["executable"].toString())==gameKey(executable))follows=true;for(const auto& item:next["games"].toList())if(item.toMap()["id"]==m_options["selectedGame"])follows=true;if(follows)activateGameProfile(executable);}

    if(next["inGameSupported"]==false&&m_options["mode"]==2){setOption("mode",1);notify("This renderer uses the external overlay. Optional Windows in-game limiting supports DX11/DX12.");}
    if(next.contains("volume")&&m_clock.elapsed()>m_volumeUntil){m_options["volume"]=qBound(0,next["volume"].toInt(),200);emit optionsChanged();}
    if(next.contains("games")){m_state["windows"]=next.take("games");}
    if(next.contains("microphones")){const auto list=next.take("microphones").toList();m_state["engineMicrophones"]=list;}
    if(next.contains("graph"))m_graph=next.take("graph").toList();
    for(auto it=next.begin();it!=next.end();++it)m_state[it.key()]=it.value();
    emit stateChanged();if(!m_test)assessCurrentGame();
}
void Controller::sampleFrames(const QList<double>& times,const QString& app,quint32 pid){
    const double now=m_clock.elapsed()/1000.;m_livePid=pid;
    for(double ms:times){if(!std::isfinite(ms)||ms<=0||ms>60000)continue;m_frameTime+=ms/1000.;m_store.add({app.toStdString(),"Linux",pid,m_frameTime,ms},now);}
    const auto snapshot=m_store.snapshot(pid,now);m_state["fps"]=snapshot.fps;m_state["average"]=snapshot.average;m_state["low"]=snapshot.low;m_state["frameTime"]=snapshot.ms;m_state["live"]=snapshot.live;m_state["game"]=app;
    m_graph.clear();const auto first=snapshot.frames.size()>180?snapshot.frames.size()-180:0;for(size_t i=first;i<snapshot.frames.size();++i)m_graph<<snapshot.frames[i].ms;
    emit stateChanged();
}
void Controller::poll(){m_platform->poll();if(m_livePid&&!m_store.snapshot(m_livePid,m_clock.elapsed()/1000.).live&&m_state["live"].toBool()){m_state["live"]=false;emit stateChanged();}}
QString Controller::launchCommand()const{return m_platform->launchCommand();}
void Controller::writeCsv(const QString& path){
    if(path.isEmpty())return;
#ifdef Q_OS_WIN
    m_platform->action("export",{{"path",path}});
#else
    const auto snapshot=m_store.snapshot(m_livePid,m_clock.elapsed()/1000.);if(snapshot.frames.empty()){notify("Capture game frames before exporting.");return;}
    std::ostringstream data;fg::writeCaptureCsv(data,snapshot);QSaveFile file(path);const auto bytes=QByteArray::fromStdString(data.str());if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit()){notify("The capture could not be exported.","warning");return;}notify("Rolling capture exported.");
#endif
}
void Controller::action(const QString& name,const QVariantMap& args){
    if(name=="processRefresh"){refreshProcesses();return;}
    if(name=="hideToTray"){emit hideRequested();return;}
    if(name=="refresh"){refreshDevices();refreshCapabilities();return;}
    if(name=="record"){if(m_test)return;auto device=QByteArray::fromBase64(m_options["micId"].toByteArray());
#ifndef Q_OS_WIN
        if(m_state["micConfigured"].toBool())for(const auto& input:QMediaDevices::audioInputs())if(input.id()=="framoryx_mic"||input.description()=="Framoryx Microphone"){device=input.id();break;}
#endif
        m_audio->record(device);return;}
    if(name=="playSample"){if(!m_test)m_audio->play();return;}
    if(name=="stopSample"){m_audio->stop();return;}
    if(name=="export"){writeCsv(QUrl(args["url"].toString()).toLocalFile());return;}
    if(name=="openConfig"){QDesktopServices::openUrl(QUrl::fromLocalFile(m_root));return;}
    if(name=="sensorGuide"){QDesktopServices::openUrl(QUrl("https://github.com/LibreHardwareMonitor/LibreHardwareMonitor#readme"));return;}
    if(name=="guide"){QDesktopServices::openUrl(QUrl("https://github.com/NeTriXoN-BoT/Framoryx#readme"));return;}
    if(name=="release"){QDesktopServices::openUrl(QUrl("https://github.com/NeTriXoN-BoT/Framoryx/releases/latest"));return;}
    if(name=="quit"){emit quitRequested();return;}
    if(name=="resetOverlay"){auto values=defaults();for(const auto& key:QStringList{"fields","order","style","opacity","overlayScale","fontScale","hudX","hudY","accent"})m_options[key]=values[key];rememberOverlay();emit optionsChanged();emit overlayChanged();scheduleSave();if(!m_test)m_platform->applyOptions();return;}
    if(name=="enableHook"){
        if(!args["confirmed"].toBool()||m_state["antiCheatStatus"]=="detected"){notify("In-game hooks are blocked while known anti-cheat indicators are present.","warning");return;}
        m_manualRiskKey=m_safetyKey+":"+m_state["antiCheatStatus"].toString();if(!m_test)m_platform->action("safetyFallback",{{"enabled",false}});
    }
    if(!m_test)m_platform->action(name,args);
}
bool Controller::selfTest(QString* error){
    int count=0;auto check=[&](bool ok,const char* text){if(!ok){*error=QString::fromUtf8(text);return false;}++count;return true;};
    if(!check(m_test,"Tests must use isolated mode"))return false;
    setOption("opacity",999);if(!check(m_options["opacity"]==100,"Clamp opacity"))return false;
    if(!check(unlock::supported("/games/hl2.exe")&&!unlock::supported("/games/unknown.exe")&&unlock::arguments({"-windowed","+fps_max","60","+map","test"},true)==QStringList{"-windowed","+map","test","+fps_max","0"},"Game-supported cap override preserves unrelated launch arguments"))return false;
    if(!check(lifecycle::selfTest(error),"Startup commands remain escaped without system changes"))return false;
    setOption("opacity",0);if(!check(m_options["opacity"]==0,"Allow a fully transparent background"))return false;setOption("opacity",100);
    setOption("accent",9);setOption("style",4);if(!check(m_options["accent"]==9&&m_options["style"]==4,"Additional accents and layouts survive validation"))return false;
    setOption("uiScale",0);if(!check(m_options["uiScale"]==0,"Preserve Auto sizing"))return false;
    setOption("cap",2);if(!check(m_options["cap"]==10,"Reject invalid low cap"))return false;
    setOption("volume",200);if(!check(m_options["volume"]==200,"Allow 200 percent"))return false;
    setOption("voicePreset",4);if(!check(m_options["low"]==6&&m_options["mid"]==-3&&m_options["high"]==0,"Bassy voice"))return false;
    setOption("low",0);if(!check(m_options["voicePreset"]==5,"Custom preset label"))return false;
    setOption("order",QVariantList{9,9,50,-1,2});if(!check(m_options["order"].toList().size()==16&&m_options["order"].toList().first()==9,"Repair malformed order"))return false;
    for(int i=0;i<16;++i){setReadout(i,false);if(!check(!(m_options["fields"].toInt()&(1<<i)),"Toggle readouts"))return false;}
    setReadout(0,true);moveReadout(0,-1);if(!check(readouts().size()==16,"Keep all reorder entries"))return false;
    saveProfile("Studio");if(!check(m_profiles.size()==1,"Save named preset"))return false;
    setOption("opacity",40);applyProfile(0);if(!check(m_options["opacity"]==100,"Restore preset"))return false;
    if(!check(save(),"Save configuration"))return false;
    Controller restored(m_root,true);if(!check(restored.m_options==m_options,"Round trip configuration"))return false;
    const auto exported=QUrl::fromLocalFile(m_root+"/presets.json");if(!check(exportProfiles(exported),"Export presets"))return false;
    if(!check(importProfiles(exported),"Import bounded preset file"))return false;
    setOption("deny",QStringList{"example","../bad","bad,name","x\ny"});if(!check(m_options["deny"].toStringList()==QStringList{"example"},"Reject rule paths and control characters"))return false;
    m_processes=QVariantList{QVariantMap{{"name","fixture.exe"},{"pid",123},{"protected",false}},QVariantMap{{"name","dwm.exe"},{"pid",124},{"protected",true}}};
    addProcessRule("fixture.exe",false);if(!check(m_options["allow"].toStringList().contains("fixture.exe"),"Add a listed executable to allow rules"))return false;
    addProcessRule("fixture.exe",true);if(!check(processRuleStatus("fixture.exe")=="Blocked","A process block wins over allow rules"))return false;
    const auto allow=m_options["allow"];addProcessRule("dwm.exe",false);addProcessRule("unlisted.exe",false);if(!check(m_options["allow"]==allow,"Protect immutable and absent process names"))return false;
#ifndef Q_OS_WIN
    const QByteArray malformed("not a zip file");const auto digest=QString::fromLatin1(QCryptographicHash::hash(malformed,QCryptographicHash::Sha256).toHex());
    if(!check(installNoiseArchive(m_root+"/invalid-plugin",malformed,digest,"v1.10").isEmpty()&&verifiedNoiseCache(m_root+"/invalid-plugin").isEmpty(),"Reject malformed plugin archives without installing code"))return false;
#endif
    if(!check(Telemetry::selfTest(m_root,error),"Read sensor units and reject unavailable or invalid values"))return false;
    if(!check(changelog().size()==5,"Include the full offline release history"))return false;
    sampleFrames(QList<double>(120,16.666666),"Fixture",42);if(!check(m_state["fps"].toDouble()>50&&!m_graph.isEmpty(),"Compute real portable frame statistics"))return false;
    const auto base=m_baseOverlay;const auto audio=m_options["volume"];
    m_games<<QVariantMap{{"id","profile-a"},{"name","Profile A"},{"executable","/games/a.exe"}}<<QVariantMap{{"id","profile-b"},{"name","Profile B"},{"executable","/games/b.exe"}};
    selectGame("profile-a");setOption("opacity",12);setOption("cap",144);setOption("style",4);selectGame("profile-b");
    if(!check(m_options["opacity"]==base["opacity"]&&m_options["cap"]==base["cap"],"New games inherit the default profile"))return false;
    setOption("opacity",78);setOption("cap",30);selectGame("profile-a");if(!check(m_options["opacity"]==12&&m_options["cap"]==144&&m_options["style"]==4&&m_options["volume"]==audio,"Game switches restore their own layout and limit"))return false;
    if(!check(save(),"Persist per-game profiles"))return false;Controller profilesRestored(m_root,true);profilesRestored.selectGame("profile-b");if(!check(profilesRestored.m_options["opacity"]==78&&profilesRestored.m_options["cap"]==30,"Per-game profiles survive restart"))return false;
    qInfo("PASS: %d desktop model/profile/audio-setting checks; no device changes",count);return true;
}
