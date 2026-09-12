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
    char buffer[4096];
    while (running_.load()) {
        DWORD bytes = 0;
        const BOOL ok = ReadDirectoryChangesW(
            (HANDLE)directory_, buffer, sizeof(buffer), TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE,
            &bytes, nullptr, nullptr);
        if (!ok)
            break;
        if (bytes > 0) {
            dirty_ = true;
            ++changeCount_;
        }
    }
}

bool ProjectWatcher::poll(std::vector<std::string>* outPaths)
{
    if (!dirty_.exchange(false))
        return false;
    if (outPaths)
        outPaths->clear();
    return true;
}

} // namespace Project
