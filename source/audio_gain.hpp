#pragma once
#include "audio.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
namespace fg {
class GainWorker {
    struct Request {
        std::wstring directory, guid;
        double db;
    };
    std::thread worker;
    std::mutex mutex;
    std::condition_variable wake;
    std::optional<Request> pending;
    std::optional<std::wstring> result;
    bool stopped = false, writing = false;

  public:
    ~GainWorker() {
        {
            std::lock_guard lock(mutex);
            stopped = true;
            pending.reset();
        }
        wake.notify_one();
        if (worker.joinable())
            worker.join();
    }
    void request(const std::wstring& directory, const std::wstring& guid, double db) {
        {
            std::lock_guard lock(mutex);
            pending = Request{directory, guid, db};
        }
        if (!worker.joinable())
            worker = std::thread([this] {
                std::unique_lock lock(mutex);
                while (!stopped) {
                    wake.wait(lock, [this] { return stopped || pending.has_value(); });
                    if (stopped)
                        break;
                    // Coalesce rapid slider updates without blocking the window thread.
                    wake.wait_for(lock, std::chrono::milliseconds(100), [this] { return stopped; });
                    if (stopped)
                        break;
                    auto request = *pending;
                    pending.reset();
                    writing = true;
                    lock.unlock();
                    AudioBoost boost;
                    boost.directory(request.directory);
                    auto error =
                        request.db > 0 ? boost.apply(request.db, request.guid) : boost.disable();
                    lock.lock();
                    writing = false;
                    result = std::move(error);
                }
            });
        wake.notify_one();
    }
    std::optional<std::wstring> takeResult() {
        std::lock_guard lock(mutex);
        auto out = result;
        result.reset();
        return out;
    }
    bool busy() {
        std::lock_guard lock(mutex);
        return writing || pending.has_value();
    }
};
} // namespace fg
