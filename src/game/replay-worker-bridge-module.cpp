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
#include "game/job-bridge-module.h"
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

ReplaySaveWorker::ReplaySaveWorker() = default;
ReplaySaveWorker::~ReplaySaveWorker() = default;

void ReplaySaveWorker::drain() {
    // Jobs are dispatched through the resident jobs capability. There is no
    // generation-owned thread to wait on here.
}

bool ReplaySaveWorker::enqueue(std::function<void()> job) {
    if (!job) return false;
    return mimita::game::runDetached(std::move(job), "replay-save");
}

size_t ReplaySaveWorker::queueDepth() const {
    return 0;
}
