// 09 17 2026
/* purpose
* The single per-instance tool state owner: ammo, reserve, cooldown, and reload
* progress live on the equipped tool entity (one dynamic component), not on the
* actor and not in a per-weapon cold map. Two actors holding the same definition
* therefore have independent state. One canonical reload transition
* (tryStartReload / completeReload) owns all ammo math, per the weapons spec.
* Hot-only header: not a GameAPI context field; no cold edit to add a tool.
* Does NOT own damage, firing, or animation.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"
#include "hot-reload/hot-tool-action.h"

static constexpr std::uint64_t HOT_TOOL_STATE_COMPONENT =
    gameHash("ToolInstanceState");
static constexpr std::uint32_t HOT_TOOL_STATE_VERSION = 1;

// Reload trigger reasons (plain numbers; not a kernel enum).
static constexpr std::uint32_t TOOL_RELOAD_MANUAL = 0;
static constexpr std::uint32_t TOOL_RELOAD_EMPTY_MAGAZINE = 1;
static constexpr std::uint32_t TOOL_RELOAD_UNEQUIPPED = 2;

struct ToolInstanceStateV1 {
    std::uint32_t version;          // HOT_TOOL_STATE_VERSION
    std::uint32_t byteSize;         // sizeof(ToolInstanceStateV1)
    std::uint64_t toolEntity;
    std::uint64_t definitionId;     // runtime tool key
    std::uint64_t actorEntity;      // current wielder (0 = unknown)
    std::int32_t currentAmmo;
    std::int32_t reserveAmmo;
    float cooldownRemaining;        // seconds until the next allowed use
    float reloadRemaining;          // seconds until reload completes
    std::uint32_t isReloading;
    std::uint32_t stateVersion;     // bumped on every authoritative change
};

inline bool toolStateRead(GameplayContextV1* ctx, std::uint64_t toolEntity,
                          ToolInstanceStateV1* out)
{
    if (!ctx || !ctx->dynamicReadComponent || toolEntity == 0 || !out)
        return false;
    return ctx->dynamicReadComponent(ctx->host, toolEntity,
                                     HOT_TOOL_STATE_COMPONENT, out, sizeof(*out));
}

inline bool toolStateWrite(GameplayContextV1* ctx, std::uint64_t toolEntity,
                           const ToolInstanceStateV1& in)
{
    if (!ctx || !ctx->dynamicWriteComponent || toolEntity == 0)
        return false;
    return ctx->dynamicWriteComponent(ctx->host, toolEntity,
                                      HOT_TOOL_STATE_COMPONENT, &in,
                                      sizeof(in));
}

// Read-or-create. Ammo starts full for a magazine tool; reserve is caller-supplied
// (negative = leave reserve at 0 for definitions without reserve).
inline ToolInstanceStateV1 toolStateEnsure(GameplayContextV1* ctx,
                                           std::uint64_t toolEntity,
                                           std::uint64_t definitionId,
                                           std::int32_t magazineSize,
                                           std::int32_t reserveAmmo,
                                           std::uint64_t actorEntity = 0)
{
    ToolInstanceStateV1 s{};
    if (toolStateRead(ctx, toolEntity, &s) &&
        s.version == HOT_TOOL_STATE_VERSION) {
        if (actorEntity != 0 && s.actorEntity != actorEntity) {
            s.actorEntity = actorEntity;
            toolStateWrite(ctx, toolEntity, s);
        }
        return s;
    }
    s.version = HOT_TOOL_STATE_VERSION;
    s.byteSize = sizeof(ToolInstanceStateV1);
    s.toolEntity = toolEntity;
    s.definitionId = definitionId;
    s.actorEntity = actorEntity;
    s.currentAmmo = magazineSize > 0 ? magazineSize : 0;
    s.reserveAmmo = reserveAmmo > 0 ? reserveAmmo : 0;
    s.cooldownRemaining = 0.0f;
    s.reloadRemaining = 0.0f;
    s.isReloading = 0;
    s.stateVersion = 1;
    toolStateWrite(ctx, toolEntity, s);
    return s;
}

// Advance one fixed tick. Completes reload and clears cooldown; returns the
// number of reload completions (0 or 1) so the caller can emit the fact.
inline std::uint32_t toolStateAdvance(GameplayContextV1* ctx,
                                      ToolInstanceStateV1& s, float dt)
{
    std::uint32_t completed = 0;
    if (s.cooldownRemaining > 0.0f) {
        s.cooldownRemaining -= dt;
        if (s.cooldownRemaining < 0.0f)
            s.cooldownRemaining = 0.0f;
    }
    if (s.isReloading) {
        s.reloadRemaining -= dt;
        if (s.reloadRemaining <= 0.0f) {
            s.reloadRemaining = 0.0f;
            s.isReloading = 0;
            completed = 1;
        }
    }
    if (completed)
        s.stateVersion++;
    toolStateWrite(ctx, s.toolEntity, s);
    return completed;
}

// The one reload transition. Manual/empty/unequipped all funnel here.
// Pure state transition (no component access) so cold and hot share one owner.
inline bool toolStateBeginReload(ToolInstanceStateV1& s,
                                 std::int32_t magazineSize,
                                 float reloadTimeSeconds)
{
    if (s.isReloading)
        return false;
    if (magazineSize <= 0)
        return false;                       // melee / no-ammo tools never reload
    if (s.currentAmmo >= magazineSize)
        return false;                       // magazine already full
    if (s.reserveAmmo <= 0)
        return false;                       // nothing in reserve
    s.isReloading = 1;
    s.reloadRemaining = reloadTimeSeconds > 0.0f ? reloadTimeSeconds : 0.0f;
    s.stateVersion++;
    return true;
}

inline bool toolStateTryStartReload(GameplayContextV1* ctx,
                                    ToolInstanceStateV1& s,
                                    std::int32_t magazineSize,
                                    float reloadTimeSeconds,
                                    std::uint32_t /*reason*/)
{
    if (!toolStateBeginReload(s, magazineSize, reloadTimeSeconds))
        return false;
    toolStateWrite(ctx, s.toolEntity, s);
    return true;
}

// The one completion ammo math: needed = mag - current; transfer from reserve.
// Pure state transition (no component access) so cold and hot share one owner.
inline bool toolStateFinishReload(ToolInstanceStateV1& s,
                                  std::int32_t magazineSize)
{
    if (magazineSize <= 0)
        return false;
    const std::int32_t needed = magazineSize - s.currentAmmo;
    std::int32_t transferred = needed < s.reserveAmmo ? needed : s.reserveAmmo;
    if (transferred < 0)
        transferred = 0;
    s.currentAmmo += transferred;
    s.reserveAmmo -= transferred;
    s.isReloading = 0;
    s.reloadRemaining = 0.0f;
    s.stateVersion++;
    return transferred > 0;
}

inline bool toolStateCompleteReload(GameplayContextV1* ctx,
                                    ToolInstanceStateV1& s,
                                    std::int32_t magazineSize)
{
    if (!toolStateFinishReload(s, magazineSize))
        return false;
    toolStateWrite(ctx, s.toolEntity, s);
    return true;
}

// Consume one use: returns false when the magazine is empty (a dry fire).
inline bool toolStateConsume(GameplayContextV1* ctx, ToolInstanceStateV1& s,
                             std::int32_t ammoCost)
{
    if (ammoCost <= 0)
        return true;
    if (s.currentAmmo < ammoCost)
        return false;
    s.currentAmmo -= ammoCost;
    s.stateVersion++;
    toolStateWrite(ctx, s.toolEntity, s);
    return true;
}
