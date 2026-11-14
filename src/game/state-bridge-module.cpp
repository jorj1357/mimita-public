// 2026-11-13
// Game-DLL side of the B-lite resident menu state.

#include "game/state-bridge-module.h"

namespace mimita::game {
namespace {
const mimita::host::StateCapabilityV1* gState = nullptr;
void* gHostData = nullptr;
}  // namespace

void initializeHostState(const mimita::host::HostServicesV1* services) {
    gState = nullptr;
    gHostData = services ? services->hostUserData : nullptr;
    if (!services || !services->get_capability) return;
    auto* capability = static_cast<const mimita::host::StateCapabilityV1*>(
        services->get_capability(
            services->hostUserData,
            static_cast<std::uint32_t>(mimita::host::HostCapabilityId::State),
            mimita::host::kStateCapabilityVersion));
    if (capability &&
        capability->version == mimita::host::kStateCapabilityVersion &&
        capability->structSize >= sizeof(mimita::host::StateCapabilityV1)) {
        gState = capability;
    }
}

mimita::host::MenuStateV1* menuState() {
    return gState && gState->menu_state ? gState->menu_state(gHostData) : nullptr;
}

void* ensureResidentObject(mimita::host::StateObjectId id, std::size_t size,
                           std::size_t alignment, bool* created) {
    if (created) *created = false;
    if (!gState || !gState->ensure_object || size > UINT32_MAX ||
        alignment > UINT32_MAX || alignment == 0) {
        return nullptr;
    }
    std::int32_t first = 0;
    void* result = gState->ensure_object(
        gHostData, id, static_cast<std::uint32_t>(size),
        static_cast<std::uint32_t>(alignment), &first);
    if (created) *created = first != 0;
    return result;
}
}  // namespace mimita::game
