#include "telemetry.hpp"
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QLibrary>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QtConcurrent>
#include <cmath>
#include <optional>
#include <tuple>
#include <vector>
#ifdef Q_OS_WIN
#include <windows.h>
#include <dxgi1_2.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <wbemidl.h>
#include <oleauto.h>
#include <powrprof.h>
#endif
namespace {
QByteArray readSmall(const QString& path){QFile file(path);if(!file.open(QIODevice::ReadOnly))return {};const auto data=file.read(4097);return data.size()<=4096?data.trimmed():QByteArray();}
std::optional<double> number(const QString& path,double divisor=1.){bool ok=false;const double value=readSmall(path).toDouble(&ok)/divisor;if(!ok||!std::isfinite(value)||value<0)return {};return value;}
std::optional<double> hwmonTemperature(const QString& channel) {
    if (number(channel + "_fault").value_or(0) != 0) return {};
    const auto value = number(channel + "_input", 1000.);
    return value && *value <= 150 ? value : std::nullopt;
}
void put(QVariantMap& values,const QString& key,std::optional<double> value,double maximum){if(value&&*value<=maximum)values[key]=*value;}
QVariantMap empty(){QVariantMap result;for(const auto& key:QStringList{"gpu","vramPercent","vramUsed","vramTotal","gpuTemp","gpuHotspot","cpuTemp","gpuPower","gpuClock","gpuFan","cpuClock"})result[key]=QVariant();result["gpuName"]="GPU sensors unavailable";result["sensorSource"]="The driver did not expose these sensors. No readings are estimated.";return result;}
using Device=void*;
struct Utilization{unsigned gpu,memory;};
struct Memory{unsigned long long total,free,used;};
struct RecentTemperature {
    QString device;
    QVariant reading;
    QElapsedTimer age;

    bool update(QVariant& value, const QString& identity) {
        if (device != identity) {
            device = identity;
            age.invalidate();
        }
        if (value.isValid()) {
            reading = value;
            age.start();
        } else if (!identity.isEmpty() && age.isValid() && age.elapsed() < 4000) {
            value = reading;
            return true;
        }
        return false;
    }
};
}
class HardwareBackend {
    QString sys;
    QString preferredDrmCard, preferredDrmAdapter;
    QLibrary nvml;
    bool initialized=false;
    QElapsedTimer nvmlRetry;
    RecentTemperature cpuTemperature, gpuTemperature, gpuJunction;
#ifdef Q_OS_WIN
    PDH_HQUERY query=nullptr;PDH_HCOUNTER engineCounter=nullptr,memoryCounter=nullptr;
    QString adapterName, adapterLuid, adapterPciBus;
    LUID adapterLuidValue{};
    quint64 adapterMemory = 0;
    UINT adapterVendor = 0;
    bool adapterTried = false;
    QElapsedTimer missingCpuProvider;
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
    void windowsCpuTemperature(QVariantMap& values) {
        const auto noProvider = QStringLiteral(" CPU temperature requires a running Libre Hardware Monitor or Open Hardware Monitor WMI provider.");
        auto describe = [&values](const QString& detail) {
            values["sensorSource"] = values["sensorSource"].toString() + detail;
        };
        if (missingCpuProvider.isValid() && missingCpuProvider.elapsed() < 5000) {
            describe(noProvider);
            return;
        }
        missingCpuProvider.invalidate();

        const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(initialized)) {
            describe(QStringLiteral(" Windows could not start the CPU temperature query."));
            return;
        }
        struct Apartment { ~Apartment() { CoUninitialize(); } } apartment;
        IWbemLocator* locator = nullptr;
        if (FAILED(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_IWbemLocator, reinterpret_cast<void**>(&locator)))) {
            describe(QStringLiteral(" Windows could not open the CPU temperature provider."));
            return;
        }

        std::optional<double> hottest;
        QString provider;
        bool namespacesMissing = true;
        const wchar_t* namespaces[] = {L"ROOT\\LibreHardwareMonitor", L"ROOT\\OpenHardwareMonitor"};
        const QString providers[] = {QStringLiteral("Libre Hardware Monitor"), QStringLiteral("Open Hardware Monitor")};
        for (int i = 0; i < 2 && !hottest; ++i) {
            BSTR path = SysAllocString(namespaces[i]);
            IWbemServices* service = nullptr;
            const auto connected = locator->ConnectServer(path, nullptr, nullptr, nullptr,
                WBEM_FLAG_CONNECT_USE_MAX_WAIT, nullptr, nullptr, &service);
            SysFreeString(path);
            if (connected == WBEM_E_INVALID_NAMESPACE) continue;
            namespacesMissing = false;
            if (FAILED(connected)) continue;

            CoSetProxyBlanket(service, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
            BSTR language = SysAllocString(L"WQL");
            BSTR queryText = SysAllocString(L"SELECT Identifier, Value FROM Sensor WHERE SensorType='Temperature'");
            IEnumWbemClassObject* results = nullptr;
            const auto queried = service->ExecQuery(language, queryText,
                WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &results);
            SysFreeString(language);
            SysFreeString(queryText);
            if (SUCCEEDED(queried) && results) {
                QElapsedTimer elapsed;
                elapsed.start();
                for (int count = 0; count < 512 && elapsed.elapsed() < 300; ++count) {
                    IWbemClassObject* item = nullptr;
                    ULONG returned = 0;
                    const auto status = results->Next(40, 1, &item, &returned);
                    if (status == WBEM_S_TIMEDOUT && !returned) continue;
                    if (FAILED(status) || !returned || !item) break;

                    VARIANT identifier, value;
                    VariantInit(&identifier);
                    VariantInit(&value);
                    item->Get(L"Identifier", 0, &identifier, nullptr, nullptr);
                    item->Get(L"Value", 0, &value, nullptr, nullptr);
                    const auto length = identifier.vt == VT_BSTR ? SysStringLen(identifier.bstrVal) : 0;
                    const QString id = length > 0 && length < 4096
                        ? QString::fromWCharArray(identifier.bstrVal, length).toLower() : QString();
                    double temperature = -1;
                    if (value.vt == VT_R4) temperature = value.fltVal;
                    if (value.vt == VT_R8) temperature = value.dblVal;
                    if ((id.startsWith("/intelcpu/") || id.startsWith("/amdcpu/") || id.startsWith("/cpu/")) &&
                        std::isfinite(temperature) && temperature >= 0 && temperature <= 150 &&
                        (!hottest || temperature > *hottest)) {
                        hottest = temperature;
                        provider = providers[i];
                    }
                    VariantClear(&identifier);
                    VariantClear(&value);
                    item->Release();
                }
                results->Release();
            }
            service->Release();
        }
        locator->Release();
        if (hottest) {
            values["cpuTemp"] = *hottest;
            values["cpuTempLabel"] = "Hottest CPU sensor from " + provider;
            describe(" CPU temperature is read from " + provider + ".");
        } else if (namespacesMissing) {
            missingCpuProvider.start();
            describe(noProvider);
        } else {
            describe(QStringLiteral(" The CPU temperature provider did not return a CPU reading."));
        }
    }

    static bool queryWindowsAdapter(const LUID& luid, UINT type, void* data, UINT size) {
        // Documented D3DKMT layouts. MinGW does not ship d3dkmthk.h.
        struct OpenAdapter { LUID luid; UINT handle; };
        struct AdapterQuery { UINT handle, type; void* data; UINT size; };
        struct CloseAdapter { UINT handle; };
        const auto gdi = GetModuleHandleW(L"gdi32.dll");
        if (!gdi) return false;
        const auto open = reinterpret_cast<LONG(WINAPI*)(OpenAdapter*)>(GetProcAddress(gdi, "D3DKMTOpenAdapterFromLuid"));
        const auto query = reinterpret_cast<LONG(WINAPI*)(const AdapterQuery*)>(GetProcAddress(gdi, "D3DKMTQueryAdapterInfo"));
        const auto close = reinterpret_cast<LONG(WINAPI*)(const CloseAdapter*)>(GetProcAddress(gdi, "D3DKMTCloseAdapter"));
        if (!open || !query || !close) return false;
        OpenAdapter adapter{luid, 0};
        if (open(&adapter) != 0) return false;
        const AdapterQuery request{adapter.handle, type, data, size};
        const auto status = query(&request);
        const CloseAdapter closing{adapter.handle};
        close(&closing);
        return status == 0;
    }

    static QString windowsPciBus(const LUID& luid) {
        struct AdapterAddress { UINT bus, device, function; } address{};
        constexpr UINT adapterAddressQuery = 6; // KMTQAITYPE_ADAPTERADDRESS
        if (!queryWindowsAdapter(luid, adapterAddressQuery, &address, sizeof(address)) ||
            address.bus > 255 || address.device > 31 || address.function > 7) return {};
        return QString("%1:%2.%3").arg(address.bus, 2, 16, QLatin1Char('0'))
            .arg(address.device, 2, 16, QLatin1Char('0')).arg(address.function, 1, 16);
    }

    void windowsGpuTemperature(QVariantMap& values) const {
        if (adapterPciBus.isEmpty()) return;
        // D3DKMT_ADAPTER_PERFDATA reports main GPU temperature in tenths of a degree Celsius.
        struct AdapterPerfData {
            UINT physicalAdapterIndex;
            alignas(8) ULONGLONG memoryFrequency, maxMemoryFrequency, maxMemoryFrequencyOC;
            alignas(8) ULONGLONG memoryBandwidth, pcieBandwidth;
            ULONG fanRpm, power, temperature;
            UCHAR powerStateOverride;
        } data{};
        static_assert(sizeof(AdapterPerfData) == 64);
        constexpr UINT adapterPerfQuery = 62; // KMTQAITYPE_ADAPTERPERFDATA
        if (queryWindowsAdapter(adapterLuidValue, adapterPerfQuery, &data, sizeof(data)) &&
            data.temperature > 0 && data.temperature <= 1500) {
            values["gpuTemp"] = data.temperature / 10.;
            values["sensorSource"] = values["sensorSource"].toString() + " GPU temperature is reported by the Windows graphics driver.";
        }
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
        const bool firstSample = !adapterTried;
        if(firstSample){adapterTried=true;IDXGIFactory1* factory=nullptr;
            if(SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),reinterpret_cast<void**>(&factory)))){
                for(UINT i=0;i<32;++i){IDXGIAdapter1* adapter=nullptr;if(factory->EnumAdapters1(i,&adapter)!=S_OK)break;DXGI_ADAPTER_DESC1 description{};
                    if(SUCCEEDED(adapter->GetDesc1(&description))&&!(description.Flags&(DXGI_ADAPTER_FLAG_SOFTWARE|DXGI_ADAPTER_FLAG_REMOTE))&&(adapterName.isEmpty()||description.DedicatedVideoMemory>adapterMemory)){
                        adapterMemory=description.DedicatedVideoMemory;adapterVendor=description.VendorId;
                        adapterName=QString::fromWCharArray(description.Description);adapterLuidValue=description.AdapterLuid;
                    }adapter->Release();
                }factory->Release();
            }
            if (!adapterName.isEmpty()) {
                adapterLuid=QString("luid_0x%1_0x%2").arg(quint32(adapterLuidValue.HighPart),8,16,QLatin1Char('0')).arg(adapterLuidValue.LowPart,8,16,QLatin1Char('0'));
                adapterPciBus = windowsPciBus(adapterLuidValue);
                if(PdhOpenQueryW(nullptr,0,&query)==ERROR_SUCCESS){PdhAddEnglishCounterW(query,L"\\GPU Engine(*)\\Utilization Percentage",0,&engineCounter);PdhAddEnglishCounterW(query,L"\\GPU Adapter Memory(*)\\Dedicated Usage",0,&memoryCounter);PdhCollectQueryData(query);}
            }
        }
        if (adapterName.isEmpty()) return;
        values["gpuName"] = adapterName;
        values["_gpuVendorId"] = adapterVendor;
        values["_gpuPciBus"] = adapterPciBus;
        values["_gpuAdapterKey"] = "dxgi:" + adapterLuid;
        values["_gpuAdapterMemory"] = adapterMemory;
        if (adapterMemory) values["vramTotal"] = double(adapterMemory) / 1073741824.;
        values["sensorSource"] = "Windows GPU engine counters and DXGI dedicated-memory capacity.";
        windowsGpuTemperature(values);
        // The first PDH sample only establishes the utilization interval.
        if(firstSample||!query||PdhCollectQueryData(query)!=ERROR_SUCCESS)return;
        QMap<QString,double> engines;
        for(const auto& entry:counters(engineCounter)){if(!entry.first.contains(adapterLuid))continue;const int start=entry.first.indexOf("luid_");if(start>=0)engines[entry.first.mid(start)]+=entry.second;}
        if(!engines.isEmpty()){double busiest=0;for(double value:engines)busiest=std::max(busiest,value);values["gpu"]=std::min(100.,busiest);}
        double used=0;bool memoryValid=false;for(const auto& entry:counters(memoryCounter))if(entry.first.contains(adapterLuid)){used+=entry.second;memoryValid=true;}
        if(memoryValid&&adapterMemory>0){values["vramUsed"]=used/1073741824.;values["vramTotal"]=adapterMemory/1073741824.;values["vramPercent"]=std::min(100.,100.*used/adapterMemory);}
    }
#endif
    void nvidia(QVariantMap& values){
        if(!initialized){
            if(nvmlRetry.isValid()&&nvmlRetry.elapsed()<15000)return;
            nvmlRetry.start();
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
        const auto pciFn=reinterpret_cast<int(*)(const char*,Device*)>(nvml.resolve("nvmlDeviceGetHandleByPciBusId_v2"));
        const auto utilizationFn=reinterpret_cast<int(*)(Device,Utilization*)>(nvml.resolve("nvmlDeviceGetUtilizationRates"));
        const auto memoryFn=reinterpret_cast<int(*)(Device,Memory*)>(nvml.resolve("nvmlDeviceGetMemoryInfo"));
        const auto pciBus=values.value("_gpuPciBus").toByteArray();
        Device selected=nullptr;
        const bool sameAdapter=!pciBus.isEmpty()&&pciFn&&pciFn(pciBus.constData(),&selected)==0&&selected;
        unsigned selectedIndex=0;
        if(!sameAdapter){
            selected=nullptr;
            unsigned count=0;
            if(!countFn||!handleFn||countFn(&count)!=0)return;
            unsigned long long largestMemory=0;
            for(unsigned i=0;i<std::min(count,16u);++i){
                Device device=nullptr;
                if(handleFn(i,&device)!=0||!device)continue;
                Memory memory{};
                if(memoryFn&&memoryFn(device,&memory)!=0)memory={};
                if(!selected||memory.total>largestMemory){selected=device;selectedIndex=i;largestMemory=memory.total;}
            }
            if(!selected)return;
            const auto existingMemory=values.value("_gpuAdapterMemory").toULongLong();
            const auto existingVendor=values.value("_gpuVendorId").toUInt();
            if(!values.value("_gpuAdapterKey").toString().isEmpty()&&
               (existingVendor!=0x10de||!pciBus.isEmpty())&&existingMemory>=largestMemory)return;

            // Never combine sensor readings from two different adapters.
            for(const auto& key:QStringList{"gpu","vramPercent","vramUsed","vramTotal","gpuTemp","gpuHotspot","gpuPower","gpuClock","gpuFan"})
                values[key]=QVariant();
            const auto uuidFn=reinterpret_cast<int(*)(Device,char*,unsigned)>(nvml.resolve("nvmlDeviceGetUUID"));
            char uuid[128]{};
            values["_gpuAdapterKey"]="nvml:"+(uuidFn&&uuidFn(selected,uuid,sizeof(uuid))==0
                ?QString::fromUtf8(uuid):QString::number(selectedIndex));
        }
        Utilization utilization{};
        if(utilizationFn&&utilizationFn(selected,&utilization)==0&&utilization.gpu<=100)values["gpu"]=utilization.gpu;
        const auto nameFn=reinterpret_cast<int(*)(Device,char*,unsigned)>(nvml.resolve("nvmlDeviceGetName"));char name[128]{};if(nameFn&&nameFn(selected,name,sizeof(name))==0)values["gpuName"]=QString::fromUtf8(name);
        else if(!sameAdapter)values["gpuName"]="NVIDIA GPU";
        Memory memory{};
        if(memoryFn&&memoryFn(selected,&memory)==0&&memory.total&&memory.used<=memory.total){values["vramUsed"]=double(memory.used)/1073741824.;values["vramTotal"]=double(memory.total)/1073741824.;values["vramPercent"]=100.*double(memory.used)/double(memory.total);}
        const auto temperatureFn=reinterpret_cast<int(*)(Device,unsigned,unsigned*)>(nvml.resolve("nvmlDeviceGetTemperature"));unsigned value=0;
        const bool nvidiaTemperature=temperatureFn&&temperatureFn(selected,0,&value)==0&&value>0&&value<=150;
        if(nvidiaTemperature)values["gpuTemp"]=value;
        const auto powerFn=reinterpret_cast<int(*)(Device,unsigned*)>(nvml.resolve("nvmlDeviceGetPowerUsage"));if(powerFn&&powerFn(selected,&value)==0&&value<2000000)values["gpuPower"]=value/1000.;
        const auto clockFn=reinterpret_cast<int(*)(Device,unsigned,unsigned*)>(nvml.resolve("nvmlDeviceGetClockInfo"));if(clockFn&&clockFn(selected,0,&value)==0&&value<=10000)values["gpuClock"]=value;
        const auto fanFn=reinterpret_cast<int(*)(Device,unsigned*)>(nvml.resolve("nvmlDeviceGetFanSpeed"));if(fanFn&&fanFn(selected,&value)==0&&value<=100)values["gpuFan"]=value;
        const auto priorSource=values["sensorSource"].toString();
        values["sensorSource"]="NVIDIA driver sensors (NVML), device-wide readings.";
        if(!nvidiaTemperature&&sameAdapter&&values["gpuTemp"].isValid())
            values["sensorSource"]=values["sensorSource"].toString()+" "+priorSource;
    }
    QVariantMap readDrmGpu(const QString& card) const {
        const QString root = sys + "/class/drm/" + card + "/device";
        const QString device = QFileInfo(root).canonicalFilePath();
        if (device.isEmpty()) return {};

        static const QRegularExpression pciAddress("^[a-f0-9]{4}:[a-f0-9]{2}:[a-f0-9]{2}\\.[0-7]$");
        const auto address = QFileInfo(device).fileName().toLower();
        const bool pci = pciAddress.match(address).hasMatch();
        const auto vendor = readSmall(root + "/vendor").toUInt(nullptr, 0);
        const auto total = number(root + "/mem_info_vram_total");
        const auto used = number(root + "/mem_info_vram_used");
        QVariantMap gpu;
        gpu["_gpuVendorId"] = vendor;
        gpu["_gpuPciBus"] = pci ? address : QString();
        gpu["_gpuAdapterKey"] = pci ? "pci:" + address : "drm:" + device;
        gpu["_gpuAdapterMemory"] = total.value_or(0);
        gpu["gpuName"] = (vendor == 0x1002 ? "AMD GPU · " : vendor == 0x8086 ? "Intel GPU · " :
                          vendor == 0x10de ? "NVIDIA GPU · " : "GPU · ") + card;
        put(gpu, "gpu", number(root + "/gpu_busy_percent"), 100);
        if (total && used && *total > 0 && *used <= *total) {
            gpu["vramUsed"] = *used / 1073741824.;
            gpu["vramTotal"] = *total / 1073741824.;
            gpu["vramPercent"] = 100. * *used / *total;
        }

        std::optional<double> unlabeledTemperature;
        for (const auto& monitor : QDir(root + "/hwmon").entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            const QString hwmon = root + "/hwmon/" + monitor;
            const auto driver = readSmall(hwmon + "/name");
            for (const auto& input : QDir(hwmon).entryList({"temp*_input"}, QDir::Files)) {
                const auto channel = input.chopped(6);
                const auto path = hwmon + "/" + channel;
                const auto label = readSmall(path + "_label").toLower();
                const auto value = hwmonTemperature(path);
                if (!value) continue;
                if (label.contains("junction") || label.contains("hotspot") || label.contains("hot spot")) {
                    gpu["gpuHotspot"] = *value;
                } else if (label == "edge" || label == "gpu" || label == "gpu core" || label == "package" ||
                           (label.isEmpty() && driver == "xe" && channel == "temp2")) {
                    gpu["gpuTemp"] = *value;
                } else if (label.isEmpty() && driver != "xe" && channel == "temp1" && !unlabeledTemperature) {
                    unlabeledTemperature = value;
                }
            }
            auto power = number(hwmon + "/power1_average", 1000000.);
            if (!power) power = number(hwmon + "/power1_input", 1000000.);
            put(gpu, "gpuPower", power, 2000);
            put(gpu, "gpuClock", number(hwmon + "/freq1_input", 1000000.), 10000);
        }
        if (!gpu.contains("gpuTemp")) put(gpu, "gpuTemp", unlabeledTemperature, 150);
        return gpu;
    }
    void sysfs(QVariantMap& values){
        static const QStringList cpuDrivers{"coretemp", "k10temp", "zenpower", "cpu_thermal"};
        static const QStringList cpuZones{"x86_pkg_temp", "cpu-thermal", "cpu_thermal"};
        std::optional<double> cpu;
        QString cpuLabel;
        for (const auto& directory : QDir(sys + "/class/hwmon").entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            const QString root = sys + "/class/hwmon/" + directory;
            const auto driver = QString::fromLatin1(readSmall(root + "/name"));
            if (!cpuDrivers.contains(driver)) continue;
            for (const auto& input : QDir(root).entryList({"temp*_input"}, QDir::Files)) {
                const auto channel = root + "/" + input.chopped(6);
                const auto value = hwmonTemperature(channel);
                if (!value || (cpu && *value <= *cpu)) continue;
                const auto label = QString::fromUtf8(readSmall(channel + "_label"));
                cpu = value;
                cpuLabel = driver + (label.isEmpty() ? QString() : " · " + label);
            }
        }
        if (!cpu) {
            for (const auto& zone : QDir(sys + "/class/thermal").entryList({"thermal_zone*"}, QDir::Dirs)) {
                const auto root = sys + "/class/thermal/" + zone;
                const auto type = QString::fromLatin1(readSmall(root + "/type"));
                if (!cpuZones.contains(type)) continue;
                const auto value = number(root + "/temp", 1000.);
                if (!value || *value > 150 || (cpu && *value <= *cpu)) continue;
                cpu = value;
                cpuLabel = type;
            }
        }
        if (cpu) {
            values["cpuTemp"] = *cpu;
            values["cpuTempLabel"] = cpuLabel;
        }

        QVariantMap gpu;
        if (!preferredDrmCard.isEmpty()) {
            gpu = readDrmGpu(preferredDrmCard);
            if (gpu.value("_gpuAdapterKey").toString() != preferredDrmAdapter) gpu.clear();
        }
        if (gpu.isEmpty()) {
            static const QRegularExpression drmCard("^card[0-9]+$");
            const auto score = [](const QVariantMap& card) {
                return std::tuple{card.contains("gpuTemp") ? 2 : card.contains("gpuHotspot") ? 1 : 0,
                                  card.value("_gpuAdapterMemory").toDouble(), card.value("gpu").toDouble()};
            };
            preferredDrmCard.clear();
            preferredDrmAdapter.clear();
            for (const auto& card : QDir(sys + "/class/drm").entryList({"card*"}, QDir::Dirs)) {
                if (!drmCard.match(card).hasMatch()) continue;
                auto candidate = readDrmGpu(card);
                if (candidate.isEmpty() || (!gpu.isEmpty() && score(candidate) <= score(gpu))) continue;
                gpu = std::move(candidate);
                // Once an adapter exposes temperatures, keep using it while it remains present.
                // A busier GPU or a transient failure should not move a reading between cards.
                preferredDrmCard = (gpu.contains("gpuTemp") || gpu.contains("gpuHotspot")) ? card : QString();
                preferredDrmAdapter = gpu.value("_gpuAdapterKey").toString();
            }
        }
        for (auto it = gpu.cbegin(); it != gpu.cend(); ++it) values[it.key()] = it.value();
        if (!gpu.isEmpty() || cpu) values["sensorSource"] = "Linux DRM, hwmon and CPU thermal zones. GPU readings belong to one adapter.";
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
        const auto gpu=result.take("_gpuAdapterKey").toString();
        for(const auto& key:QStringList{"_gpuVendorId","_gpuPciBus","_gpuAdapterMemory"})result.remove(key);
        result["cpuTempRecent"]=cpuTemperature.update(result["cpuTemp"],"cpu");
        result["gpuTempRecent"]=gpuTemperature.update(result["gpuTemp"],gpu);
        result["gpuHotspotRecent"]=gpuJunction.update(result["gpuHotspot"],gpu);
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
    const QTemporaryDir fixture(root + "/sensor-fixture-XXXXXX");
    if (!fixture.isValid()) { *error = "Sensor fixture directory could not be created"; return false; }
    const QString sys=fixture.path();const QString cpu=sys+"/class/hwmon/hwmon0",gpu=sys+"/class/drm/card0/device";
    if(!write(cpu+"/name","coretemp")||!write(cpu+"/temp1_input","65000")||!write(cpu+"/temp2_input","nan")||!write(gpu+"/vendor","0x1002")||!write(gpu+"/gpu_busy_percent","72")||!write(gpu+"/mem_info_vram_total","8589934592")||!write(gpu+"/mem_info_vram_used","4294967296")||!write(gpu+"/hwmon/hwmon1/temp1_input","71000")||!write(gpu+"/hwmon/hwmon1/power1_average","123000000")||!write(gpu+"/hwmon/hwmon1/freq1_input","2100000000")){*error="Sensor fixtures could not be written";return false;}
    HardwareBackend backend(sys);const auto values=backend.read();
    if(values["cpuTemp"]!=65.||values["gpu"]!=72.||values["vramPercent"]!=50.||values["vramUsed"]!=4.||values["gpuTemp"]!=71.||values["gpuPower"]!=123.||values["gpuClock"]!=2100.||values["gpuHotspot"].isValid()){*error="Sensor units, percentages or unavailable values are incorrect";return false;}
    write(gpu+"/hwmon/hwmon1/temp2_input","90000");if(backend.read()["gpuHotspot"].isValid()){*error="An unlabeled temperature was incorrectly called a hotspot";return false;}
    write(gpu+"/hwmon/hwmon1/temp2_label","junction");if(backend.read()["gpuHotspot"]!=90.){*error="A labeled GPU junction sensor was not read";return false;}
    write(gpu+"/gpu_busy_percent","200");write(cpu+"/temp1_input","999999");const auto invalid=backend.read();
    if(invalid["gpu"].isValid()||invalid["cpuTemp"].isValid()){*error="Invalid sensor ranges were accepted";return false;}

    const auto thermal = sys + "/class/thermal";
    const auto hwmon = gpu + "/hwmon/hwmon1";
    const auto xe = sys + "/class/drm/card1/device";
    const auto xeHwmon = xe + "/hwmon/hwmon2";
    if (!write(thermal + "/thermal_zone0/type", "acpitz") || !write(thermal + "/thermal_zone0/temp", "88000") ||
        !write(thermal + "/thermal_zone1/type", "x86_pkg_temp") || !write(thermal + "/thermal_zone1/temp", "54500") ||
        !write(hwmon + "/temp1_label", "mem") || !write(hwmon + "/temp2_fault", "1") ||
        !write(hwmon + "/temp3_label", "edge") || !write(hwmon + "/temp3_input", "69000") ||
        !write(xe + "/vendor", "0x8086") || !write(xe + "/gpu_busy_percent", "98") ||
        !write(xeHwmon + "/name", "xe") || !write(xeHwmon + "/temp2_input", "62000") ||
        !write(xeHwmon + "/temp3_input", "98000")) {
        *error = "Thermal zone and second GPU fixtures could not be written";
        return false;
    }
    const auto selected = backend.read();
    if (selected["cpuTemp"] != 54.5 || selected["cpuTempLabel"] != "x86_pkg_temp" ||
        selected["gpuName"] != values["gpuName"] || selected["gpuTemp"] != 69. ||
        selected["gpuHotspot"].isValid()) {
        *error = "CPU thermal fallback, labeled GPU channels, or stable GPU selection failed";
        return false;
    }
    if (!write(hwmon + "/temp3_fault", "1")) { *error = "GPU fault fixture could not be written"; return false; }
    if (backend.read()["gpuTemp"].isValid()) {
        *error = "Faulty GPU sensor was accepted or replaced by a different adapter's reading";
        return false;
    }
    HardwareBackend newBackend(sys);
    if (newBackend.read()["gpuTemp"] != 62.) {
        *error = "Intel Xe package temperature was not detected";
        return false;
    }
    return true;
}
