#pragma once
#include "core.hpp"
#include <iomanip>
#include <locale>
#include <ostream>
#include <sstream>

namespace fg {
inline std::string csvText(const std::string& value) {
    std::string quoted = "\"";
    for (char c : value) {
        if (c == '"')
            quoted += '"';
        quoted += c;
    }
    quoted += '"';
    return quoted;
}
inline bool writeCaptureCsv(std::ostream& destination, const Stats& stats) {
    std::ostringstream text;
    text.imbue(std::locale::classic());
    text << "\xEF\xBB\xBF"
         << "Application,ProcessID,SwapChainAddress,TimeInSeconds,MsBetweenPresents\n"
         << std::fixed << std::setprecision(6);
    double first = stats.frames.empty() ? 0 : stats.frames.front().time;
    auto prefix =
        csvText(stats.app) + ',' + std::to_string(stats.pid) + ',' + csvText(stats.chain) + ',';
    for (const auto& frame : stats.frames)
        text << prefix << frame.time - first << ',' << frame.ms << '\n';
    auto bytes = text.str();
    destination.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return bool(destination);
}
} // namespace fg
