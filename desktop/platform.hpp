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
#ifndef Q_OS_WIN
#include "x11_window.hpp"
#endif
class Controller;
class PassiveCapture;
class MangoClient;
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
private:
    Controller* c;
    std::unique_ptr<PassiveCapture> m_passive;
    std::unique_ptr<MangoClient> m_mango;
    bool attachPassive(bool explain=false);
    QString m_passiveName;quint32 m_passivePid=0;
    QSet<QProcess*> m_expectedStops;
    QMap<QProcess*,QString> m_pendingAudio;
    QMap<QProcess*,quint64> m_audioGeneration;
    QString m_outputTarget;
    void startAudio(QProcess* process);
    QProcess m_engine, m_game, m_mic, m_output;
    QByteArray m_incoming;
    QString m_token;
    qint64 m_launchedPid = 0;
    QString m_lastLaunchId,m_launchStart;
    QMap<qint64,QString> m_fixtures;
    void stopFixtures();
    bool m_monitoring = false;
    bool m_volumeWriting = false, m_volumePending = false;
    QTimer m_scanTimer, m_volumeTimer, m_optionTimer;
    QSet<QString> m_pendingOptions;
    void flushOptions();
    void flushVolume();
#ifndef Q_OS_WIN
    QMap<QString,x11::WindowState> m_windowRestores;
#endif
    QNetworkAccessManager m_network;
    bool m_closing = false, m_refreshing = false;
    quint64 m_oldCpuTotal = 0, m_oldCpuIdle = 0;
    QList<QProcess*> m_commands;
    void command(const QString& program, const QStringList& args, std::function<void(int,QByteArray)> complete = {}, int timeout = 6000);
    void send(const QJsonObject& request);
    void linuxScan();
    void linuxLaunch();
    QString mangoPrefix() const;
    QString mangoConfiguration() const;
    int mangoFontPixels() const;
    QString mangoExecutable() const;
    void linuxAudio(bool mic,bool verified=false);
    void updateNoise(bool force);
    void beginNoiseDownload(const QVariantMap& cached);
    void borderless(bool restore);
};
