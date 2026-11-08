#include "noise_updates.hpp"
#include "microphone.hpp"
#include <iostream>
#include <thread>

int wmain(int argc, wchar_t** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    if (argc != 2)
        return 2;
    namespace fs = std::filesystem;
    auto root = fs::absolute(argv[1]);
    fs::create_directories(root);
    int checks = 0;
    auto check = [&](bool ok, const char* why) {
        if (!ok)
            throw std::runtime_error(why);
        ++checks;
    };
    auto completed = [&](bool force) {
        fg::NoiseUpdater updater;
        updater.start((root / L"cache").wstring(), force);
        auto deadline = GetTickCount64() + 180000;
        fg::NoiseUpdateSnapshot outcome;
        std::optional<fg::NoiseRelease> release;
        do {
            auto snapshot = updater.poll();
            if (snapshot.release)
                release = std::move(snapshot.release);
            outcome = std::move(snapshot);
            if (!outcome.running)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        } while (GetTickCount64() < deadline);
        if (outcome.running)
            throw std::runtime_error("RNNoise integration timed out");
        outcome.release = std::move(release);
        return outcome;
    };
    try {
        auto result = completed(true);
        std::cout << fg::utf8(result.message) << '\n';
        check(result.release.has_value(),
              "download official current Windows plugin and pass isolated audio probes");
        auto release = *result.release;
        check(release.monoDigest.size() == 64 && release.stereoDigest.size() == 64 &&
                  fg::rnnoisePlugin(release.mono, 1) && fg::rnnoisePlugin(release.stereo, 2),
              "select validated mono and stereo plugins");
        auto cached = completed(false);
        check(cached.release && cached.message.find(L"checksums verified") != std::wstring::npos,
              "reuse verified current cache without another download");
        auto config = root / L"isolated-config";
        fs::create_directories(config);
        fg::atomicFileWrite(config / L"config.txt",
                            "# Offline test only; never loaded by system APO\r\n");
        fg::Microphone mic{L"Offline fixture", L"offline",
                           L"{01234567-89ab-cdef-0123-456789abcdef}", 48000, 1};
        fg::MicrophoneOptions options;
        options.noise = true;
        options.equalizer = true;
        options.low = 6;
        options.mid = -3;
        check(fg::MicrophoneEffects::apply(config.wstring(), mic, release.mono, options,
                                           release.monoDigest)
                  .empty(),
              "install verified official plugin and combined mic EQ into isolated config");
        auto installed =
            config / L"FramoryxPlugins" / fg::wide(release.monoDigest) / L"rnnoise_mono.dll";
        auto installedBytes = fg::readRegularFile(installed, 32u * 1024u * 1024u);
        check(installedBytes && fg::sha256Bytes(*installedBytes) == release.monoDigest,
              "installed plugin matches verified source bytes");
        auto changed = fg::readRegularFile(release.mono, 32u * 1024u * 1024u);
        if (!changed || changed->empty())
            throw std::runtime_error("Missing cache fixture");
        changed->back() ^= 1;
        fg::atomicFileWrite(release.mono, std::string(changed->begin(), changed->end()));
        auto invalid = completed(false);
        check(!invalid.release && invalid.message.find(L"changed") != std::wstring::npos,
              "reject changed cached DLL without loading it");
        auto preserved = fg::readRegularFile(installed, 32u * 1024u * 1024u);
        check(preserved && fg::sha256Bytes(*preserved) == release.monoDigest &&
                  fg::MicrophoneEffects::configured(config.wstring(), mic.guid),
              "preserve previously installed chain when cache changes");
        check(fg::MicrophoneEffects::disable(config.wstring()).empty() &&
                  !fg::MicrophoneEffects::configured(config.wstring(), mic.guid),
              "disable isolated effects cleanly");
        std::cout << "PASS: " << checks
                  << " official RNNoise download/probe/staging checks; no recording, audio devices "
                     "or system config changes\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
