// 09 15 2026
/* purpose
* Transitional compatibility bridge for the menu shell: projects typed cold
* profile/version/connection data ONCE into the generic MenuShellState dynamic
* component so hot menu policy can compose account/status chrome without reading
* cold GUI or account objects. Not the long-term authority.
* Does NOT own auth or account state.
*/
#pragma once

namespace MenuShell {

// Project profile/version/connection into generic MenuShellState on the local
// actor entity. Safe to call every frame; no-op when no entity exists.
void project();

} // namespace MenuShell
