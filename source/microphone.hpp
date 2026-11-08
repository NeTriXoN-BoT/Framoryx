#pragma once
#include "audio.hpp"
#include <audioclient.h>
#include <cstdint>

namespace fg {
struct MicrophoneOptions {
    bool noise = true, equalizer = false;
    int preset = 1, low = 0, mid = 0, high = 0;
};
struct Microphone {
    std::wstring name, id, guid;
    unsigned rate = 0, channels = 0;
};
inline std::vector<Microphone> microphones() {
    std::vector<Microphone> result;
    Com<IMMDeviceEnumerator> enumerator;
    Com<IMMDeviceCollection> devices;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator),
                                reinterpret_cast<void**>(enumerator.put()))) ||
        FAILED(enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, devices.put())))
        return result;
    UINT count = 0;
    devices->GetCount(&count);
    std::wstring defaultId;
    Com<IMMDevice> defaultDevice;
    if (SUCCEEDED(
            enumerator->GetDefaultAudioEndpoint(eCapture, eCommunications, defaultDevice.put()))) {
        LPWSTR id = nullptr;
        if (SUCCEEDED(defaultDevice->GetId(&id))) {
            defaultId = id;
            CoTaskMemFree(id);
        }
    }
    for (UINT i = 0; i < count; ++i) {
        Com<IMMDevice> device;
        Com<IPropertyStore> props;
        Com<IAudioClient> client;
        if (FAILED(devices->Item(i, device.put())))
            continue;
        Microphone mic;
        LPWSTR id = nullptr;
        if (SUCCEEDED(device->GetId(&id))) {
            mic.id = id;
            auto pos = mic.id.find_last_of(L'{');
            if (pos != std::wstring::npos)
                mic.guid = mic.id.substr(pos);
            CoTaskMemFree(id);
        }
        if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, props.put()))) {
            PROPVARIANT v;
            PropVariantInit(&v);
            if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &v)) && v.vt == VT_LPWSTR)
                mic.name = v.pwszVal;
            PropVariantClear(&v);
        }
        if (SUCCEEDED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                       reinterpret_cast<void**>(client.put())))) {
            WAVEFORMATEX* format = nullptr;
            if (SUCCEEDED(client->GetMixFormat(&format))) {
                mic.rate = format->nSamplesPerSec;
                mic.channels = format->nChannels;
                CoTaskMemFree(format);
            }
        }
        if (!mic.guid.empty())
            result.push_back(std::move(mic));
    }
    auto preferred = std::find_if(result.begin(), result.end(),
                                  [&](const auto& mic) { return mic.id == defaultId; });
    if (preferred != result.end())
        std::rotate(result.begin(), preferred, preferred + 1);
    return result;
}
inline bool endpointGuid(const std::wstring& guid) {
    if (guid.size() != 38 || guid.front() != L'{' || guid.back() != L'}')
        return false;
    for (size_t i = 1; i < 37; ++i) {
        wchar_t c = guid[i];
        bool dash = i == 9 || i == 14 || i == 19 || i == 24;
        if (dash ? c != L'-'
                 : !((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f') ||
                     (c >= L'A' && c <= L'F')))
            return false;
    }
    return true;
}
// File-format validation only: the app never loads a selected audio plugin.
inline bool rnnoisePlugin(const std::filesystem::path& path, unsigned channels) {
    if (channels < 1 || channels > 2)
        return false;
    auto name = lower(utf8(path.filename().wstring()));
    if (name != (channels == 1 ? "rnnoise_mono.dll" : "rnnoise_stereo.dll"))
        return false;
    std::ifstream file(path, std::ios::binary);
    unsigned char header[64]{};
    if (!file.read(reinterpret_cast<char*>(header), 64) || header[0] != 'M' || header[1] != 'Z')
        return false;
    uint32_t offset = uint32_t(header[60]) | uint32_t(header[61]) << 8 |
                      uint32_t(header[62]) << 16 | uint32_t(header[63]) << 24;
    if (offset < 64 || offset > 16 * 1024 * 1024)
        return false;
    file.seekg(offset);
    unsigned char pe[6]{};
    return bool(file.read(reinterpret_cast<char*>(pe), 6)) && pe[0] == 'P' && pe[1] == 'E' &&
           pe[2] == 0 && pe[3] == 0 && pe[4] == 0x64 && pe[5] == 0x86;
}
inline std::string noiseChunk(int preset) {
    // Public JUCE XML-state wire format, independently serialized. No plugin code is linked.
    const char* thresholds[]{"0.50", "0.85", "0.95"};
    std::string xml = "<RNNoise><PARAM id=\"vad_threshold\" value=\"" +
                      std::string(thresholds[std::clamp(preset, 0, 2)]) +
                      "\"/><PARAM id=\"vad_grace_period\" value=\"20\"/>"
                      "<PARAM id=\"vad_retroactive_grace_period\" value=\"0\"/></RNNoise>";
    std::string bytes = "VC2!";
    uint32_t length = uint32_t(xml.size());
    for (int i = 0; i < 4; ++i)
        bytes.push_back(char(length >> (i * 8)));
    bytes += xml;
    bytes.push_back(0);
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (size_t i = 0; i < bytes.size(); i += 3) {
        uint32_t n = uint32_t(uint8_t(bytes[i])) << 16;
        if (i + 1 < bytes.size())
            n |= uint32_t(uint8_t(bytes[i + 1])) << 8;
        if (i + 2 < bytes.size())
            n |= uint8_t(bytes[i + 2]);
        out += alphabet[(n >> 18) & 63];
        out += alphabet[(n >> 12) & 63];
        out += i + 1 < bytes.size() ? alphabet[(n >> 6) & 63] : '=';
        out += i + 2 < bytes.size() ? alphabet[n & 63] : '=';
    }
    return out;
}
class MicrophoneEffects {
    friend class OutputBass;
    static constexpr const char* marker = "# Framoryx microphone noise suppression";
    static bool read(const std::filesystem::path& p, std::string& out) {
        std::ifstream f(p, std::ios::binary);
        if (!f)
            return false;
        out.assign(std::istreambuf_iterator<char>(f), {});
        return f.good() || f.eof();
    }
    static bool write(const std::filesystem::path& p, const std::string& value) {
        auto temp = p;
        temp += L".framoryx.tmp";
        std::ofstream f(temp, std::ios::binary);
        f << value;
        f.close();
        bool ok = bool(f) && MoveFileExW(temp.c_str(), p.c_str(),
                                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
        if (!ok)
            DeleteFileW(temp.c_str());
        return ok;
    }
    static std::string trim(const std::string& s) {
        auto a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
        return a == std::string::npos ? "" : s.substr(a, b - a + 1);
    }
    static bool included(const std::filesystem::path& base, const std::string& text,
                         const wchar_t* name = L"framoryx-microphone.txt") {
        auto expected = (base / name).lexically_normal();
        std::istringstream lines(text);
        std::string line;
        while (std::getline(lines, line)) {
            if (line.compare(0, 3, "\xEF\xBB\xBF") == 0)
                line.erase(0, 3);
            auto colon = line.find(':');
            if (colon == std::string::npos || lower(trim(line.substr(0, colon))) != "include")
                continue;
            auto value = trim(line.substr(colon + 1));
            if (value.size() > 1 && value.front() == '"' && value.back() == '"')
                value = value.substr(1, value.size() - 2);
            std::filesystem::path p(wide(value));
            if (!p.is_absolute())
                p = base / p;
            p = p.lexically_normal();
            if (_wcsicmp(p.c_str(), expected.c_str()) == 0)
                return true;
        }
        return false;
    }

  public:
    static bool equalizerConfigured(const std::wstring& directory, const std::wstring& guid) {
        std::string root, filter;
        std::filesystem::path base(directory);
        return endpointGuid(guid) && read(base / L"config.txt", root) && included(base, root) &&
               read(base / L"framoryx-microphone.txt", filter) && filter.rfind(marker, 0) == 0 &&
               filter.find("Device: " + utf8(guid) + "\r\n") != std::string::npos &&
               filter.find("Filter: ON LS Fc 150 Hz") != std::string::npos;
    }
    static bool configured(const std::wstring& directory, const std::wstring& guid) {
        std::string root, filter;
        std::filesystem::path base(directory);
        return endpointGuid(guid) && read(base / L"config.txt", root) && included(base, root) &&
               read(base / L"framoryx-microphone.txt", filter) && filter.rfind(marker, 0) == 0 &&
               filter.find("Device: " + utf8(guid) + "\r\n") != std::string::npos &&
               filter.find("\r\nVSTPlugin: ") != std::string::npos;
    }
    static std::wstring apply(const std::wstring& directory, const Microphone& mic,
                              const std::wstring& plugin, const MicrophoneOptions& options) {
        if (!endpointGuid(mic.guid))
            return L"Select an available microphone first.";
        if (options.noise && (mic.rate != 48000 || mic.channels < 1 || mic.channels > 2))
            return L"RNNoise needs a mono or stereo microphone at 48,000 Hz. Change its format in "
                   L"Windows Sound settings, then Refresh.";
        if (options.preset < 0 || options.preset > 2 || options.low < -12 || options.low > 12 ||
            options.mid < -12 || options.mid > 12 || options.high < -12 || options.high > 12)
            return L"Choose EQ gains between −12 and +12 dB and an available noise preset.";
        if (options.noise &&
            (plugin.find_first_of(L"\"\r\n") != std::wstring::npos ||
             !std::filesystem::path(plugin).is_absolute() || !rnnoisePlugin(plugin, mic.channels)))
            return L"Choose the official x64 VST2 rnnoise_mono.dll or rnnoise_stereo.dll matching "
                   L"this microphone’s channels.";
        std::filesystem::path base(directory);
        std::string root, prior;
        if (!read(base / L"config.txt", root))
            return L"Install Equalizer APO and choose its config folder first.";
        if (root.find('\0') != std::string::npos)
            return L"Save Equalizer APO config.txt as UTF-8 before adding microphone effects.";
        auto filter = base / L"framoryx-microphone.txt";
        if (read(filter, prior) && prior.rfind(marker, 0) != 0)
            return L"The microphone config file belongs to another program and was not changed.";
        bool linked = included(base, root);
        if (!linked) {
            auto backup =
                base / (L"config.before-framoryx-mic-" + std::to_wstring(GetTickCount64()) + L"-" +
                        std::to_wstring(GetCurrentProcessId()) + L".txt");
            if (!CopyFileW((base / L"config.txt").c_str(), backup.c_str(), TRUE))
                return L"Cannot back up the APO config. Administrator rights may be required for "
                       L"this folder.";
        }
        std::string body =
            std::string(marker) +
            "\r\n# Selected capture endpoint only. No microphone audio is recorded.\r\nDevice: " +
            utf8(mic.guid) + "\r\nStage: capture\r\nChannel: all\r\n";
        if (options.noise)
            body += "If: sampleRate == 48000\r\nVSTPlugin: Library \"" + utf8(plugin) +
                    "\" ChunkData \"" + noiseChunk(options.preset) + "\"\r\nEndIf:\r\n";
        if (options.equalizer) {
            int headroom =
                std::max(0, options.low) + std::max(0, options.mid) + std::max(0, options.high);
            body += "Preamp: -" + std::to_string(headroom) +
                    " dB\r\nFilter: ON LS Fc 150 Hz Gain " + std::to_string(options.low) +
                    " dB\r\nFilter: ON PK Fc 1200 Hz Gain " + std::to_string(options.mid) +
                    " dB Q 0.8\r\nFilter: ON HS Fc 6000 Hz Gain " + std::to_string(options.high) +
                    " dB\r\n";
        }
        if (!options.noise && !options.equalizer)
            body = std::string(marker) + "\r\n# Disabled.\r\n";
        if (!write(filter, body))
            return L"Cannot save the microphone config. Administrator rights may be required.";
        if (!linked) {
            // Includes inherit their parent's device/channel/stage selection. Reset before entry.
            root += "\r\n# Framoryx microphone noise suppression\r\nDevice: all\r\nInclude: "
                    "framoryx-microphone.txt\r\n";
            if (!write(base / L"config.txt", root)) {
                write(filter,
                      std::string(marker) + "\r\n# Disabled: root config could not be linked.\r\n");
                return L"Could not link microphone effects. Nothing was enabled.";
            }
        }
        return {};
    }
    static std::wstring disable(const std::wstring& directory) {
        auto path = std::filesystem::path(directory) / L"framoryx-microphone.txt";
        std::string prior;
        if (!read(path, prior))
            return {};
        if (prior.rfind(marker, 0) != 0)
            return L"The microphone file belongs to another program and was not changed.";
        return write(path, std::string(marker) + "\r\n# Disabled. No audio processing.\r\n")
                   ? L""
                   : L"Could not disable microphone effects. Check write access to the APO config "
                     L"folder.";
    }
};
class OutputBass {
    static constexpr const char* marker = "# Framoryx output bass";
    using Config = MicrophoneEffects;

  public:
    static std::optional<int> configured(const std::wstring& directory, const std::wstring& guid) {
        std::filesystem::path base(directory);
        std::string root, filter;
        if (!endpointGuid(guid) || !Config::read(base / L"config.txt", root) ||
            !Config::included(base, root, L"framoryx-bass.txt") ||
            !Config::read(base / L"framoryx-bass.txt", filter) || filter.rfind(marker, 0) != 0 ||
            filter.find("Device: " + utf8(guid) + "\r\n") == std::string::npos)
            return {};
        auto start = filter.find("# Gain: ");
        if (start == std::string::npos)
            return {};
        auto end = filter.find('\r', start);
        auto n = number(filter.substr(start + 8, end - start - 8));
        return n && *n >= 0 && *n <= 9 ? std::optional<int>(int(*n)) : std::nullopt;
    }
    static std::wstring apply(const std::wstring& directory, const std::wstring& guid, int db) {
        if (!endpointGuid(guid) || db < 0 || db > 9)
            return L"Choose an output device and 0–9 dB of bass.";
        std::filesystem::path base(directory);
        std::string root, prior;
        if (!Config::read(base / L"config.txt", root))
            return L"Install Equalizer APO and configure your playback device first.";
        if (root.find('\0') != std::string::npos)
            return L"Save APO config.txt as UTF-8 before adding bass enhancement.";
        auto path = base / L"framoryx-bass.txt";
        if (Config::read(path, prior) && prior.rfind(marker, 0) != 0)
            return L"The bass config belongs to another program and was not changed.";
        bool linked = Config::included(base, root, L"framoryx-bass.txt");
        if (!linked) {
            auto backup =
                base / (L"config.before-framoryx-bass-" + std::to_wstring(GetTickCount64()) + L"-" +
                        std::to_wstring(GetCurrentProcessId()) + L".txt");
            if (!CopyFileW((base / L"config.txt").c_str(), backup.c_str(), TRUE))
                return L"Cannot back up the APO config. Administrator rights may be required.";
        }
        std::string body = std::string(marker) + "\r\n# Gain: " + std::to_string(db) +
                           "\r\nDevice: " + utf8(guid) +
                           "\r\nStage: post-mix\r\nChannel: all\r\nPreamp: -" + std::to_string(db) +
                           " dB\r\nFilter: ON LS Fc 90 Hz Gain " + std::to_string(db) + " dB\r\n";
        if (!Config::write(path, body))
            return L"Cannot save bass enhancement. Check APO config-folder write access.";
        if (!linked &&
            !Config::write(base / L"config.txt", root + "\r\n# Framoryx output bass\r\nDevice: "
                                                        "all\r\nInclude: framoryx-bass.txt\r\n")) {
            Config::write(path, std::string(marker) +
                                    "\r\n# Disabled: root config could not be linked.\r\n");
            return L"Could not link bass enhancement. Nothing was enabled.";
        }
        return {};
    }
    static std::wstring disable(const std::wstring& directory) {
        auto path = std::filesystem::path(directory) / L"framoryx-bass.txt";
        std::string prior;
        if (!Config::read(path, prior))
            return {};
        if (prior.rfind(marker, 0) != 0)
            return L"The bass config belongs to another program and was not changed.";
        return Config::write(path, std::string(marker) + "\r\n# Disabled. No audio processing.\r\n")
                   ? L""
                   : L"Could not disable bass. Check APO config-folder write access.";
    }
};
} // namespace fg
