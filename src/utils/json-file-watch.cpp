#include "utils/json-file-watch.h"

#include <algorithm>
#include <filesystem>

JsonFileWatch::JsonFileWatch(std::vector<std::string> roots,
                             std::chrono::milliseconds interval)
    : mRoots(std::move(roots))
    , mInterval(std::max(interval, std::chrono::milliseconds(50)))
    , mThread(&JsonFileWatch::run, this)
{
}

JsonFileWatch::~JsonFileWatch()
{
    mStop.store(true, std::memory_order_release);
    if (mThread.joinable())
        mThread.join();
}

bool JsonFileWatch::consumeDirty()
{
    return mDirty.exchange(false, std::memory_order_acq_rel);
}

void JsonFileWatch::markDirty()
{
    mDirty.store(true, std::memory_order_release);
}

void JsonFileWatch::collect(const std::string& root, Stamps& out)
{
    std::error_code ec;
    if (!std::filesystem::is_directory(root, ec))
        return;
    for (std::filesystem::recursive_directory_iterator it(
             root, std::filesystem::directory_options::skip_permission_denied, ec),
         end; it != end; it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        std::error_code fileEc;
        if (!it->is_regular_file(fileEc) || fileEc)
            continue;
        if (it->path().extension() != ".json")
            continue;
        const auto write = std::filesystem::last_write_time(it->path(), fileEc);
        if (fileEc)
            continue;
        out.emplace_back(it->path().generic_string(),
                         (std::uint64_t)write.time_since_epoch().count());
    }
}

JsonFileWatch::Stamps JsonFileWatch::scan() const
{
    Stamps stamps;
    for (const std::string& root : mRoots)
        collect(root, stamps);
    std::sort(stamps.begin(), stamps.end());
    return stamps;
}

void JsonFileWatch::run()
{
    Stamps last = scan();
    while (!mStop.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(mInterval);
        Stamps next = scan();
        if (next != last) {
            last = std::move(next);
            mDirty.store(true, std::memory_order_release);
        }
    }
}
