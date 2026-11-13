// 2026-11-13
// Game-DLL side of the resident job executor.

#include "game/job-bridge-module.h"

#include <cstdint>
#include <utility>

#include "host/abi/lifetime-capability.h"

namespace mimita::game {
namespace {
const mimita::host::JobsCapabilityV1* gJobs = nullptr;
void* gHostData = nullptr;

struct JobBox {
    std::function<void()> work;
};

void invokeBox(void* data) {
    auto* box = static_cast<JobBox*>(data);
    if (box && box->work) box->work();
}

void destroyBox(void* data) {
    delete static_cast<JobBox*>(data);
}
}  // namespace

void initializeHostJobs(const mimita::host::HostServicesV1* services) {
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

HostWorker::~HostWorker() {
    release();
}

bool HostWorker::start(std::function<void()> work, const char* tag) {
    if (handle_ || !work) return false;
    if (!gJobs || !gJobs->start_worker) return false;
    auto* box = new JobBox{std::move(work)};
    handle_ = gJobs->start_worker(gHostData, box, &invokeBox, &destroyBox, tag);
    if (!handle_) delete box;
    return handle_ != 0;
}

void HostWorker::wait() {
    if (handle_ && gJobs && gJobs->wait_worker)
        gJobs->wait_worker(gHostData, handle_);
}

void HostWorker::release() {
    if (!handle_) return;
    wait();
    if (gJobs && gJobs->release_worker)
        gJobs->release_worker(gHostData, handle_);
    handle_ = 0;
}

bool runDetached(std::function<void()> work, const char* tag) {
    if (!work) return false;
    if (!gJobs || !gJobs->submit) {
        work();
        return true;
    }
    auto* box = new JobBox{std::move(work)};
    gJobs->submit(gHostData, box, &invokeBox, &destroyBox, tag);
    return true;
}
}  // namespace mimita::game
