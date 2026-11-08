#include "process_catalog.hpp"
#include "core.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#endif
QVariantList runningProcesses() {
    QVariantList list;
    auto append=[&](quint32 pid,const QString& name){
        if (list.size()>=32768 || name.isEmpty()) return;
        const auto native=name.toStdString();
        const bool system=fg::matches(fg::systemDeny,native)||fg::matches(fg::antiCheatNames,native)||
            QStringList{"framoryx","systemd","pipewire","wireplumber","Xorg","Xwayland","gnome-shell","kwin_wayland","kwin_x11"}.contains(name,Qt::CaseInsensitive);
        list<<QVariantMap{{"pid",pid},{"name",name},{"protected",system}};
    };
#ifdef Q_OS_WIN
    const HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snapshot==INVALID_HANDLE_VALUE)return list;
    PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);
    if(Process32FirstW(snapshot,&entry))do{append(entry.th32ProcessID,QString::fromWCharArray(entry.szExeFile));}while(Process32NextW(snapshot,&entry));
    CloseHandle(snapshot);
#else
    for(const auto& item:QDir("/proc").entryList(QDir::Dirs|QDir::NoDotAndDotDot)){
        bool ok=false;const auto pid=item.toUInt(&ok);if(!ok)continue;
        QString name=QFileInfo(QFileInfo("/proc/"+item+"/exe").symLinkTarget()).fileName();
        if(name.isEmpty()||name.startsWith("wine")){QFile comm("/proc/"+item+"/comm");if(comm.open(QIODevice::ReadOnly))name=QString::fromUtf8(comm.read(512)).trimmed();}
        append(pid,name);
    }
#endif
    std::sort(list.begin(),list.end(),[](const QVariant& a,const QVariant& b){const auto first=a.toMap(),second=b.toMap();const int order=QString::compare(first["name"].toString(),second["name"].toString(),Qt::CaseInsensitive);return order?order<0:first["pid"].toUInt()<second["pid"].toUInt();});
    return list;
}
