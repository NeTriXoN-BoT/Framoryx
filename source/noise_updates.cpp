#include "noise_updates.hpp"
#include "microphone.hpp"
#include "vendor/json/json.hpp"
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "vendor/miniz/miniz.h"
#include <wininet.h>
#include <bcrypt.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <stdexcept>
namespace fg {
namespace {
using Json = nlohmann::json;
namespace fs = std::filesystem;
Json parseMetadata(const std::vector<unsigned char>& bytes, bool strict = true) {
    return Json::parse(
        bytes.begin(), bytes.end(),
        [](int depth, Json::parse_event_t, Json&) {
            if (depth > 48)
                throw std::runtime_error("RNNoise metadata is nested too deeply.");
            return true;
        },
        strict);
}
struct Internet {
    HINTERNET value{};
    ~Internet() {
        if (value)
            InternetCloseHandle(value);
    }
};
bool trustedHost(const std::wstring& host) {
    return host == L"api.github.com" || host == L"github.com" ||
           host == L"release-assets.githubusercontent.com" ||
           host == L"objects.githubusercontent.com";
}
std::string sha256(const std::vector<unsigned char>& data) {
    auto result = sha256Bytes(data);
    if (result.empty())
        throw std::runtime_error("Could not verify the downloaded archive.");
    return result;
}
void save(const fs::path& path, const std::string& value) {
    if (!atomicFileWrite(path, value))
        throw std::runtime_error("Cannot save RNNoise. Move Framoryx to a writable folder.");
}

bool versionTag(const std::string& tag) {
    if (tag.empty() || tag.size() > 64 || tag.find("..") != std::string::npos)
        return false;
    for (auto c : tag)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '.' || c == '-' || c == '_'))
            return false;
    return true;
}
NoiseRelease cachedRelease(const fs::path& base, const Json& data) {
    auto version = data.at("version").get<std::string>();
    if (!versionTag(version))
        throw std::runtime_error("Invalid cached RNNoise version.");
    auto folder = base / wide("rnnoise-" + version);
    NoiseRelease release{(folder / L"rnnoise_mono.dll").wstring(),
                         (folder / L"rnnoise_stereo.dll").wstring(), wide(version)};
    if (version == "v1.10") {
        release.monoDigest = "7debdfc4843461c47b23ac7a736b4663fe581643563ae764cde395e006004080";
        release.stereoDigest = "b2cd23b388bd95cc7a1a3a084b0bd9873992c54f379eed8935e1dc1c3ad07bfe";
    } else {
        if (data.contains("mono_sha256") && data["mono_sha256"].is_string())
            release.monoDigest = data["mono_sha256"];
        if (data.contains("stereo_sha256") && data["stereo_sha256"].is_string())
            release.stereoDigest = data["stereo_sha256"];
    }
    auto mono = readRegularFile(release.mono, 32u * 1024u * 1024u),
         stereo = readRegularFile(release.stereo, 32u * 1024u * 1024u);
    if (release.monoDigest.size() != 64 || release.stereoDigest.size() != 64 || !mono || !stereo ||
        sha256Bytes(*mono) != release.monoDigest || sha256Bytes(*stereo) != release.stereoDigest ||
        !rnnoisePlugin(release.mono, 1) || !rnnoisePlugin(release.stereo, 2))
        throw std::runtime_error(
            "RNNoise files changed or are missing. The existing mic configuration was kept. Review "
            "the files, then use Check for updates.");
    return release;
}
} // namespace
struct NoiseUpdater::State {
    std::thread worker;
    std::atomic<bool> busy{false}, cancel{false};
    std::atomic<HINTERNET> request{nullptr};
    mutable std::mutex mutex;
    std::wstring message = L"RNNoise downloads automatically when you open Microphone.";
    std::optional<NoiseRelease> result;
    void status(const std::wstring& text) {
        std::lock_guard lock(mutex);
        message = text;
    }
    std::vector<unsigned char> get(std::wstring url, size_t limit, bool binary = false) {
        Internet session{
            InternetOpenW(L"Framoryx", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0)};
        if (!session.value)
            throw std::runtime_error("Network access is unavailable.");
        DWORD timeout = 10000;
        for (auto option : {INTERNET_OPTION_CONNECT_TIMEOUT, INTERNET_OPTION_SEND_TIMEOUT,
                            INTERNET_OPTION_RECEIVE_TIMEOUT})
            InternetSetOptionW(session.value, option, &timeout, sizeof(timeout));
        for (int redirect = 0; redirect < 6 && !cancel; ++redirect) {
            URL_COMPONENTS parts{};
            parts.dwStructSize = sizeof(parts);
            parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength = DWORD(-1);
            if (!InternetCrackUrlW(url.c_str(), 0, 0, &parts) ||
                parts.nScheme != INTERNET_SCHEME_HTTPS || parts.nPort != 443)
                throw std::runtime_error("RNNoise requires an official HTTPS download.");
            std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
            if (!trustedHost(host))
                throw std::runtime_error("Unexpected RNNoise download host.");
            std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
            if (parts.dwExtraInfoLength)
                path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
            Internet connection{InternetConnectW(session.value, host.c_str(), 443, nullptr, nullptr,
                                                 INTERNET_SERVICE_HTTP, 0, 0)};
            HINTERNET handle = HttpOpenRequestW(
                connection.value, L"GET", path.c_str(), nullptr, nullptr, nullptr,
                INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_AUTO_REDIRECT | INTERNET_FLAG_NO_COOKIES |
                    INTERNET_FLAG_NO_AUTH | INTERNET_FLAG_NO_UI | INTERNET_FLAG_RELOAD |
                    INTERNET_FLAG_NO_CACHE_WRITE,
                0);
            if (!handle)
                throw std::runtime_error("Could not open the RNNoise download.");
            request = handle;
            struct RequestGuard {
                State* state;
                HINTERNET handle;
                ~RequestGuard() {
                    HINTERNET expected = handle;
                    if (state->request.compare_exchange_strong(expected, nullptr))
                        InternetCloseHandle(handle);
                }
            } guard{this, handle};
            const wchar_t* headers =
                binary ? L"Accept: application/octet-stream\r\nCache-Control: no-cache\r\n"
                       : L"Accept: application/vnd.github+json\r\nCache-Control: no-cache\r\n";
            if (cancel || !HttpSendRequestW(handle, headers, DWORD(wcslen(headers)), nullptr, 0))
                throw std::runtime_error("RNNoise network error " + std::to_string(GetLastError()) +
                                         " from " + utf8(host) + ". The current plugin was kept.");
            DWORD code = 0, size = sizeof(code);
            HttpQueryInfoW(handle, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &code, &size,
                           nullptr);
            if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
                DWORD length = 0;
                HttpQueryInfoW(handle, HTTP_QUERY_LOCATION, nullptr, &length, nullptr);
                std::wstring location(length / sizeof(wchar_t), 0);
                if (!length ||
                    !HttpQueryInfoW(handle, HTTP_QUERY_LOCATION, location.data(), &length, nullptr))
                    throw std::runtime_error("Invalid RNNoise download redirect.");
                location.resize(wcslen(location.c_str()));
                url = location;
                continue;
            }
            if (code >= 500 && redirect < 3) {
                for (int i = 0; i < 10 && !cancel; ++i)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            if (code != 200)
                throw std::runtime_error(code == 403 || code == 429
                                             ? "GitHub rate limit reached. Try again later."
                                             : "RNNoise release is unavailable (HTTP " +
                                                   std::to_string(code) + ", " + utf8(host) +
                                                   "). The current plugin was kept.");
            std::vector<unsigned char> bytes;
            unsigned char block[65536];
            DWORD count = 0;
            while (!cancel) {
                if (!InternetReadFile(handle, block, sizeof(block), &count))
                    throw std::runtime_error("RNNoise download was interrupted.");
                if (!count)
                    return bytes;
                if (bytes.size() + count > limit)
                    throw std::runtime_error("RNNoise download exceeds its size limit.");
                bytes.insert(bytes.end(), block, block + count);
            }
        }
        throw std::runtime_error("RNNoise download cancelled or redirected too many times.");
    }
    void run(const fs::path& base, bool force) {
        try {
            std::error_code error;
            fs::create_directories(base, error);
            if (error)
                throw std::runtime_error("The RNNoise folder is not writable.");
            auto now = std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
            Json cache;
            if (auto bytes = readRegularFile(base / L"latest.json", 65536))
                cache = parseMetadata(*bytes, false);
            bool fresh = cache.is_object() && cache.contains("checked") &&
                         cache["checked"].is_number_integer();
            int64_t checked = fresh ? cache["checked"].get<int64_t>() : 0;
            fresh = fresh && checked > now - 86400 && checked <= now + 300;
            if (!force && fresh) {
                auto release = cachedRelease(base, cache);
                std::lock_guard lock(mutex);
                result = release;
                message = L"RNNoise " + release.version + L" ready · file checksums verified";
                busy = false;
                return;
            }
            status(L"Checking the latest official RNNoise release…");
            Json release;
            try {
                auto metadata = get(L"https://api.github.com/repos/werman/"
                                    L"noise-suppression-for-voice/releases/latest",
                                    2 * 1024 * 1024);
                release = parseMetadata(metadata);
            } catch (const std::exception&) {
                if (cancel)
                    throw;
                // GitHub's latest endpoint can fail while the release-list endpoint is healthy.
                auto metadata =
                    get(L"https://api.github.com/repos/werman/noise-suppression-for-voice/releases",
                        2 * 1024 * 1024);
                for (const auto& item : parseMetadata(metadata))
                    if (!item.value("prerelease", false) && !item.value("draft", false)) {
                        release = item;
                        break;
                    }
                if (release.is_null())
                    throw std::runtime_error("No stable RNNoise release is available.");
            }
            auto tag = release.at("tag_name").get<std::string>();
            if (!versionTag(tag) || release.value("prerelease", false) ||
                release.value("draft", false))
                throw std::runtime_error("No compatible stable RNNoise release was found.");
            Json asset;
            for (const auto& a : release.at("assets"))
                if (a.value("name", std::string()) == "win-rnnoise.zip")
                    asset = a;
            if (!asset.is_object())
                throw std::runtime_error("The latest release has no compatible Windows VST2 "
                                         "package. The current plugin was kept.");
            auto url = asset.at("browser_download_url").get<std::string>();
            if (url.rfind(
                    "https://github.com/werman/noise-suppression-for-voice/releases/download/" +
                        tag + "/",
                    0) != 0)
                throw std::runtime_error("Unexpected RNNoise release asset.");
            auto folder = base / wide("rnnoise-" + tag);
            Json record{
                {"version", tag},
                {"checked", now},
                {"source", "https://github.com/werman/noise-suppression-for-voice/tree/" + tag}};
            NoiseRelease installed;
            bool exists = false;
            try {
                installed = cachedRelease(base, cache.is_object() &&
                                                        cache.value("version", std::string()) == tag
                                                    ? cache
                                                    : record);
                exists = true;
            } catch (...) {
            }
            if (!exists) {
                status(L"Downloading RNNoise " + wide(tag) +
                       L" · the audio controls stay available");
                std::vector<unsigned char> archive;
                try {
                    archive = get(wide(url), 96 * 1024 * 1024, true);
                } catch (const std::exception&) {
                    if (cancel)
                        throw;
                    auto api = asset.at("url").get<std::string>();
                    if (api.rfind("https://api.github.com/repos/werman/noise-suppression-for-voice/"
                                  "releases/assets/",
                                  0) != 0)
                        throw;
                    archive = get(wide(api), 96 * 1024 * 1024, true);
                }
                if (archive.size() != asset.at("size").get<size_t>())
                    throw std::runtime_error("RNNoise archive size does not match its release.");
                auto hash = sha256(archive);
                auto digest = asset.contains("digest") && asset["digest"].is_string()
                                  ? asset["digest"].get<std::string>()
                                  : "";
                if (!digest.empty() && digest != "sha256:" + hash)
                    throw std::runtime_error("RNNoise archive checksum mismatch.");
                if (tag == "v1.10" &&
                    hash != "e4dcc186102850f8dcebeb766ad1bf34c24f6280bd614ef84b72dae4783fc517")
                    throw std::runtime_error("RNNoise v1.10 does not match the verified archive.");
                record["archive_sha256"] = hash;
                mz_zip_archive zip{};
                if (!mz_zip_reader_init_mem(&zip, archive.data(), archive.size(), 0))
                    throw std::runtime_error("Invalid RNNoise archive.");
                struct ZipGuard {
                    mz_zip_archive* zip;
                    ~ZipGuard() {
                        mz_zip_reader_end(zip);
                    }
                } zipGuard{&zip};
                fs::create_directories(folder);
                for (const char* file : {"rnnoise_mono.dll", "rnnoise_stereo.dll"}) {
                    int index = -1;
                    for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&zip); ++i) {
                        mz_zip_archive_file_stat stat{};
                        if (mz_zip_reader_file_stat(&zip, i, &stat)) {
                            std::string name = stat.m_filename;
                            if (name == std::string("win-rnnoise/vst/") + file ||
                                name == std::string("vst/") + file) {
                                if (stat.m_uncomp_size < 64 ||
                                    stat.m_uncomp_size > 32 * 1024 * 1024 || stat.m_is_encrypted)
                                    throw std::runtime_error(
                                        "Unsupported RNNoise plugin size or encryption.");
                                index = int(i);
                                break;
                            }
                        }
                    }
                    if (index < 0)
                        throw std::runtime_error("Latest RNNoise package has no compatible VST2 "
                                                 "plugins. The current version was kept.");
                    size_t length = 0;
                    auto data = static_cast<const char*>(
                        mz_zip_reader_extract_to_heap(&zip, mz_uint(index), &length, 0));
                    if (!data)
                        throw std::runtime_error("RNNoise archive failed its CRC check.");
                    std::string binary(data, length);
                    std::vector<unsigned char> decoded(binary.begin(), binary.end());
                    record[std::string(file).find("mono") != std::string::npos ? "mono_sha256"
                                                                               : "stereo_sha256"] =
                        sha256Bytes(decoded);
                    mz_free(const_cast<char*>(data));
                    auto path = folder / wide(file);
                    save(path, binary);
                    if (!rnnoisePlugin(path,
                                       std::string(file).find("mono") != std::string::npos ? 1 : 2))
                        throw std::runtime_error(
                            "Latest RNNoise plugin is not compatible x64 VST2.");
                }
                installed = cachedRelease(base, record);
                save(folder / L"SOURCE.txt",
                     record["source"].get<std::string>() +
                         "\nRNNoise plugin: GPL-3.0. Downloaded from the upstream release.\n");
            }
            record["mono_sha256"] = installed.monoDigest;
            record["stereo_sha256"] = installed.stereoDigest;
            if (cancel)
                throw std::runtime_error("RNNoise update cancelled.");
            status(L"Checking RNNoise presets in an isolated audio helper…");
            for (const auto& plugin : {installed.mono, installed.stereo}) {
                auto helper = executableDir() + L"\\tools\\FramoryxAudioProbe.exe";
                std::wstring command = L"\"" + helper + L"\" \"" + plugin + L"\"";
                STARTUPINFOW start{};
                start.cb = sizeof(start);
                PROCESS_INFORMATION process{};
                if (!CreateProcessW(helper.c_str(), command.data(), nullptr, nullptr, FALSE,
                                    CREATE_NO_WINDOW, nullptr, nullptr, &start, &process))
                    throw std::runtime_error("The audio compatibility helper is missing or "
                                             "blocked. The current plugin was kept.");
                Handle child(process.hProcess), thread(process.hThread);
                DWORD waited = WAIT_TIMEOUT;
                for (int i = 0; i < 200 && !cancel && waited == WAIT_TIMEOUT; ++i)
                    waited = WaitForSingleObject(child.h, 100);
                if (waited != WAIT_OBJECT_0) {
                    TerminateProcess(child.h, 1);
                    WaitForSingleObject(child.h, 2000);
                    throw std::runtime_error(
                        "RNNoise compatibility check timed out. The current plugin was kept.");
                }
                DWORD code = 1;
                GetExitCodeProcess(child.h, &code);
                if (code)
                    throw std::runtime_error("The latest RNNoise plugin did not pass its "
                                             "audio/preset check. The current plugin was kept.");
            }
            save(base / L"latest.json", record.dump(2));
            {
                std::lock_guard lock(mutex);
                result = installed;
                message = L"RNNoise " + installed.version + L" ready · official Windows VST2";
            }
        } catch (const std::exception& e) {
            status(wide(e.what()));
        }
        busy = false;
    }
};
NoiseUpdater::NoiseUpdater() : state(std::make_unique<State>()) {}
NoiseUpdater::~NoiseUpdater() {
    state->cancel = true;
    if (auto request = state->request.exchange(nullptr))
        InternetCloseHandle(request);
    if (state->worker.joinable())
        state->worker.join();
}
void NoiseUpdater::start(const std::wstring& directory, bool force) {
    if (state->busy.exchange(true))
        return;
    if (state->worker.joinable())
        state->worker.join();
    state->cancel = false;
    {
        std::lock_guard lock(state->mutex);
        state->result.reset();
    }
    try {
        state->worker = std::thread([this, directory, force] { state->run(directory, force); });
    } catch (const std::exception&) {
        state->status(L"Windows could not start the update worker. Try again later.");
        state->busy = false;
    }
}
bool NoiseUpdater::running() const {
    return state->busy;
}
std::wstring NoiseUpdater::status() const {
    std::lock_guard lock(state->mutex);
    return state->message;
}
std::optional<NoiseRelease> NoiseUpdater::takeResult() {
    std::lock_guard lock(state->mutex);
    auto result = state->result;
    state->result.reset();
    return result;
}
NoiseUpdateSnapshot NoiseUpdater::poll() {
    std::lock_guard lock(state->mutex);
    NoiseUpdateSnapshot snapshot{state->busy.load(), state->message, std::move(state->result)};
    state->result.reset();
    return snapshot;
}
} // namespace fg
