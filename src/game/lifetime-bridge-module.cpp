// 2026-11-12
// Game-DLL lifetime phone for CAP_JOBS/CAP_CALLBACKS/CAP_RESOURCES.
//
// Leases mark generation-owned work so the barrier can refuse unload while any
// work can still re-enter the generation.

#include <cstddef>
#include <cstdint>

#include "host/abi/game-host-abi.h"
#include "host/abi/lifetime-capability.h"

namespace mimita::game {

void initializeHostLifetime(const mimita::host::HostServicesV1* services);

std::uint64_t acquireHostJob(const char* tag);
mimita::host::HostResult releaseHostJob(std::uint64_t lease);
std::uint32_t hostJobsBusy();

std::uint64_t registerHostCallback(std::uint32_t kind, const char* tag);
mimita::host::HostResult unregisterHostCallback(std::uint64_t token);
std::uint32_t hostCallbacksBusy();

std::uint64_t allocateHostResource(std::uint32_t kind, std::size_t bytes,
                                   std::size_t alignment, const char* tag);
void releaseHostResource(std::uint64_t resource);
std::uint32_t hostResourcesBusy();

namespace {
const mimita::host::JobsCapabilityV1* gJobs = nullptr;
const mimita::host::CallbacksCapabilityV1* gCallbacks = nullptr;
const mimita::host::ResourcesCapabilityV1* gResources = nullptr;
void* gHostData = nullptr;
}  // namespace

void initializeHostLifetime(const mimita::host::HostServicesV1* services) {
    gJobs = nullptr;
    gCallbacks = nullptr;
    gResources = nullptr;
    gHostData = services ? services->hostUserData : nullptr;
    if (!services || !services->get_capability) {
        return;
    }
    gJobs = static_cast<const mimita::host::JobsCapabilityV1*>(
        services->get_capability(
            services->hostUserData,
            static_cast<std::uint32_t>(mimita::host::HostCapabilityId::Jobs),
            mimita::host::kJobsCapabilityVersion));
    gCallbacks = static_cast<const mimita::host::CallbacksCapabilityV1*>(
        services->get_capability(
            services->hostUserData,
            static_cast<std::uint32_t>(mimita::host::HostCapabilityId::Callbacks),
            mimita::host::kCallbacksCapabilityVersion));
    gResources = static_cast<const mimita::host::ResourcesCapabilityV1*>(
        services->get_capability(
            services->hostUserData,
            static_cast<std::uint32_t>(mimita::host::HostCapabilityId::Resources),
            mimita::host::kResourcesCapabilityVersion));
}

std::uint64_t acquireHostJob(const char* tag) {
    return gJobs && gJobs->acquire ? gJobs->acquire(gHostData, tag) : 0;
}

mimita::host::HostResult releaseHostJob(std::uint64_t lease) {
    return gJobs && gJobs->release ? gJobs->release(gHostData, lease)
                                   : mimita::host::HostResult::Unsupported;
}

std::uint32_t hostJobsBusy() {
    return gJobs && gJobs->busy ? gJobs->busy(gHostData) : 0;
}

std::uint64_t registerHostCallback(std::uint32_t kind, const char* tag) {
    return gCallbacks && gCallbacks->register_callback
        ? gCallbacks->register_callback(gHostData, kind, tag) : 0;
}

mimita::host::HostResult unregisterHostCallback(std::uint64_t token) {
    return gCallbacks && gCallbacks->unregister_callback
        ? gCallbacks->unregister_callback(gHostData, token)
        : mimita::host::HostResult::Unsupported;
}

std::uint32_t hostCallbacksBusy() {
    return gCallbacks && gCallbacks->busy ? gCallbacks->busy(gHostData) : 0;
}

std::uint64_t allocateHostResource(std::uint32_t kind, std::size_t bytes,
                                   std::size_t alignment, const char* tag) {
    return gResources && gResources->allocate
        ? gResources->allocate(gHostData, kind, bytes, alignment, tag) : 0;
}

void releaseHostResource(std::uint64_t resource) {
    if (gResources && gResources->release) {
        gResources->release(gHostData, resource);
    }
}

std::uint32_t hostResourcesBusy() {
    return gResources && gResources->busy ? gResources->busy(gHostData) : 0;
}

}  // namespace mimita::game
