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

// Debug flag
extern bool gDisagreementDebug;

} // namespace MimitaNet
