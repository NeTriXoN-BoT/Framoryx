#include "audio_monitor.hpp"
#include "controller.hpp"
#include <QMediaDevices>
#include <QtEndian>
#include <cmath>
#include <cstring>

AudioMonitor::AudioMonitor(Controller* controller) : QObject(controller), m_controller(controller) {
    m_stop.setSingleShot(true);
    connect(&m_stop,&QTimer::timeout,this,[this]{stop();});
    m_meter.setInterval(50);
    connect(&m_meter,&QTimer::timeout,this,[this]{
        m_controller->setState("inputLevel",qBound(0.f,m_peak,1.f));
        m_peak *= .72f;
    });
}
AudioMonitor::~AudioMonitor(){stop();}
void AudioMonitor::record(const QByteArray& id){
    stop();
    QAudioDevice device = id.isEmpty()?QMediaDevices::defaultAudioInput():QAudioDevice();
    for(const auto& candidate:QMediaDevices::audioInputs()) if(candidate.id()==id)device=candidate;
    if(device.isNull()){m_controller->notify("No microphone is available.","warning");return;}
    m_format=device.preferredFormat();
    if(m_format.sampleRate()<8000 || m_format.channelCount()<1 || m_format.channelCount()>8){m_controller->notify("This microphone format is not supported for testing.","warning");return;}
    m_recording.clear();m_peak=0;
    m_input=std::make_unique<QAudioSource>(device,m_format);
    m_stream=m_input->start();
    if(!m_stream || m_input->error()!=QAudio::NoError){stop();m_controller->notify("The microphone could not be opened. Check system permissions and device use.","warning");return;}
    connect(m_stream,&QIODevice::readyRead,this,&AudioMonitor::consume);
    m_controller->setState("recording",true);
    m_controller->setState("sampleReady",false);
    m_controller->setState("recordingDevice",device.description());
    m_stop.start(5000);m_meter.start();
    m_controller->notify("Recording a five-second test in memory. Nothing is saved or uploaded.");
}
void AudioMonitor::consume(){
    if(!m_stream)return;
    const QByteArray bytes=m_stream->readAll();
    if(bytes.size()+m_recording.size()>16*1024*1024){stop();return;}
    m_recording+=bytes;
    const int sampleBytes=m_format.bytesPerSample();
    if(sampleBytes<=0)return;
    for(qsizetype p=0;p+sampleBytes<=bytes.size();p+=sampleBytes){
        float value=0;
        switch(m_format.sampleFormat()){
        case QAudioFormat::Float:{float sample;std::memcpy(&sample,bytes.constData()+p,4);value=std::isfinite(sample)?std::abs(sample):0;break;}
        case QAudioFormat::Int16:{qint16 sample;std::memcpy(&sample,bytes.constData()+p,2);value=std::abs(float(sample)/32768.f);break;}
        case QAudioFormat::Int32:{qint32 sample;std::memcpy(&sample,bytes.constData()+p,4);value=float(std::abs(double(sample)/2147483648.));break;}
        case QAudioFormat::UInt8:value=std::abs((quint8(bytes[p])-128.f)/128.f);break;
        default:break;
        }
        m_peak=std::max(m_peak,value);
    }
}
void AudioMonitor::play(){
    if(m_recording.isEmpty()){m_controller->notify("Record a microphone test first.");return;}
    stop();
    const auto device=QMediaDevices::defaultAudioOutput();
    if(!device.isFormatSupported(m_format)){m_controller->notify("Your output does not support this recording format.","warning");return;}
    m_output=std::make_unique<QAudioSink>(device,m_format);
    m_playback.setData(m_recording);m_playback.open(QIODevice::ReadOnly);
    m_controller->setState("playbackFinished",false);
    m_output->start(&m_playback);
    if(m_output->error()!=QAudio::NoError){stop();m_controller->notify("The output device could not play this sample.","warning");return;}
    const auto generation=m_generation;
    connect(m_output.get(),&QAudioSink::stateChanged,this,[this,generation](QAudio::State state){if(state==QAudio::IdleState&&generation==m_generation)QTimer::singleShot(0,this,[this,generation]{if(generation==m_generation){m_controller->setState("playbackFinished",true);stop();}});});
    m_controller->setState("playing",true);
    m_stop.start(6500);
}
void AudioMonitor::stop(){
    ++m_generation;
    m_stop.stop();m_meter.stop();
    if(m_input){m_input->stop();m_input.reset();}m_stream=nullptr;
    if(m_output){m_output->stop();m_output.reset();}
    m_playback.close();
    if(m_controller){m_controller->setState("recording",false);m_controller->setState("playing",false);m_controller->setState("inputLevel",0.);m_controller->setState("sampleReady",!m_recording.isEmpty());}
}
