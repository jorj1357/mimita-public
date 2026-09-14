// 09 12 2026
/* purpose
* Recursive filesystem watcher (ReadDirectoryChangesW) that reports that the
* project changed. Complements hash scanning: the scan remains the source of
* truth, the watcher reduces latency and catches create/delete/rename/move.
* Does NOT own the project tree or builds.
*/
#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Project {

// One filesystem change observed under the watched root.
enum class WatchAction : std::uint8_t {
    Added = 0,
    Removed = 1,
    Modified = 2,
    RenamedOld = 3,
    RenamedNew = 4,
};

struct WatchEvent {
    WatchAction action = WatchAction::Modified;
    std::string path;  // relative to the watched root, '/'-separated
};

class ProjectWatcher {
public:
    ProjectWatcher() = default;
    ~ProjectWatcher();
    ProjectWatcher(const ProjectWatcher&) = delete;
    ProjectWatcher& operator=(const ProjectWatcher&) = delete;

    bool start(const std::filesystem::path& root);
    void stop();
    bool running() const { return running_.load(); }

    // Returns true when any change was observed since the last poll and clears
    // the queue. `outPaths` receives changed paths when requested.
    bool poll(std::vector<std::string>* outPaths = nullptr);

    // Per-path CREATE/MODIFY/DELETE/RENAME/MOVE events (clears the queue).
    bool pollEvents(std::vector<WatchEvent>& out);

    std::uint64_t changeCount() const { return changeCount_.load(); }

private:
    void run();

    void* directory_ = nullptr;  // HANDLE
    std::filesystem::path root_;
    std::atomic<bool> running_{false};
    std::atomic<bool> dirty_{false};
    std::atomic<std::uint64_t> changeCount_{0};
    std::mutex mutex_;
    std::vector<WatchEvent> events_;
    std::thread worker_;
};

} // namespace Project
