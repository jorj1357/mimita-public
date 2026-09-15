// 09 14 2026
/* purpose
* Hot AI consumer: reads the generic ActorRoleState / ActorTeamState /
* ActorProfileState dynamic components for any entity (NPC, player, future
* monster) and derives a per-entity AI selection component. This is a real hot
* behavior consuming generic state instead of typed Npc fields; no MonsterType
* or NPC-specific selector exists.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

#include <cstdint>

namespace {

const std::uint64_t kRoleState = gameHash("ActorRoleState");
const std::uint64_t kTeamState = gameHash("ActorTeamState");
const std::uint64_t kProfileState = gameHash("ActorProfileState");
const std::uint64_t kSelectionState = gameHash("NpcAiSelection");

struct ActorRoleStateV1 {
    std::uint64_t roleHash;
    std::uint32_t roleIndex;
    std::uint32_t reserved;
};
struct ActorTeamStateV1 {
    std::int32_t team;
    std::uint32_t reserved;
};
struct ActorProfileStateV1 {
    std::uint64_t movementPresetHash;
    std::uint64_t behaviorProfileHash;
    std::uint32_t flags;
    std::uint32_t reserved;
};
struct NpcAiSelectionV1 {
    std::uint64_t roleHash;
    std::uint64_t behaviorHash;
    std::int32_t team;
    std::uint32_t reserved;
};

void MIMITA_GAME_CALL npcAiStateTick(void* host, std::uint64_t /*tick*/, float /*dt*/)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->dynamicEnumerateComponent || !ctx->dynamicReadComponent ||
        !ctx->dynamicWriteComponent)
        return;

    std::uint64_t entities[64] = {0};
    const std::uint32_t count =
        ctx->dynamicEnumerateComponent(ctx->host, kRoleState, entities, 64);
    for (std::uint32_t i = 0; i < count; ++i) {
        ActorRoleStateV1 role{};
        if (!ctx->dynamicReadComponent(ctx->host, entities[i], kRoleState, &role,
                                       sizeof(role)))
            continue;
        ActorTeamStateV1 team{};
        ctx->dynamicReadComponent(ctx->host, entities[i], kTeamState, &team,
                                  sizeof(team));
        ActorProfileStateV1 profile{};
        ctx->dynamicReadComponent(ctx->host, entities[i], kProfileState, &profile,
                                  sizeof(profile));

        NpcAiSelectionV1 selection{};
        selection.roleHash = role.roleHash;
        selection.behaviorHash = profile.behaviorProfileHash;
        selection.team = team.team;
        ctx->dynamicWriteComponent(ctx->host, entities[i], kSelectionState, &selection,
                                   sizeof(selection));
    }
}

} // namespace

const MimitaHotPackage::SystemRegistrar s_npcAiState{
    {gameHash("npc.ai-state"), GAME_DOMAIN_GAMEPLAY, 700, 0, npcAiStateTick,
     "npc.ai-state"}};
const MimitaHotPackage::SchemaRegistrar s_npcAiSelectionSchema{
    {kSelectionState, gameHash("NpcAiSelection.v1"), sizeof(NpcAiSelectionV1), 8,
     GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE, "NpcAiSelection", 1, 0}};

#endif
