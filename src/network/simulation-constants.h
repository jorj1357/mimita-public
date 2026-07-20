// 07 19 2026 1500
/* purpose
* Shared gameplay simulation constants for both client and server.
* Single source of truth for tick rate and fixed delta time.
* Server code may additionally define SERVER_TICK_RATE = GAMEPLAY_SIMULATION_HZ
* and SERVER_DT = GAMEPLAY_FIXED_DT if needed.
* Does NOT define server structs, player state, or network protocol types.
*/

#pragma once

#include <cstdint>

namespace MimitaNet {

constexpr uint32_t GAMEPLAY_SIMULATION_HZ = 60;
constexpr float GAMEPLAY_FIXED_DT = 1.0f / static_cast<float>(GAMEPLAY_SIMULATION_HZ);

} // namespace MimitaNet
