// 09 15 2026
/* purpose
* Explicit mapping between the authoritative SERVER tick domain (where a
* generation SWITCH tick is defined) and a client's LOCAL simulation tick domain.
* The two counters are not assumed equal; the mapping is delta-based off the
* latest authoritative server tick the client observed and its local simulation
* tick at that moment. Pure function, no state.
* Does NOT own the switch protocol or any tick source.
*/
#pragma once

#include <cstdint>

namespace MimitaRuntime {

// Given the latest authoritative server tick the client has observed
// (`latestServerTick`), the client's local simulation tick when it observed it
// (`localSimTickAtReceipt`), and the server-side switch tick `serverSwitchTick`,
// return the client-local simulation tick at which generation G must activate.
// If the switch tick is at or before the latest observed server tick, activation
// is already due and maps to the current local tick.
inline std::uint32_t mapServerSwitchTickToClientLocal(
    std::uint32_t latestServerTick, std::uint32_t localSimTickAtReceipt,
    std::uint32_t serverSwitchTick)
{
    if (serverSwitchTick <= latestServerTick)
        return localSimTickAtReceipt;
    return localSimTickAtReceipt + (serverSwitchTick - latestServerTick);
}

} // namespace MimitaRuntime
