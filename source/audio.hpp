#pragma once
#include "platform.hpp"
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <functiondiscoverykeys_devpkey.h>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fg {
template <class T> struct Com {
    T* p = nullptr;
    ~Com() {
        if (p)
            p->Release();
    }
    T** put() {
        return &p;
    }
    T* operator->() const {
        return p;
    }
    explicit operator bool() const {
        return p != nullptr;
    }
};
struct AudioState {
    std::wstring device = L"No default audio output", guid;
    float volume = 0;
    bool available = false, mute = false;
};
inline AudioState audioState() {
    AudioState state;
    Com<IMMDeviceEnumerator> enumerator;
    Com<IMMDevice> device;
    Com<IAudioEndpointVolume> volume;
    Com<IPropertyStore> props;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator),
                                reinterpret_cast<void**>(enumerator.put()))))
        return state;
    if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, device.put())))
        return state;
    if (FAILED(device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
                                reinterpret_cast<void**>(volume.put()))))
        return state;
    if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, props.put()))) {
        PROPVARIANT v;
        PropVariantInit(&v);
        if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &v)) && v.vt == VT_LPWSTR)
            state.device = v.pwszVal;
        PropVariantClear(&v);
    }
    LPWSTR id = nullptr;
    if (SUCCEEDED(device->GetId(&id))) {
        std::wstring s = id;
        auto pos = s.find_last_of(L'{');
        if (pos != std::wstring::npos)
            state.guid = s.substr(pos);
        CoTaskMemFree(id);
    }
    BOOL mute = FALSE;
    volume->GetMute(&mute);
    state.mute = mute != FALSE;
    state.available = SUCCEEDED(volume->GetMasterVolumeLevelScalar(&state.volume));
    return state;
}
inline bool setAudioVolume(float amount) {
    Com<IMMDeviceEnumerator> enumerator;
    Com<IMMDevice> device;
    Com<IAudioEndpointVolume> volume;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator),
                                reinterpret_cast<void**>(enumerator.put()))))
        return false;
    if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, device.put())))
        return false;
    if (FAILED(device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
                                reinterpret_cast<void**>(volume.put()))))
        return false;
    return SUCCEEDED(volume->SetMasterVolumeLevelScalar(std::clamp(amount, 0.0f, 1.0f), nullptr));
}
class AudioBoost {
    std::wstring directory_;
    unsigned backupSerial = 0;
    bool hasInclude(const std::string& config) const {
        auto trim = [](std::string value) {
            auto begin = value.find_first_not_of(" \t\r");
            auto end = value.find_last_not_of(" \t\r");
            return begin == std::string::npos ? std::string()
                                              : value.substr(begin, end - begin + 1);
        };
        std::istringstream input(config);
        std::string line;
        std::error_code error;
        auto expected = std::filesystem::absolute(
                            std::filesystem::path(directory_) / L"framoryx-boost.txt", error)
                            .lexically_normal();
        if (error)
            return false;
        while (std::getline(input, line)) {
            if (line.compare(0, 3, "\xEF\xBB\xBF") == 0)
                line.erase(0, 3);
            line = trim(line);
            if (line.empty() || line[0] == '#')
                continue;
            auto colon = line.find(':');
            if (colon == std::string::npos || lower(trim(line.substr(0, colon))) != "include")
                continue;
            auto value = trim(line.substr(colon + 1));
            if (value.size() > 1 && value.front() == '"' && value.back() == '"')
                value = value.substr(1, value.size() - 2);
            std::filesystem::path candidate(wide(value));
            if (!candidate.is_absolute())
                candidate = std::filesystem::path(directory_) / candidate;
            error.clear();
            candidate = std::filesystem::absolute(candidate, error).lexically_normal();
            if (error)
                continue;
            if (_wcsicmp(candidate.c_str(), expected.c_str()) == 0)
                return true;
        }
        return false;
    }
    static bool read(const std::filesystem::path& p, std::string& out) {
        std::ifstream f(p, std::ios::binary);
        if (!f)
            return false;
        out.assign(std::istreambuf_iterator<char>(f), {});
        return f.good() || f.eof();
    }
    static bool write(const std::filesystem::path& p, const std::string& text) {
        auto temp = p;
        temp += L".framoryx.tmp";
        std::ofstream f(temp, std::ios::binary);
        f << text;
        f.close();
        return bool(f) && MoveFileExW(temp.c_str(), p.c_str(),
                                      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    }

  public:
    AudioBoost() {
        wchar_t path[32768]{};
        DWORD size = sizeof(path);
        if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\EqualizerAPO", L"ConfigPath",
                         RRF_RT_REG_SZ, nullptr, path, &size) == ERROR_SUCCESS)
            directory_ = path;
        if (directory_.empty()) {
            GetEnvironmentVariableW(L"ProgramFiles", path, 32768);
            directory_ = std::wstring(path) + L"\\EqualizerAPO\\config";
        }
    }
    const std::wstring& directory() const {
        return directory_;
    }
    void directory(const std::wstring& path) {
        directory_ = path;
    }
    bool available() const {
        return GetFileAttributesW((directory_ + L"\\config.txt").c_str()) !=
               INVALID_FILE_ATTRIBUTES;
    }
    std::optional<int> configuredGain(const std::wstring& endpointGuid) const {
        if (endpointGuid.empty())
            return {};
        std::string config, filter;
        if (!read(std::filesystem::path(directory_) / L"config.txt", config) || !hasInclude(config))
            return {};
        if (!read(std::filesystem::path(directory_) / L"framoryx-boost.txt", filter) ||
            filter.rfind("# Framoryx audio boost", 0) != 0)
            return {};
        if (filter.find("Device:") != std::string::npos &&
            filter.find(utf8(endpointGuid)) == std::string::npos)
            return {};
        auto pos = filter.find("Preamp: ");
        if (pos == std::string::npos)
            return {};
        pos += 8;
        auto end = filter.find(' ', pos);
        auto n = number(filter.substr(pos, end - pos));
        return n ? std::optional<int>(int(*n)) : std::nullopt;
    }
    std::wstring apply(int db, const std::wstring& endpointGuid) {
        bool validGuid = endpointGuid.size() == 38 && endpointGuid.front() == L'{' &&
                         endpointGuid.back() == L'}';
        if (validGuid)
            for (size_t i = 1; i < 37; ++i) {
                bool dash = i == 9 || i == 14 || i == 19 || i == 24;
                wchar_t c = endpointGuid[i];
                if (dash ? c != L'-'
                         : !((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f') ||
                             (c >= L'A' && c <= L'F')))
                    validGuid = false;
            }
        if (db < 0 || db > 12 || !validGuid)
            return L"Choose a default output device and a boost between 0 and 12 dB.";
        if (!available())
            return L"Equalizer APO is not installed or its config folder was not found. Open Setup "
                   L"for the one-time installation steps.";
        std::filesystem::path base(directory_);
        std::string config;
        if (!read(base / L"config.txt", config))
            return L"Cannot read Equalizer APO’s config.txt.";
        if (config.find('\0') != std::string::npos)
            return L"This audio config uses an unsupported encoding. Save it as UTF-8 in Equalizer "
                   L"APO before adding a boost.";
        std::string previous;
        if (read(base / L"framoryx-boost.txt", previous) &&
            previous.rfind("# Framoryx audio boost", 0) != 0)
            return L"framoryx-boost.txt already exists and is not owned by this app. Choose a "
                   L"different Equalizer APO config folder.";
        bool installed = hasInclude(config);
        if (!installed) {
            auto backup = base / (L"config.before-framoryx-" + std::to_wstring(GetTickCount64()) +
                                  L"-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                                  std::to_wstring(++backupSerial) + L".txt");
            if (!CopyFileW((base / L"config.txt").c_str(), backup.c_str(), TRUE))
                return L"Cannot back up audio configuration. Run Framoryx as administrator to "
                       L"apply boost.";
        }
        std::string filter =
            "# Framoryx audio boost. 0 dB disables amplification.\r\nDevice: " +
            utf8(endpointGuid) +
            "\r\nStage: post-mix\r\nChannel: all\r\nPreamp: " + std::to_string(db) + " dB\r\n";
        if (!write(base / L"framoryx-boost.txt", filter))
            return L"Cannot save boost. Run Framoryx as administrator to change the Equalizer APO "
                   L"config.";
        if (!installed) {
            config += "\r\n# Framoryx audio boost\r\nInclude: framoryx-boost.txt\r\n";
            if (!write(base / L"config.txt", config)) {
                write(base / L"framoryx-boost.txt", "# Framoryx audio boost\r\nPreamp: 0 dB\r\n");
                return L"Cannot link the audio configuration. No boost applied.";
            }
        }
        return {};
    }
    std::wstring disable() {
        if (!available())
            return L"No Equalizer APO config found.";
        std::string prior;
        if (!read(std::filesystem::path(directory_) / L"framoryx-boost.txt", prior))
            return {};
        if (prior.rfind("# Framoryx audio boost", 0) != 0)
            return L"The boost file belongs to another program; it was not changed.";
        if (!write(std::filesystem::path(directory_) / L"framoryx-boost.txt",
                   "# Framoryx audio boost\r\nPreamp: 0 dB\r\n"))
            return L"Could not disable boost. Run as administrator, or remove the Framoryx Include "
                   L"line in Equalizer APO.";
        return {};
    }
};
} // namespace fg
