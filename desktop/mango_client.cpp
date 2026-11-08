#include "mango_client.hpp"
#include <QFile>
#include <QRegularExpression>
#include <sys/socket.h>
#include <unistd.h>
#include <cmath>

MangoClient::MangoClient(QObject* parent):QObject(parent){
    m_clock.start();m_timer.setInterval(100);m_socket.setSocketOptions(QLocalSocket::AbstractNamespaceOption);
    connect(&m_timer,&QTimer::timeout,this,&MangoClient::poll);
    connect(&m_socket,&QLocalSocket::connected,this,[this]{
        ucred peer{};socklen_t size=sizeof(peer);
        if(getsockopt(int(m_socket.socketDescriptor()),SOL_SOCKET,SO_PEERCRED,&peer,&size)||peer.uid!=getuid()||quint32(peer.pid)!=m_pid){m_socket.abort();return;}
        m_verified=true;m_socket.write(":framoryx_frames;");
    });
    connect(&m_socket,&QLocalSocket::readyRead,this,&MangoClient::receive);
    connect(&m_socket,&QLocalSocket::disconnected,this,[this]{m_verified=false;m_supported=false;m_bytes.clear();m_retry=m_clock.elapsed()+1000;emit disconnectedFromGame();});
    connect(&m_socket,&QLocalSocket::errorOccurred,this,[this](QLocalSocket::LocalSocketError){m_verified=false;m_retry=m_clock.elapsed()+1000;});
}
void MangoClient::start(const QString& prefix,bool collecting){if(m_prefix==prefix&&m_timer.isActive()){setCollecting(collecting);return;}stop();if(!QRegularExpression("^framoryx-[a-f0-9]{24}-$").match(prefix).hasMatch())return;m_prefix=prefix;m_collecting=collecting;m_retry=0;m_timer.start();poll();}
void MangoClient::setCollecting(bool collecting){m_collecting=collecting;if(connected())m_socket.write(collecting?":framoryx_frames;":":framoryx_stop;");}
void MangoClient::stop(){m_timer.stop();if(m_verified){m_socket.write(":framoryx_stop;");m_socket.flush();}m_verified=false;m_supported=false;m_socket.abort();m_prefix.clear();m_bytes.clear();m_pid=0;m_dropped=0;m_surfaceWidth=0;m_surfaceHeight=0;}
bool MangoClient::connected()const{return m_verified&&m_supported;}
void MangoClient::setLimit(int cap,int fontPixels){const auto packet=":framoryx_limit="+QByteArray::number(qBound(0,cap,1000))+'|'+QByteArray::number(qBound(12,fontPixels,200))+';';if(packet==m_limitPacket)return;m_limitPacket=packet;if(connected())m_socket.write(packet);}
void MangoClient::configure(const QVariantMap& options,bool visible){
    QList<QByteArray> values{QByteArray("1")};
    for(const auto& key:QStringList{"fields","overlayScale","opacity","accent","style","fontScale"})values<<QByteArray::number(options[key].toInt());
    values<<QByteArray::number(qRound(options["hudX"].toDouble()*10000))<<QByteArray::number(qRound(options["hudY"].toDouble()*10000))<<QByteArray::number(options["cap"].toInt())<<QByteArray::number(visible?1:0);
    for(const auto& item:options["order"].toList())values<<QByteArray::number(item.toInt());
    QByteArray body;for(const auto& value:values){if(!body.isEmpty())body+='|';body+=value;}
    const auto packet=":framoryx_hud="+body+';';if(packet==m_hudPacket)return;m_hudPacket=packet;if(connected())m_socket.write(packet);
}
void MangoClient::metrics(const QVariantMap& state){
    QByteArray body;
    for(const auto& key:QStringList{"average","low","cpu","ram","gpu","vramPercent","gpuTemp","cpuTemp","gpuPower","gpuClock"}){
        bool ok=false;const double number=state[key].toDouble(&ok);const bool staleFrame=(key=="average"||key=="low")&&!state["live"].toBool();const int value=!staleFrame&&ok&&!state[key].isNull()&&std::isfinite(number)&&number>=0&&number<=1000000?qRound(number*100):-1;
        if(!body.isEmpty())body+='|';body+=QByteArray::number(value);
    }m_metricPacket=":framoryx_metrics="+body+';';if(connected())m_socket.write(m_metricPacket);
}
void MangoClient::poll(){
    if(m_verified){if(m_socket.bytesToWrite()<16384){if(m_collecting||!m_supported)m_socket.write(":framoryx_frames;");if(connected()&&++m_pollCount%10==0){if(!m_hudPacket.isEmpty())m_socket.write(m_hudPacket);if(!m_limitPacket.isEmpty())m_socket.write(m_limitPacket);}}else m_socket.abort();return;}
    if(m_socket.state()==QLocalSocket::ConnectingState){if(m_clock.elapsed()-m_connectTime>3000)m_socket.abort();return;}
    if(m_prefix.isEmpty()||m_clock.elapsed()<m_retry)return;m_retry=m_clock.elapsed()+1000;
    QFile sockets("/proc/net/unix");if(!sockets.open(QIODevice::ReadOnly))return;
    const auto lines=sockets.read(2*1024*1024).split('\n');
    for(const auto& line:lines){const auto cells=line.simplified().split(' ');if(cells.size()<8)continue;const QString path=QString::fromLatin1(cells.last());if(!path.startsWith('@'+m_prefix))continue;bool ok=false;const auto pid=path.mid(m_prefix.size()+1).toUInt(&ok);if(!ok||!pid)continue;m_pid=pid;m_connectTime=m_clock.elapsed();m_socket.connectToServer(path.mid(1),QIODevice::ReadWrite);return;}
}
void MangoClient::receive(){
    m_bytes+=m_socket.readAll();if(m_bytes.size()>65536){m_socket.abort();m_bytes.clear();return;}
    int end;while((end=m_bytes.indexOf(';'))>=0){const auto packet=m_bytes.left(end);m_bytes.remove(0,end+1);if(!m_verified)continue;if(packet.startsWith(":FramoryxSurface=")){const auto parts=packet.mid(17).split('|');if(parts.size()==3){bool drawn=false;const auto count=parts[2].toULongLong(&drawn);if(drawn)emit hudFrames(count);bool a=false,b=false;const int width=parts[0].toInt(&a),height=parts[1].toInt(&b);if(a&&b&&width>0&&height>0&&width<=32768&&height<=32768&&(width!=m_surfaceWidth||height!=m_surfaceHeight)){m_surfaceWidth=width;m_surfaceHeight=height;emit surfaceSize(width,height);}}continue;}if(packet.startsWith(":FramoryxLimit=")){bool ok=false;const int value=packet.mid(15).toInt(&ok);if(ok&&value>=0&&value<=1000)emit limitApplied(value);continue;}if(!packet.startsWith(":FramoryxFrames="))continue;const auto body=packet.mid(16);const auto divider=body.indexOf('|');if(divider<0)continue;bool valid=false;const auto dropped=body.left(divider).toULongLong(&valid);if(!valid)continue;if(!m_supported){m_supported=true;if(!m_hudPacket.isEmpty())m_socket.write(m_hudPacket);if(!m_metricPacket.isEmpty())m_socket.write(m_metricPacket);if(!m_limitPacket.isEmpty())m_socket.write(m_limitPacket);if(!m_collecting)m_socket.write(":framoryx_stop;");emit connectedToGame(m_pid);}if(dropped!=m_dropped){m_dropped=dropped;emit lostSamples(dropped);}
        QList<double> durations;const auto pieces=body.mid(divider+1).split(',');if(pieces.size()>250)continue;for(const auto& value:pieces){const auto microseconds=value.toUInt(&valid);if(valid&&microseconds>0&&microseconds<60000000)durations<<microseconds/1000.;}if(m_collecting&&!durations.isEmpty())emit frames(durations,m_pid);
    }
}
