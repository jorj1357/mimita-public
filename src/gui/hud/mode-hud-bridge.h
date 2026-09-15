// 09 15 2026
/* purpose
* Transitional compatibility bridge for the match HUD. Projects the typed cold
* CommunityMatchClient match state ONCE into the generic MatchHudState dynamic
* component (+ a ModeHudClaim) so hot.match-hud can compose the HUD without hot
* code depending on CommunityMatchClient. This is not the long-term authority:
* the forward path is the authoritative hot mode writing/replicating the generic
* state directly. Cold HUD composition yields only for modes hot fully covers.
* Does NOT own gameplay/match state.
*/
#pragma once

namespace ModeHud {

// Project the active client match state into generic MatchHudState + claim.
// No-op when no match is active or no generic match entity exists.
void projectFromClient();

// True when the generic claim says the hot composition owns the active mode's
// HUD, so the cold client HUD must yield.
bool hotOwned();

} // namespace ModeHud
