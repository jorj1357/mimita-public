// 07 19 2026, 13 05
/* purpose
* Own generic client presentation for server-confirmed damage events.
* Converts authoritative damage facts into attacker-only hit feedback.
* Exposes a small test sink so validation can count presentation without audio/UI.
* Does NOT change health, death, score, knockback, or projectile state.
* Does NOT validate or calculate damage.
* Does NOT branch on weapon names for presentation behavior.
*/

#pragma once

#include "effects/hit-effects.h"
#include "network/packets.h"

namespace MimitaNet {

struct MultiplayerContext;

struct ConfirmedDamagePresentationSink
{
    void* user = nullptr;
    void (*showHitmarker)(int damage, void* user) = nullptr;
    void (*playHitSound)(int damage, void* user) = nullptr;
    void (*showDamageNumber)(const HitEvent& event, void* user) = nullptr;
};

bool presentConfirmedDamage(MultiplayerContext& ctx,
                            const DamageConfirmedEventPacket& event,
                            const ConfirmedDamagePresentationSink* sink = nullptr);

} // namespace MimitaNet
