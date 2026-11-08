#pragma once
#include <QObject>
#include <QLocalSocket>
#include <QTimer>
#include <QElapsedTimer>
#include <QList>
#include <QVariantMap>
class MangoClient final : public QObject {
    Q_OBJECT
public:
    explicit MangoClient(QObject* parent=nullptr);
    void start(const QString& prefix,bool collecting=true);
    void stop();
    void setCollecting(bool collecting);
    void setLimit(int cap,int fontPixels);
    void configure(const QVariantMap& options,bool visible);
    void metrics(const QVariantMap& state);
    bool connected() const;
signals:
    void frames(const QList<double>& durations,quint32 pid);
    void connectedToGame(quint32 pid);
    void disconnectedFromGame();
    void lostSamples(quint64 count);
    void limitApplied(int cap);
    void surfaceSize(int width,int height);
private:
    QLocalSocket m_socket;
    QTimer m_timer;
    QElapsedTimer m_clock;
    QString m_prefix;
    QByteArray m_bytes;
    QByteArray m_hudPacket,m_metricPacket,m_limitPacket;
    unsigned m_pollCount=0;
    quint32 m_pid=0;
    int m_surfaceWidth=0,m_surfaceHeight=0;
    quint64 m_dropped=0;
    qint64 m_retry=0,m_connectTime=0;
    bool m_verified=false,m_supported=false,m_collecting=true;
    void poll();
    void receive();
};
