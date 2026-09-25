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

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

#include "ecs/actor-entities.h"
#include "ecs/dynamic-components.h"
#include "ecs/entity-registry.h"
#include "ecs/entity-types.h"
#include "hot-reload/game-api.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-npc-intent.h"
#include "hot-reload/hot-npc-target-select.h"
#include "hot-reload/hot-npc-weapon-select.h"
#include "hot-reload/hot-npc-combat-decision.h"
#include "hot-reload/hot-npc-state-select.h"
#include "hot-reload/hot-npc-nav-goal.h"
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

    // Phase 4: the wire-facing ActorNetState projection and its generic weapon
    // presentation input round-trip through the generic component store.
    {
        MimitaNet::ActorNetStateV1 ns{};
        ns.position[0] = 7.0f;
        ns.position[1] = 2.0f;
        ns.position[2] = -3.0f;
        ns.velocity[0] = 0.0f;
        ns.velocity[1] = 0.0f;
        ns.velocity[2] = 4.0f;
        ns.aim[0] = 0.0f;
        ns.aim[1] = 1.0f;
        ns.aim[2] = 0.0f;
        ns.yaw = 1.25f;
        ns.onGround = 1u;
        ns.equippedSlot = 3;
        ns.weaponState = 0x02u;
        check(MimitaNet::actorStateWriteNetState(raw, ns),
              "actor net-state component written", report);
        MimitaNet::ActorNetStateV1 back{};
        check(MimitaNet::actorStateReadNetState(raw, &back) &&
                  back.position[2] == -3.0f && back.velocity[2] == 4.0f &&
                  back.onGround == 1u && back.equippedSlot == 3 &&
                  back.weaponState == 0x02u,
              "actor net-state round-trips position/velocity/ground/weapon",
              report);

        check(MimitaNet::actorStateWriteWeaponState(raw, 2, 0x04u),
              "actor weapon-state input written", report);
        std::int16_t slot = -1;
        std::uint8_t ws = 0;
        check(MimitaNet::actorStateReadWeaponState(raw, &slot, &ws) &&
                  slot == 2 && ws == 0x04u,
              "actor weapon-state input round-trips", report);
    }

    // Phase 5a: the shared NPC movement/facing intent policy (the hot provider
    // runs this same code) is deterministic and turn-speed limited.
    {
        NpcIntentPolicyV1 r{};
        r.structSize = sizeof(NpcIntentPolicyV1);
        r.dt = 1.0f / 60.0f;
        r.rawMoveX = 0.25f;
        r.rawMoveY = -0.5f;
        r.movementPressed = 0u;
        r.currentFacing[0] = 1.0f;
        r.currentFacing[1] = 0.0f;
        r.currentFacing[2] = 0.0f;
        r.hasTarget = 1u;
        r.targetPos[0] = 0.0f;
        r.targetPos[1] = 10.0f;
        r.targetPos[2] = 0.0f;
        r.turnSpeed = 0.0f;
        r.aimAtTargetMin = 1.0f;
        r.aimAtTargetMax = 1.0f;
        r.facingModeTimer = 0.0f;
        r.facingTargetMode = 0u;
        r.rngState = 12345u;
        MimitaNet::HotNpcIntentImpl::evaluate(r);
        check(r.handled == 1u && r.facingTargetMode == 1u,
              "npc intent toggles into aim mode", report);
        check(std::fabs(r.outFacing[0]) < 0.01f && r.outFacing[1] > 0.99f,
              "npc intent aims at the target", report);
        check(std::fabs(r.outYaw - 90.0f) < 1.0f,
              "npc intent yaw faces the target", report);
        check(r.outMoveX == r.rawMoveX && r.outMoveY == r.rawMoveY,
              "npc intent passes movement through", report);

        NpcIntentPolicyV1 t{};
        t.structSize = sizeof(NpcIntentPolicyV1);
        t.dt = 1.0f / 60.0f;
        t.currentFacing[0] = 1.0f;
        t.currentFacing[1] = 0.0f;
        t.currentFacing[2] = 0.0f;
        t.hasTarget = 1u;
        t.targetPos[0] = 0.0f;
        t.targetPos[1] = 10.0f;
        t.targetPos[2] = 0.0f;
        t.turnSpeed = 60.0f;
        t.aimAtTargetMax = -1.0f;
        t.rngState = 1u;
        MimitaNet::HotNpcIntentImpl::evaluate(t);
        check(t.outFacing[1] > 0.0f && t.outFacing[1] < 0.1f,
              "npc intent turn-speed limits the facing", report);
    }

    // Phase 5b: the shared NPC target-selection policy is deterministic.
    {
        NpcTargetSelectPolicyV1 p{};
        p.structSize = sizeof(NpcTargetSelectPolicyV1);
        p.behaviorActive = 1u;
        p.currentTargetId = 0u;
        p.targetSwitchThreshold = 0.0f;
        p.candidateCount = 2;
        p.candidates[0].id = 11u;
        p.candidates[0].kind = 0u;
        p.candidates[0].score = 5.0f;
        p.candidates[0].alive = 1u;
        p.candidates[1].id = 22u;
        p.candidates[1].kind = 1u;
        p.candidates[1].score = 9.0f;
        p.candidates[1].alive = 1u;
        MimitaNet::HotNpcTargetSelectImpl::evaluate(p);
        check(p.handled && p.chosenId == 22u && p.chosenKind == 1u,
              "target select picks the highest score", report);

        NpcTargetSelectPolicyV1 q{};
        q.structSize = sizeof(NpcTargetSelectPolicyV1);
        q.behaviorActive = 1u;
        q.currentTargetId = 11u;
        q.targetSwitchThreshold = 10.0f;
        q.candidateCount = 2;
        q.candidates[0].id = 11u;
        q.candidates[0].kind = 0u;
        q.candidates[0].score = 5.0f;
        q.candidates[0].isCurrent = 1u;
        q.candidates[0].alive = 1u;
        q.candidates[1].id = 22u;
        q.candidates[1].kind = 1u;
        q.candidates[1].score = 9.0f;
        q.candidates[1].alive = 1u;
        MimitaNet::HotNpcTargetSelectImpl::evaluate(q);
        check(q.chosenId == 11u,
              "target select keeps current within threshold", report);

        NpcTargetSelectPolicyV1 n2{};
        n2.structSize = sizeof(NpcTargetSelectPolicyV1);
        n2.behaviorActive = 0u;
        n2.candidateCount = 2;
        n2.candidates[0].id = 11u;
        n2.candidates[0].kind = 0u;
        n2.candidates[0].distanceSq = 100.0f;
        n2.candidates[0].alive = 1u;
        n2.candidates[1].id = 22u;
        n2.candidates[1].kind = 1u;
        n2.candidates[1].distanceSq = 4.0f;
        n2.candidates[1].alive = 1u;
        MimitaNet::HotNpcTargetSelectImpl::evaluate(n2);
        check(n2.chosenId == 22u, "target select legacy picks nearest", report);
    }

    // Phase 5c: the shared NPC scored weapon-selection policy is deterministic.
    {
        NpcWeaponSelectPolicyV1 w{};
        w.structSize = sizeof(NpcWeaponSelectPolicyV1);
        w.distance = 3.0f;
        w.weaponRangeBias = 0.4f;
        w.weaponDamageBias = 0.4f;
        w.weaponSafetyBias = 0.2f;
        w.weaponSwitchThreshold = 0.0f;
        w.currentWeaponHash = 100u;
        w.candidateCount = 2;
        w.candidates[0].weaponHash = 100u;
        w.candidates[0].usable = 1u;
        w.candidates[0].effRange = 30.0f;
        w.candidates[0].damage = 34.0f;
        w.candidates[0].pelletCount = 1u;
        w.candidates[0].behaviorType = 0u;
        w.candidates[1].weaponHash = 200u;
        w.candidates[1].usable = 1u;
        w.candidates[1].effRange = 8.0f;
        w.candidates[1].damage = 12.0f;
        w.candidates[1].pelletCount = 6u;
        w.candidates[1].behaviorType = 0u;
        MimitaNet::HotNpcWeaponSelectImpl::evaluate(w);
        check(w.handled && w.chosenWeaponHash == 200u,
              "weapon select picks close-range burst", report);

        w.weaponSwitchThreshold = 10.0f;
        w.candidates[0].isEquipped = 1u;
        MimitaNet::HotNpcWeaponSelectImpl::evaluate(w);
        check(w.chosenWeaponHash == 100u,
              "weapon select keeps current within threshold", report);
    }

    // Phase 5d: the shared NPC combat-decision policy is deterministic.
    {
        NpcCombatDecisionPolicyV1 c{};
        c.structSize = sizeof(NpcCombatDecisionPolicyV1);
        c.attackCooldown = 0.0f;
        c.distance = 10.0f;
        c.rangeCap = 999999.0f;
        c.hasWeapon = 1u;
        c.ammoCurrent = 0;
        c.ammoReserve = 6;
        c.isReloading = 0u;
        c.reloadTime = 1.5f;
        c.healthFrac = 1.0f;
        c.targetDistance = 10.0f;
        c.visible = 1u;
        c.effectiveAggressionBase = 0.5f;
        MimitaNet::HotNpcCombatDecisionImpl::evaluate(c);
        check(c.handled && c.shouldFire == 0u && c.startReload == 1u &&
                  c.outReloadSeconds == 1.5f,
              "combat decision starts reload when empty", report);

        c.ammoCurrent = 6;
        c.isReloading = 0u;
        c.attackCooldown = 0.0f;
        c.losBlocked = 0u;
        MimitaNet::HotNpcCombatDecisionImpl::evaluate(c);
        check(c.shouldFire == 1u && c.startReload == 0u,
              "combat decision fires when ready", report);

        c.attackCooldown = 0.5f;
        MimitaNet::HotNpcCombatDecisionImpl::evaluate(c);
        check(c.shouldFire == 0u, "combat decision blocks on cooldown", report);
    }

    // Phase 5e: the shared NPC state-selection policy is deterministic.
    {
        NpcStateSelectPolicyV1 s{};
        s.structSize = sizeof(NpcStateSelectPolicyV1);
        s.currentState = 0u;   // Idle
        s.hasTarget = 0u;
        s.isStuck = 0u;
        s.lastKnownAge = 1.0f;
        s.distToLastKnown = 5.0f;
        s.randomnessScale = 1.0f;
        s.rngState = 123u;
        MimitaNet::HotNpcStateSelectImpl::evaluate(s);
        check(s.handled && s.chosenState == 2u,
              "state select searches last known position", report);

        s.hasTarget = 1u;
        s.currentState = 5u;   // Retreat
        s.retreatTimer = 99.0f;
        s.difficulty01 = 0.0f;
        s.distance = 3.0f;
        s.effectiveAggression = 0.5f;
        s.weaponRange = 20.0f;
        s.randomnessScale = 1.0f;
        MimitaNet::HotNpcStateSelectImpl::evaluate(s);
        check(s.chosenState == 3u,
              "state select exits retreat to circle when close", report);
    }

    // Phase 5f: the shared NPC navigation-goal policy is deterministic.
    {
        NpcNavGoalPolicyV1 g{};
        g.structSize = sizeof(NpcNavGoalPolicyV1);
        g.hasTarget = 0u;
        g.currentState = 2u;   // Chase
        g.lastKnownTarget[0] = 1.0f;
        g.lastKnownTarget[1] = 2.0f;
        g.lastKnownTarget[2] = 3.0f;
        g.maintainDistanceScale = 1.0f;
        MimitaNet::HotNpcNavGoalImpl::evaluate(g);
        check(g.handled && g.goalKind == 1u && g.goalTargetPos[1] == 2.0f,
              "nav goal reaches last known position", report);

        NpcNavGoalPolicyV1 d{};
        d.structSize = sizeof(NpcNavGoalPolicyV1);
        d.hasTarget = 1u;
        d.currentState = 5u;   // Retreat
        d.maintainDistanceScale = 1.0f;
        MimitaNet::HotNpcNavGoalImpl::evaluate(d);
        check(d.goalKind == 4u && d.goalDesiredDistance == 8.0f,
              "nav goal flees on retreat", report);

        NpcNavGoalPolicyV1 m{};
        m.structSize = sizeof(NpcNavGoalPolicyV1);
        m.hasTarget = 1u;
        m.currentState = 3u;   // Circle
        m.effectiveRange = 20.0f;
        m.preferredRange = 0.0f;
        m.maintainDistanceScale = 1.0f;
        MimitaNet::HotNpcNavGoalImpl::evaluate(m);
        check(m.goalKind == 3u && std::fabs(m.goalDesiredDistance - 12.0f) < 0.01f,
              "nav goal maintains weapon range", report);
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
