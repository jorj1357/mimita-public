#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// Background dirty flag for live-edited JSON trees. The gameplay thread only
// consumes a flag; recursive directory scans and file stamps never run there.
class JsonFileWatch
{
public:
    JsonFileWatch(std::vector<std::string> roots,
                  std::chrono::milliseconds interval);
    ~JsonFileWatch();

    JsonFileWatch(const JsonFileWatch&) = delete;
    JsonFileWatch& operator=(const JsonFileWatch&) = delete;

    bool consumeDirty();
    void markDirty();

private:
    using Stamps = std::vector<std::pair<std::string, std::uint64_t>>;

    static void collect(const std::string& root, Stamps& out);
    Stamps scan() const;
    void run();

    std::vector<std::string> mRoots;
    std::chrono::milliseconds mInterval;
    std::atomic<bool> mDirty{true};
    std::atomic<bool> mStop{false};
    std::thread mThread;
};
