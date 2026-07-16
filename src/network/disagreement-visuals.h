#pragma once

#include "network/packets.h"
#include "network/multiplayer-context.h"

#include <string>

namespace MimitaNet {

// Spawn a world-space visual effect for a server disagreement event.
// Called from engineTickNet when disagreementEvents are processed.
void spawnDisagreementEffect(const DisagreementEvent& event);

// Log a disagreement to the structured log and console.
void logDisagreement(const DisagreementEvent& event);

// Spawn a local-only correction indicator (arrow + text) for the corrected player.
// Does not replicate to other players.
void spawnLocalDisagreementIndicator(const DisagreementEvent& event);

// Debug flag
extern bool gDisagreementDebug;

// Hot-reload from config/serverdisagree.json
void pollDisagreementReload();

} // namespace MimitaNet
