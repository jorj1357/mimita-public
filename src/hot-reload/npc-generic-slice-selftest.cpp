// 09 24 2026
/* purpose
* Implements the generic NPC actor vertical-slice falsification selftest.
* Proves the NPC migration Phase 1-3 contract at the generic component layer:
* create identity, write/read generic actor components, project the actor-state
* envelope, keep entity id stable across a respawn generation, distinguish
* automatic vs manual origin, and destroy exactly one entity. No NPC-specific
* field is consulted.
* Does NOT own gameplay state or networking.
*/
#include "hot-reload/npc-generic-slice-selftest.h"

#include <cstdint>
#include <cstdio>
#include <string>

#include "ecs/actor-entities.h"
#include "ecs/dynamic-components.h"
#include "ecs/entity-registry.h"
#include "ecs/entity-types.h"
#include "hot-reload/game-api.h"
#include "hot-reload/generic-runtime.h"
#include "network/actor-health.h"
#include "network/actor-state.h"

namespace {

bool gPass = true;

void check(bool condition, const char* what, std::string& report)
{
    if (!condition)
        gPass = false;
    report += condition ? "  [ok] " : "  [FAIL] ";
    report += what;
    report += "\n";
}

GameActorStateV1 readState(std::uint64_t entity)
{
    GameActorStateV1 out{};
    auto fn = reinterpret_cast<GameActorStateReadFn>(
        MimitaRuntime::GenericRuntime::instance().capability(
            GAME_CAP_ACTOR_STATE_READ));
    if (fn)
        fn(nullptr, entity, &out);
    return out;
}

} // namespace

bool runNpcGenericSliceSelfTest(std::string& report)
{
    gPass = true;

    // Create a generic NPC actor entity through the ONE allocator.
    const EntityId npc = EntityRegistry::instance().create(
        EntityRealm::Server, EntityDomain::Npc, 1001u, 1u);
    check(npc != kInvalidEntityId && entityDomain(npc) == EntityDomain::Npc,
          "generic npc entity created with actor kind", report);
    const std::uint64_t raw = Ecs::raw(npc);

    // Generic transform/velocity/health components.
    Ecs::setTransform(npc, glm::vec3(4.0f, 1.0f, 2.0f),
                      glm::vec3(1.0f, 0.0f, 0.0f), 0.5f, 0.0f);
    Ecs::setVelocity(npc, glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f));
    MimitaNet::actorHealthInit(raw, 100);
    Ecs::setHealth(npc, 100, 100, false);

    // Generic origin/lifecycle/avatar components (Phase 1).
    check(MimitaNet::actorStateWriteOrigin(raw, GAME_NPC_ORIGIN_MANUAL, 0),
          "actor origin component written", report);
    check(MimitaNet::actorStateWriteLifecycle(raw, 1u, 0u, 0.0f),
          "actor lifecycle component written", report);
    check(MimitaNet::actorStateWriteAvatar(raw, gameHash("avatar-a"), 1u),
          "actor avatar hash component written", report);

    std::uint32_t origin = 0xffffffffu;
    check(MimitaNet::actorStateReadOrigin(raw, &origin, nullptr) &&
              origin == GAME_NPC_ORIGIN_MANUAL,
          "origin reads back as manual", report);
    std::uint64_t avatarHash = 0;
    std::uint32_t avatarGen = 0;
    check(MimitaNet::actorStateReadAvatar(raw, &avatarHash, &avatarGen) &&
              avatarHash == gameHash("avatar-a") && avatarGen == 1u,
          "avatar hash reads back", report);

    // Generic actor-state envelope projects the components (no NPC pointer).
    {
        GameActorStateV1 st = readState(raw);
        check(st.entity == raw && st.actorKind == 2u,
              "actor-state envelope carries identity + actor kind", report);
        check(st.position[0] == 4.0f && st.position[2] == 2.0f &&
                  st.velocity[2] == -1.0f,
              "actor-state envelope carries transform/velocity", report);
        check(st.health == 100 && st.maxHealth == 100,
              "actor-state envelope carries health", report);
        check(st.origin == GAME_NPC_ORIGIN_MANUAL &&
                  st.avatarHash == gameHash("avatar-a"),
              "actor-state envelope carries origin + avatar", report);
    }

    // Respawn keeps the SAME entity id but bumps the generic life generation.
    {
        const std::uint32_t before = entityGeneration(npc);
        check(MimitaNet::actorStateWriteLifecycle(raw, 2u, 0u, 0.0f),
              "respawn generation write", report);
        std::uint32_t lifeGen = 0;
        MimitaNet::actorStateReadLifecycle(raw, &lifeGen, nullptr, nullptr);
        check(lifeGen == 2u, "respawn bumped generic life generation", report);
        check(EntityRegistry::instance().alive(npc) &&
                  entityGeneration(npc) == before,
              "entity id stable across respawn", report);
        // Avatar changes per life via the generic component.
        MimitaNet::actorStateWriteAvatar(raw, gameHash("avatar-b"), 2u);
        std::uint64_t h2 = 0;
        MimitaNet::actorStateReadAvatar(raw, &h2, nullptr);
        check(h2 == gameHash("avatar-b"),
              "avatar changes per generic life", report);
    }

    // Manual origin is distinguished from automatic (reconciliation safety).
    {
        const EntityId autoNpc = EntityRegistry::instance().create(
            EntityRealm::Server, EntityDomain::Npc, 1002u, 1u);
        MimitaNet::actorStateWriteOrigin(Ecs::raw(autoNpc),
                                         GAME_NPC_ORIGIN_STARTUP, 0);
        std::uint32_t o = 0xffffffffu;
        MimitaNet::actorStateReadOrigin(Ecs::raw(autoNpc), &o, nullptr);
        check(o == GAME_NPC_ORIGIN_STARTUP,
              "automatic origin distinguished generically", report);
        EntityRegistry::instance().destroy(autoNpc);
    }

    // Generic destruction removes exactly one entity; the actor is gone.
    {
        const std::size_t before = EntityRegistry::instance().count();
        EntityRegistry::instance().destroy(npc);
        check(!EntityRegistry::instance().alive(npc),
              "destroy removes exactly the target entity", report);
        check(EntityRegistry::instance().count() + 1 == before,
              "destroy removed one entity", report);
    }

    report += gPass ? "[NPC GENERIC SLICE SELFTEST] PASS\n"
                    : "[NPC GENERIC SLICE SELFTEST] FAIL\n";
    return gPass;
}
