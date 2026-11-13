// 2026-11-13
// Game-DLL-side JsonFileWatch bridge over resident job services.

#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "host/abi/game-host-abi.h"
#include "host/abi/lifetime-capability.h"
#include "utils/json-file-watch.h"

namespace mimita::game {

void initializeHostJobsWatch(const mimita::host::HostServicesV1* services);

namespace {
const mimita::host::JobsCapabilityV1* gJobs = nullptr;
void* gHostData = nullptr;
std::unordered_map<const JsonFileWatch*, void*> gWatches;

void* handleFor(const JsonFileWatch* watch) {
    auto it = gWatches.find(watch);
    return it == gWatches.end() ? nullptr : it->second;
}
}  // namespace

void initializeHostJobsWatch(const mimita::host::HostServicesV1* services) {
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

JsonFileWatch::JsonFileWatch(std::vector<std::string> roots,
                             std::chrono::milliseconds interval) {
    const auto& jobs = *mimita::game::gJobs;
    if (jobs.json_watch_create) {
        std::array<const char*, 32> text{};
        const std::size_t count = roots.size() < text.size() ? roots.size() : text.size();
        for (std::size_t i = 0; i < count; ++i) text[i] = roots[i].c_str();
        mimita::game::gWatches[this] = jobs.json_watch_create(
            mimita::game::gHostData, text.data(),
            static_cast<std::uint32_t>(count),
            static_cast<std::uint32_t>(interval.count()));
    }
}

JsonFileWatch::~JsonFileWatch() {
    auto it = mimita::game::gWatches.find(this);
    if (it != mimita::game::gWatches.end()) {
        if (mimita::game::gJobs && mimita::game::gJobs->json_watch_destroy) {
            mimita::game::gJobs->json_watch_destroy(mimita::game::gHostData, it->second);
        }
        mimita::game::gWatches.erase(it);
    }
}

bool JsonFileWatch::consumeDirty() {
    if (!mimita::game::gJobs || !mimita::game::gJobs->json_watch_consume_dirty) {
        return false;
    }
    return mimita::game::gJobs->json_watch_consume_dirty(
               mimita::game::gHostData, mimita::game::handleFor(this)) != 0;
}

void JsonFileWatch::markDirty() {
    if (mimita::game::gJobs && mimita::game::gJobs->json_watch_mark_dirty) {
        mimita::game::gJobs->json_watch_mark_dirty(
            mimita::game::gHostData, mimita::game::handleFor(this));
    }
}
