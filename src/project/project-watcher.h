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
#include <string>
#include <thread>
#include <vector>

namespace Project {

class ProjectWatcher {
public:
    ProjectWatcher() = default;
    ~ProjectWatcher();
    ProjectWatcher(const ProjectWatcher&) = delete;
    ProjectWatcher& operator=(const ProjectWatcher&) = delete;

    bool start(const std::filesystem::path& root);
    void stop();
    bool running() const { return running_.load(); }

    // Returns true when a change was observed since the last poll and clears
    // the flag. `outPaths` receives the changed paths when requested.
    bool poll(std::vector<std::string>* outPaths = nullptr);

    std::uint64_t changeCount() const { return changeCount_.load(); }

private:
    void run();

    void* directory_ = nullptr;  // HANDLE
    std::atomic<bool> running_{false};
    std::atomic<bool> dirty_{false};
    std::atomic<std::uint64_t> changeCount_{0};
    std::thread worker_;
};

} // namespace Project
