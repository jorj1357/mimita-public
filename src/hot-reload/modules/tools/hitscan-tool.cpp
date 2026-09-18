// 09 17 2026
/* purpose
* Shared hot hitscan tool behavior (family TOOL_BEHAVIOR_HITSCAN). Reads the
* actor's generic target relationship, raycasts the world through the kernel
* query primitive, applies damage + effect, emits generic tool action events,
* and owns per-instance ammo/cooldown on the tool entity. Live values come from
* the tool definition's params when present, so editing tool-visuals.cpp changes
* behavior with no EXE rebuild.
* It CLAIMS the use (outFire = 0) only when it will actually act. When there is
* no relationship target, out of range, or a blocked shot, it leaves the use
* unclaimed so the caller's cold path stays authoritative (players). This is
* what lets one behavior serve both NPCs (relationship target -> hot) and
* players (no target -> cold hitscan) without a weapon-type switch.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-tool-action.h"
#include "hot-reload/hot-tool-state.h"
#include "hot-reload/hot-tool-visual.h"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace {

// NETWORK_WEAPON_REVOLVER value (see network/packets.h).
constexpr std::uint64_t kRevolverNetworkId = 1;
constexpr float kHitscanRange = 40.0f;
constexpr std::int32_t kHitscanDamage = 25;

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

void MIMITA_GAME_CALL hitscanUse(const ToolUsePolicyV1* use, GameplayContextV1* ctx)
{
    if (!use || !ctx)
        return;
    auto* mutableUse = const_cast<ToolUsePolicyV1*>(use);

    if (!ctx->relationshipQuery || !ctx->readComponent || !ctx->resolveCapability)
        return;

    const std::uint64_t key = use->toolId != 0 ? use->toolId : use->toolNetworkId;
    const ToolDefinitionV1* def = findToolDefinition(key);
    const float range = paramOr(def, "hotRange", kHitscanRange);
    const std::int32_t damage =
        (std::int32_t)paramOr(def, "hotDamage", (float)kHitscanDamage);
    const std::int32_t magazine = def ? def->magazineSize : 0;
    const std::int32_t reserve = def ? def->reserveAmmo : -1;

    // Resolve the generic relationship target first. No target -> do not claim;
    // the caller's cold path (player hitscan) stays in charge.
    std::uint64_t target = 0;
    if (ctx->relationshipQuery(ctx->host, gameHash("relationship.targets"),
                               use->userEntity, &target, nullptr, 1) == 0 ||
        target == 0)
        return;

    GameTransformComponentV1 tf{};
    if (!ctx->readComponent(ctx->host, target, GAME_COMPONENT_TRANSFORM, &tf,
                            sizeof(tf)))
        return;

    const float ox = use->origin[0], oy = use->origin[1], oz = use->origin[2];
    float dx = tf.position[0] - ox;
    float dy = tf.position[1] - oy;
    float dz = tf.position[2] - oz;
    const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (dist > range || dist < 0.001f)
        return;
    dx /= dist; dy /= dist; dz /= dist;

    // Low-level occlusion query stays a kernel primitive; the behavior owns the
    // policy of whether a blocked shot hits.
    float point[3] = {0.0f, 0.0f, 0.0f};
    float normal[3] = {0.0f, 0.0f, 0.0f};
    float wallDist = 0.0f;
    if (ctx->queryWorldRay &&
        ctx->queryWorldRay(ctx->host, use->origin, use->direction, range,
                           point, normal, &wallDist)) {
        if (wallDist < dist - 0.5f)
            return;  // world blocks the shot; do not claim
    }

    // We will act: claim the use and suppress the built-in fire.
    mutableUse->outFire = 0;
    mutableUse->handled = 1;

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
            return;
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
    accepted.direction[0] = use->direction[0];
    accepted.direction[1] = use->direction[1];
    accepted.direction[2] = use->direction[2];
    emitToolAction(ctx, accepted);

    auto applyDamage = reinterpret_cast<GameDamageApplyFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_DAMAGE_APPLY));
    if (applyDamage) {
        GameDamageApplyV1 request{};
        request.victimEntity = target;
        request.sourceEntity = use->userEntity;
        request.amount = damage;
        request.sourceKind = GAME_DAMAGE_SOURCE_HITSCAN;
        applyDamage(ctx->host, &request);
    }

    auto spawnEffect = reinterpret_cast<GameEffectSpawnFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_EFFECT_SPAWN));
    if (spawnEffect) {
        GameEffectSpawnV1 fx{};
        fx.kind = gameHash("effect.muzzle");
        fx.ownerEntity = use->userEntity;
        fx.count = 1;
        fx.position[0] = use->origin[0];
        fx.position[1] = use->origin[1];
        fx.position[2] = use->origin[2];
        fx.direction[0] = dx; fx.direction[1] = dy; fx.direction[2] = dz;
        fx.scale = 1.0f;
        fx.endScale = 1.0f;
        fx.speed = 0.0f;
        fx.lifetime = 0.08f;
        spawnEffect(ctx->host, &fx);
    }

    ToolActionEventV1 hit = accepted;
    hit.action = TOOL_ACTION_HIT;
    hit.amount = damage;
    hit.direction[0] = dx; hit.direction[1] = dy; hit.direction[2] = dz;
    emitToolAction(ctx, hit);

    if (hasState) {
        state.cooldownRemaining = def ? def->fireDelay : 0.02f;
        if (magazine > 0 && state.currentAmmo <= 0 &&
            toolStateTryStartReload(ctx, state, magazine,
                                    def ? def->reloadTime : 1.0f,
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
