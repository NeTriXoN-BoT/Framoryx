#pragma once
#include <QObject>
#include <QLocalSocket>
#include <QTimer>
#include <QElapsedTimer>
#include <QList>
class MangoClient final : public QObject {
    Q_OBJECT
public:
    explicit MangoClient(QObject* parent=nullptr);
    void start(const QString& prefix);
    void stop();
    void reload();
    bool connected() const;
signals:
    void frames(const QList<double>& durations,quint32 pid);
    void connectedToGame(quint32 pid);
    void lostSamples(quint64 count);
private:
    QLocalSocket m_socket;
    QTimer m_timer;
    QElapsedTimer m_clock;
    QString m_prefix;
    QByteArray m_bytes;
    quint32 m_pid=0;
    quint64 m_dropped=0;
    qint64 m_retry=0,m_connectTime=0;
    bool m_verified=false,m_supported=false;
    void poll();
    void receive();
};
