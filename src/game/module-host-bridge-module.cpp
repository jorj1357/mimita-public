// 2026-11-12
// Game-DLL-side partial ModuleHost bridge for CAP_MODULES.
//
// Only the small query/control surface crosses the boundary:
// instance, activeGeneration, module/predicate queries, fixed-tick gate,
// startup/poll/shutdown. Typed policy methods remain UNRESOLVED until their
// operations are added. This bridge owns no module state or module DLLs.

#include <cstring>

#include "host/abi/game-host-abi.h"
#include "host/abi/module-capability.h"
#include "hot-reload/module-host.h"

namespace mimita::game {

void initializeHostModules(const mimita::host::HostServicesV1* services);

namespace {
const mimita::host::ModuleCapabilityV1* gModules = nullptr;
void* gHostData = nullptr;

bool invoke(mimita::host::ModuleOp op, const void* input, std::uint32_t inputSize,
            void* output, std::uint32_t outputSize) {
    if (!gModules || !gModules->invoke) {
        return false;
    }
    return gModules->invoke(gHostData, op, input, inputSize, output, outputSize) != 0;
}
}  // namespace

void initializeHostModules(const mimita::host::HostServicesV1* services) {
    gModules = nullptr;
    gHostData = services ? services->hostUserData : nullptr;
    if (!services || !services->get_capability) {
        return;
    }
    auto* capability = static_cast<const mimita::host::ModuleCapabilityV1*>(
        services->get_capability(
            services->hostUserData,
            static_cast<std::uint32_t>(mimita::host::HostCapabilityId::Modules),
            mimita::host::kModuleCapabilityVersion));
    if (capability &&
        capability->version == mimita::host::kModuleCapabilityVersion &&
        capability->structSize >= sizeof(mimita::host::ModuleCapabilityV1)) {
        gModules = capability;
    }
}

}  // namespace mimita::game

ModuleHost& ModuleHost::instance() {
    static ModuleHost host;
    return host;
}

ModuleHost::~ModuleHost() = default;

void ModuleHost::startup() {
    // The resident host owns module startup. A game generation must not start
    // a second module set.
}

void ModuleHost::shutdown() {}

void ModuleHost::poll() {
    mimita::game::invoke(mimita::host::ModuleOp::Poll, nullptr, 0, nullptr, 0);
}

void ModuleHost::beginFixedTick() {
    mimita::game::invoke(mimita::host::ModuleOp::BeginFixedTick, nullptr, 0, nullptr, 0);
}

void ModuleHost::endFixedTick() {
    mimita::game::invoke(mimita::host::ModuleOp::EndFixedTick, nullptr, 0, nullptr, 0);
}

bool ModuleHost::moduleLoaded(const char* moduleName) const {
    std::int32_t loaded = 0;
    if (!moduleName) {
        return false;
    }
    mimita::game::invoke(mimita::host::ModuleOp::ModuleLoaded, moduleName,
                         static_cast<std::uint32_t>(std::strlen(moduleName) + 1),
                         &loaded, sizeof(loaded));
    return loaded != 0;
}

bool ModuleHost::mapEditorLoaded() const {
    std::int32_t loaded = 0;
    mimita::game::invoke(mimita::host::ModuleOp::MapEditorLoaded, nullptr, 0,
                         &loaded, sizeof(loaded));
    return loaded != 0;
}

bool ModuleHost::npcPerceptionLoaded() const {
    std::int32_t loaded = 0;
    mimita::game::invoke(mimita::host::ModuleOp::NpcPerceptionLoaded, nullptr, 0,
                         &loaded, sizeof(loaded));
    return loaded != 0;
}

bool ModuleHost::npcBehaviorLoaded() const {
    std::int32_t loaded = 0;
    mimita::game::invoke(mimita::host::ModuleOp::NpcBehaviorLoaded, nullptr, 0,
                         &loaded, sizeof(loaded));
    return loaded != 0;
}

std::uint32_t ModuleHost::activeGeneration(const char* moduleName) const {
    std::uint32_t generation = 0;
    if (!moduleName) {
        return 0;
    }
    mimita::game::invoke(mimita::host::ModuleOp::ActiveGeneration, moduleName,
                         static_cast<std::uint32_t>(std::strlen(moduleName) + 1),
                         &generation, sizeof(generation));
    return generation;
}
