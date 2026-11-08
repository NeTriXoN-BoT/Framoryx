#include "legacy_settings.hpp"
#include "presentation.hpp"
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <optional>

namespace {
std::optional<int> integer(const QString& text,int minimum,int maximum){bool ok=false;const int value=text.toInt(&ok);if(!ok||value<minimum||value>maximum)return {};return value;}
bool processName(const QString& name){return !name.isEmpty()&&name.size()<=160&&!name.contains(QRegularExpression("[\\x00-\\x1f/\\\\:,\"|*?]"));}
QVariantMap profile(const QString& text){
    const auto pieces=text.split('|');if(pieces.size()!=6&&pieces.size()!=7&&pieces.size()!=20&&pieces.size()!=26)return {};
    QList<int> numbers;for(const auto& piece:pieces){const auto value=integer(piece,0,numbers.isEmpty()?65535:10001);if(!value)return {};numbers<<*value;}
    fg::HudOptions hud;hud.fields=uint32_t(numbers[0]);hud.scale=numbers[1];hud.opacity=numbers[2];hud.accent=numbers[3];if(numbers.size()>=7)hud.style=numbers[6];
    if(numbers.size()>=20){hud.fontScale=numbers[7];hud.x=numbers[8]-1;hud.y=numbers[9]-1;hud.order=0;for(int i=10;i<numbers.size();++i){if(numbers[i]>15)return {};hud.order|=uint64_t(numbers[i])<<(4*(i-10));}if(numbers.size()==20)hud.order|=0xfedcba0000000000ull;}
    hud.validate();QVariantList order;for(int i=0;i<16;++i)order<<int((hud.order>>(4*i))&15);
    const int corner=std::clamp(numbers[4],0,3);
    return {{"fields",hud.fields},{"overlayScale",hud.scale},{"opacity",hud.opacity},{"accent",hud.accent},{"style",hud.style},{"fontScale",hud.fontScale},{"order",order},{"hudX",hud.x<0?(corner%2?.97:.03):hud.x/10000.},{"hudY",hud.y<0?(corner>=2?.97:.03):hud.y/10000.},{"mode",std::min(numbers[5],1)}};
}
}
LegacySettings readLegacySettings(const QString& path,const QVariantMap& defaults){
    LegacySettings result;const QFileInfo info(path);if(!info.isFile()||info.isSymLink()||info.size()>1024*1024)return result;
    QFile file(path);if(!file.open(QIODevice::ReadOnly))return result;result.native=file.read(1024*1024+1);if(result.native.size()>1024*1024){result.native.clear();return result;}
    const auto lines=QString::fromUtf8(result.native).remove(QChar(0xfeff)).split('\n');QVariantMap global=defaults,profiles,limits;QStringList allow,deny;bool haveRules=false,recognized=false;
    for(auto line:lines){if(line.endsWith('\r'))line.chop(1);if(line.startsWith(';')||line.size()>8192)continue;const int divider=line.indexOf('=');if(divider<1)continue;const auto key=line.left(divider),value=line.mid(divider+1);
        if(key=="hud_base"){const auto decoded=profile(value);if(!decoded.isEmpty()){for(auto i=decoded.begin();i!=decoded.end();++i)global[i.key()]=i.value();recognized=true;}}
        else if(key=="hud_profile"){const int split=value.indexOf('|');const auto name=value.left(split).toLower();const auto decoded=profile(value.mid(split+1));if(split>0&&processName(name)&&!decoded.isEmpty()&&profiles.size()<256){profiles[name]=decoded;recognized=true;}}
        else if(key=="limit"){const int split=value.indexOf('|');const auto name=value.left(split).toLower();const auto cap=integer(value.mid(split+1),0,1000);if(split>0&&processName(name)&&cap&&(*cap==0||*cap>=10)&&limits.size()<256)limits[name]=*cap;}
        else if(key=="allow"||key=="deny"){haveRules=true;auto& list=key=="allow"?allow:deny;if(list.size()<128)list<<value;}
        else if(key=="desktop_scale"){const auto scale=integer(value,0,175);if(scale&&(*scale==0||*scale>=60)){global["uiScale"]=*scale;recognized=true;}}
        else if(key=="cap"){const auto cap=integer(value,0,1000);if(cap&&(*cap==0||*cap>=10)){global["cap"]=*cap;recognized=true;}}
        else if(key=="motion")global["motion"]=value!="0";
        else if(key=="auto_borderless")global["autoBorderless"]=value=="1";
        else if(key=="auto_noise_updates")global["autoNoise"]=value!="0";
        else if(key=="mic_id")global["micId"]=QString::fromLatin1(value.toUtf8().toBase64());
        else if(key=="mic_options"){
            const auto pieces=value.split(' ',Qt::SkipEmptyParts);if(pieces.size()!=6)continue;QList<int> values;bool valid=true;for(const auto& piece:pieces){const auto number=integer(piece,-12,12);if(!number){valid=false;break;}values<<*number;}if(!valid)continue;
            global["noise"]=values[0]!=0;global["eq"]=values[1]!=0;global["noisePreset"]=std::clamp(values[2],0,2);global["low"]=values[3];global["mid"]=values[4];global["high"]=values[5];global["voicePreset"]=5;
        }
    }
    if(!recognized){result.native.clear();return result;}
    if(haveRules){global["allow"]=allow;global["deny"]=deny;}
    for(auto item=limits.begin();item!=limits.end();++item){if(!profiles.contains(item.key())&&profiles.size()>=256)continue;auto saved=profiles[item.key()].toMap();saved["cap"]=item.value();profiles[item.key()]=saved;}
    result.valid=true;result.options=global;result.gameOptions=profiles;return result;
}
