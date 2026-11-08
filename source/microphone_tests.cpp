#include "microphone.hpp"
#include <iostream>
#include <stdexcept>
int main(int argc, char** argv) {
    if (argc != 2)
        return 2;
    namespace fs = std::filesystem;
    fs::path scratch = fs::absolute(argv[1]) / ("effects-" + std::to_string(GetCurrentProcessId()) +
                                                "-" + std::to_string(GetTickCount64()));
    fs::create_directories(scratch);
    int checks = 0;
    auto check = [&](bool ok, const char* message) {
        if (!ok)
            throw std::runtime_error(message);
        ++checks;
    };
    auto read = [](const fs::path& path) {
        std::ifstream f(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(f), {});
    };
    auto write = [](const fs::path& path, const std::string& data) {
        std::ofstream(path, std::ios::binary) << data;
    };
    try {
        const std::string original = "# Existing filters\r\nDevice: Speakers\r\nPreamp: -1 dB\r\n";
        write(scratch / "config.txt", original);
        fg::Microphone mic{L"Test microphone", L"test", L"{01234567-89ab-cdef-0123-456789abcdef}",
                           48000, 1};
        std::string pe(128, '\0');
        pe[0] = 'M';
        pe[1] = 'Z';
        pe[60] = 64;
        pe[64] = 'P';
        pe[65] = 'E';
        pe[68] = 0x64;
        pe[69] = char(0x86);
        auto plugin = scratch / L"rnnoise_mono.dll";
        write(plugin, pe);
        fg::MicrophoneOptions options;
        auto apply = [&] {
            return fg::MicrophoneEffects::apply(scratch.wstring(), mic, plugin.wstring(), options);
        };
        check(fg::rnnoisePlugin(plugin, 1), "recognize x64 mono PE format without loading code");
        check(!fg::rnnoisePlugin(plugin, 2), "reject a plugin with the wrong channel layout");
        check(!fg::rnnoisePlugin(plugin, 0), "reject unsupported capture channels");
        write(scratch / L"wrong.dll", pe);
        check(!fg::rnnoisePlugin(scratch / L"wrong.dll", 1),
              "restrict selection to expected RNNoise names");
        mic.rate = 44100;
        check(!apply().empty(), "reject RNNoise at 44.1 kHz");
        mic.rate = 48000;
        options.low = 13;
        check(!apply().empty(), "reject invalid EQ gain");
        options.low = 0;
        options.preset = 3;
        check(!apply().empty(), "reject unknown voice preset");
        options.preset = 1;
        check(apply().empty(), "write isolated noise suppression config");
        auto root = read(scratch / "config.txt"), body = read(scratch / "framoryx-microphone.txt");
        check(root.rfind(original, 0) == 0, "preserve all existing audio filters");
        check(root.find("Device: all\r\nInclude: framoryx-microphone.txt") != std::string::npos,
              "reset parent endpoint before including capture effects");
        check(body.find("Device: " + fg::utf8(mic.guid)) != std::string::npos,
              "scope processing to the selected microphone");
        check(body.find("Stage: capture") != std::string::npos,
              "never run microphone effects on a playback endpoint");
        check(body.find("If: sampleRate == 48000") != std::string::npos,
              "guard the plugin if the format later changes");
        check(body.find(fg::noiseChunk(1)) != std::string::npos, "store balanced voice state");
        check(fg::noiseChunk(0) != fg::noiseChunk(2),
              "different voice presets produce different plugin states");
        check(fg::MicrophoneEffects::configured(scratch.wstring(), mic.guid),
              "report configured noise state");
        check(!fg::MicrophoneEffects::equalizerConfigured(scratch.wstring(), mic.guid),
              "do not claim EQ when absent");
        check(apply().empty() && read(scratch / "config.txt") == root,
              "updates do not duplicate root includes");
        bool backup = false;
        for (auto& file : fs::directory_iterator(scratch))
            if (file.path().filename().wstring().rfind(L"config.before-framoryx-mic-", 0) == 0)
                backup = read(file.path()) == original;
        check(backup, "retain the original config backup");
        options.equalizer = true;
        options.low = 3;
        options.mid = -3;
        options.high = 6;
        check(apply().empty(), "combine noise suppression and EQ");
        body = read(scratch / "framoryx-microphone.txt");
        check(body.find("Preamp: -9 dB") != std::string::npos,
              "compensate the sum of positive EQ gains");
        check(body.find("Fc 150 Hz Gain 3 dB") != std::string::npos &&
                  body.find("Fc 1200 Hz Gain -3 dB Q 0.8") != std::string::npos &&
                  body.find("Fc 6000 Hz Gain 6 dB") != std::string::npos,
              "write all three voice bands");
        check(fg::MicrophoneEffects::equalizerConfigured(scratch.wstring(), mic.guid),
              "report configured microphone EQ");
        options.noise = false;
        mic.rate = 44100;
        check(apply().empty(), "EQ works without RNNoise or a 48 kHz plugin");
        check(!fg::MicrophoneEffects::configured(scratch.wstring(), mic.guid),
              "EQ-only does not claim noise suppression");
        check(fg::MicrophoneEffects::disable(scratch.wstring()).empty(),
              "disable microphone effects");
        check(!fg::MicrophoneEffects::equalizerConfigured(scratch.wstring(), mic.guid),
              "disabled EQ is not reported as configured");
        write(scratch / "framoryx-microphone.txt", "foreign");
        check(!apply().empty() && read(scratch / "framoryx-microphone.txt") == "foreign",
              "preserve an unowned microphone file");
        check(!fg::MicrophoneEffects::disable(scratch.wstring()).empty(),
              "never disable unowned microphone effects");
        check(fg::OutputBass::apply(scratch.wstring(), mic.guid, 6).empty(),
              "configure isolated bass");
        auto bass = read(scratch / "framoryx-bass.txt");
        check(bass.find("Preamp: -6 dB") != std::string::npos &&
                  bass.find("Fc 90 Hz Gain 6 dB") != std::string::npos,
              "match bass lift with headroom compensation");
        check(bass.find("Stage: post-mix") != std::string::npos,
              "bass applies only to output processing");
        check(fg::OutputBass::configured(scratch.wstring(), mic.guid) == 6,
              "read configured bass amount");
        check(!fg::OutputBass::configured(scratch.wstring(),
                                          L"{ffffffff-ffff-ffff-ffff-ffffffffffff}"),
              "do not claim bass on another endpoint");
        check(!fg::OutputBass::apply(scratch.wstring(), mic.guid, 10).empty(),
              "reject excessive bass gain");
        auto previous = read(scratch / "config.txt");
        check(fg::OutputBass::apply(scratch.wstring(), mic.guid, 3).empty() &&
                  previous == read(scratch / "config.txt"),
              "bass updates do not duplicate includes");
        check(fg::OutputBass::disable(scratch.wstring()).empty() &&
                  !fg::OutputBass::configured(scratch.wstring(), mic.guid),
              "disable and clear bass status");
        write(scratch / "framoryx-bass.txt", "foreign");
        check(!fg::OutputBass::apply(scratch.wstring(), mic.guid, 3).empty() &&
                  !fg::OutputBass::disable(scratch.wstring()).empty(),
              "preserve unowned bass files");
        std::cout << "PASS: " << checks
                  << " microphone/bass configuration checks; no system audio changes\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
