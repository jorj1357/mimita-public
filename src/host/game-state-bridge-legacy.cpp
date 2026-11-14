// 2026-11-14
// EXE-side compatibility for game code that calls the state bridge directly.
#include "host/host-runtime.h"
#include "game/state-bridge-module.h"

#ifndef MIMITA_GAME_DLL
namespace mimita::game {
void initializeHostState(const mimita::host::HostServicesV1*) {}

mimita::host::MenuStateV1* menuState() {
    return mimita::host::HostRuntime::instance().menuState();
}


void* ensureResidentObjectMigrating(mimita::host::StateObjectId id,
                                    std::size_t size, std::size_t alignment,
                                    mimita::host::StateMigrateFn migrate,
                                    bool* created) {
    if (!created) return nullptr;
    std::int32_t first = 0;
    void* object = mimita::host::HostRuntime::instance().ensureStateObjectMigrating(
        static_cast<std::uint32_t>(id), static_cast<std::uint32_t>(size),
        static_cast<std::uint32_t>(alignment), migrate, &first);
    *created = first != 0;
    return object;
}

void* ensureResidentObject(mimita::host::StateObjectId id, std::size_t size,
                           std::size_t alignment, bool* created) {
    if (!created) return nullptr;
    std::int32_t first = 0;
    void* object = mimita::host::HostRuntime::instance().ensureStateObject(
        static_cast<std::uint32_t>(id), static_cast<std::uint32_t>(size),
        static_cast<std::uint32_t>(alignment), &first);
    *created = first != 0;
    return object;
}
}  // namespace mimita::game
#endif
