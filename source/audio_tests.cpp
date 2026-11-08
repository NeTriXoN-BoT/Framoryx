#include "audio.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <stdexcept>
int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Provide a scratch test directory. This test never changes system audio.\n";
        return 2;
    }
    namespace fs = std::filesystem;
    fs::path scratch = fs::absolute(argv[1]) / ("run-" + std::to_string(GetCurrentProcessId()) +
                                                "-" + std::to_string(GetTickCount64()));
    fs::create_directories(scratch);
    int count = 0;
    auto check = [&](bool ok, const char* why) {
        if (!ok)
            throw std::runtime_error(why);
        ++count;
    };
    auto read = [](const fs::path& p) {
        std::ifstream f(p, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(f), {});
    };
    try {
        const std::string original =
            "# Existing user audio configuration\r\nFilter: ON PK Fc 200 Hz Gain -1 dB Q 1\r\n";
        std::ofstream(scratch / "config.txt", std::ios::binary) << original;
        fg::AudioBoost boost;
        boost.directory(scratch.wstring());
        const std::wstring device = L"{01234567-89ab-cdef-0123-456789abcdef}";
        check(boost.available(), "find test config");
        check(!boost.apply(6, L"not-a-device").empty(), "reject invalid endpoint GUID");
        check(boost.apply(6, device).empty(), "configure gain in scratch directory");
        auto config = read(scratch / "config.txt");
        check(config.rfind(original, 0) == 0, "preserve existing user filters byte for byte");
        auto include = std::string("Include: framoryx-boost.txt");
        check(config.find(include) != std::string::npos, "include boost file");
        auto filter = read(scratch / "framoryx-boost.txt");
        check(filter.find("Preamp: 6 dB") != std::string::npos &&
                  filter.find(fg::utf8(device)) != std::string::npos,
              "gain applies to selected output only");
        check(boost.configuredGain(device) == 6, "read configured gain");
        check(boost.apply(3, device).empty(), "update existing gain");
        double maximum=20*std::log10(2.0);
        check(boost.apply(maximum,device).empty() && boost.configuredGain(device) &&
              std::abs(100*std::pow(10.,*boost.configuredGain(device)/20.)-200)<.001,
              "200 percent corresponds to precise double-amplitude gain");
        check(boost.apply(3, device).empty(),"restore gentle gain after maximum check");
        config = read(scratch / "config.txt");
        auto pos = config.find(include);
        check(config.find(include, pos + 1) == std::string::npos, "no duplicate include");
        int backups = 0;
        for (auto& item : fs::directory_iterator(scratch))
            if (item.path().filename().string().rfind("config.before-framoryx-", 0) == 0) {
                check(read(item.path()) == original, "backup preserves original config");
                ++backups;
            }
        check(backups >= 1, "original config backup created");
        check(boost.disable().empty() && boost.configuredGain(device) == 0,
              "disable sets zero gain");
        std::ofstream(scratch / "config.txt", std::ios::binary)
            << "# Include: framoryx-boost.txt\r\n";
        check(boost.apply(6, device).empty() && boost.configuredGain(device) == 6,
              "a commented include does not masquerade as an active boost");
        std::string absoluteInclude =
            "Include: \"" + fg::utf8((scratch / "framoryx-boost.txt").wstring()) + "\"\r\n";
        std::ofstream(scratch / "config.txt", std::ios::binary) << absoluteInclude;
        check(boost.apply(3, device).empty() && read(scratch / "config.txt") == absoluteInclude,
              "an absolute include is recognized without applying the boost twice");
        check(!boost.configuredGain(L""),
              "no configured output gain is claimed when no endpoint exists");
        std::ofstream(scratch / "framoryx-boost.txt") << "foreign config";
        check(!boost.apply(6, device).empty() &&
                  read(scratch / "framoryx-boost.txt") == "foreign config",
              "do not overwrite an unowned file");
        check(!boost.disable().empty(), "do not disable an unowned file");
        std::ofstream(scratch / "config.txt", std::ios::binary) << std::string("\xff\xfe\0\0", 4);
        check(!boost.apply(6, device).empty(), "reject unsupported Unicode encoding");
        std::cout << "PASS: " << count << " audio configuration checks; no system audio changes\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
