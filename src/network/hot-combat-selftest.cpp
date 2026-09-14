// 09 14 2026
/* purpose
* Implements the headless hot-combat policy self-test.
* Does NOT own gameplay systems or the live-code pipeline.
*/
#include "network/hot-combat-selftest.h"

#include <string>

#include <unordered_map>

#include "ecs/actor-entities.h"
#include "ecs/components.h"
#include "ecs/dynamic-components.h"
#include "ecs/entity-registry.h"
#include "ecs/components.h"
#include "hot-reload/game-api.h"
#include "hot-reload/hot-projectile.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-reload-system.h"
#include "live-code/live-behavior.h"
#include "network/server-context.h"
#include "network/server-gamemode.h"
#include "network/server-weapon-state.h"

using namespace MimitaNet;

using namespace MimitaRuntime;
using namespace MimitaNet;

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

const std::uint64_t kBananaTool = gameHash("banana.launcher");
const std::uint64_t kBananaProjectile = gameHash("banana.projectile");
const std::uint64_t kBananaState = gameHash("BananaLauncherState");

struct BananaLauncherState {
    std::int32_t shotsFired;
    float power;
    std::uint64_t toolEntity;
};

ToolUsePolicyV1 makeUse(std::uint64_t toolId)
{
    ToolUsePolicyV1 use{};
    use.ownerId = 7;
    use.toolId = toolId;
    use.toolNetworkId = static_cast<std::uint32_t>(toolId);
    use.kind = 0;
    use.baseFire = 1;
    use.outFire = 1;
    use.ammoCost = 1;
    use.tick = 1;
    return use;
}

ProjectileImpactPolicyV1 makeImpact(std::uint64_t typeId)
{
    ProjectileImpactPolicyV1 impact{};
    impact.projectileTypeId = typeId;
    impact.ownerId = 7;
    impact.weaponNetworkId = static_cast<std::uint32_t>(typeId);
    impact.hitKind = 1;  // world
    return impact;
}

} // namespace

bool runHotCombatSelfTest(std::string& report)
{
    bool ok = true;

    EntityRegistry::instance().destroyAll();
    DynamicComponentStore::instance().clear();

    HotReloadSystem::instance().startup();
    GenericRuntime& runtime = GenericRuntime::instance();
    ok &= check(runtime.active(), "hot package active", report);

    MimitaNet::ServerGamemodeState& d = MimitaNet::serverGamemodeState();
    d.enabled = true;
    MimitaNet::serverMatchResetEntity();
    const std::uint64_t match = MimitaNet::serverMatchEntity();
    ok &= check(match != 0, "kernel match entity available", report);

    // ── Generic authoritative server context ─────────────────────────
    std::unordered_map<std::uint32_t, MimitaNet::ServerPlayer> players;
    std::unordered_map<std::uint32_t, MimitaNet::ServerNpc> npcs;
    std::unordered_map<std::uint32_t, MimitaNet::ServerProjectile> projectiles;
    std::uint32_t nextProjectileId = 1;
    std::uint32_t serverTick = 1;
    std::uint64_t totalPacketsOut = 0;

    players.try_emplace(7);
    {
        MimitaNet::ServerPlayer& victim = players[7];
        victim.id = 7;
        victim.spawnState = MimitaNet::ServerPlayer::Active;
        victim.dead = false;
        victim.health = 100;
        victim.maxHealth = 100;
        victim.pos = glm::vec3(5.0f, 0.0f, 0.0f);
    }

    MimitaNet::ServerContextV1 context;
    context.players = &players;
    context.npcs = &npcs;
    context.projectiles = &projectiles;
    context.nextProjectileId = &nextProjectileId;
    context.tick = &serverTick;
    context.totalPacketsOut = &totalPacketsOut;
    MimitaNet::setActiveServerContext(&context);

    // ── Brand-new tool: use is owned by the hot behavior ─────────────
    {
        ToolUsePolicyV1 use = makeUse(kBananaTool);
        use.origin[0] = 0.0f; use.origin[1] = 0.0f; use.origin[2] = 1.0f;
        use.direction[0] = 1.0f; use.direction[1] = 0.0f; use.direction[2] = 0.0f;
        const bool handled = LiveBehavior::dispatchToolUse(use, 1);
        ok &= check(handled && use.handled == 1 && use.outFire == 0,
                    "banana.launcher tool use handled by hot behavior", report);
        ok &= check(projectiles.size() == 0,
                    "hot tool no longer spawns a kernel-container projectile", report);
        std::uint64_t found[8] = {0};
        const std::uint32_t hotProjectileCount =
            DynamicComponentStore::instance().enumerate(
                HOT_PROJECTILE_COMPONENT, found, 8);
        ok &= check(hotProjectileCount >= 1,
                    "hot tool spawned a composition-driven projectile entity", report);

        BananaLauncherState state{};
        const bool has = DynamicComponentStore::instance().read(
            match, kBananaState, &state, sizeof(state));
        ok &= check(has && state.shotsFired == 1 && state.toolEntity != 0,
                    "new tool recorded package state and created a tool entity", report);
    }

    // ── Real rocket uses the canonical hot projectile path ───────────
    {
        std::uint64_t found[8] = {0};
        const std::uint32_t before = DynamicComponentStore::instance().enumerate(
            HOT_PROJECTILE_COMPONENT, found, 8);
        ToolUsePolicyV1 use = makeUse(5);  // NETWORK_WEAPON_ROCKET_LAUNCHER
        use.origin[0] = 0.0f; use.origin[1] = 0.0f; use.origin[2] = 1.0f;
        use.direction[0] = 1.0f; use.direction[1] = 0.0f; use.direction[2] = 0.0f;
        const bool handled = LiveBehavior::dispatchToolUse(use, 3);
        std::uint64_t after[8] = {0};
        const std::uint32_t now = DynamicComponentStore::instance().enumerate(
            HOT_PROJECTILE_COMPONENT, after, 8);
        ok &= check(handled && use.outFire == 0 && now == before + 1 &&
                        projectiles.empty(),
                    "real rocket uses the canonical hot projectile path", report);
    }

    // ── Grenade launcher uses the same canonical hot projectile path ─
    {
        std::uint64_t found[8] = {0};
        const std::uint32_t before = DynamicComponentStore::instance().enumerate(
            HOT_PROJECTILE_COMPONENT, found, 8);
        ToolUsePolicyV1 use = makeUse(7);  // NETWORK_WEAPON_GRENADE_LAUNCHER
        use.origin[0] = 0.0f; use.origin[1] = 0.0f; use.origin[2] = 1.0f;
        use.direction[0] = 1.0f; use.direction[1] = 0.0f; use.direction[2] = 0.0f;
        const bool handled = LiveBehavior::dispatchToolUse(use, 4);
        std::uint64_t after[8] = {0};
        const std::uint32_t now = DynamicComponentStore::instance().enumerate(
            HOT_PROJECTILE_COMPONENT, after, 8);
        ok &= check(handled && use.outFire == 0 && now == before + 1,
                    "grenade uses the canonical hot projectile path", report);
    }

    // ── A second use increments the same package state ───────────────
    {
        ToolUsePolicyV1 use = makeUse(kBananaTool);
        LiveBehavior::dispatchToolUse(use, 2);
        BananaLauncherState state{};
        DynamicComponentStore::instance().read(match, kBananaState, &state, sizeof(state));
        ok &= check(state.shotsFired == 2, "tool state persists across uses", report);
    }

    // ── Unknown tool falls back to the cold path (not handled) ───────
    {
        ToolUsePolicyV1 use = makeUse(gameHash("unknown.tool"));
        const bool handled = LiveBehavior::dispatchToolUse(use, 3);
        ok &= check(!handled && use.handled == 0,
                    "unregistered tool leaves the cold path in charge", report);
    }

    // ── Brand-new projectile impact is owned by the hot behavior ─────
    {
        ProjectileImpactPolicyV1 impact = makeImpact(kBananaProjectile);
        const bool handled = LiveBehavior::dispatchProjectileImpact(impact, 4);
        ok &= check(handled && impact.handled == 1 && impact.outExplode == 1,
                    "banana.projectile impact handled by hot behavior", report);
    }

    // ── Real rocket projectile policy is hot (network id 5) ──────────
    {
        ProjectileImpactPolicyV1 impact = makeImpact(5);
        const bool handled = LiveBehavior::dispatchProjectileImpact(impact, 5);
        ok &= check(handled && impact.outExplode == 1,
                    "real rocket projectile impact policy is hot", report);
    }

    // ── Unknown projectile type falls back to cold per-type flags ────
    {
        ProjectileImpactPolicyV1 impact = makeImpact(gameHash("unknown.projectile"));
        const bool handled = LiveBehavior::dispatchProjectileImpact(impact, 6);
        ok &= check(!handled && impact.handled == 0,
                    "unregistered projectile leaves the cold flags in charge", report);
    }

    // ── Per-entity behavior binding owns the tool use ────────────────
    {
        const EntityId toolEntity =
            EntityRegistry::instance().createGeneric(EntityRealm::Server);
        BehaviorBindingsComponent bindings;
        BehaviorBinding binding;
        binding.eventType = static_cast<std::uint32_t>(gameHash("on.primary-use"));
        binding.behaviorId = gameHash("banana.launcher.use");
        bindings.add(binding);
        EntityRegistry::instance().add<BehaviorBindingsComponent>(toolEntity, bindings);

        ToolUsePolicyV1 use = makeUse(kBananaTool);
        use.toolEntity = static_cast<std::uint64_t>(toolEntity);
        const bool handled = LiveBehavior::dispatchToolUse(use, 7);
        ok &= check(handled && use.handled == 1 && use.outFire == 0,
                    "per-entity behavior binding owns tool use", report);
    }

    // ── Real input path: generic action intent → equipped runtime tool ──
    {
        FireIntentPacket pkt{};
        pkt.header.type = PACKET_FIRE_INTENT_REQUEST;
        pkt.header.playerId = 7;
        pkt.action = FIRE_INTENT_START;
        pkt.intentId = 42;
        pkt.toolId = kBananaTool;  // runtime tool: no network weapon id
        pkt.startTick = serverTick;
        pkt.originX = 0.0f; pkt.originY = 0.0f; pkt.originZ = 1.0f;
        pkt.dirX = 1.0f; pkt.dirY = 0.0f; pkt.dirZ = 0.0f;
        MimitaNet::handleFireIntentPacket(0, reinterpret_cast<const char*>(&pkt),
                                          static_cast<int>(sizeof(pkt)), players,
                                          serverTick);
        auto pit = players.find(7);
        ok &= check(pit != players.end() && pit->second.runtimeToolId == kBananaTool &&
                        pit->second.equippedToolEntity != 0,
                    "generic action intent equipped a runtime tool (no network weapon)", report);
        // The authoritative held-fire tick dispatches the equipped tool's
        // behavior; no registered weapon definition is consulted.
        MimitaNet::tickHeldFireIntents(0, players, npcs, projectiles, nextProjectileId,
                                       serverTick, totalPacketsOut);
        ok &= check(pit != players.end() && pit->second.heldFire.lastEmitTick == serverTick,
                    "held-fire tick dispatched the generic action", report);
    }

    // ── Generic action spawns an authoritative projectile ────────────
    {
        std::uint64_t beforeBuf[64] = {0};
        const std::uint32_t beforeCount = DynamicComponentStore::instance().enumerate(
            gameHash("HotProjectileState"), beforeBuf, 64);
        FireIntentPacket pkt{};
        pkt.header.type = PACKET_FIRE_INTENT_REQUEST;
        pkt.header.playerId = 7;
        pkt.action = FIRE_INTENT_START;
        pkt.intentId = 43;
        pkt.toolId = 5;  // rocket behavior key
        pkt.startTick = serverTick;
        pkt.originX = 0.0f; pkt.originY = 0.0f; pkt.originZ = 1.0f;
        pkt.dirX = 1.0f; pkt.dirY = 0.0f; pkt.dirZ = 0.0f;
        MimitaNet::handleFireIntentPacket(0, reinterpret_cast<const char*>(&pkt),
                                          static_cast<int>(sizeof(pkt)), players,
                                          serverTick);
        MimitaNet::tickHeldFireIntents(0, players, npcs, projectiles, nextProjectileId,
                                       serverTick, totalPacketsOut);
        std::uint64_t afterBuf[64] = {0};
        const std::uint32_t afterCount = DynamicComponentStore::instance().enumerate(
            gameHash("HotProjectileState"), afterBuf, 64);
        ok &= check(afterCount > beforeCount,
                    "generic input spawned an authoritative projectile", report);
    }

    // ── Generic authoritative damage capability ──────────────────────
    {
        const EntityId victimEntity =
            Ecs::ensure(EntityRealm::Server, EntityDomain::Player, 7);
        GameplayContextV1* contextHost = LiveBehavior::hostContext(7);
        GameDamageApplyFn apply = nullptr;
        if (contextHost && contextHost->resolveCapability)
            apply = reinterpret_cast<GameDamageApplyFn>(
                contextHost->resolveCapability(contextHost->host, GAME_CAP_DAMAGE_APPLY));
        ok &= check(apply != nullptr, "damage.apply capability resolved", report);
        if (apply) {
            GameDamageApplyV1 request{};
            request.victimEntity = static_cast<std::uint64_t>(victimEntity);
            request.amount = 30;
            request.sourceKind = GAME_DAMAGE_SOURCE_HITSCAN;
            apply(contextHost->host, &request);
            auto it = players.find(7);
            ok &= check(request.applied && it != players.end() &&
                            it->second.health < 100,
                        "hot code applied real authoritative damage", report);
        }
    }

    // ── Generic damage across entity types ───────────────────────────
    {
        npcs.try_emplace(3);
        MimitaNet::ServerNpc& npc = npcs[3];
        npc.entityId = 3;
        npc.health = 100;
        npc.pos = glm::vec3(9.0f, 0.0f, 0.0f);
        const EntityId npcEntity =
            Ecs::ensure(EntityRealm::Server, EntityDomain::Npc, 3);
        GameplayContextV1* host = LiveBehavior::hostContext(11);
        GameDamageApplyFn apply = reinterpret_cast<GameDamageApplyFn>(
            host->resolveCapability(host->host, GAME_CAP_DAMAGE_APPLY));
        GameDamageApplyV1 npcReq{};
        npcReq.victimEntity = static_cast<std::uint64_t>(npcEntity);
        npcReq.amount = 25;
        npcReq.sourceKind = GAME_DAMAGE_SOURCE_HITSCAN;
        apply(host->host, &npcReq);
        ok &= check(npcReq.applied && npcReq.healthAfter == 75,
                    "damage.apply works on an NPC victim (generic)", report);

        const EntityId obj = EntityRegistry::instance().createGeneric(EntityRealm::Server);
        HealthComponent hc;
        hc.current = 50;
        hc.max = 50;
        EntityRegistry::instance().add<HealthComponent>(obj, hc);
        GameDamageApplyV1 objReq{};
        objReq.victimEntity = static_cast<std::uint64_t>(obj);
        objReq.amount = 20;
        objReq.sourceKind = GAME_DAMAGE_SOURCE_EXPLOSION;
        apply(host->host, &objReq);
        ok &= check(objReq.applied && objReq.healthAfter == 30,
                    "damage.apply works on a non-actor damageable entity", report);
    }

    // ── Generic item containment/equip lifecycle ─────────────────────
    {
        const EntityId item =
            EntityRegistry::instance().createGeneric(EntityRealm::Server);
        MimitaNet::serverItemEquip(7, static_cast<std::uint64_t>(item));
        ok &= check(MimitaNet::serverItemContains(7, static_cast<std::uint64_t>(item)),
                    "item contained after equip", report);
        MimitaNet::serverItemUnequip(7);
        MimitaNet::serverItemDrop(7, static_cast<std::uint64_t>(item));
        ok &= check(!MimitaNet::serverItemContains(7, static_cast<std::uint64_t>(item)),
                    "item not contained after drop", report);
        MimitaNet::serverItemPickup(7, static_cast<std::uint64_t>(item));
        MimitaNet::serverItemEquip(7, static_cast<std::uint64_t>(item));
        ok &= check(EntityRegistry::instance().alive(item) &&
                        MimitaNet::serverItemContains(7, static_cast<std::uint64_t>(item)),
                    "same item EntityId survives inventory->equip->drop->pickup->re-equip",
                    report);
    }

    // ── Migrated registered weapon: revolver state on its tool entity ─
    {
        std::unordered_map<std::uint32_t, MimitaNet::ServerPlayer> wp;
        wp.try_emplace(7);
        MimitaNet::ServerPlayer& p = wp[7];
        p.id = 7;
        // Seed the legacy map with bogus values that must NOT be authoritative.
        MimitaNet::ServerPlayer::ServerWeaponRuntime scratch{};
        scratch.initialized = true;
        scratch.magazineAmmo = 999;
        scratch.nextAllowedFireTick = 4242;
        p.weaponRuntimes["revolver"] = scratch;

        const std::uint64_t tool =
            MimitaNet::serverWeaponToolEntity(p, "revolver", true);
        ok &= check(tool != 0, "revolver state tool entity created", report);

        MimitaNet::serverWeaponStateWriteComponent(p, "revolver", 6, 12, 100);
        std::int32_t mag = 0, res = 0;
        std::uint64_t cd = 0;
        ok &= check(MimitaNet::serverWeaponStateReadComponent(p, "revolver", &mag, &res,
                                                              &cd) &&
                        mag == 6 && res == 12 && cd == 100,
                    "revolver ammo/cooldown live on the tool entity component", report);

        MimitaNet::serverWeaponStateLoad(p, "revolver");
        ok &= check(p.weaponRuntimes["revolver"].magazineAmmo == 6 &&
                        p.weaponRuntimes["revolver"].nextAllowedFireTick == 100,
                    "legacy weapon map is not authoritative (component wins)", report);

        MimitaNet::serverItemEquip(7, tool);
        MimitaNet::serverItemDrop(7, tool);
        MimitaNet::serverItemPickup(7, tool);
        MimitaNet::serverItemEquip(7, tool);
        std::int32_t mag2 = 0, res2 = 0;
        std::uint64_t cd2 = 0;
        ok &= check(EntityRegistry::instance().alive(static_cast<EntityId>(tool)) &&
                        MimitaNet::serverWeaponToolEntity(p, "revolver", false) == tool &&
                        MimitaNet::serverWeaponStateReadComponent(p, "revolver", &mag2,
                                                                  &res2, &cd2) &&
                        mag2 == 6,
                    "revolver tool EntityId + state survive equip/drop/pickup/re-equip",
                    report);
    }

    MimitaNet::setActiveServerContext(nullptr);
    // ── Hot projectile lifecycle (projectiles.60) owns the entity ────
    {
        const EntityId proj =
            EntityRegistry::instance().createGeneric(EntityRealm::Server);
        HotProjectileStateV1 s{};
        s.velocity[0] = 10.0f;
        s.lifetime = 1.0f;
        s.radius = 0.1f;
        s.impactDamage = 1.0f;
        s.flags = 0;  // pure integration: no explode
        DynamicComponentStore::instance().write(proj, HOT_PROJECTILE_COMPONENT, &s,
                                                sizeof(s));

        const float dt = 1.0f / 60.0f;
        runtime.runDomain(gameHash("projectiles.60"), 100, dt,
                          LiveBehavior::hostContext(100));

        HotProjectileStateV1 after{};
        const bool read = DynamicComponentStore::instance().read(
            proj, HOT_PROJECTILE_COMPONENT, &after, sizeof(after));
        ok &= check(read && after.age > 0.0f && after.position[0] > 0.0f,
                    "hot projectiles.60 system simulates the projectile entity",
                    report);
    }

    GenericRuntime::instance().deactivate();
    HotReloadSystem::instance().unloadGameDLL();
    return ok;
}
