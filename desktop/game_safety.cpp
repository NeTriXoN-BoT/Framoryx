#include "game_safety.hpp"
#include "process_catalog.hpp"
#include "core.hpp"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#endif
namespace {
bool indicator(const QString& name) {
    static const std::vector<std::string> names{
        "easyanticheat*","battleye*","beservice*","beclient*","bedaisy*","vgk.sys","vgc.exe",
        "eaanticheat*","eanticheat*","faceit*","equ8*","anticheatexpert*","ace-base*","ace-protect*","sguard*"};
    return fg::matches(names,name.toStdString());
}
}
QVariantMap assessGame(const QString& requested,quint32 pid) {
    QString executable=requested;QStringList evidence;bool inspected=false,complete=true;
    const auto processes=runningProcesses();
    if(processes.isEmpty()||processes.size()>=32768)complete=false;
    for(const auto& item:processes){const auto process=item.toMap();if(indicator(process["name"].toString())){evidence<<"Active process (system-wide): "+process["name"].toString();if(evidence.size()>=8)break;}}
#ifdef Q_OS_WIN
    if(pid){
        HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
        wchar_t path[32768]{};DWORD length=32768;
        if(process){if(QueryFullProcessImageNameW(process,0,path,&length)){
            const QString actual=QString::fromWCharArray(path,int(length));
            if(!executable.isEmpty()&&QFileInfo(actual).fileName().compare(QFileInfo(executable).fileName(),Qt::CaseInsensitive)!=0)complete=false;
            executable=actual;
        }else complete=false;CloseHandle(process);}else complete=false;
        HANDLE modules=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);
        if(modules!=INVALID_HANDLE_VALUE){MODULEENTRY32W module{};module.dwSize=sizeof(module);if(Module32FirstW(modules,&module)){inspected=true;int count=0;do{if(indicator(QString::fromWCharArray(module.szModule))&&evidence.size()<16)evidence<<"Game module: "+QString::fromWCharArray(module.szModule);if(++count>=8192){complete=false;break;}}while(Module32NextW(modules,&module));}CloseHandle(modules);}else complete=false;
    }
    SC_HANDLE manager=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_ENUMERATE_SERVICE);
    if(manager){DWORD bytes=0,count=0,resume=0;EnumServicesStatusExW(manager,SC_ENUM_PROCESS_INFO,SERVICE_WIN32|SERVICE_DRIVER,SERVICE_ACTIVE,nullptr,0,&bytes,&count,&resume,nullptr);
        if(bytes>0&&bytes<=8*1024*1024){std::vector<BYTE> buffer(bytes+1024);resume=0;if(EnumServicesStatusExW(manager,SC_ENUM_PROCESS_INFO,SERVICE_WIN32|SERVICE_DRIVER,SERVICE_ACTIVE,buffer.data(),DWORD(buffer.size()),&bytes,&count,&resume,nullptr)){
            auto entries=reinterpret_cast<ENUM_SERVICE_STATUS_PROCESSW*>(buffer.data());
            for(DWORD i=0;i<count&&evidence.size()<16;++i){const auto name=QString::fromWCharArray(entries[i].lpServiceName);if(indicator(name)||name.compare("vgk",Qt::CaseInsensitive)==0||name.compare("vgc",Qt::CaseInsensitive)==0)evidence<<"Active service (system-wide): "+name;}
        }else complete=false;}else complete=false;CloseServiceHandle(manager);
    }else complete=false;
#else
    if(pid){
        const auto actual=QFileInfo(QString("/proc/%1/exe").arg(pid)).symLinkTarget();if(!actual.isEmpty())executable=actual;
        QFile maps(QString("/proc/%1/maps").arg(pid));
        if(maps.open(QIODevice::ReadOnly)){const auto bytes=maps.read(8*1024*1024+1);inspected=bytes.size()<=8*1024*1024;if(!inspected)complete=false;
            QSet<QString> seen;for(const auto& line:QString::fromUtf8(bytes).split('\n')){const int slash=line.indexOf('/');if(slash<0)continue;const auto file=QFileInfo(line.mid(slash)).fileName();if(indicator(file)&&!seen.contains(file)&&evidence.size()<16){seen.insert(file);evidence<<"Game module: "+file;}}
        }else complete=false;
    }
#endif
    const QFileInfo game(executable);
    if(!game.isFile()||!game.isAbsolute()||executable.startsWith("//"))complete=false;
    else {
        // Names only, bounded known locations; no game-memory reads or recursive disk scan.
        const QStringList folders{"","EasyAntiCheat","EasyAntiCheat_EOS","BattlEye","AntiCheat","Binaries/Win64","Game/Binaries/Win64"};
        for(const auto& relative:folders){const QString directory=game.absolutePath()+"/"+relative;if(!QFileInfo::exists(directory))continue;QDirIterator iterator(directory,QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot|QDir::NoSymLinks);int visited=0;
            while(iterator.hasNext()){iterator.next();if(++visited>512){complete=false;break;}if(indicator(iterator.fileName())&&evidence.size()<16)evidence<<"Game installation: "+(relative.isEmpty()?QString():relative+"/")+iterator.fileName();}
        }
    }
    const QString status=!evidence.isEmpty()?"detected":complete&&inspected?"no-known-indicator":"unknown";
    return {{"status",status},{"evidence",evidence},{"detail",!evidence.isEmpty()?evidence.join("\n"):status=="unknown"?"Inspection is incomplete or the game has not started. An external overlay is preferred.":"No known indicators were found in the checked processes, modules and installation locations. This does not establish game approval."},{"executable",executable},{"pid",pid}};
}
