#pragma once
#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <memory>
class HardwareBackend;
class Telemetry : public QObject {
    Q_OBJECT
public:
    explicit Telemetry(QObject* parent=nullptr);
    void start();
    static bool selfTest(const QString& root,QString* error);
signals:
    void sampled(const QVariantMap& values);
private:
    QTimer m_timer;
    bool m_busy=false;
    std::shared_ptr<HardwareBackend> m_backend;
    void poll();
};
