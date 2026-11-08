#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace fg {
inline std::string lower(std::string s) {
    for (auto& c : s)
        if (c >= 'A' && c <= 'Z')
            c += 'a' - 'A';
    return s;
}
inline bool wildcard(std::string pattern, std::string text) {
    pattern = lower(pattern);
    text = lower(text);
    size_t p = 0, t = 0, star = std::string::npos, retry = 0;
    while (t < text.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == text[t])) {
            ++p;
            ++t;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star = p++;
            retry = t;
        } else if (star != std::string::npos) {
            p = star + 1;
            t = ++retry;
        } else
            return false;
    }
    while (p < pattern.size() && pattern[p] == '*')
        ++p;
    return p == pattern.size();
}
inline bool matches(const std::vector<std::string>& patterns, const std::string& name) {
    for (const auto& p : patterns)
        if (wildcard(p, name))
            return true;
    return false;
}
inline const std::vector<std::string> systemDeny = {
    "framoryx.exe", "framoryxengine.exe", "presentmon*.exe", "dwm.exe", "explorer.exe",
    "svchost.exe",         "csrss.exe",       "winlogon.exe", "lsass.exe",
    "services.exe",        "smss.exe",        "system",       "registry",
    "securityhealth*.exe", "msmpeng.exe"};
inline const std::vector<std::string> antiCheatNames = {
    "easyanticheat*.exe", "beservice*.exe",       "battleye*.exe",    "vgc.exe",
    "vgtray.exe",         "eanticheat*.exe",      "eaanticheat*.exe", "faceit*.exe",
    "ace-service*.exe",   "anticheatexpert*.exe", "sguard*.exe",      "equ8*.exe",
    "ricochet*.exe"};
struct Rules {
    std::vector<std::string> allow{"FramoryxTest.exe"};
    std::vector<std::string> deny{"RiotClientServices.exe"};
    bool blocked(const std::string& name) const {
        return matches(systemDeny, name) || matches(antiCheatNames, name) || matches(deny, name);
    }
    bool hookAllowed(const std::string& name) const {
        return !blocked(name) && matches(allow, name);
    }
};
inline bool validRule(const std::string& s) {
    if (s.empty() || s.size() > 200)
        return false;
    for (unsigned char c : s)
        if (c < 32 || c == '/' || c == '\\' || c == ':' || c == '"' || c == ',')
            return false;
    return s.size() >= 4 && lower(s.substr(s.size() - 4)) == ".exe";
}
inline std::vector<std::string> csv(const std::string& line) {
    std::vector<std::string> cells;
    std::string v;
    bool quote = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '"') {
            if (quote && i + 1 < line.size() && line[i + 1] == '"') {
                v += '"';
                ++i;
            } else
                quote = !quote;
        } else if (c == ',' && !quote) {
            cells.push_back(v);
            v.clear();
        } else if (c != '\r' && c != '\n')
            v += c;
    }
    if (quote)
        return {};
    cells.push_back(v);
    return cells;
}
inline std::optional<double> number(const std::string& s) {
    if (s.empty())
        return {};
    char* end = nullptr;
    double v = std::strtod(s.c_str(), &end);
    if (end != s.c_str() + s.size() || !std::isfinite(v))
        return {};
    return v;
}
struct Frame {
    std::string app, chain;
    uint32_t pid = 0;
    double time = 0, ms = 0;
};
class Parser {
    std::map<std::string, size_t> header;

  public:
    bool ready() const {
        return header.count("application") && header.count("msbetweenpresents");
    }
    std::optional<Frame> parse(std::string line) {
        if (line.compare(0, 3, "\xEF\xBB\xBF") == 0)
            line.erase(0, 3);
        auto c = csv(line);
        if (c.empty())
            return {};
        if (c[0] == "Application") {
            header.clear();
            for (size_t i = 0; i < c.size(); ++i)
                header[lower(c[i])] = i;
            return {};
        }
        auto cell = [&](const char* key) -> std::string {
            auto i = header.find(lower(key));
            return i != header.end() && i->second < c.size() ? c[i->second] : "";
        };
        auto pid = number(cell("ProcessID")), time = number(cell("TimeInSeconds")),
             ms = number(cell("MsBetweenPresents"));
        if (!pid || !time || !ms || *pid < 1 || *pid > UINT32_MAX || std::floor(*pid) != *pid ||
            *time < 0 || *ms <= 0 || *ms > 60000 || cell("Application").empty())
            return {};
        return Frame{cell("Application"), cell("SwapChainAddress"), uint32_t(*pid), *time, *ms};
    }
};
struct Stream {
    struct Sample {
        double time, ms;
    };
    std::string app, chain;
    uint32_t pid = 0;
    double arrival = 0;
    std::deque<Sample> frames;
};
struct Stats {
    double fps = 0, average = 0, low = 0, ms = 0;
    bool live = false;
    std::string app, chain;
    uint32_t pid = 0;
    std::vector<Stream::Sample> frames;
};
class Store {
    std::map<std::pair<uint32_t, std::string>, Stream> streams;
    size_t frameCount = 0;

  public:
    static constexpr size_t frameBudget = 262144;
    size_t storedFrameCount() const {
        return frameCount;
    }
    void clear() {
        streams.clear();
        frameCount = 0;
    }
    void add(const Frame& f, double now) {
        auto key = std::make_pair(f.pid, f.chain);
        if (!streams.count(key) && streams.size() >= 128) {
            auto oldest = std::min_element(streams.begin(), streams.end(), [](auto& a, auto& b) {
                return a.second.arrival < b.second.arrival;
            });
            frameCount -= oldest->second.frames.size();
            streams.erase(oldest);
        }
        auto& s = streams[key];
        if ((!s.app.empty() && s.app != f.app) ||
            (!s.frames.empty() && f.time < s.frames.back().time)) {
            frameCount -= s.frames.size();
            s.frames.clear();
        }
        s.app = f.app;
        s.pid = f.pid;
        s.chain = f.chain;
        s.arrival = now;
        s.frames.push_back({f.time, f.ms});
        ++frameCount;
        while (!s.frames.empty() &&
               (s.frames.size() > 32768 || f.time - s.frames.front().time > 30.0)) {
            s.frames.pop_front();
            --frameCount;
        }
        while (frameCount > frameBudget && streams.size() > 1) {
            auto oldest = streams.end();
            for (auto it = streams.begin(); it != streams.end(); ++it)
                if (it->first != key &&
                    (oldest == streams.end() || it->second.arrival < oldest->second.arrival))
                    oldest = it;
            if (oldest == streams.end())
                break;
            frameCount -= oldest->second.frames.size();
            streams.erase(oldest);
        }
    }
    Stats snapshot(uint32_t pid, double now) const {
        const Stream* best = nullptr;
        size_t largest = 0;
        for (const auto& item : streams) {
            const auto& s = item.second;
            if (s.pid != pid || s.frames.empty())
                continue;
            size_t count = 0;
            for (auto it = s.frames.rbegin();
                 it != s.frames.rend() && s.frames.back().time - it->time <= 2.0; ++it)
                ++count;
            // Prefer live streams over a formerly faster, stale menu swapchain.
            bool live = now - s.arrival < 2.0;
            bool bestLive = best && now - best->arrival < 2.0;
            if (!best || (live && !bestLive) || (live == bestLive && count > largest)) {
                best = &s;
                largest = count;
            }
        }
        Stats out;
        if (!best)
            return out;
        out.app = best->app;
        out.pid = pid;
        out.chain = best->chain;
        out.live = now - best->arrival < 2.0;
        out.frames.reserve(best->frames.size());
        for (auto& sample : best->frames)
            out.frames.push_back(sample);
        double sum = 0, recent = 0;
        size_t n = 0;
        std::vector<double> slow;
        slow.reserve(out.frames.size());
        for (auto& f : out.frames) {
            sum += f.ms;
            slow.push_back(f.ms);
            if (best->frames.back().time - f.time <= 1.0) {
                recent += f.ms;
                ++n;
            }
        }
        if (sum > 0)
            out.average = 1000.0 * slow.size() / sum;
        if (recent > 0 && out.live) {
            out.fps = 1000.0 * n / recent;
            out.ms = recent / n;
        }
        size_t k = std::max<size_t>(1, size_t(std::ceil(slow.size() * 0.01)));
        if (k < slow.size())
            std::nth_element(slow.begin(), slow.begin() + k, slow.end(), std::greater<double>());
        double lowSum = 0;
        for (size_t i = 0; i < k; ++i)
            lowSum += slow[i];
        if (lowSum > 0)
            out.low = 1000.0 * k / lowSum;
        return out;
    }
};
} // namespace fg
