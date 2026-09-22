// 09 17 2026
/* purpose
* Shared hot hitscan tool behavior (family TOOL_BEHAVIOR_HITSCAN). Ports the
* cold authoritative `WeaponExecution::traceHitscan` into hot behavior using
* only generic capabilities: the fixed pellet grid (pellet-pattern.h), an actor
* scan (findEntities + readComponent TRANSFORM/BODY/HEALTH), a world occlusion
* ray (queryWorldRay), the v2.0.6 damage model (base x part x falloff), per
* target knockback aggregation, and damage.apply. It owns per-instance
* ammo/cooldown/reload on the tool entity and emits generic tool action events.
* Live values come from the tool definition's params when present, so editing
* tool-visuals.cpp changes behavior with no EXE rebuild.
* It CLAIMS the use (outFire = 0) only when it will actually act; a dry fire or
* an active cooldown declines so the caller's cold path stays authoritative.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-consequences.h"
#include "hot-reload/hot-hitscan-target.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-tool-action.h"
#include "hot-reload/hot-tool-state.h"
#include "hot-reload/hot-tool-tuning.h"
#include "hot-reload/hot-tool-visual.h"

#include "combat/hitscan-model.h"
#include "combat/pellet-pattern.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#include <glm/glm.hpp>

namespace {

// NETWORK_WEAPON_REVOLVER value (see network/packets.h).
constexpr std::uint64_t kRevolverNetworkId = 1;
// Matches WeaponExecution::DEFAULT_HITSCAN_RANGE / DEFAULT_BODY_* (cold owner).
constexpr float kDefaultRange = 250.0f;
constexpr float kDefaultBodyRadius = 0.65f;
constexpr float kDefaultBodyHeight = 3.5f;
constexpr int kMaxTargets = 64;

float paramOr(const ToolDefinitionV1* def, const char* key, float fallback)
{
    if (!def || !def->params)
        return fallback;
    for (std::uint32_t i = 0; i < def->paramCount; ++i) {
        if (def->params[i].key && std::strcmp(def->params[i].key, key) == 0)
            return def->params[i].value;
    }
    return fallback;
}

// Bool part flags from normalized hit height (0 = feet, 1 = head) for the
// capsule fallback path; the box path knows the part directly.
void hotHitscanPartFlags(float heightFraction, bool& head, bool& leg)
{
    head = heightFraction >= 0.85f;
    leg = !head && heightFraction <= 0.35f;
}

// Ray vs axis-aligned box (outDistance = entry distance; false = no hit).
bool rayAabb(const float origin[3], const float dir[3], const float bmin[3],
             const float bmax[3], float maxDistance, float& outDistance)
{
    float tmin = 0.0f;
    float tmax = maxDistance;
    for (int axis = 0; axis < 3; ++axis)
    {
        if (std::fabs(dir[axis]) < 1e-8f)
        {
            if (origin[axis] < bmin[axis] || origin[axis] > bmax[axis])
                return false;
            continue;
        }
        const float inv = 1.0f / dir[axis];
        float t1 = (bmin[axis] - origin[axis]) * inv;
        float t2 = (bmax[axis] - origin[axis]) * inv;
        if (t1 > t2)
            std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin > tmax)
            return false;
    }
    outDistance = tmin;
    return true;
}

struct HotTarget
{
    std::uint64_t entity = 0;
    // Authoritative rewound body-part hitboxes, published by the cold server
    // for this shot. When absent (hot-side/legacy callers) the body capsule
    // below is the fallback.
    bool hasParts = false;
    HotHitscanTargetV1 geometry{};
    float center[3] = {0.0f, 0.0f, 0.0f};
    float half[3] = {0.0f, 0.0f, 0.0f};
};

// One aggregate per victim, matching traceHitscan's HitscanDamageAggregate.
struct DamageAggregate
{
    bool used = false;
    std::uint64_t entity = 0;
    int damage = 0;
    int pelletHits = 0;
    bool headshot = false;
    float knockback[3] = {0.0f, 0.0f, 0.0f};
    float hitPosition[3] = {0.0f, 0.0f, 0.0f};
    float hitNormal[3] = {0.0f, 0.0f, 0.0f};
};

void MIMITA_GAME_CALL hitscanUse(const ToolUsePolicyV1* use, GameplayContextV1* ctx)
{
    if (!use || !ctx)
        return;
    auto* mutableUse = const_cast<ToolUsePolicyV1*>(use);

    if (!ctx->findEntities || !ctx->readComponent || !ctx->resolveCapability)
        return;

    // Player and NPC uses are both hot-ownable: authoritative consequences
    // (damage policy, DamageConfirmed/NPC-damage events, kills, shot visuals)
    // are handed to the shared cold pipeline via GAME_CAP_HITSCAN_RESOLVE.
    if (use->userEntity == 0)
        return;

    const std::uint64_t key = use->toolId != 0 ? use->toolId : use->toolNetworkId;
    const ToolDefinitionV1* def = findToolDefinition(key);

    // Prefer the authoritative registry tuning (honors weapons.json and the
    // json/cpp `behaviorSource`) over the recipe literals; fall back to the
    // recipe/params when the weapon has no registered tuning.
    GameWeaponTuningV1 tuning{};
    const bool hasTuning = hotQueryWeaponTuning(ctx, use->toolNetworkId, tuning);
    auto tuningParam = [&](const char* name, float fallback) -> float {
        float value = 0.0f;
        if (hasTuning && hotTuningHasParam(tuning, name, &value))
            return value;
        return paramOr(def, name, fallback);
    };

    const float range = tuningParam("range", tuningParam("maxRange",
        paramOr(def, "hotRange", kDefaultRange)));
    const float beamThickness =
        std::max(0.0f, hasTuning ? tuning.beamThickness : paramOr(def, "beamThickness", 0.0f));
    const float knockbackPerDamage = hasTuning
        ? tuning.victimKnockbackPerDamage
        : paramOr(def, "victimKnockbackPerDamage", paramOr(def, "knockbackPerDamage", 0.08f));
    const int pelletCount = std::max(1, hasTuning ? tuning.pelletCount
                                                  : (def ? def->pelletCount : 1));
    const float spread = tuningParam("gridSpreadDegrees",
        hasTuning ? tuning.spread : (def ? def->spread : 0.0f));
    const std::int32_t magazine =
        hasTuning ? tuning.magazineSize : (def ? def->magazineSize : 0);
    const std::int32_t reserve =
        hasTuning ? tuning.reserveAmmo : (def ? def->reserveAmmo : -1);
    const float fireDelay =
        hasTuning ? tuning.fireDelay : (def ? def->fireDelay : 0.02f);
    const float reloadTime =
        hasTuning ? tuning.reloadTime : (def ? def->reloadTime : 1.0f);

    HitscanDamageParams damageParams;
    damageParams.baseDamage = hasTuning
        ? tuning.damage
        : (def && def->damage > 0.0f ? def->damage : paramOr(def, "hotDamage", 25.0f));
    damageParams.headshotMultiplier =
        hasTuning ? tuning.headshotMultiplier : (def ? def->headshotMultiplier : 2.0f);
    damageParams.limbDamageMultiplier = tuningParam("limbDamageMultiplier", 0.75f);
    damageParams.distanceFalloffStart = tuningParam("distanceFalloffStart", 0.0f);
    damageParams.minDamageFraction = tuningParam("minDamageFraction", 0.1f);
    damageParams.falloffExponent = tuningParam("falloffExponent", 1.0f);

    // Per-instance ammo/cooldown on the tool entity (independent per actor).
    ToolInstanceStateV1 state{};
    const bool hasState = use->toolEntity != 0 && ctx->dynamicReadComponent &&
                          ctx->dynamicWriteComponent;
    if (hasState) {
        state = toolStateEnsure(ctx, use->toolEntity, key, magazine, reserve,
                                use->userEntity);
        if (state.cooldownRemaining > 0.0f) {
            toolLogEvent(ctx, 1, "tool.hitscan", "cooldown active", "cooldown",
                         use->toolEntity, use->userEntity, 1, use->tick);
            return;  // cold path stays authoritative; do not claim
        }
        if (magazine > 0 && !toolStateConsume(ctx, state, 1)) {
            ToolActionEventV1 dry{};
            dry.actorEntity = use->userEntity;
            dry.toolEntity = use->toolEntity;
            dry.toolId = key;
            dry.behaviorId = TOOL_BEHAVIOR_HITSCAN;
            dry.action = TOOL_ACTION_DRY_FIRE;
            dry.simulationTick = use->tick;
            emitToolAction(ctx, dry);
            return;
        }
    }

    // Normalize the aim direction and build the fixed pellet grid.
    glm::vec3 aim(use->direction[0], use->direction[1], use->direction[2]);
    if (glm::length(aim) <= 1e-4f)
        aim = glm::vec3(1.0f, 0.0f, 0.0f);
    aim = glm::normalize(aim);

    glm::vec3 pelletDirs[MAX_PELLETS_PER_BLAST]{};
    const int pellets = buildFixedPelletDirections(
        aim, pelletCount, spread, pelletDirs, MAX_PELLETS_PER_BLAST);

    const float origin[3] = {use->origin[0], use->origin[1], use->origin[2]};
    const float aimDir[3] = {aim.x, aim.y, aim.z};

    // Single world ray along the aim direction defines the occlusion distance,
    // exactly like the cold path's worldBlockDistance.
    float worldBlockDistance = range;
    float worldHitPt[3] = {origin[0] + aimDir[0] * range,
                           origin[1] + aimDir[1] * range,
                           origin[2] + aimDir[2] * range};
    float worldNormal[3] = {-aimDir[0], -aimDir[1], -aimDir[2]};
    if (ctx->queryWorldRay) {
        float wp[3] = {0.0f, 0.0f, 0.0f};
        float wn[3] = {0.0f, 0.0f, 0.0f};
        float wd = 0.0f;
        if (ctx->queryWorldRay(ctx->host, origin, aimDir, range, wp, wn, &wd)) {
            worldBlockDistance = wd;
            worldHitPt[0] = wp[0]; worldHitPt[1] = wp[1]; worldHitPt[2] = wp[2];
            worldNormal[0] = wn[0]; worldNormal[1] = wn[1]; worldNormal[2] = wn[2];
        }
    }

    // Candidate actors: everything alive that is not the shooter.
    std::uint64_t actors[kMaxTargets];
    const std::uint32_t actorCount =
        ctx->findEntities(ctx->host, 0, GAME_COMPONENT_HEALTH, actors, kMaxTargets);

    HotTarget targets[kMaxTargets];
    int targetCount = 0;
    for (std::uint32_t i = 0; i < actorCount && targetCount < kMaxTargets; ++i)
    {
        if (actors[i] == use->userEntity)
            continue;
        GameHealthComponentV1 hp{};
        if (!ctx->readComponent(ctx->host, actors[i], GAME_COMPONENT_HEALTH, &hp,
                                sizeof(hp)) || hp.dead)
            continue;
        GameTransformComponentV1 tf{};
        if (!ctx->readComponent(ctx->host, actors[i], GAME_COMPONENT_TRANSFORM, &tf,
                                sizeof(tf)))
            continue;
        GameBodyComponentV1 body{};
        const bool hasBody = ctx->readComponent(
            ctx->host, actors[i], GAME_COMPONENT_BODY, &body, sizeof(body));
        const float radius = (hasBody && body.radius > 0.0f)
                                 ? body.radius : kDefaultBodyRadius;
        const float height = (hasBody && body.height > 0.0f)
                                 ? body.height : kDefaultBodyHeight;
        HotTarget& t = targets[targetCount++];
        t.entity = actors[i];
        t.center[0] = tf.position[0];
        t.center[1] = tf.position[1];
        t.center[2] = tf.position[2];
        t.half[0] = radius;
        t.half[1] = radius;
        t.half[2] = height * 0.5f;
        if (ctx->dynamicReadComponent &&
            ctx->dynamicReadComponent(ctx->host, actors[i],
                                      hotHitscanTargetComponentId(), &t.geometry,
                                      sizeof(t.geometry)) &&
            t.geometry.partCount > 0)
            t.hasParts = true;
    }

    DamageAggregate aggregates[kMaxTargets];
    GameHitscanPelletV1 pelletResults[GAME_MAX_HITSCAN_PELLETS]{};

    for (int p = 0; p < pellets; ++p)
    {
        const glm::vec3& pelletDir = pelletDirs[p];
        const float dir[3] = {pelletDir.x, pelletDir.y, pelletDir.z};

        int bestIndex = -1;
        float bestDistance = worldBlockDistance;
        float bestHeightFraction = 0.5f;
        bool bestHead = false;
        bool bestLeg = false;
        for (int t = 0; t < targetCount; ++t)
        {
            if (targets[t].hasParts)
            {
                // Authoritative rewound body-part boxes: exactly what the cold
                // trace validates against (no capsule approximation).
                for (std::uint32_t part = 0; part < targets[t].geometry.partCount;
                     ++part)
                {
                    const HotHitscanPartV1& box = targets[t].geometry.parts[part];
                    const float bmin[3] = {
                        box.center[0] - box.half[0] - beamThickness,
                        box.center[1] - box.half[1] - beamThickness,
                        box.center[2] - box.half[2] - beamThickness};
                    const float bmax[3] = {
                        box.center[0] + box.half[0] + beamThickness,
                        box.center[1] + box.half[1] + beamThickness,
                        box.center[2] + box.half[2] + beamThickness};
                    float distance = 0.0f;
                    if (!rayAabb(origin, dir, bmin, bmax, bestDistance, distance))
                        continue;
                    if (bestIndex >= 0 && distance >= bestDistance)
                        continue;
                    bestIndex = t;
                    bestDistance = distance;
                    bestHead = box.bodyPart == 1u;
                    bestLeg = box.bodyPart == 2u;
                }
                continue;
            }

            const float bmin[3] = {
                targets[t].center[0] - targets[t].half[0] - beamThickness,
                targets[t].center[1] - targets[t].half[1] - beamThickness,
                targets[t].center[2] - targets[t].half[2] - beamThickness};
            const float bmax[3] = {
                targets[t].center[0] + targets[t].half[0] + beamThickness,
                targets[t].center[1] + targets[t].half[1] + beamThickness,
                targets[t].center[2] + targets[t].half[2] + beamThickness};
            float distance = 0.0f;
            if (!rayAabb(origin, dir, bmin, bmax, bestDistance, distance))
                continue;
            if (bestIndex >= 0 && distance >= bestDistance)
                continue;
            bestIndex = t;
            bestDistance = distance;
            const float zMin = targets[t].center[2] - targets[t].half[2];
            const float zSpan = targets[t].half[2] * 2.0f;
            bestHeightFraction = zSpan > 1e-4f
                ? std::clamp((origin[2] + dir[2] * distance - zMin) / zSpan, 0.0f, 1.0f)
                : 0.5f;
            bestHead = bestHeightFraction >= 0.85f;
            bestLeg = !bestHead && bestHeightFraction <= 0.35f;
        }

        GameHitscanPelletV1& pellet =
            pelletResults[p < GAME_MAX_HITSCAN_PELLETS ? p : GAME_MAX_HITSCAN_PELLETS - 1];
        if (bestIndex < 0)
        {
            // Miss / world-blocked: report the world endpoint for the tracer.
            const float endDist = worldBlockDistance < range ? worldBlockDistance : range;
            pellet.hit = 0;
            pellet.victimEntity = 0;
            pellet.hitPosition[0] = origin[0] + dir[0] * endDist;
            pellet.hitPosition[1] = origin[1] + dir[1] * endDist;
            pellet.hitPosition[2] = origin[2] + dir[2] * endDist;
            pellet.hitNormal[0] = -dir[0];
            pellet.hitNormal[1] = -dir[1];
            pellet.hitNormal[2] = -dir[2];
            continue;
        }

        const float hitX = origin[0] + dir[0] * bestDistance;
        const float hitY = origin[1] + dir[1] * bestDistance;
        const float hitZ = origin[2] + dir[2] * bestDistance;
        pellet.hit = 1;
        pellet.victimEntity = targets[bestIndex].entity;
        pellet.hitPosition[0] = hitX;
        pellet.hitPosition[1] = hitY;
        pellet.hitPosition[2] = hitZ;
        pellet.hitNormal[0] = -dir[0];
        pellet.hitNormal[1] = -dir[1];
        pellet.hitNormal[2] = -dir[2];
        pellet.headshot = bestHead ? 1u : 0u;

        bool head = targets[bestIndex].hasParts ? bestHead : false;
        bool leg = targets[bestIndex].hasParts ? bestLeg : false;
        if (!targets[bestIndex].hasParts)
            hotHitscanPartFlags(bestHeightFraction, head, leg);
        const int damage = computeHitscanDamage(damageParams, head, leg, bestDistance);

        DamageAggregate* agg = nullptr;
        for (int a = 0; a < kMaxTargets; ++a)
        {
            if (aggregates[a].used && aggregates[a].entity == targets[bestIndex].entity)
            {
                agg = &aggregates[a];
                break;
            }
        }
        if (!agg)
        {
            for (int a = 0; a < kMaxTargets; ++a)
            {
                if (!aggregates[a].used)
                {
                    agg = &aggregates[a];
                    break;
                }
            }
            if (!agg)
                break;
            agg->used = true;
            agg->entity = targets[bestIndex].entity;
            agg->hitPosition[0] = hitX;
            agg->hitPosition[1] = hitY;
            agg->hitPosition[2] = hitZ;
            agg->hitNormal[0] = -dir[0];
            agg->hitNormal[1] = -dir[1];
            agg->hitNormal[2] = -dir[2];
        }
        agg->damage += damage;
        agg->pelletHits += 1;
        agg->headshot = agg->headshot || head;
        agg->knockback[0] += pelletDir.x * (float)damage * knockbackPerDamage;
        agg->knockback[1] += pelletDir.y * (float)damage * knockbackPerDamage;
        agg->knockback[2] += pelletDir.z * (float)damage * knockbackPerDamage;
    }

    // We will act: claim the use and suppress the built-in fire.
    mutableUse->outFire = 0;
    mutableUse->handled = 1;

    ToolActionEventV1 accepted{};
    accepted.actorEntity = use->userEntity;
    accepted.toolEntity = use->toolEntity;
    accepted.toolId = key;
    accepted.behaviorId = TOOL_BEHAVIOR_HITSCAN;
    accepted.action = TOOL_ACTION_PRIMARY_ACCEPTED;
    accepted.simulationTick = use->tick;
    accepted.actionSequence = state.stateVersion;
    accepted.origin[0] = use->origin[0];
    accepted.origin[1] = use->origin[1];
    accepted.origin[2] = use->origin[2];
    accepted.direction[0] = aimDir[0];
    accepted.direction[1] = aimDir[1];
    accepted.direction[2] = aimDir[2];
    emitToolAction(ctx, accepted);

    // Shared hot consequence orchestration: damage policy, DamageConfirmed/
    // NPC-damage events, kills, and shot visuals. Same code the cold trace
    // uses, so a consequence bug is a hot fix.
    {
        std::vector<HotConsequences::HitscanVictim> victims;
        victims.reserve((std::size_t)kMaxTargets);
        for (int a = 0; a < kMaxTargets; ++a)
        {
            if (!aggregates[a].used || aggregates[a].damage <= 0)
                continue;
            HotConsequences::HitscanVictim v;
            v.entity = aggregates[a].entity;
            v.pelletHits = (std::uint32_t)aggregates[a].pelletHits;
            v.damage = aggregates[a].damage;
            v.headshot = aggregates[a].headshot;
            v.knockback[0] = aggregates[a].knockback[0];
            v.knockback[1] = aggregates[a].knockback[1];
            v.knockback[2] = aggregates[a].knockback[2];
            v.hitPosition[0] = aggregates[a].hitPosition[0];
            v.hitPosition[1] = aggregates[a].hitPosition[1];
            v.hitPosition[2] = aggregates[a].hitPosition[2];
            v.hitNormal[0] = aggregates[a].hitNormal[0];
            v.hitNormal[1] = aggregates[a].hitNormal[1];
            v.hitNormal[2] = aggregates[a].hitNormal[2];
            victims.push_back(v);
        }
        HotConsequences::resolveHitscan(
            ctx, use->userEntity, WeaponDefinition{}, use->toolNetworkId,
            use->toolNetworkId, (std::uint32_t)pellets,
            (std::uint32_t)use->predictionKey, use->clientSimulationTick,
            use->claimedTargetId, origin, aimDir, worldHitPt, worldNormal, range,
            worldBlockDistance, victims);
    }

    // Hot-side presentation facts (animation/effects modules consume these).
    for (int a = 0; a < kMaxTargets; ++a)
    {
        if (!aggregates[a].used || aggregates[a].damage <= 0)
            continue;
        ToolActionEventV1 hit = accepted;
        hit.action = TOOL_ACTION_HIT;
        hit.amount = aggregates[a].damage;
        emitToolAction(ctx, hit);
    }

    if (hasState) {
        state.cooldownRemaining = fireDelay;
        if (magazine > 0 && state.currentAmmo <= 0 &&
            toolStateTryStartReload(ctx, state, magazine, reloadTime,
                                    TOOL_RELOAD_EMPTY_MAGAZINE)) {
            ToolActionEventV1 reload = accepted;
            reload.action = TOOL_ACTION_RELOAD_STARTED;
            reload.strength100 =
                (std::uint32_t)(state.reloadRemaining * 100.0f);
            emitToolAction(ctx, reload);
        }
        toolStateWrite(ctx, state.toolEntity, state);
    }

    ToolActionEventV1 fired = accepted;
    fired.action = TOOL_ACTION_FIRED;
    fired.strength100 = (std::uint32_t)(state.cooldownRemaining * 100.0f);
    fired.amount = state.currentAmmo;
    emitToolAction(ctx, fired);
}

} // namespace

// Shared family registrations. Pellet tools (shotgun/AA12) reuse the same
// function; the pellet count/spread live in the definition, so no second
// implementation exists. Plus the legacy per-tool key for the revolver.
const MimitaHotPackage::BehaviorIdRegistrar s_hitscanBehavior{
    TOOL_BEHAVIOR_HITSCAN, hitscanUse};
const MimitaHotPackage::BehaviorIdRegistrar s_pelletBehavior{
    TOOL_BEHAVIOR_PELLET, hitscanUse};
const MimitaHotPackage::ToolBehaviorRegistrar s_hitscanTool{kRevolverNetworkId,
                                                            hitscanUse};

#endif
