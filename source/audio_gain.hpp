#pragma once
#include "audio.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <deque>
#include <functional>
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
            try {
                worker = std::thread([this] {
                    std::unique_lock lock(mutex);
                    while (!stopped) {
                        wake.wait(lock, [this] { return stopped || pending.has_value(); });
                        if (stopped)
                            break;
                        // Coalesce rapid slider updates without blocking the window thread.
                        wake.wait_for(lock, std::chrono::milliseconds(100),
                                      [this] { return stopped; });
                        if (stopped)
                            break;
                        auto request = *pending;
                        pending.reset();
                        writing = true;
                        lock.unlock();
                        std::wstring error;
                        try {
                            AudioBoost boost;
                            boost.directory(request.directory);
                            error = request.db > 0 ? boost.apply(request.db, request.guid)
                                                   : boost.disable();
                        } catch (const std::exception&) {
                            error = L"Audio update could not finish. Check the config folder "
                                    L"before retrying.";
                        }
                        lock.lock();
                        writing = false;
                        result = std::move(error);
                    }
                });
            } catch (const std::exception&) {
                std::lock_guard lock(mutex);
                pending.reset();
                result = L"Windows could not start the audio worker.";
            }
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
class AudioEffectWorker {
    std::thread worker;
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<std::function<std::wstring()>> queue;
    std::optional<std::wstring> result;
    bool stopped = false, writing = false;

  public:
    ~AudioEffectWorker() {
        {
            std::lock_guard lock(mutex);
            stopped = true;
            queue.clear();
        }
        wake.notify_one();
        if (worker.joinable())
            worker.join();
    }
    bool submit(std::function<std::wstring()> task) {
        {
            std::lock_guard lock(mutex);
            if (stopped || queue.size() >= 8)
                return false;
            queue.push_back(std::move(task));
        }
        if (!worker.joinable())
            try {
                worker = std::thread([this] {
                    std::unique_lock lock(mutex);
                    while (!stopped) {
                        wake.wait(lock, [this] { return stopped || !queue.empty(); });
                        if (stopped)
                            break;
                        auto task = std::move(queue.front());
                        queue.pop_front();
                        writing = true;
                        lock.unlock();
                        std::wstring message;
                        try {
                            message = task();
                        } catch (const std::exception&) {
                            message = L"Audio operation could not finish. Check the configuration "
                                      L"before retrying.";
                        }
                        lock.lock();
                        writing = false;
                        result = std::move(message);
                    }
                });
            } catch (const std::exception&) {
                std::lock_guard lock(mutex);
                queue.clear();
                result = L"Windows could not start the audio worker.";
                return false;
            }
        wake.notify_one();
        return true;
    }
    bool busy() {
        std::lock_guard lock(mutex);
        return writing || !queue.empty();
    }
    std::optional<std::wstring> takeResult() {
        std::lock_guard lock(mutex);
        auto out = std::move(result);
        result.reset();
        return out;
    }
};
} // namespace fg
