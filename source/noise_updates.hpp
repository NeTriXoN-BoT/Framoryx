#pragma once
#include <memory>
#include <optional>
#include <string>
namespace fg {
struct NoiseRelease {
    std::wstring mono, stereo, version;
    std::string monoDigest, stereoDigest;
};
struct NoiseUpdateSnapshot {
    bool running = false;
    std::wstring message;
    std::optional<NoiseRelease> release;
};
class NoiseUpdater {
    struct State;
    std::unique_ptr<State> state;

  public:
    NoiseUpdater();
    ~NoiseUpdater();
    NoiseUpdater(const NoiseUpdater&) = delete;
    NoiseUpdater& operator=(const NoiseUpdater&) = delete;
    void start(const std::wstring& directory, bool force = false);
    bool running() const;
    std::wstring status() const;
    std::optional<NoiseRelease> takeResult();
    NoiseUpdateSnapshot poll();
};
} // namespace fg
