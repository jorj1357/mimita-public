// 2026-11-13
// Game-DLL side of the resident ReplaySaveWorker.
//
// The worker object and thread are resident. These two methods are compiled in
// the game DLL because callers and the queued jobs are game code. A queued job
// holds a host job lease until it finishes, so the barrier refuses unload while
// game code can still run on the worker thread.

#include <cstdint>
#include <functional>
#include <mutex>
#include <utility>

#include "host/abi/game-host-abi.h"
#include "host/abi/lifetime-capability.h"
#include "replay/replay-factory-worker.h"

namespace mimita::game {
void initializeHostReplayWorker(const mimita::host::HostServicesV1* services);
namespace {
const mimita::host::JobsCapabilityV1* gJobs = nullptr;
void* gHostData = nullptr;
}  // namespace

void initializeHostReplayWorker(const mimita::host::HostServicesV1* services) {
    gJobs = nullptr;
    gHostData = services ? services->hostUserData : nullptr;
    if (!services || !services->get_capability) return;
    auto* capability = static_cast<const mimita::host::JobsCapabilityV1*>(
        services->get_capability(
            services->hostUserData,
            static_cast<std::uint32_t>(mimita::host::HostCapabilityId::Jobs),
            mimita::host::kJobsCapabilityVersion));
    if (capability &&
        capability->version == mimita::host::kJobsCapabilityVersion &&
        capability->structSize >= sizeof(mimita::host::JobsCapabilityV1)) {
        gJobs = capability;
    }
}
}  // namespace mimita::game

bool ReplaySaveWorker::enqueue(std::function<void()> job) {
    std::uint64_t lease = 0;
    if (mimita::game::gJobs && mimita::game::gJobs->acquire) {
        lease = mimita::game::gJobs->acquire(mimita::game::gHostData, "replay-save");
    }
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mShutdown || mJobs.size() >= kMaxQueuedJobs) {
            if (lease && mimita::game::gJobs->release)
                mimita::game::gJobs->release(mimita::game::gHostData, lease);
            return false;
        }
        mJobs.push([job = std::move(job), lease]() mutable {
            struct LeaseGuard {
                std::uint64_t value;
                ~LeaseGuard() {
                    if (value && mimita::game::gJobs && mimita::game::gJobs->release)
                        mimita::game::gJobs->release(mimita::game::gHostData, value);
                }
            } guard{lease};
            if (job) job();
        });
    }
    mCv.notify_one();
    return true;
}

size_t ReplaySaveWorker::queueDepth() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mJobs.size() + mActiveJobs;
}
