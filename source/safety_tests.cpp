#include "microphone.hpp"
#include <array>
#include <barrier>
#include <iostream>
#include <thread>
#include <stdexcept>

int main(int argc, char** argv) {
    if (argc != 2)
        return 2;
    namespace fs = std::filesystem;
    auto scratch = fs::absolute(argv[1]) / ("safety-" + std::to_string(GetCurrentProcessId()) +
                                            "-" + std::to_string(GetTickCount64()));
    fs::create_directories(scratch);
    int checks = 0;
    auto check = [&](bool ok, const char* message) {
        if (!ok)
            throw std::runtime_error(message);
        ++checks;
    };
    auto write = [](const fs::path& p, const std::string& data) {
        std::ofstream(p, std::ios::binary) << data;
    };
    try {
        for (auto path : {L"relative", L"\\\\server\\config", L"C:\\config\\file:stream",
                          L"C:\\bad\nname", L"C:\\bad|name"})
            check(!fg::localFilePath(path), "reject nonlocal or ambiguous config paths");
        check(fg::localFilePath(scratch.wstring()), "allow an ordinary local absolute folder");
        write(scratch / "config.txt", "# Existing audio filters\r\n");
        auto plugin = scratch / L"rnnoise_mono.dll";
        write(plugin, "test plugin bytes");
        auto bytes = fg::readRegularFile(plugin, 128);
        auto digest = fg::sha256Bytes(*bytes);
        auto staged = fg::stageAudioPlugin(scratch.wstring(), plugin.wstring(), digest);
        check(staged && *staged != plugin &&
                  fg::sha256Bytes(*fg::readRegularFile(*staged, 128)) == digest,
              "stage verified bytes separately from the download cache");
        write(plugin, "tampered source");
        check(!fg::stageAudioPlugin(scratch.wstring(), plugin.wstring(), digest),
              "reject changed plugin digest");
        check(fg::sha256Bytes(*fg::readRegularFile(*staged, 128)) == digest,
              "source edits cannot change installed bytes");
        write(plugin, "test plugin bytes");
        write(*staged, "tampered installed copy");
        check(!fg::stageAudioPlugin(scratch.wstring(), plugin.wstring(), digest),
              "reject tampered staged plugin");
        write(scratch / "oversized.txt", std::string(1025, 'x'));
        check(!fg::readRegularFile(scratch / "oversized.txt", 1024),
              "bound config reads before allocation");
        check(fg::atomicFileWrite(scratch / "atomic.txt", "first") &&
                  fg::atomicFileWrite(scratch / "atomic.txt", "second") &&
                  *fg::readRegularFile(scratch / "atomic.txt", 32) ==
                      std::vector<unsigned char>({'s', 'e', 'c', 'o', 'n', 'd'}),
              "atomic replacement preserves complete contents");
        fg::Microphone mic{L"Test", L"test", L"{01234567-89ab-cdef-0123-456789abcdef}", 48000, 1};
        fg::MicrophoneOptions options;
        options.noise = false;
        options.equalizer = true;
        options.low = fg::voicePresets[4].low;
        options.mid = fg::voicePresets[4].mid;
        options.high = fg::voicePresets[4].high;
        for (int trial = 0; trial < 12; ++trial) {
            auto base = scratch / std::to_string(trial);
            fs::create_directory(base);
            write(base / "config.txt", "# Existing filters\r\n");
            std::barrier ready(3);
            std::array<std::wstring, 3> errors;
            std::thread gain([&] {
                ready.arrive_and_wait();
                fg::AudioBoost b;
                b.directory(base.wstring());
                errors[0] = b.apply(6, mic.guid);
            });
            std::thread bass([&] {
                ready.arrive_and_wait();
                errors[1] = fg::OutputBass::apply(base.wstring(), mic.guid, 6);
            });
            std::thread eq([&] {
                ready.arrive_and_wait();
                errors[2] = fg::MicrophoneEffects::apply(base.wstring(), mic, L"", options);
            });
            gain.join();
            bass.join();
            eq.join();
            check(errors[0].empty() && errors[1].empty() && errors[2].empty(),
                  "concurrent audio transactions all finish");
            fg::AudioBoost b;
            b.directory(base.wstring());
            check(b.configuredGain(mic.guid) == 6 &&
                      fg::OutputBass::configured(base.wstring(), mic.guid) == 6 &&
                      fg::MicrophoneEffects::equalizerConfigured(base.wstring(), mic.guid),
                  "concurrent gain, bass and mic updates retain all includes");
            auto filter = fg::readRegularFile(base / "framoryx-microphone.txt", 4096);
            std::string body(filter->begin(), filter->end());
            check(body.find("Preamp: -6 dB") != std::string::npos &&
                      body.find("Fc 150 Hz Gain 6 dB") != std::string::npos &&
                      body.find("Fc 1200 Hz Gain -3 dB") != std::string::npos,
                  "bassy voice writes warm EQ with headroom");
        }
        auto huge = std::string(4 * 1024 * 1024 + 1, 'x');
        write(scratch / "framoryx-bass.txt", huge);
        check(!fg::OutputBass::apply(scratch.wstring(), mic.guid, 3).empty() &&
                  fs::file_size(scratch / "framoryx-bass.txt") == huge.size(),
              "preserve an oversized unowned effects file");
        std::cout << "PASS: " << checks
                  << " file integrity and concurrent audio checks; no system audio changes\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
