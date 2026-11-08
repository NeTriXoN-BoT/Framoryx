#include "telemetry.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QLibrary>
#include <QRegularExpression>
#include <QSaveFile>
#include <QtConcurrent>
#include <cmath>
#include <optional>
#include <vector>
#ifdef Q_OS_WIN
#include <windows.h>
#include <dxgi1_2.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <wbemidl.h>
#include <oleauto.h>
#include <powrprof.h>
#include <QElapsedTimer>
#endif
namespace {
QByteArray readSmall(const QString& path){QFile file(path);if(!file.open(QIODevice::ReadOnly))return {};const auto data=file.read(4097);return data.size()<=4096?data.trimmed():QByteArray();}
std::optional<double> number(const QString& path,double divisor=1.){bool ok=false;const double value=readSmall(path).toDouble(&ok)/divisor;if(!ok||!std::isfinite(value)||value<0)return {};return value;}
void put(QVariantMap& values,const QString& key,std::optional<double> value,double maximum){if(value&&*value<=maximum)values[key]=*value;}
QVariantMap empty(){QVariantMap result;for(const auto& key:QStringList{"gpu","vramPercent","vramUsed","vramTotal","gpuTemp","gpuHotspot","cpuTemp","gpuPower","gpuClock","gpuFan","cpuClock"})result[key]=QVariant();result["gpuName"]="GPU sensors unavailable";result["sensorSource"]="The driver did not expose these sensors. No readings are estimated.";return result;}
using Device=void*;
struct Utilization{unsigned gpu,memory;};
struct Memory{unsigned long long total,free,used;};
}
class HardwareBackend {
    QString sys;
    QLibrary nvml;
    bool initialized=false,tried=false;
#ifdef Q_OS_WIN
    PDH_HQUERY query=nullptr;PDH_HCOUNTER engineCounter=nullptr,memoryCounter=nullptr;
    QString adapterName,adapterLuid;double adapterMemory=0;bool adapterTried=false;
    int sensorRetry=0;
    HMODULE powerModule=nullptr;bool powerTried=false;
    void windowsCpuClock(QVariantMap& values){
        if(!powerTried){powerTried=true;powerModule=LoadLibraryExW(L"powrprof.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);}
        if(!powerModule)return;
        const auto queryPower=reinterpret_cast<decltype(&CallNtPowerInformation)>(GetProcAddress(powerModule,"CallNtPowerInformation"));if(!queryPower)return;
        // PROCESSOR_POWER_INFORMATION's documented ABI; some SDKs omit its declaration.
        struct ProcessorPowerState{ULONG number,maxMhz,currentMhz,limit,maxIdle,currentIdle;};static_assert(sizeof(ProcessorPowerState)==24);
        SYSTEM_INFO system{};GetSystemInfo(&system);if(!system.dwNumberOfProcessors||system.dwNumberOfProcessors>4096)return;
        std::vector<ProcessorPowerState> processors(system.dwNumberOfProcessors);
        if(queryPower(ProcessorInformation,nullptr,0,processors.data(),ULONG(processors.size()*sizeof(ProcessorPowerState)))!=0)return;
        double total=0;unsigned count=0;for(const auto& processor:processors)if(processor.currentMhz>0&&processor.currentMhz<=10000){total+=processor.currentMhz;++count;}
        if(count)values["cpuClock"]=total/count;
    }
    void windowsCpuTemperature(QVariantMap& values){
        if(sensorRetry>0){--sensorRetry;return;}
        const HRESULT initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);if(FAILED(initialized))return;
        struct Apartment{~Apartment(){CoUninitialize();}} apartment;
        IWbemLocator* locator=nullptr;
        if(FAILED(CoCreateInstance(CLSID_WbemLocator,nullptr,CLSCTX_INPROC_SERVER,IID_IWbemLocator,reinterpret_cast<void**>(&locator)))){sensorRetry=30;return;}
        std::optional<double> hottest;
        for(const auto space:{L"ROOT\\LibreHardwareMonitor",L"ROOT\\OpenHardwareMonitor"}){
            BSTR path=SysAllocString(space);IWbemServices* service=nullptr;const auto connected=locator->ConnectServer(path,nullptr,nullptr,nullptr,WBEM_FLAG_CONNECT_USE_MAX_WAIT,nullptr,nullptr,&service);SysFreeString(path);if(FAILED(connected))continue;
            CoSetProxyBlanket(service,RPC_C_AUTHN_WINNT,RPC_C_AUTHZ_NONE,nullptr,RPC_C_AUTHN_LEVEL_CALL,RPC_C_IMP_LEVEL_IMPERSONATE,nullptr,EOAC_NONE);
            BSTR language=SysAllocString(L"WQL"),queryText=SysAllocString(L"SELECT Identifier, Value FROM Sensor WHERE SensorType='Temperature'");IEnumWbemClassObject* results=nullptr;
            const auto queried=service->ExecQuery(language,queryText,WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY,nullptr,&results);SysFreeString(language);SysFreeString(queryText);
            if(SUCCEEDED(queried)){
                QElapsedTimer elapsed;elapsed.start();
                for(int count=0;count<512&&elapsed.elapsed()<300;++count){IWbemClassObject* item=nullptr;ULONG returned=0;if(results->Next(40,1,&item,&returned)!=S_OK||returned!=1)break;
                    VARIANT identifier,value;VariantInit(&identifier);VariantInit(&value);item->Get(L"Identifier",0,&identifier,nullptr,nullptr);item->Get(L"Value",0,&value,nullptr,nullptr);
                    const QString id=identifier.vt==VT_BSTR&&SysStringLen(identifier.bstrVal)<4096?QString::fromWCharArray(identifier.bstrVal).toLower():QString();
                    double temperature=-1;if(value.vt==VT_R4)temperature=value.fltVal;else if(value.vt==VT_R8)temperature=value.dblVal;
                    if((id.startsWith("/intelcpu/")||id.startsWith("/amdcpu/")||id.startsWith("/cpu/"))&&std::isfinite(temperature)&&temperature>=0&&temperature<=150&&(!hottest||temperature>*hottest))hottest=temperature;
                    VariantClear(&identifier);VariantClear(&value);item->Release();
                }results->Release();
            }service->Release();if(hottest)break;
        }locator->Release();
        if(hottest){values["cpuTemp"]=*hottest;values["cpuTempLabel"]="Hottest CPU sensor from an existing hardware-monitor WMI provider";values["sensorSource"]=values["sensorSource"].toString()+" CPU temperature is read from your existing hardware-monitor service.";}else sensorRetry=30;
    }

    QList<QPair<QString,double>> counters(PDH_HCOUNTER counter){
        QList<QPair<QString,double>> result;if(!counter)return result;DWORD bytes=0,count=0;
        const DWORD flags=PDH_FMT_DOUBLE|PDH_FMT_NOCAP100;
        if(PdhGetFormattedCounterArrayW(counter,flags,&bytes,&count,nullptr)!=PDH_MORE_DATA||bytes>8*1024*1024)return result;
        std::vector<unsigned char> buffer(bytes);auto items=reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());
        if(PdhGetFormattedCounterArrayW(counter,flags,&bytes,&count,items)!=ERROR_SUCCESS||count>32768||count>buffer.size()/sizeof(PDH_FMT_COUNTERVALUE_ITEM_W))return result;
        for(DWORD i=0;i<count;++i)if(items[i].FmtValue.CStatus==PDH_CSTATUS_VALID_DATA||items[i].FmtValue.CStatus==PDH_CSTATUS_NEW_DATA){const double value=items[i].FmtValue.doubleValue;if(std::isfinite(value)&&value>=0)result<<qMakePair(QString::fromWCharArray(items[i].szName).toLower(),value);}
        return result;
    }
    void windowsGpu(QVariantMap& values){
        if(!adapterTried){adapterTried=true;IDXGIFactory1* factory=nullptr;
            if(SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),reinterpret_cast<void**>(&factory)))){
                for(UINT i=0;i<32;++i){IDXGIAdapter1* adapter=nullptr;if(factory->EnumAdapters1(i,&adapter)!=S_OK)break;DXGI_ADAPTER_DESC1 description{};
                    if(SUCCEEDED(adapter->GetDesc1(&description))&&!(description.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)&&(adapterName.isEmpty()||double(description.DedicatedVideoMemory)>adapterMemory)){
                        adapterMemory=double(description.DedicatedVideoMemory);adapterName=QString::fromWCharArray(description.Description);adapterLuid=QString("luid_0x%1_0x%2").arg(quint32(description.AdapterLuid.HighPart),8,16,QLatin1Char('0')).arg(description.AdapterLuid.LowPart,8,16,QLatin1Char('0'));
                    }adapter->Release();
                }factory->Release();
            }
            if(PdhOpenQueryW(nullptr,0,&query)==ERROR_SUCCESS){PdhAddEnglishCounterW(query,L"\\GPU Engine(*)\\Utilization Percentage",0,&engineCounter);PdhAddEnglishCounterW(query,L"\\GPU Adapter Memory(*)\\Dedicated Usage",0,&memoryCounter);PdhCollectQueryData(query);}
            return; // A utilization interval requires two samples.
        }
        if(adapterName.isEmpty())return;values["gpuName"]=adapterName;
        if(!query||PdhCollectQueryData(query)!=ERROR_SUCCESS)return;
        QMap<QString,double> engines;
        for(const auto& entry:counters(engineCounter)){if(!entry.first.contains(adapterLuid))continue;const int start=entry.first.indexOf("luid_");if(start>=0)engines[entry.first.mid(start)]+=entry.second;}
        if(!engines.isEmpty()){double busiest=0;for(double value:engines)busiest=std::max(busiest,value);values["gpu"]=std::min(100.,busiest);}
        double used=0;bool memoryValid=false;for(const auto& entry:counters(memoryCounter))if(entry.first.contains(adapterLuid)){used+=entry.second;memoryValid=true;}
        if(memoryValid&&adapterMemory>0){values["vramUsed"]=used/1073741824.;values["vramTotal"]=adapterMemory/1073741824.;values["vramPercent"]=std::min(100.,100.*used/adapterMemory);}
        values["sensorSource"]="Windows GPU engine counters and DXGI dedicated-memory capacity. Temperature requires driver sensor support.";
    }
#endif
    void nvidia(QVariantMap& values){
        if(!tried){tried=true;
#ifdef Q_OS_WIN
            wchar_t directory[32768]{};const UINT length=GetSystemDirectoryW(directory,32768);if(!length||length>=32768)return;nvml.setFileName(QString::fromWCharArray(directory,int(length))+"/nvml.dll");
#else
            nvml.setFileName("libnvidia-ml.so.1");
#endif
            if(!nvml.load())return;const auto init=reinterpret_cast<int(*)()>(nvml.resolve("nvmlInit_v2"));initialized=init&&init()==0;
        }
        if(!initialized)return;
        const auto countFn=reinterpret_cast<int(*)(unsigned*)>(nvml.resolve("nvmlDeviceGetCount_v2"));
        const auto handleFn=reinterpret_cast<int(*)(unsigned,Device*)>(nvml.resolve("nvmlDeviceGetHandleByIndex_v2"));
        const auto utilizationFn=reinterpret_cast<int(*)(Device,Utilization*)>(nvml.resolve("nvmlDeviceGetUtilizationRates"));
        unsigned count=0;if(!countFn||!handleFn||countFn(&count)!=0)return;
        Device selected=nullptr;int best=-1;Utilization utilization{};
        for(unsigned i=0;i<std::min(count,16u);++i){Device device=nullptr;Utilization candidate{};if(handleFn(i,&device)!=0)continue;const bool valid=utilizationFn&&utilizationFn(device,&candidate)==0;const int score=valid?int(candidate.gpu):0;if(!selected||score>best){selected=device;best=score;utilization=candidate;if(!valid)best=0;}}
        if(!selected)return;
        if(values["gpu"].isValid()&&values["gpu"].toDouble()>best)return;
        // Clear readings from another adapter rather than mixing different GPUs.
        for(const auto& key:QStringList{"gpu","vramPercent","vramUsed","vramTotal","gpuTemp","gpuHotspot","gpuPower","gpuClock","gpuFan"})values[key]=QVariant();
        if(utilizationFn&&utilizationFn(selected,&utilization)==0&&utilization.gpu<=100)values["gpu"]=utilization.gpu;
        const auto nameFn=reinterpret_cast<int(*)(Device,char*,unsigned)>(nvml.resolve("nvmlDeviceGetName"));char name[128]{};if(nameFn&&nameFn(selected,name,sizeof(name))==0)values["gpuName"]=QString::fromUtf8(name);
        const auto memoryFn=reinterpret_cast<int(*)(Device,Memory*)>(nvml.resolve("nvmlDeviceGetMemoryInfo"));Memory memory{};
        if(memoryFn&&memoryFn(selected,&memory)==0&&memory.total&&memory.used<=memory.total){values["vramUsed"]=double(memory.used)/1073741824.;values["vramTotal"]=double(memory.total)/1073741824.;values["vramPercent"]=100.*double(memory.used)/double(memory.total);}
        const auto temperatureFn=reinterpret_cast<int(*)(Device,unsigned,unsigned*)>(nvml.resolve("nvmlDeviceGetTemperature"));unsigned value=0;
        if(temperatureFn&&temperatureFn(selected,0,&value)==0&&value<=150)values["gpuTemp"]=value;
        const auto powerFn=reinterpret_cast<int(*)(Device,unsigned*)>(nvml.resolve("nvmlDeviceGetPowerUsage"));if(powerFn&&powerFn(selected,&value)==0&&value<2000000)values["gpuPower"]=value/1000.;
        const auto clockFn=reinterpret_cast<int(*)(Device,unsigned,unsigned*)>(nvml.resolve("nvmlDeviceGetClockInfo"));if(clockFn&&clockFn(selected,0,&value)==0&&value<=10000)values["gpuClock"]=value;
        const auto fanFn=reinterpret_cast<int(*)(Device,unsigned*)>(nvml.resolve("nvmlDeviceGetFanSpeed"));if(fanFn&&fanFn(selected,&value)==0&&value<=100)values["gpuFan"]=value;
        values["sensorSource"]="NVIDIA driver sensors (NVML), device-wide readings. The busiest NVIDIA adapter is selected.";
    }
    void sysfs(QVariantMap& values){
        std::optional<double> cpu;QString cpuLabel;
        for(const auto& directory:QDir(sys+"/class/hwmon").entryList(QDir::Dirs|QDir::NoDotAndDotDot)){
            const QString root=sys+"/class/hwmon/"+directory;const auto driver=QString::fromLatin1(readSmall(root+"/name"));
            if(!QStringList{"coretemp","k10temp","zenpower","cpu_thermal"}.contains(driver))continue;
            for(const auto& input:QDir(root).entryList({"temp*_input"},QDir::Files)){
                auto fault=number(root+"/"+QString(input).replace("_input","_fault"));if(fault&&*fault!=0)continue;
                const auto value=number(root+"/"+input,1000.);if(!value||*value>150)continue;
                const auto label=QString::fromUtf8(readSmall(root+"/"+QString(input).replace("_input","_label")));
                if(!cpu||*value>*cpu){cpu=value;cpuLabel=driver+(label.isEmpty()?QString():" · "+label);}
            }
        }
        put(values,"cpuTemp",cpu,150);if(cpu)values["cpuTempLabel"]=cpuLabel;
        QVariantMap gpu;double best=-1;
        for(const auto& card:QDir(sys+"/class/drm").entryList(QDir::Dirs|QDir::NoDotAndDotDot)){
            if(!QRegularExpression("^card[0-9]+$").match(card).hasMatch())continue;const QString root=sys+"/class/drm/"+card+"/device";
            const auto busy=number(root+"/gpu_busy_percent"),total=number(root+"/mem_info_vram_total"),used=number(root+"/mem_info_vram_used");
            const double score=busy?1000000.+*busy:total?*total/1073741824.:0;if(score<best)continue;best=score;gpu.clear();
            const auto vendor=readSmall(root+"/vendor");gpu["gpuName"]=(vendor=="0x1002"?"AMD GPU · ":vendor=="0x8086"?"Intel GPU · ":"GPU · ")+card;
            put(gpu,"gpu",busy,100);
            if(total&&used&&*total>0&&*used<=*total){gpu["vramUsed"]=*used/1073741824.;gpu["vramTotal"]=*total/1073741824.;gpu["vramPercent"]=100.*(*used)/(*total);}
            for(const auto& monitor:QDir(root+"/hwmon").entryList(QDir::Dirs|QDir::NoDotAndDotDot)){
                const QString hwmon=root+"/hwmon/"+monitor;
                put(gpu,"gpuTemp",number(hwmon+"/temp1_input",1000.),150);
                const auto label=readSmall(hwmon+"/temp2_label").toLower();if(label.contains("junction")||label.contains("hotspot"))put(gpu,"gpuHotspot",number(hwmon+"/temp2_input",1000.),150);
                auto power=number(hwmon+"/power1_average",1000000.);if(!power)power=number(hwmon+"/power1_input",1000000.);put(gpu,"gpuPower",power,2000);
                put(gpu,"gpuClock",number(hwmon+"/freq1_input",1000000.),10000);
            }
        }
        for(auto it=gpu.begin();it!=gpu.end();++it)values[it.key()]=it.value();
        if(!gpu.isEmpty()||cpu)values["sensorSource"]="Read-only Linux DRM and hwmon sensors. CPU temperature is the hottest exposed CPU sensor; GPU readings belong to one adapter.";
        int count=0;double sum=0;for(const auto& cpuDir:QDir(sys+"/devices/system/cpu").entryList({"cpu[0-9]*"},QDir::Dirs)){const auto value=number(sys+"/devices/system/cpu/"+cpuDir+"/cpufreq/scaling_cur_freq",1000.);if(value&&*value<=10000){sum+=*value;++count;}}
        if(count)values["cpuClock"]=sum/count;
    }
public:
    explicit HardwareBackend(QString root="/sys"):sys(std::move(root)){}
    ~HardwareBackend(){
        if(initialized){const auto shutdown=reinterpret_cast<int(*)()>(nvml.resolve("nvmlShutdown"));if(shutdown)shutdown();}
#ifdef Q_OS_WIN
        if(query)PdhCloseQuery(query);
        if(powerModule)FreeLibrary(powerModule);
#endif
    }
    QVariantMap read(){auto result=empty();if(sys!="/sys"){sysfs(result);return result;}
#ifdef Q_OS_WIN
        windowsGpu(result);
#else
        sysfs(result);
#endif
        nvidia(result);
#ifdef Q_OS_WIN
        windowsCpuClock(result);
        windowsCpuTemperature(result);
#endif
        return result;
    }
};
Telemetry::Telemetry(QObject* parent):QObject(parent),m_backend(std::make_shared<HardwareBackend>()){
    m_timer.setInterval(1000);connect(&m_timer,&QTimer::timeout,this,&Telemetry::poll);
}
void Telemetry::start(){poll();m_timer.start();}
void Telemetry::poll(){
    if(m_busy)return;m_busy=true;auto watcher=new QFutureWatcher<QVariantMap>(this);
    connect(watcher,&QFutureWatcher<QVariantMap>::finished,this,[this,watcher]{m_busy=false;emit sampled(watcher->result());watcher->deleteLater();});
    const auto backend=m_backend;watcher->setFuture(QtConcurrent::run([backend]{return backend->read();}));
}
bool Telemetry::selfTest(const QString& root,QString* error){
    auto write=[&](const QString& path,const QByteArray& data){QDir().mkpath(QFileInfo(path).absolutePath());QSaveFile file(path);return file.open(QIODevice::WriteOnly)&&file.write(data)==data.size()&&file.commit();};
    const QString sys=root+"/sensor-fixture";const QString cpu=sys+"/class/hwmon/hwmon0",gpu=sys+"/class/drm/card0/device";
    QFile::remove(gpu+"/hwmon/hwmon1/temp2_input");QFile::remove(gpu+"/hwmon/hwmon1/temp2_label");
    if(!write(cpu+"/name","coretemp")||!write(cpu+"/temp1_input","65000")||!write(cpu+"/temp2_input","nan")||!write(gpu+"/vendor","0x1002")||!write(gpu+"/gpu_busy_percent","72")||!write(gpu+"/mem_info_vram_total","8589934592")||!write(gpu+"/mem_info_vram_used","4294967296")||!write(gpu+"/hwmon/hwmon1/temp1_input","71000")||!write(gpu+"/hwmon/hwmon1/power1_average","123000000")||!write(gpu+"/hwmon/hwmon1/freq1_input","2100000000")){*error="Sensor fixtures could not be written";return false;}
    HardwareBackend backend(sys);const auto values=backend.read();
    if(values["cpuTemp"]!=65.||values["gpu"]!=72.||values["vramPercent"]!=50.||values["vramUsed"]!=4.||values["gpuTemp"]!=71.||values["gpuPower"]!=123.||values["gpuClock"]!=2100.||values["gpuHotspot"].isValid()){*error="Sensor units, percentages or unavailable values are incorrect";return false;}
    write(gpu+"/hwmon/hwmon1/temp2_input","90000");if(backend.read()["gpuHotspot"].isValid()){*error="An unlabeled temperature was incorrectly called a hotspot";return false;}
    write(gpu+"/hwmon/hwmon1/temp2_label","junction");if(backend.read()["gpuHotspot"]!=90.){*error="A labeled GPU junction sensor was not read";return false;}
    write(gpu+"/gpu_busy_percent","200");write(cpu+"/temp1_input","999999");const auto invalid=backend.read();
    if(invalid["gpu"].isValid()||invalid["cpuTemp"].isValid()){*error="Invalid sensor ranges were accepted";return false;}
    return true;
}
