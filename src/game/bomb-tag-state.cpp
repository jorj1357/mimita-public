// 09 06 2026, 00 00
/* purpose
* Sets NPC AI flags for Bomb Tag behavior (chase/flee) based on
* server-authoritative bomb holder state received via CommunityMatchClient.
* Does NOT simulate bomb ownership, timers, or transfers — the server owns all gameplay.
* Does NOT render the bomb visual or HUD — that is handled by BombTagManager.
* Does NOT produce per-frame log spam — uses throttled debug logging.
*/

#include "bomb-tag.h"

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

#include "entities/player.h"
#include "npc/npc.h"
#include "world/world.h"
#include "debug/debug-log.h"
#include "network/community-match-client.h"
#include "network/multiplayer-context.h"
#include "terminal/terminal-state.h"
#include "config.h"

using namespace MimitaNet;

void updateBombTagNpcFlags(Player& player, NpcSystem& npcs) {
    const auto& c = CommunityMatchClient::instance();
    if (!c.isBombTag() || c.phase() != DUEL_PHASE_ACTIVE) {
        for (auto& n : npcs.all()) {
            n.bombTagActive = false;
            n.bombTagHasBomb = false;
        }
        return;
    }

    uint32_t localId = MP_CONTEXT.localPlayerId;
    bool localPlayerHasBomb = (c.bombOwnerType() == BOMB_OWNER_PLAYER &&
                               c.bombOwnerPlayerId() == localId);
    glm::vec3 bombHolderPos = c.bombPosition();

    for (int i = 0; i < (int)npcs.all().size(); ++i) {
        Npc& npc = npcs.all()[i];
        npc.bombTagActive = true;

        if (localPlayerHasBomb) {
            npc.bombTagHasBomb = false;
            npc.bombTagFleeFrom = player.pos;
        } else if (c.bombOwnerType() == BOMB_OWNER_PLAYER) {
            npc.bombTagHasBomb = false;
            npc.bombTagFleeFrom = bombHolderPos;
        } else {
            npc.bombTagHasBomb = false;
            npc.bombTagChaseTarget = player.pos;
        }

        setArmToWeaponPose(npc.body, npc.bombTagHasBomb);
    }
}
