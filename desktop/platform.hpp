#pragma once
#include <QObject>
#include <QSet>
#include <QProcess>
#include <QTimer>
#include <QFile>
#include <QVariantMap>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <functional>
#include <memory>
class Controller;
class PassiveCapture;
class Platform : public QObject {
    Q_OBJECT
public:
    explicit Platform(Controller* controller);
    ~Platform() override;
    void start();
    void poll();
    void refresh();
    void applyOptions(const QString& key = {});
    void select(const QString& id);
    void action(const QString& name, const QVariantMap& args);
    void shutdown();
    QString launchCommand() const;
    QString overlayConfigPath() const;
private:
    Controller* c;
    std::unique_ptr<PassiveCapture> m_passive;
    bool attachPassive(bool explain=false);
    QString m_passiveName;quint32 m_passivePid=0;
    QSet<QProcess*> m_expectedStops;
    QMap<QProcess*,QString> m_pendingAudio;
    QMap<QProcess*,quint64> m_audioGeneration;
    QString m_outputTarget;
    void startAudio(QProcess* process);
    QProcess m_engine, m_game, m_mic, m_output;
    QByteArray m_incoming;
    QString m_token, m_logFile;
    qint64 m_launchedPid = 0;
    QString m_lastLaunchId,m_launchStart;
    QMap<qint64,QString> m_fixtures;
    void stopFixtures();
    bool m_monitoring = false;
    qint64 m_logOffset = 0;
    QByteArray m_logPending;
    QStringList m_logHeaders;
    QTimer m_scanTimer, m_volumeTimer, m_optionTimer;
    QSet<QString> m_pendingOptions;
    void flushOptions();
    void flushVolume();
    QVariantMap m_windowRestores;
    QNetworkAccessManager m_network;
    bool m_closing = false, m_refreshing = false;
    quint64 m_oldCpuTotal = 0, m_oldCpuIdle = 0;
    QList<QProcess*> m_commands;
    void command(const QString& program, const QStringList& args, std::function<void(int,QByteArray)> complete = {}, int timeout = 6000);
    void send(const QJsonObject& request);
    void linuxScan();
    void linuxLaunch();
    void readLog();
    void linuxAudio(bool mic,bool verified=false);
    void updateNoise(bool force);
    void beginNoiseDownload(const QVariantMap& cached);
    void borderless(bool restore);
};
