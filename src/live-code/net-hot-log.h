// 09 23 2026
/* purpose
* Cold-side bridge that routes networking lifecycle diagnostics through the
* generic `log.event` capability when a hot provider is registered, falling back
* to the append-only LiveEventJournal otherwise. Changing event names/fields then
* becomes a hot edit instead of a cold rebuild of the networking files.
* POD only; no STL or engine objects cross the hot boundary.
* Does NOT own the journal, the logger, or gameplay policy.
*/
#pragma once

#include <cstdint>
#include <cstring>

#include "hot-reload/game-api.h"
#include "hot-reload/generic-runtime.h"
#include "live-code/live-journal.h"

namespace MimitaNet {

// Emit one lifecycle record. `category`/`name`/`message` are copied into a fixed
// POD event; the hot provider (log.event) is preferred, the journal is fallback.
inline void emitNetworkLog(std::uint32_t level, const char* category,
                           const char* name, const char* message,
                           std::uint64_t tick)
{
    if (!name)
        return;

    auto fn = reinterpret_cast<GameLogEventFn>(
        MimitaRuntime::GenericRuntime::instance().capability(GAME_CAP_LOG_EVENT));
    if (fn) {
        GameLogEventV1 ev{};
        ev.level = level;
        ev.simulationTick = static_cast<std::uint32_t>(tick);
        ev.serverTick = tick;
        if (category)
            std::strncpy(ev.category, category, sizeof(ev.category) - 1);
        std::strncpy(ev.name, name, sizeof(ev.name) - 1);
        if (message)
            std::strncpy(ev.message, message, sizeof(ev.message) - 1);
        fn(nullptr, &ev);
        return;
    }

    LiveEventJournal::Fields f;
    f.tick = tick;
    if (message && message[0])
        f.result = message;
    LiveEventJournal::instance().record(name, f);
}

} // namespace MimitaNet
