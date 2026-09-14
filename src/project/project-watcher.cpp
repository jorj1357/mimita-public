// 09 12 2026
/* purpose
* Implements the recursive ReadDirectoryChangesW watcher.
* Does NOT own the project tree or builds.
*/
#include "project/project-watcher.h"

#include <cstdio>
#include <cstring>

#include <windows.h>

namespace Project {

ProjectWatcher::~ProjectWatcher()
{
    stop();
}

bool ProjectWatcher::start(const std::filesystem::path& root)
{
    if (running_.load())
        return true;

    HANDLE handle = CreateFileW(
        root.wstring().c_str(), FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        std::printf("[PROJECT WATCHER] CreateFile failed error=%lu\n", GetLastError());
        return false;
    }

    directory_ = handle;
    root_ = root;
    running_ = true;
    worker_ = std::thread(&ProjectWatcher::run, this);
    return true;
}

void ProjectWatcher::stop()
{
    if (!running_.exchange(false))
        return;
    if (directory_) {
        CancelIoEx((HANDLE)directory_, nullptr);
        CloseHandle((HANDLE)directory_);
        directory_ = nullptr;
    }
    if (worker_.joinable())
        worker_.join();
}

void ProjectWatcher::run()
{
    // Larger buffer so a burst of edits is not split/truncated.
    char buffer[16384];
    while (running_.load()) {
        DWORD bytes = 0;
        const BOOL ok = ReadDirectoryChangesW(
            (HANDLE)directory_, buffer, sizeof(buffer), TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE,
            &bytes, nullptr, nullptr);
        if (!ok)
            break;
        if (bytes == 0)
            continue;

        std::vector<WatchEvent> batch;
        const char* cursor = buffer;
        for (;;) {
            const FILE_NOTIFY_INFORMATION* info =
                reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(cursor);
            if (info->FileNameLength > 0) {
                const std::wstring wide(
                    info->FileName, info->FileNameLength / sizeof(wchar_t));
                std::string relative;
                relative.reserve(wide.size());
                for (wchar_t ch : wide)
                    relative.push_back(ch == L'\\' ? '/' : (char)ch);
                WatchAction action = WatchAction::Modified;
                switch (info->Action) {
                case FILE_ACTION_ADDED: action = WatchAction::Added; break;
                case FILE_ACTION_REMOVED: action = WatchAction::Removed; break;
                case FILE_ACTION_MODIFIED: action = WatchAction::Modified; break;
                case FILE_ACTION_RENAMED_OLD_NAME: action = WatchAction::RenamedOld; break;
                case FILE_ACTION_RENAMED_NEW_NAME: action = WatchAction::RenamedNew; break;
                default: action = WatchAction::Modified; break;
                }
                batch.push_back({action, std::move(relative)});
            }
            if (info->NextEntryOffset == 0)
                break;
            cursor += info->NextEntryOffset;
        }
        if (!batch.empty()) {
            std::lock_guard<std::mutex> lock(mutex_);
            for (WatchEvent& event : batch)
                events_.push_back(std::move(event));
            dirty_ = true;
            ++changeCount_;
        }
    }
}

bool ProjectWatcher::poll(std::vector<std::string>* outPaths)
{
    std::vector<WatchEvent> events;
    if (!pollEvents(events))
        return false;
    if (outPaths) {
        outPaths->clear();
        outPaths->reserve(events.size());
        for (const WatchEvent& event : events)
            outPaths->push_back(event.path);
    }
    return true;
}

bool ProjectWatcher::pollEvents(std::vector<WatchEvent>& out)
{
    const bool had = dirty_.exchange(false);
    std::lock_guard<std::mutex> lock(mutex_);
    if (had || !events_.empty()) {
        out = std::move(events_);
        events_.clear();
        return true;
    }
    out.clear();
    return false;
}

} // namespace Project
