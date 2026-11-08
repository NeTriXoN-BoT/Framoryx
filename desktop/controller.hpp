#pragma once
#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <QTimer>
#include <QProcess>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QFile>
#include <QUrl>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QNetworkAccessManager>
#include <memory>
#include "core.hpp"

class AudioMonitor;
class Platform;
class Controller : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap options READ options NOTIFY optionsChanged)
    Q_PROPERTY(QVariantList games READ games NOTIFY gamesChanged)
    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY profilesChanged)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(QVariantList capabilities READ capabilities NOTIFY capabilitiesChanged)
    Q_PROPERTY(QVariantList readouts READ readouts NOTIFY optionsChanged)
    Q_PROPERTY(QVariantList graph READ graph NOTIFY stateChanged)
    Q_PROPERTY(QString platformName READ platformName CONSTANT)
    Q_PROPERTY(bool testMode READ testMode CONSTANT)
public:
    explicit Controller(QString root, bool testing = false, QObject* parent = nullptr);
    ~Controller() override;
    QVariantMap state() const {return m_state;}
    QVariantMap options() const {return m_options;}
    QVariantList games() const {return m_games;}
    QVariantList profiles() const {return m_profiles;}
    QVariantList devices() const {return m_devices;}
    QVariantList capabilities() const {return m_capabilities;}
    QVariantList readouts() const;
    QVariantList graph() const {return m_graph;}
    QString platformName() const;
    bool testMode() const {return m_test;}
    QString root() const {return m_root;}
    Q_INVOKABLE void setOption(const QString& key, const QVariant& value);
    Q_INVOKABLE void setReadout(int index, bool visible);
    Q_INVOKABLE void moveReadout(int index, int direction);
    Q_INVOKABLE void selectGame(const QString& id);
    Q_INVOKABLE void addGame(const QUrl& executable, const QString& name, const QString& arguments);
    Q_INVOKABLE void removeGame(const QString& id);
    Q_INVOKABLE void saveProfile(const QString& name);
    Q_INVOKABLE void applyProfile(int index);
    Q_INVOKABLE void removeProfile(int index);
    Q_INVOKABLE bool exportProfiles(const QUrl& destination);
    Q_INVOKABLE bool importProfiles(const QUrl& source);
    Q_INVOKABLE void action(const QString& name, const QVariantMap& args = {});
    Q_INVOKABLE void notify(const QString& text, const QString& severity = QStringLiteral("info"));
    Q_INVOKABLE void setWindowActive(bool active);
    Q_INVOKABLE QString launchCommand() const;
    void receive(const QJsonObject& state);
    void refreshDevices();
    void refreshCapabilities();
    void sampleFrames(const QList<double>& frameTimes, const QString& app, quint32 pid);
    void setState(const QString& key, const QVariant& value);
    bool save();
    bool selfTest(QString* error);
signals:
    void stateChanged();
    void optionsChanged();
    void gamesChanged();
    void profilesChanged();
    void devicesChanged();
    void capabilitiesChanged();
    void toast(const QString& text, const QString& severity);
    void showRequested();
    void quitRequested();
private:
    friend class Platform;
    friend class AudioMonitor;
    QString m_root;
    bool m_test = false, m_active = true;
    QVariantMap m_state, m_options;
    QVariantList m_games, m_profiles, m_devices, m_capabilities, m_graph;
    QMediaDevices m_media;
    QTimer m_poll, m_saveTimer;
    QElapsedTimer m_clock;
    fg::Store m_store;
    double m_frameTime = 0;
    qint64 m_volumeUntil = 0;
    quint32 m_livePid = 0;
    std::unique_ptr<Platform> m_platform;
    std::unique_ptr<AudioMonitor> m_audio;
    void load();
    void poll();
    void scheduleSave();
    QVariantMap cleanOptions(const QVariantMap& options) const;
    void writeCsv(const QString& path);
};
