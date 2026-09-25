// 09 24 2026
/* purpose
* Hot-editable NPC navigation/pathfinding logic (migration Phase 5g). This file
* is source for the replaceable game DLL only: edit the steering, obstacle,
* cover, stuck, and ground logic here (or add/remove whole files in
* src/hot-reload/packages/navigation/) and the running process picks it up on the
* next live generation. Cold keeps its original implementation as the fallback
* when no provider is registered.
* The only world access is the generic `queryWorldRay` context primitive plus
* the plain actor state in NpcNavigationV1; no World&, Npc&, STL, or engine
* object crosses the boundary.
* Does NOT own movement physics, combat, rendering, or the fixed-tick loop.
*/
#pragma once

#if defined(MIMITA_GAME_DLL)

#include <cmath>
#include <cstdint>

#include "hot-reload/game-api.h"

namespace HotNavigationPackage {

inline float random01(std::uint32_t& state)
{
    state = state * 1664525u + 1013904223u;
    return static_cast<float>((state >> 8) & 0x00ffffffu) /
           static_cast<float>(0x01000000u);
}

inline float length2(float x, float y)
{
    return std::sqrt(x * x + y * y);
}

inline bool rayHit(GameplayContextV1* ctx, const float origin[3],
                   const float dir[3], float maxDist, float* outNormal,
                   float* outDist)
{
    if (!ctx || !ctx->queryWorldRay)
        return false;
    return ctx->queryWorldRay(ctx->host, origin, dir, maxDist, nullptr,
                              outNormal, outDist);
}

// Obstacle probe: a wall within checkDist along the planar dir.
inline bool obstacle(GameplayContextV1* ctx, const NpcNavigationV1& r)
{
    const float len = length2(r.dir[0], r.dir[1]);
    if (len < 0.001f)
        return false;
    const float o[3] = {r.pos[0], r.pos[1], r.pos[2] + 0.5f};
    const float d[3] = {r.dir[0] / len, r.dir[1] / len, 0.0f};
    return rayHit(ctx, o, d, r.checkDist, nullptr, nullptr);
}

// Climbable wall: a mostly vertical wall within checkDist at chest height.
inline bool climbable(GameplayContextV1* ctx, NpcNavigationV1& r)
{
    const float len = length2(r.dir[0], r.dir[1]);
    if (len < 0.001f)
        return false;
    const float o[3] = {r.pos[0], r.pos[1], r.pos[2] + 1.1f};
    const float d[3] = {r.dir[0] / len, r.dir[1] / len, 0.0f};
    float normal[3] = {0.0f, 0.0f, 0.0f};
    float dist = 0.0f;
    if (!rayHit(ctx, o, d, r.checkDist, normal, &dist))
        return false;
    if (std::fabs(normal[2]) >= 0.3f || dist >= r.checkDist * 0.9f)
        return false;
    const float nl = length2(normal[0], normal[1]);
    if (nl < 0.0001f)
        return false;
    r.outNormal[0] = normal[0] / nl;
    r.outNormal[1] = normal[1] / nl;
    r.outNormal[2] = 0.0f;
    return true;
}

// Steer around a wall toward desiredDir by probing rotated alternatives.
inline void wallAvoid(GameplayContextV1* ctx, NpcNavigationV1& r)
{
    float dx = r.dir[0];
    float dy = r.dir[1];
    float len = length2(dx, dy);
    if (len < 0.001f) {
        r.outDir[0] = r.dir[0];
        r.outDir[1] = r.dir[1];
        r.outDir[2] = 0.0f;
        return;
    }
    dx /= len;
    dy /= len;
    const float o[3] = {r.pos[0], r.pos[1], r.pos[2] + 0.5f};
    const float checkDist = 1.5f;
    const float stepAngle = 3.14159265358979323846f / 6.0f;

    const float probe[3] = {dx, dy, 0.0f};
    if (!rayHit(ctx, o, probe, checkDist, nullptr, nullptr)) {
        r.outDir[0] = dx;
        r.outDir[1] = dy;
        r.outDir[2] = 0.0f;
        return;
    }
    for (int side = 0; side < 4; ++side) {
        const float angle = stepAngle * (side + 1);
        for (int s = 0; s < 2; ++s) {
            const float sign = (s == 0) ? 1.0f : -1.0f;
            const float c = std::cos(angle * sign);
            const float sn = std::sin(angle * sign);
            const float alt[3] = {dx * c - dy * sn, dx * sn + dy * c, 0.0f};
            if (!rayHit(ctx, o, alt, checkDist, nullptr, nullptr)) {
                r.outDir[0] = alt[0];
                r.outDir[1] = alt[1];
                r.outDir[2] = 0.0f;
                return;
            }
        }
    }
    const float perp[3] = {-dy, dx, 0.0f};
    if (!rayHit(ctx, o, perp, checkDist, nullptr, nullptr)) {
        r.outDir[0] = perp[0];
        r.outDir[1] = perp[1];
        r.outDir[2] = 0.0f;
        return;
    }
    r.outDir[0] = -perp[0];
    r.outDir[1] = -perp[1];
    r.outDir[2] = 0.0f;
}

// Highest floor below pos within searchDist (downward ray).
inline bool groundHeight(GameplayContextV1* ctx, NpcNavigationV1& r)
{
    const float o[3] = {r.pos[0], r.pos[1], r.pos[2] + 1.0f};
    const float d[3] = {0.0f, 0.0f, -1.0f};
    float dist = 0.0f;
    if (!rayHit(ctx, o, d, r.searchDist, nullptr, &dist))
        return false;
    r.outHeight = o[2] - dist;
    return true;
}

inline bool isStuck(const NpcNavigationV1& r)
{
    const float tryingMove = length2(r.lastMoveInput[0], r.lastMoveInput[1]);
    const float mx = r.pos[0] - r.prevPos[0];
    const float my = r.pos[1] - r.prevPos[1];
    const float mz = r.pos[2] - r.prevPos[2];
    const float moveSpeed = std::sqrt(mx * mx + my * my + mz * mz);
    return tryingMove > 0.1f && r.grounded != 0u && moveSpeed < 0.02f;
}

// Pick the most open of eight random directions (nearest wall maximised).
inline void unstuck(GameplayContextV1* ctx, NpcNavigationV1& r)
{
    const float o[3] = {r.pos[0], r.pos[1], r.pos[2] + 0.5f};
    float bestDir[3] = {1.0f, 0.0f, 0.0f};
    float bestDist = 0.0f;
    for (int i = 0; i < 8; ++i) {
        const float angle = random01(r.rngState) * 6.283185307179586f;
        const float test[3] = {std::cos(angle), std::sin(angle), 0.0f};
        float dist = 0.0f;
        const bool hit = rayHit(ctx, o, test, 10.0f, nullptr, &dist);
        const float closest = hit ? dist : 10.0f;
        if (closest > bestDist) {
            bestDist = closest;
            bestDir[0] = test[0];
            bestDir[1] = test[1];
            bestDir[2] = 0.0f;
        }
    }
    r.outDir[0] = bestDir[0];
    r.outDir[1] = bestDir[1];
    r.outDir[2] = bestDir[2];
    r.outRngState = r.rngState;
}

// Direction toward a position whose LOS to the threat is broken and walkable.
inline void cover(GameplayContextV1* ctx, NpcNavigationV1& r)
{
    r.outDir[0] = 0.0f;
    r.outDir[1] = 0.0f;
    r.outDir[2] = 0.0f;
    const float fx = r.threatPos[0] - r.pos[0];
    const float fy = r.threatPos[1] - r.pos[1];
    const float threatDist = length2(fx, fy);
    if (threatDist < 1.0f)
        return;
    const float tx = fx / threatDist;
    const float ty = fy / threatDist;
    const float testDirs[5][3] = {
        {-ty, tx, 0.0f},
        {ty, -tx, 0.0f},
        {-tx, -ty, 0.0f},
        {-ty * 0.7f - tx * 0.3f, tx * 0.7f - ty * 0.3f, 0.0f},
        {ty * 0.7f - tx * 0.3f, -tx * 0.7f - ty * 0.3f, 0.0f}};
    const float coverDist = 4.0f;
    for (int i = 0; i < 5; ++i) {
        const float dir[3] = {testDirs[i][0], testDirs[i][1], 0.0f};
        const float testPos[3] = {r.pos[0] + dir[0] * coverDist,
                                  r.pos[1] + dir[1] * coverDist,
                                  r.pos[2] + 0.8f};
        const float cx = r.threatPos[0] - testPos[0];
        const float cy = r.threatPos[1] - testPos[1];
        const float cz = r.threatPos[2] - testPos[2];
        const float coverToThreat = std::sqrt(cx * cx + cy * cy + cz * cz);
        if (coverToThreat < 1.0f)
            continue;
        const float toThreat[3] = {cx / coverToThreat, cy / coverToThreat,
                                   cz / coverToThreat};
        float hitDist = 0.0f;
        if (rayHit(ctx, testPos, toThreat, coverToThreat, nullptr, &hitDist) &&
            hitDist > 0.1f && hitDist < coverToThreat - 1.0f) {
            const float o[3] = {r.pos[0], r.pos[1], r.pos[2] + 0.5f};
            const float len = length2(dir[0], dir[1]);
            if (len > 0.0001f) {
                const float walkDir[3] = {dir[0] / len, dir[1] / len, 0.0f};
                if (!rayHit(ctx, o, walkDir, 1.5f, nullptr, nullptr)) {
                    r.outDir[0] = walkDir[0];
                    r.outDir[1] = walkDir[1];
                    r.outDir[2] = 0.0f;
                    return;
                }
            }
        }
    }
}

inline void MIMITA_GAME_CALL evaluate(void* host, NpcNavigationV1* request)
{
    if (!request)
        return;
    auto* ctx = static_cast<GameplayContextV1*>(host);
    NpcNavigationV1& r = *request;
    if (r.structSize == 0)
        r.structSize = sizeof(NpcNavigationV1);
    r.handled = 1u;
    r.result = 0u;
    r.hit = 0u;
    r.outDir[0] = r.outDir[1] = r.outDir[2] = 0.0f;
    r.outNormal[0] = r.outNormal[1] = r.outNormal[2] = 0.0f;
    r.outHeight = 0.0f;
    r.outRngState = r.rngState;

    if (!ctx || !ctx->queryWorldRay)
        return;

    switch (r.op) {
    case GAME_NPC_NAV_OBSTACLE:
        r.hit = obstacle(ctx, r) ? 1u : 0u;
        r.result = 1u;
        break;
    case GAME_NPC_NAV_CLIMBABLE:
        r.hit = climbable(ctx, r) ? 1u : 0u;
        r.result = 1u;
        break;
    case GAME_NPC_NAV_WALL_AVOID:
        wallAvoid(ctx, r);
        r.result = 1u;
        break;
    case GAME_NPC_NAV_GROUND_HEIGHT:
        r.hit = groundHeight(ctx, r) ? 1u : 0u;
        r.result = 1u;
        break;
    case GAME_NPC_NAV_IS_STUCK:
        r.hit = isStuck(r) ? 1u : 0u;
        r.result = 1u;
        break;
    case GAME_NPC_NAV_UNSTUCK:
        unstuck(ctx, r);
        r.result = 1u;
        break;
    case GAME_NPC_NAV_COVER:
        cover(ctx, r);
        r.result = 1u;
        break;
    default:
        break;
    }
}

} // namespace HotNavigationPackage

#endif // MIMITA_GAME_DLL
