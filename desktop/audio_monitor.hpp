#pragma once
#include <QObject>
#include <QAudioSource>
#include <QAudioSink>
#include <QAudioDevice>
#include <QBuffer>
#include <QTimer>
#include <memory>
class Controller;
class AudioMonitor : public QObject {
    Q_OBJECT
public:
    explicit AudioMonitor(Controller* controller);
    ~AudioMonitor() override;
    void record(const QByteArray& deviceId);
    void play();
    void stop();
private:
    Controller* m_controller;
    std::unique_ptr<QAudioSource> m_input;
    std::unique_ptr<QAudioSink> m_output;
    QIODevice* m_stream = nullptr;
    QAudioFormat m_format;
    QByteArray m_recording;
    QBuffer m_playback;
    QTimer m_stop, m_meter;
    quint64 m_generation=0;
    float m_peak = 0;
    void consume();
};
