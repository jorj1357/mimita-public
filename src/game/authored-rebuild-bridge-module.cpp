// 2026-11-13
// Game-DLL side of the resident authored-world rebuild worker.

#include <cstdint>
#include <memory>
#include <utility>

#include "host/abi/authored-rebuild-capability.h"
#include "host/abi/game-host-abi.h"
#include "host/abi/lifetime-capability.h"
#include "network/server-authored-rebuild.h"

namespace mimita::game {
void initializeHostAuthoredRebuild(const mimita::host::HostServicesV1* services);
namespace {
const mimita::host::AuthoredRebuildCapabilityV1* gAuthored = nullptr;
const mimita::host::JobsCapabilityV1* gJobs = nullptr;
void* gHostData = nullptr;
std::uint64_t gLease = 0;

void releaseLease() {
    if (gLease && gJobs && gJobs->release) {
        gJobs->release(gHostData, gLease);
    }
    gLease = 0;
}
}  // namespace

void initializeHostAuthoredRebuild(const mimita::host::HostServicesV1* services) {
    gAuthored = nullptr;
    gJobs = nullptr;
    gHostData = services ? services->hostUserData : nullptr;
    if (!services || !services->get_capability) return;
    auto* capability = static_cast<const mimita::host::AuthoredRebuildCapabilityV1*>(
        services->get_capability(
            services->hostUserData,
            static_cast<std::uint32_t>(mimita::host::HostCapabilityId::AuthoredRebuild),
            mimita::host::kAuthoredRebuildCapabilityVersion));
    if (capability &&
        capability->version == mimita::host::kAuthoredRebuildCapabilityVersion &&
        capability->structSize >= sizeof(mimita::host::AuthoredRebuildCapabilityV1)) {
        gAuthored = capability;
    }
    auto* jobs = static_cast<const mimita::host::JobsCapabilityV1*>(
        services->get_capability(
            services->hostUserData,
            static_cast<std::uint32_t>(mimita::host::HostCapabilityId::Jobs),
            mimita::host::kJobsCapabilityVersion));
    if (jobs && jobs->version == mimita::host::kJobsCapabilityVersion &&
        jobs->structSize >= sizeof(mimita::host::JobsCapabilityV1)) {
        gJobs = jobs;
    }
}
}  // namespace mimita::game

MimitaNet::AuthoredWorldRebuildWorker&
MimitaNet::AuthoredWorldRebuildWorker::instance() {
    void* worker = mimita::game::gAuthored && mimita::game::gAuthored->worker
        ? mimita::game::gAuthored->worker(mimita::game::gHostData)
        : nullptr;
    return *static_cast<AuthoredWorldRebuildWorker*>(worker);
}

void MimitaNet::AuthoredWorldRebuildWorker::submit(AuthoredRebuildRequest request) {
    if (!mimita::game::gAuthored || !mimita::game::gAuthored->submit) return;
    if (!mimita::game::gLease && mimita::game::gJobs && mimita::game::gJobs->acquire) {
        mimita::game::gLease =
            mimita::game::gJobs->acquire(mimita::game::gHostData, "authored-rebuild");
    }
    mimita::game::gAuthored->submit(mimita::game::gHostData, this, &request);
}

std::shared_ptr<MimitaNet::AuthoredRebuildResult>
MimitaNet::AuthoredWorldRebuildWorker::tryTakeResult() {
    std::shared_ptr<AuthoredRebuildResult> result;
    if (mimita::game::gAuthored && mimita::game::gAuthored->try_take_result) {
        mimita::game::gAuthored->try_take_result(
            mimita::game::gHostData, this, &result);
    }
    if (result) mimita::game::releaseLease();
    return result;
}

void MimitaNet::AuthoredWorldRebuildWorker::retire(
    std::shared_ptr<AuthoredRebuildResult> result) {
    if (mimita::game::gAuthored && mimita::game::gAuthored->retire) {
        mimita::game::gAuthored->retire(mimita::game::gHostData, this, &result);
    }
    mimita::game::releaseLease();
}

void MimitaNet::AuthoredWorldRebuildWorker::cancelAndWait() {
    if (mimita::game::gAuthored && mimita::game::gAuthored->cancel_and_wait)
        mimita::game::gAuthored->cancel_and_wait(mimita::game::gHostData, this);
    mimita::game::releaseLease();
}
