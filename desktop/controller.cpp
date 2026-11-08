#include "controller.hpp"
#include "audio_monitor.hpp"
#include "platform.hpp"
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
const QStringList labels{"Live FPS","Average FPS","1% low","Frame time","Frame graph","System CPU","System RAM","FPS limit","Renderer","Framoryx label"};
const QVariantList defaultOrder{0,1,2,3,4,5,6,7,8,9};
QVariantMap defaults(){return {{"motion",true},{"uiScale",0},{"overlayScale",0},{"opacity",90},{"style",0},{"accent",0},{"fields",1023},{"order",defaultOrder},{"hudX",.03},{"hudY",.03},{"fontScale",100},{"cap",60},{"mode",0},{"volume",100},{"bass",3},{"noise",false},{"eq",false},{"low",6},{"mid",-3},{"high",0},{"voicePreset",4},{"noisePreset",1},{"autoNoise",true},{"micId",""},{"selectedGame","auto"},{"autoBorderless",false},{"gamescope",false},{"allow",QStringList{"FramoryxTest.exe","FramoryxScene"}},{"deny",QStringList{"RiotClientServices.exe"}}};}
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
    QDir().mkpath(m_root);m_options=defaults();
    m_state={{"connected",false},{"capture",false},{"live",false},{"overlay",false},{"fps",0.},{"average",0.},{"low",0.},{"frameTime",0.},{"cpu",0.},{"ram",0.},{"totalRam",0.},{"renderer","Waiting for frames"},{"game","Choose a game"},{"notice","Ready when you are."},{"audioStatus","Choose effects, then Apply."},{"noiseStatus","Automatic updates are ready."},{"noiseBusy",false},{"inputLevel",0.},{"outputLevel",0.},{"recording",false},{"playing",false},{"sampleReady",false},{"volume",100},{"micConfigured",false},{"bassConfigured",false},{"boostConfigured",false},{"hookActive",false},{"pending",false},{"session",qEnvironmentVariable("XDG_SESSION_TYPE","unknown")}};
    load();m_clock.start();
    m_audio=std::make_unique<AudioMonitor>(this);
    m_platform=std::make_unique<Platform>(this);
    m_saveTimer.setSingleShot(true);m_saveTimer.setInterval(220);
    connect(&m_saveTimer,&QTimer::timeout,this,[this]{save();});
    connect(&m_poll,&QTimer::timeout,this,&Controller::poll);
    connect(&m_media,&QMediaDevices::audioInputsChanged,this,&Controller::refreshDevices);
    connect(&m_media,&QMediaDevices::audioOutputsChanged,this,&Controller::refreshDevices);
    if(!m_test){refreshDevices();m_platform->start();m_poll.start(250);}
}
Controller::~Controller(){m_poll.stop();m_audio->stop();m_platform->shutdown();save();}
QString Controller::platformName()const{
#ifdef Q_OS_WIN
    return "Windows";
#else
    return "Linux";
#endif
}
QVariantMap Controller::cleanOptions(const QVariantMap& input)const{
    auto out=defaults();
    const QMap<QString,QPair<int,int>> ranges{{"uiScale",{60,175}},{"overlayScale",{50,300}},{"opacity",{30,100}},{"style",{0,2}},{"accent",{0,3}},{"fontScale",{80,140}},{"cap",{0,1000}},{"mode",{0,2}},{"volume",{0,200}},{"bass",{0,9}},{"low",{-12,12}},{"mid",{-12,12}},{"high",{-12,12}},{"voicePreset",{0,5}},{"noisePreset",{0,2}}};
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
        }else if(it.key()=="fields")out[it.key()]=it->toUInt()&1023u;
        else if(it.key()=="order"){
            const auto list=it->toList();QSet<int> seen;QVariantList order;
            for(const auto& item:list){const int n=item.toInt();if(n>=0&&n<10&&!seen.contains(n)){seen.insert(n);order<<n;}}
            for(int n=0;n<10;++n)if(!seen.contains(n))order<<n;
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
    auto cleanList=[&](const QJsonArray& list,bool games){
        QVariantList out;
        for(const auto& value:list){if(out.size()>=128||!value.isObject())break;
            const auto object=value.toObject();const auto name=object.value("name").toString().trimmed().left(80);if(name.isEmpty())continue;
            if(games){const auto exe=object.value("executable").toString();if(exe.size()>4096||!QDir::isAbsolutePath(exe))continue;
                out<<QVariantMap{{"id",object.value("id").toString().left(80)},{"name",name},{"executable",exe},{"arguments",object.value("arguments").toString().left(4096)}};
            }else out<<QVariantMap{{"name",name},{"options",cleanOptions(object.value("options").toObject().toVariantMap())}};
        }return out;
    };
    m_games=cleanList(document.value("games").toArray(),true);
    m_profiles=cleanList(document.value("profiles").toArray(),false);
}
bool Controller::save(){
    QSaveFile file(m_root+"/settings.json");
    const QJsonObject object{{"schema",3},{"options",QJsonObject::fromVariantMap(m_options)},{"games",QJsonArray::fromVariantList(m_games)},{"profiles",QJsonArray::fromVariantList(m_profiles)}};
    if(!file.open(QIODevice::WriteOnly)||file.write(QJsonDocument(object).toJson())<0||!file.commit()){
        notify("Preferences could not be saved. Check the configuration folder.","warning");return false;
    }return true;
}
void Controller::scheduleSave(){m_saveTimer.start();}
void Controller::setOption(const QString& key,const QVariant& value){
    if(!m_options.contains(key))return;
    auto requested=m_options;requested[key]=value;
    auto next=cleanOptions(requested);if(next==m_options)return;
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
    if(key=="volume")setState("volume",m_options["volume"]);
    emit optionsChanged();scheduleSave();
    if(!m_test)m_platform->applyOptions(key);
}
QVariantList Controller::readouts()const{
    QVariantList out;const int fields=m_options["fields"].toInt();
    for(const auto& value:m_options["order"].toList()){
        const int i=value.toInt();out<<QVariantMap{{"index",i},{"name",labels.value(i)},{"enabled",bool(fields&(1<<i))}};
    }return out;
}
void Controller::setReadout(int index,bool visible){if(index<0||index>=10)return;int fields=m_options["fields"].toInt();setOption("fields",visible?fields|(1<<index):fields&~(1<<index));}
void Controller::moveReadout(int index,int direction){auto order=m_options["order"].toList();const int at=order.indexOf(index),to=at+(direction<0?-1:1);if(at<0||to<0||to>=order.size())return;order.swapItemsAt(at,to);setOption("order",order);}
void Controller::selectGame(const QString& id){setOption("selectedGame",id);if(!m_test)m_platform->select(id);}
void Controller::addGame(const QUrl& url,const QString& label,const QString& arguments){
    const QString path=QFileInfo(url.toLocalFile()).absoluteFilePath();
    const QFileInfo info(path);
    if(!url.isLocalFile()||!info.isFile()||info.isSymLink()||arguments.size()>4096){notify("Choose a local game executable and valid arguments.","warning");return;}
    if(m_games.size()>=128){notify("The game library is full.","warning");return;}
    for(const auto& entry:m_games)if(entry.toMap()["executable"].toString()==path){selectGame(entry.toMap()["id"].toString());notify("That game is already in your library.");return;}
    const QString name=label.trimmed().isEmpty()?info.completeBaseName():label.trimmed().left(80);
    const QString id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_games<<QVariantMap{{"id",id},{"name",name},{"executable",path},{"arguments",arguments}};
    emit gamesChanged();selectGame(id);scheduleSave();notify("Game added. Launch it from your library when ready.");
}
void Controller::removeGame(const QString& id){for(int i=0;i<m_games.size();++i)if(m_games[i].toMap()["id"].toString()==id){m_games.removeAt(i);break;}if(m_options["selectedGame"]==id)selectGame("auto");emit gamesChanged();scheduleSave();}
void Controller::saveProfile(const QString& name){const auto clean=name.trimmed().left(80);if(clean.isEmpty())return;for(int i=0;i<m_profiles.size();++i)if(m_profiles[i].toMap()["name"].toString()==clean){m_profiles[i]=QVariantMap{{"name",clean},{"options",m_options}};emit profilesChanged();scheduleSave();notify("Preset updated.");return;}if(m_profiles.size()>=128){notify("Remove a preset before adding another.","warning");return;}m_profiles<<QVariantMap{{"name",clean},{"options",m_options}};emit profilesChanged();scheduleSave();notify("Preset saved.");}
void Controller::applyProfile(int index){if(index<0||index>=m_profiles.size())return;const auto selected=m_options["selectedGame"];const auto mic=m_options["micId"];const auto volume=m_options["volume"];m_options=cleanOptions(m_profiles[index].toMap()["options"].toMap());m_options["selectedGame"]=selected;m_options["micId"]=mic;m_options["volume"]=volume;emit optionsChanged();scheduleSave();if(!m_test)m_platform->applyOptions();notify("Preset loaded. Apply audio effects separately when ready.");}
void Controller::removeProfile(int index){if(index<0||index>=m_profiles.size())return;m_profiles.removeAt(index);emit profilesChanged();scheduleSave();}
bool Controller::exportProfiles(const QUrl& url){if(!url.isLocalFile())return false;QSaveFile file(url.toLocalFile());QVariantList exported;for(const auto& value:m_profiles){auto preset=value.toMap();auto options=preset["options"].toMap();options.remove("micId");options.remove("selectedGame");preset["options"]=options;exported<<preset;}QJsonObject document{{"format","Framoryx presets"},{"version",1},{"presets",QJsonArray::fromVariantList(exported)}};if(!file.open(QIODevice::WriteOnly)||file.write(QJsonDocument(document).toJson())<0||!file.commit()){notify("Presets could not be exported.","warning");return false;}notify("Presets exported. Device identifiers and game paths are excluded from the export.");return true;}
bool Controller::importProfiles(const QUrl& url){QJsonObject object;if(!url.isLocalFile()||!boundedJson(url.toLocalFile(),&object)||object["format"]!="Framoryx presets"||object["version"].toInt()!=1){notify("This is not a supported Framoryx preset file.","warning");return false;}int count=0;for(const auto& value:object["presets"].toArray()){if(m_profiles.size()>=128)break;if(!value.isObject())continue;const auto entry=value.toObject();const auto name=entry["name"].toString().trimmed().left(80);if(name.isEmpty())continue;auto options=cleanOptions(entry["options"].toObject().toVariantMap());options["micId"]="";options["selectedGame"]="auto";m_profiles<<QVariantMap{{"name",name},{"options",options}};++count;}emit profilesChanged();scheduleSave();notify(QString("Imported %1 presets. Nothing was launched or applied to audio devices.").arg(count));return count>0;}
void Controller::notify(const QString& text,const QString& severity){m_state["notice"]=text;emit stateChanged();emit toast(text,severity);}
void Controller::setState(const QString& key,const QVariant& value){if(m_state.value(key)==value)return;m_state[key]=value;emit stateChanged();}
void Controller::setWindowActive(bool active){m_active=active;}
void Controller::refreshDevices(){
    QVariantList out;for(const auto& device:QMediaDevices::audioInputs())out<<QVariantMap{{"id",QString::fromLatin1(device.id().toBase64())},{"name",device.description()},{"rate",device.preferredFormat().sampleRate()},{"channels",device.preferredFormat().channelCount()}};
    m_devices=out;emit devicesChanged();
}
void Controller::refreshCapabilities(){if(!m_test)m_platform->refresh();}
void Controller::receive(const QJsonObject& incoming){
    auto next=incoming.toVariantMap();
    if(next.contains("games")){m_state["windows"]=next.take("games");}
    if(next.contains("microphones")){const auto list=next.take("microphones").toList();m_state["engineMicrophones"]=list;}
    if(next.contains("graph"))m_graph=next.take("graph").toList();
    for(auto it=next.begin();it!=next.end();++it)m_state[it.key()]=it.value();
    emit stateChanged();
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
    if(name=="refresh"){refreshDevices();refreshCapabilities();return;}
    if(name=="record"){if(m_test)return;m_audio->record(QByteArray::fromBase64(m_options["micId"].toByteArray()));return;}
    if(name=="playSample"){if(!m_test)m_audio->play();return;}
    if(name=="stopSample"){m_audio->stop();return;}
    if(name=="export"){writeCsv(QUrl(args["url"].toString()).toLocalFile());return;}
    if(name=="openConfig"){QDesktopServices::openUrl(QUrl::fromLocalFile(m_root));return;}
    if(name=="guide"){QDesktopServices::openUrl(QUrl("https://github.com/NeTriXoN-BoT/Framoryx#readme"));return;}
    if(name=="release"){QDesktopServices::openUrl(QUrl("https://github.com/NeTriXoN-BoT/Framoryx/releases/latest"));return;}
    if(name=="quit"){emit quitRequested();return;}
    if(name=="resetOverlay"){auto values=defaults();for(const auto& key:QStringList{"fields","order","style","opacity","overlayScale","fontScale","hudX","hudY","accent"})m_options[key]=values[key];emit optionsChanged();scheduleSave();if(!m_test)m_platform->applyOptions();return;}
    if(!m_test)m_platform->action(name,args);
}
bool Controller::selfTest(QString* error){
    int count=0;auto check=[&](bool ok,const char* text){if(!ok){*error=QString::fromUtf8(text);return false;}++count;return true;};
    if(!check(m_test,"Tests must use isolated mode"))return false;
    setOption("opacity",999);if(!check(m_options["opacity"]==100,"Clamp opacity"))return false;
    setOption("uiScale",0);if(!check(m_options["uiScale"]==0,"Preserve Auto sizing"))return false;
    setOption("cap",2);if(!check(m_options["cap"]==10,"Reject invalid low cap"))return false;
    setOption("volume",200);if(!check(m_options["volume"]==200,"Allow 200 percent"))return false;
    setOption("voicePreset",4);if(!check(m_options["low"]==6&&m_options["mid"]==-3&&m_options["high"]==0,"Bassy voice"))return false;
    setOption("low",0);if(!check(m_options["voicePreset"]==5,"Custom preset label"))return false;
    setOption("order",QVariantList{9,9,50,-1,2});if(!check(m_options["order"].toList().size()==10&&m_options["order"].toList().first()==9,"Repair malformed order"))return false;
    for(int i=0;i<10;++i){setReadout(i,false);if(!check(!(m_options["fields"].toInt()&(1<<i)),"Toggle readouts"))return false;}
    setReadout(0,true);moveReadout(0,-1);if(!check(readouts().size()==10,"Keep all reorder entries"))return false;
    saveProfile("Studio");if(!check(m_profiles.size()==1,"Save named preset"))return false;
    setOption("opacity",40);applyProfile(0);if(!check(m_options["opacity"]==100,"Restore preset"))return false;
    if(!check(save(),"Save configuration"))return false;
    Controller restored(m_root,true);if(!check(restored.m_options==m_options,"Round trip configuration"))return false;
    const auto exported=QUrl::fromLocalFile(m_root+"/presets.json");if(!check(exportProfiles(exported),"Export presets"))return false;
    if(!check(importProfiles(exported),"Import bounded preset file"))return false;
    setOption("deny",QStringList{"example","../bad","bad,name","x\ny"});if(!check(m_options["deny"].toStringList()==QStringList{"example"},"Reject rule paths and control characters"))return false;
    sampleFrames(QList<double>(120,16.666666),"Fixture",42);if(!check(m_state["fps"].toDouble()>50&&!m_graph.isEmpty(),"Compute real portable frame statistics"))return false;
    qInfo("PASS: %d desktop model/profile/audio-setting checks; no device changes",count);return true;
}
