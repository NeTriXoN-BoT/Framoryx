#pragma once
#include <QFileInfo>
#include <QStringList>
namespace unlock {
inline bool supported(const QString& path) {
    // These Source games expose fps_max themselves. This is a launch option,
    // never a binary patch or an attempt to defeat a protected game cap.
    const auto file=QFileInfo(path).fileName().toLower();
    return QStringList{"hl2","hl2.exe","portal2","portal2.exe","left4dead2","left4dead2.exe"}.contains(file);
}
inline QStringList arguments(QStringList original,bool enabled) {
    if(!enabled)return original;
    QStringList result;
    for(int i=0;i<original.size();++i){
        if(original[i].compare("+fps_max",Qt::CaseInsensitive)==0){
            if(i+1<original.size()){bool numeric=false;original[i+1].toDouble(&numeric);if(numeric)++i;}
            continue;
        }
        result<<original[i];
    }
    return result<<"+fps_max"<<"0";
}
}
