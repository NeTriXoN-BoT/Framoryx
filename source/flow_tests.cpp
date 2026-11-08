#include "audio_gain.hpp"
#include "microphone.hpp"
#include "noise_updates.hpp"
#include "export.hpp"
#include "vendor/json/json.hpp"
#include <future>
#include <iostream>
#include <ctime>

int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    if (argc != 2)
        return 2;
    namespace fs = std::filesystem;
    auto root = fs::absolute(argv[1]) / ("flows-" + std::to_string(GetCurrentProcessId()) + "-" +
                                         std::to_string(GetTickCount64()));
    fs::create_directories(root);
    int checks = 0;
    auto check = [&](bool ok, const char* why) {
        if (!ok)
            throw std::runtime_error(why);
        ++checks;
    };
    try {
        fg::Stats stats;
        stats.app = "caf\xC3\xA9,\"game\".exe";
        stats.chain = "0x1,\"2\"";
        stats.pid = 42;
        stats.frames = {{123.5, 16.666667}, {123.75, 8.333333}};
        struct Comma : std::numpunct<char> {
            char do_decimal_point() const override {
                return ',';
            }
        };
        std::ostringstream csv;
        csv.imbue(std::locale(std::locale::classic(), new Comma));
        check(fg::writeCaptureCsv(csv, stats), "write export into destination");
        check(csv.str().starts_with("\xEF\xBB\xBF"), "emit UTF-8 marker for spreadsheet readers");
        std::istringstream lines(csv.str());
        std::string line;
        std::getline(lines, line);
        fg::Parser parser;
        parser.parse(line);
        std::getline(lines, line);
        auto first = parser.parse(line);
        check(first && first->app == stats.app && first->chain == stats.chain && first->pid == 42 &&
                  first->time == 0,
              "round-trip quoted UTF-8 names and swapchains with zero-based timestamps");
        std::getline(lines, line);
        auto second = parser.parse(line);
        check(second && second->time == .25 && std::abs(second->ms - 8.333333) < 1e-8,
              "export numeric data independent of destination locale");
        std::ostringstream failed;
        failed.setstate(std::ios::badbit);
        check(!fg::writeCaptureCsv(failed, stats), "report destination write failures");
        const auto directory = (root / "audio").wstring();
        fs::create_directory(directory);
        fg::atomicFileWrite(fs::path(directory) / "config.txt", "# Isolated test config\r\n");
        const std::wstring guid = L"{01234567-89ab-cdef-0123-456789abcdef}";
        fg::AudioBoost boost;
        boost.directory(directory);
        check(boost.apply(6, guid).empty(), "seed scratch gain");
        {
            fg::GainWorker worker;
            worker.request(directory, guid, 0);
        }
        check(boost.configuredGain(guid) == 0,
              "finish final disable request during worker shutdown");
        {
            fg::GainWorker worker;
            for (int i = 0; i < 100; ++i)
                worker.request(directory, guid, i % 2 ? 3 : 6);
            worker.request(directory, guid, 20 * std::log10(2.));
        }
        auto gain = boost.configuredGain(guid);
        check(gain && std::abs(*gain - 20 * std::log10(2.)) < 1e-7,
              "retain latest coalesced gain at shutdown");
        check(fg::OutputBass::apply(directory, guid, 6).empty(), "seed scratch bass");
        std::promise<void> entered, resume;
        auto resumed = resume.get_future().share();
        auto worker = std::make_unique<fg::AudioEffectWorker>();
        worker->submit([&] {
            entered.set_value();
            resumed.wait();
            return std::wstring();
        });
        entered.get_future().wait();
        check(worker->submit([=] { return fg::OutputBass::disable(directory); }),
              "accept pending disable behind another operation");
        std::thread unblock([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            resume.set_value();
        });
        worker.reset();
        unblock.join();
        check(!fg::OutputBass::configured(directory, guid),
              "drain accepted effect changes during shutdown");
        {
            std::promise<void> active, release;
            auto released = release.get_future().share();
            auto effects = std::make_unique<fg::AudioEffectWorker>();
            effects->submit([&] {
                active.set_value();
                released.wait();
                return std::wstring();
            });
            active.get_future().wait();
            int micRuns = 0, bassRuns = 0, micValue = -1;
            bool accepted = true;
            for (int i = 0; i < 100; ++i)
                accepted &= effects->submit(
                    [&, i] {
                        ++micRuns;
                        micValue = i;
                        return std::wstring();
                    },
                    1);
            accepted &= effects->submit(
                [&] {
                    ++bassRuns;
                    return std::wstring();
                },
                2);
            accepted &= effects->submit(
                [&] {
                    ++micRuns;
                    micValue = 0;
                    return std::wstring();
                },
                1);
            const bool heldResult = !effects->takeResult();
            release.set_value();
            effects.reset();
            check(accepted, "coalesce rapid edits without rejecting the final disable");
            check(heldResult, "hold intermediate results until all accepted changes finish");
            check(micRuns == 1 && micValue == 0, "apply only the latest queued mic action");
            check(bassRuns == 1, "preserve independently queued bass changes");
        }
        {
            fg::AudioEffectWorker effects;
            check(effects.submit([]() -> std::wstring { throw std::runtime_error("fixture"); }),
                  "accept an isolated failing task");
            auto deadline = GetTickCount64() + 3000;
            while (effects.busy() && GetTickCount64() < deadline)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            auto result = effects.takeResult();
            check(result && !result->empty(), "report worker failure without terminating the app");
        }
        auto cache = root / "rnnoise";
        auto folder = cache / "rnnoise-v0.0.0-fixture";
        fs::create_directories(folder);
        std::string pe(128, '\0');
        pe[0] = 'M';
        pe[1] = 'Z';
        pe[60] = 64;
        pe[64] = 'P';
        pe[65] = 'E';
        pe[68] = 0x64;
        pe[69] = char(0x86);
        auto digest = fg::sha256Bytes(std::vector<unsigned char>(pe.begin(), pe.end()));
        fg::atomicFileWrite(folder / "rnnoise_mono.dll", pe);
        fg::atomicFileWrite(folder / "rnnoise_stereo.dll", pe);
        nlohmann::json record = {{"version", "v0.0.0-fixture"},
                                 {"checked", std::time(nullptr)},
                                 {"mono_sha256", digest},
                                 {"stereo_sha256", digest}};
        fg::atomicFileWrite(cache / "latest.json", record.dump());
        for (int i = 0; i < 64; ++i) {
            fg::NoiseUpdater updater;
            updater.start(cache.wstring());
            bool received = false, finished = false;
            auto deadline = GetTickCount64() + 3000;
            while (GetTickCount64() < deadline) {
                auto snapshot = updater.poll();
                if (snapshot.release)
                    received = true;
                if (!snapshot.running) {
                    finished = true;
                    break;
                }
                std::this_thread::yield();
            }
            check(finished && received,
                  "completion snapshot never loses a successful cache result");
            check(!updater.poll().release, "consume completed release exactly once");
        }
        fg::atomicFileWrite(folder / "rnnoise_mono.dll", pe + "tampered");
        fg::NoiseUpdater bad;
        bad.start(cache.wstring());
        while (bad.running())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto result = bad.poll();
        check(!result.running && !result.release &&
                  result.message.find(L"changed") != std::wstring::npos,
              "report failed cache verification as one consistent completion");
        std::cout << "PASS: " << checks
                  << " export, worker and updater flow checks; no system audio changes or native "
                     "windows\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
