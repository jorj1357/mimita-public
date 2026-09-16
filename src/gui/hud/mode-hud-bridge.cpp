// 09 15 2026
/* purpose
* Implements the transitional match-HUD compatibility bridge: projects the typed
* cold CommunityMatchClient match state ONCE into the generic MatchHudState
* dynamic component plus a ModeHudClaim, so hot.match-hud composes the HUD
* without hot code depending on CommunityMatchClient. Also answers the cold HUD
* ownership gate. Does NOT own match state.
*/
#include "gui/hud/mode-hud-bridge.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>

#include "ecs/dynamic-components.h"
#include "ecs/entity-types.h"
#include "hot-reload/hot-ui.h"
#include "network/community-match-client.h"
#include "network/server-gamemode.h"

namespace ModeHud {

void projectFromClient()
{
    const std::uint64_t matchEntity = MimitaNet::serverMatchEntity();
    if (matchEntity == 0)
        return;   // no generic match entity (pure client): cold owns the HUD
    MimitaRuntime::DynamicComponentStore& store =
        MimitaRuntime::DynamicComponentStore::instance();
    const EntityId entity = static_cast<EntityId>(matchEntity);

    const MimitaNet::CommunityMatchClient& match =
        MimitaNet::CommunityMatchClient::instance();
    if (!match.active()) {
        store.remove(entity, HOT_MODE_HUD_CLAIM_COMPONENT);
        store.remove(entity, HOT_MATCH_HUD_COMPONENT);
        return;
    }

    const std::string mode = match.mode();
    // Hot composition fully covers these modes' HUD; others stay cold.
    const bool hotCovers = (mode == "tdm" || mode == "ffa" ||
                            mode == "counterstrike");

    HotMatchHudStateV1 hud{};
    float timer = match.phaseTimer();
    if (match.phase() == MimitaNet::DUEL_PHASE_ACTIVE &&
        match.timeLimitSeconds() > 0) {
        const std::uint32_t elapsed = match.serverTick() > match.matchStartTick()
            ? match.serverTick() - match.matchStartTick() : 0;
        timer = (float)std::max(0, match.timeLimitSeconds() - (int)(elapsed / 60));
    }
    hud.timerSeconds = timer;
    hud.scoreA = match.redScore();
    hud.scoreB = match.blueScore();
    hud.phase = match.phase() == MimitaNet::DUEL_PHASE_COUNTDOWN   ? 1u
                : match.phase() == MimitaNet::DUEL_PHASE_ACTIVE    ? 2u
                : match.phase() == MimitaNet::DUEL_PHASE_INTERMISSION ? 3u
                                                                      : 4u;
    std::snprintf(hud.labelA, sizeof(hud.labelA), "RED");
    std::snprintf(hud.labelB, sizeof(hud.labelB), "BLUE");
    if (hotCovers) {
        if (match.phase() == MimitaNet::DUEL_PHASE_COUNTDOWN)
            std::snprintf(hud.phaseText, sizeof(hud.phaseText),
                          match.goVisible() ? "GO!!!" : "COUNTDOWN");
        else if (match.phase() == MimitaNet::DUEL_PHASE_INTERMISSION)
            std::snprintf(hud.phaseText, sizeof(hud.phaseText), "INTERMISSION");
    }
    store.write(entity, HOT_MATCH_HUD_COMPONENT, &hud, sizeof(hud));

    // Generic objective presentation (transitional projection from the typed
    // client match state). Hot HUD interprets stateHash; no CS primitive.
    HotObjectiveStateV1 obj{};
    const std::uint8_t bombState = match.objectiveBombState();
    if (bombState != 0 || match.objectiveBombActive()) {
        obj.objectiveId = gameHash("objective.bomb");
        obj.stateHash = bombState >= 2 ? gameHash("bomb.defusing")
                        : bombState >= 1 ? gameHash("bomb.planted")
                                         : gameHash("bomb.carried");
        obj.progress = (float)match.objectivePlantPercent() / 100.0f;
        if (bombState >= 2)
            obj.progress = (float)match.objectiveDefusePercent() / 100.0f;
        obj.timer = match.bombSecondsRemaining();
        obj.flags = 1;   // present
        store.write(entity, HOT_OBJECTIVE_COMPONENT, &obj, sizeof(obj));
    } else {
        store.remove(entity, HOT_OBJECTIVE_COMPONENT);
    }

    HotModeHudClaimV1 claim{};
    claim.owned = hotCovers ? 1u : 0u;
    store.write(entity, HOT_MODE_HUD_CLAIM_COMPONENT, &claim, sizeof(claim));
}

bool hotOwned()
{
    const std::uint64_t matchEntity = MimitaNet::serverMatchEntity();
    if (matchEntity == 0)
        return false;
    HotModeHudClaimV1 claim{};
    if (!MimitaRuntime::DynamicComponentStore::instance().read(
            static_cast<EntityId>(matchEntity), HOT_MODE_HUD_CLAIM_COMPONENT,
            &claim, sizeof(claim)))
        return false;
    return claim.owned == 1;
}

} // namespace ModeHud
