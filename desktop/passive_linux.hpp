#pragma once
#include <QObject>
#include <QList>
#include <memory>
class PassiveCapture : public QObject {
public:
    explicit PassiveCapture(QObject* parent=nullptr);
    ~PassiveCapture() override;
    bool start(quint32 window,quint32 pid,QString* error);
    void stop();
    bool running() const;
    quint32 window() const;
    quint32 foreground() const;
    QList<double> takeFrames();
private:
    struct Data;
    std::unique_ptr<Data> d;
    void drain();
};
