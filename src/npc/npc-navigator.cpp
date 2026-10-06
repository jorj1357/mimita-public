// 09 10 2026
/* purpose
* Bounded rolling-horizon navigation for NPCs.
* Builds a small multi-level ground grid around the actor, paths to the goal
* with walk/jump/drop traversal rules (gated by the actor's MovementConfig),
* caches the route, and returns a steering direction + traversal requirements.
* Does NOT decide goals, apply combat, or generate physical input.
* Does NOT add a global graph/navmesh (maps here are far too large for this slice).
*/

#include "npc/npc-navigator.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>

#include "npc/npc.h"
#include "npc/npc-navigation.h"
#include "npc/npc-navigation-settings.h"
#include "npc/npc-movement-policy.h"
#include "npc/npc-nav-request.h"
#include "npc/npc-nav-graph.h"
#include "npc/npc-difficulty-config.h"
#include "npc/recast-navigation.h"
#include "config/movement-config.h"
#include "world/world.h"
#include "physics/physics-types.h"
#include "physics/movement/physics-collision.h"
#include "debug/debug-log.h"

namespace {

constexpr float kPlanRadius = 12.5f;   // legacy half-extent of the local plan window
constexpr float kCell = 2.5f;          // surface-grid cell size
constexpr float kUpProbe = 4.0f;       // probe start above the actor
constexpr float kProbeDepth = 12.0f;   // probe reach below the actor
constexpr float kStepUp = 0.65f;       // walk-up without jumping
constexpr float kReachXZ = 1.5f;       // waypoint arrival (planar)
// Waypoint arrival treats a waypoint as reached unless it is meaningfully ABOVE
// the actor (a stacked floor / jump target). Navigation nodes are floor surfaces
// (feet height) while body.pos is the capsule center ~1.8 m above the floor, so
// a symmetric Z tolerance made every walk waypoint unreachable and the actor
// orbited it forever.
constexpr float kReachAbove = 1.5f;    // max waypoint height above the actor
constexpr float kRepathInterval = 0.9f;
constexpr float kBlockedRetryInterval = 0.5f;  // no-route retry delay
constexpr float kGoalMoveThreshold = 2.5f;
constexpr int kMaxPlansPerSecond = 16;
constexpr int kMaxSurfacesPerColumn = 4;  // stacked floors/bridges per X/Y

bool recastCompareEnabled()
{
    const char* value = std::getenv("MIMITA_NPC_NAV_COMPARE");
    return value && (std::string(value) == "1" || std::string(value) == "true");
}

// Explicit navigation backend selection. Never inferred from gamemode. The
// default is custom; compare keeps the custom route authoritative while
// Recast is observed; recast promotes the Recast/Detour route, with the custom
// planner retained as a bounded fallback.
enum class RecastBackend { Custom, Compare, Recast };

RecastBackend recastBackendMode()
{
    const char* value = std::getenv("MIMITA_NPC_NAV_BACKEND");
    if (value) {
        const std::string v(value);
        if (v == "recast") return RecastBackend::Recast;
        if (v == "compare") return RecastBackend::Compare;
        if (v == "custom") return RecastBackend::Custom;
    }
    return recastCompareEnabled() ? RecastBackend::Compare : RecastBackend::Custom;
}
constexpr float kWalkableNormalZ = NpcNavigation::kWalkableSlopeDot;

// One standable surface node in the flattened A* graph. Multiple nodes may
// share a cell (stacked floors); a node connects only to adjacent cells.
struct PlanSurfaceNode
{
    NavigationSurface surface;
    int gx = 0;
    int gy = 0;
    float g = 1e18f;
    float f = 1e18f;
    int parent = -1;
    bool closed = false;
    bool gap = false;   // edge from parent crosses a gap (needs a dash)
    NavCapability capability = NavCapability::Walk;
};

const char* goalKindName(NpcGoalKind kind)
{
    switch (kind) {
        case NpcGoalKind::ReachPosition:     return "reach";
        case NpcGoalKind::FollowActor:       return "pursue";
        case NpcGoalKind::MaintainDistance:  return "maintain";
        case NpcGoalKind::FleeActor:         return "flee";
        case NpcGoalKind::ReachLineOfSight:  return "los";
        case NpcGoalKind::Explore:           return "explore";
        case NpcGoalKind::None:              return "none";
    }
    return "none";
}

float horizontalDistance(const glm::vec3& a, const glm::vec3& b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

// Gather every walkable surface in the vertical column at (x,y). Unlike a
// single highest-floor probe this keeps stacked floors so multi-level spaces
// path correctly. A surface is dropped when its headroom is below the actor
// height (a low ceiling, or a floor stacked too close above it).
void gatherColumnSurfaces(const World& world, const std::vector<int>& candidates,
                          float x, float y, float fromZ, float depth,
                          float walkableDot, float npcHeight,
                          std::vector<NavigationSurface>& out)
{
    out.clear();
    const glm::vec3 origin(x, y, fromZ);
    const glm::vec3 down(0.0f, 0.0f, -1.0f);
    const auto& tris = world.collisionMesh.triangles;
    for (int ti : candidates) {
        if (ti < 0 || ti >= (int)tris.size()) continue;
        const CollisionTriangle& tri = tris[ti];
        if (tri.normal.z < walkableDot) continue;
        float t = 0.0f;
        if (!NpcNavigation::rayTriangle(origin, down, tri, depth, t)) continue;
        const float z = fromZ - t;
        bool duplicate = false;
        for (const auto& s : out)
            if (std::fabs(s.height - z) <= 0.05f) { duplicate = true; break; }
        if (duplicate) continue;
        NavigationSurface s;
        s.position = glm::vec3(x, y, z);
        s.normal = tri.normal;
        s.height = z;
        s.kind = classifySurface(tri.normal, walkableDot);
        s.walkable = true;
        out.push_back(s);
    }
    std::sort(out.begin(), out.end(),
              [](const NavigationSurface& a, const NavigationSurface& b) {
                  return a.height > b.height;
              });
    if ((int)out.size() > kMaxSurfacesPerColumn) out.resize(kMaxSurfacesPerColumn);

    const glm::vec3 up(0.0f, 0.0f, 1.0f);
    out.erase(std::remove_if(out.begin(), out.end(), [&](const NavigationSurface& s) {
        const glm::vec3 o(s.position.x, s.position.y, s.height + 0.1f);
        for (int ti : candidates) {
            if (ti < 0 || ti >= (int)tris.size()) continue;
            float t = 0.0f;
            if (NpcNavigation::rayTriangle(o, up, tris[ti], npcHeight, t))
                return true;
        }
        return false;
    }), out.end());
}

// Horizontal clearance from A to B at height `z`. Walkable faces are terrain,
// not obstacles; only steep faces block. Three offset rays approximate the
// actor's width so a narrow slot between cells is rejected.
bool segmentClear(const World& world, const std::vector<int>& candidates,
                  const glm::vec3& a, const glm::vec3& b, float z,
                  float walkableDot, float radius)
{
    glm::vec3 dir(b.x - a.x, b.y - a.y, 0.0f);
    const float dist = glm::length(dir);
    if (dist < 0.001f) return true;
    dir /= dist;
    const glm::vec3 perp(-dir.y, dir.x, 0.0f);
    const float offsets[3] = {0.0f, radius * 0.7f, -radius * 0.7f};
    const auto& tris = world.collisionMesh.triangles;
    for (float off : offsets) {
        const glm::vec3 o = glm::vec3(a.x, a.y, z) + perp * off;
        for (int ti : candidates) {
            if (ti < 0 || ti >= (int)tris.size()) continue;
            const CollisionTriangle& tri = tris[ti];
            if (tri.normal.z >= walkableDot) continue;
            float t = 0.0f;
            if (NpcNavigation::rayTriangle(o, dir, tri, dist + 0.2f, t))
                return false;
        }
    }
    return true;
}

bool consumePlanToken()
{
    using clock = std::chrono::steady_clock;
    static clock::time_point windowStart = clock::now();
    static int used = 0;
    const auto now = clock::now();
    if (std::chrono::duration<double>(now - windowStart).count() >= 1.0) {
        windowStart = now;
        used = 0;
    }
    if (used >= kMaxPlansPerSecond)
        return false;
    ++used;
    return true;
}

// Pick the nearby direction with the most usable room when the direct line is
// blocked, so a policy actor turns instead of pushing into the wall. The scorer
// measures openness at several distances and uses continuity with the requested
// direction as a tiebreak, so it no longer deterministically picks "left".
glm::vec3 bestTurnDirection(const Npc& npc, glm::vec3 dir, const World& world)
{
    glm::vec3 d(dir.x, dir.y, 0.0f);
    if (glm::length(d) < 0.001f) return glm::vec3(0.0f);
    d = glm::normalize(d);
    const glm::vec3 left(-d.y, d.x, 0.0f);
    const glm::vec3 right = -left;
    const glm::vec3 back = -d;
    // The "turn" contract for a blocked direct route must not retain a forward
    // component into the blocking face, so only lateral and backward candidates
    // are considered (no forward diagonals).
    const glm::vec3 candidates[] = {
        left, right,
        glm::normalize(left + back), glm::normalize(right + back), // back diagonals
        back
    };
    auto openness = [&](const glm::vec3& probe) -> float {
        if (glm::length(probe) < 0.001f) return -1.0f;
        float best = 0.0f;
        for (float dist : {1.5f, 3.0f, 5.0f, 8.0f}) {
            if (NpcNavigation::obstacleInDirection(npc, probe, dist, world))
                break;
            best = dist;
        }
        return best;
    };
    glm::vec3 bestDir{0.0f};
    float bestScore = -1e9f;
    const float sidePref = (npc.id % 2u == 0u) ? 1.0f : -1.0f;
    for (const glm::vec3& c : candidates) {
        if (glm::length(c) < 0.001f) continue;
        const float open = openness(c);
        if (open <= 0.0f) continue;
        const glm::vec3 cn = glm::normalize(c);
        const float continuity = glm::dot(cn, d);
        // Break the left/right symmetry per actor so squad members pick
        // different sides of a corner.
        const float sideBias = sidePref * glm::dot(cn, left) *
            NpcDifficultyConfig::instance().settings().turnSideBias;
        const float score = open * 2.0f + continuity + sideBias;
        if (score > bestScore) { bestScore = score; bestDir = cn; }
    }
    return bestDir;
}

// Surface A* over the local window. Each cell may hold several stacked
// surfaces; neighbors connect only across adjacent cells and only when the
// space between them is clear, so a wall never becomes a straight route.
// Returns false when no route exists.
bool planLocalPath(const Npc& npc, const glm::vec3& goalPos, const World& world,
                   const MovementConfig& movement, const NpcNavigationSettings& settings,
                   const NpcMovementPolicy* policy,
                   std::vector<glm::vec3>& outPoints, std::vector<uint8_t>& outGaps,
                   std::vector<uint8_t>& outCaps)
{
    const float c = kCell;
    const float halfExtent = std::clamp(settings.searchRadius, 6.0f, 20.0f);
    const int half = std::max(2, (int)std::floor(halfExtent / c));
    const int N = 2 * half + 1;
    const float ox = std::floor(npc.body.pos.x / c) * c;
    const float oy = std::floor(npc.body.pos.y / c) * c;

    const float walkableDot = settings.maxWalkableSlopeDot > 0.0f
        ? settings.maxWalkableSlopeDot : kWalkableNormalZ;
    const float stepUp = settings.maxStepHeight;
    const float jump = npcMaxJumpHeight(movement);
    const bool navJumps = settings.allowNavigationJumps &&
        (policy == nullptr || npcPolicyAllowsJump(*policy, NpcJumpReason::Navigation));
    MovementCapabilities caps;

    AABB win;
    win.min = glm::vec3(ox - half * c - c, oy - half * c - c,
                        npc.body.pos.z - kProbeDepth);
    win.max = glm::vec3(ox + half * c + c, oy + half * c + c,
                        npc.body.pos.z + kUpProbe);
    static thread_local std::vector<int> candidates;
    candidates.clear();
    appendChunkTrianglesForAABB(world, win, 0.0f, candidates, "npcNavPlan");

    const Capsule cap = npc.body.getCapsule();
    const float npcRadius = std::max(0.1f, cap.r);
    const float npcHeight = std::max(1.0f, (cap.b.z - cap.a.z) + 2.0f * cap.r);

    std::vector<PlanSurfaceNode> nodes;
    std::vector<std::vector<int>> cellNodes((size_t)N * N);
    std::vector<NavigationSurface> column;
    const float fromZ = npc.body.pos.z + kUpProbe;
    const float depth = kProbeDepth + kUpProbe;
    for (int gy = 0; gy < N; ++gy) {
        for (int gx = 0; gx < N; ++gx) {
            const float x = ox + (gx - half) * c;
            const float y = oy + (gy - half) * c;
            gatherColumnSurfaces(world, candidates, x, y, fromZ, depth,
                                 walkableDot, npcHeight, column);
            const int cell = gy * N + gx;
            for (const NavigationSurface& s : column) {
                PlanSurfaceNode node;
                node.surface = s;
                node.gx = gx;
                node.gy = gy;
                cellNodes[cell].push_back((int)nodes.size());
                nodes.push_back(node);
            }
        }
    }

    auto nearestSurface = [&](const glm::vec3& p) {
        int best = -1;
        float bestD = 1e18f;
        for (int i = 0; i < (int)nodes.size(); ++i) {
            const glm::vec3 d = nodes[i].surface.position - p;
            const float dd = glm::dot(d, d);
            if (dd < bestD) { bestD = dd; best = i; }
        }
        return best;
    };

    const int start = nearestSurface(npc.body.pos);
    const int goalNode = nearestSurface(goalPos);
    if (start < 0 || goalNode < 0)
        return false;

    auto hCost = [&](int i) {
        return horizontalDistance(nodes[i].surface.position, goalPos);
    };

    std::vector<int> open;
    open.push_back(start);

    const float walkTan = std::sqrt(std::max(0.0f, 1.0f - walkableDot * walkableDot)) /
                          std::max(0.05f, walkableDot);

    auto tryConnect = [&](int from, int to, bool gap, float planar, float extraCost) {
        if (from < 0 || to < 0 || from == to || nodes[to].closed) return;
        const NavigationSurface& a = nodes[from].surface;
        const NavigationSurface& b = nodes[to].surface;
        const float dz = b.height - a.height;
        if (dz > jump) return;                                   // unreachable rise
        if (dz < -0.05f && !caps.canDrop) return;                // no drop capability
        // A continuous walkable ramp must be walked even when the per-cell rise
        // exceeds the step height; only a sharp step/wall needs a jump. Without
        // this a jump_style:"never" actor could not use a ramp at all.
        const float slopeTan = planar > 0.01f ? std::fabs(dz) / planar : 1e9f;
        const bool rampWalk = (a.kind == NavSurfaceKind::Ramp || b.kind == NavSurfaceKind::Ramp) &&
                              slopeTan <= walkTan;
        const bool needJump = (dz > stepUp) && !rampWalk;
        if (needJump && (!navJumps || !caps.canJump)) return;    // policy/skill gate

        if (gap) {
            // A dash over a void is rejected only by a center-line obstacle, so
            // side geometry the capsule can legitimately pass between is kept.
            const float maxZ = std::max(a.height, b.height);
            if (!segmentClear(world, candidates, a.position, b.position,
                              maxZ + 0.6f, walkableDot, 0.0f)) return;
        } else {
            const float baseZ = std::max(a.height, b.height) + 0.15f;
            if (!segmentClear(world, candidates, a.position, b.position,
                              baseZ, walkableDot, npcRadius)) return;
            // A jump link must have open space at the landing height so the
            // actor never launches straight into a vertical wall face.
            if (needJump && !settings.allowWallJump) {
                if (!segmentClear(world, candidates, a.position, b.position,
                                  b.height + 0.25f, walkableDot, npcRadius)) return;
            }
        }

        const NavCapability capKind = needJump ? NavCapability::Jump
            : (dz < -0.05f ? NavCapability::Drop : NavCapability::Walk);
        const float ng = nodes[from].g + extraCost + (needJump ? 0.8f : 0.0f);
        if (ng < nodes[to].g) {
            nodes[to].g = ng;
            nodes[to].parent = from;
            nodes[to].f = ng + hCost(to);
            nodes[to].gap = gap;
            nodes[to].capability = capKind;
            open.push_back(to);
        }
    };

    nodes[start].g = 0.0f;
    nodes[start].f = hCost(start);

    static const int DX[8] = {1,-1,0,0, 1,1,-1,-1};
    static const int DY[8] = {0,0,1,-1, 1,-1,1,-1};

    while (!open.empty()) {
        int bi = 0;
        for (int i = 1; i < (int)open.size(); ++i)
            if (nodes[open[i]].f < nodes[open[bi]].f) bi = i;
        const int cur = open[bi];
        open.erase(open.begin() + bi);
        if (nodes[cur].closed) continue;
        nodes[cur].closed = true;
        if (cur == goalNode) break;

        const int cx = nodes[cur].gx, cy = nodes[cur].gy;
        for (int d = 0; d < 8; ++d) {
            const int nx = cx + DX[d], ny = cy + DY[d];
            if (nx < 0 || ny < 0 || nx >= N || ny >= N) continue;
            const int ncell = ny * N + nx;
            if (cellNodes[ncell].empty()) continue;
            // Do not cut diagonal corners through empty cells.
            if (DX[d] != 0 && DY[d] != 0) {
                if (cellNodes[cy * N + nx].empty() || cellNodes[ny * N + cx].empty())
                    continue;
            }
            const float planar = (DX[d] != 0 && DY[d] != 0) ? c * 1.4142f : c;
            for (int nb : cellNodes[ncell])
                tryConnect(cur, nb, false, planar, planar);
        }

        // Gap edges: a dash crosses one empty cell to a surface two cells away.
        if (movement.dashEnabled && jump > 0.0f) {
            static const int GX[4] = {1, -1, 0, 0};
            static const int GY[4] = {0, 0, 1, -1};
            for (int gd = 0; gd < 4; ++gd) {
                const int mx = cx + GX[gd], my = cy + GY[gd];
                const int fx = cx + 2 * GX[gd], fy = cy + 2 * GY[gd];
                if (fx < 0 || fy < 0 || fx >= N || fy >= N) continue;
                if (!cellNodes[my * N + mx].empty() || cellNodes[fy * N + fx].empty())
                    continue;
                for (int nb : cellNodes[fy * N + fx])
                    tryConnect(cur, nb, true, 2.0f * c, 2.0f * c + 2.5f);
            }
        }
    }

    if (nodes[goalNode].g >= 1e17f)
        return false;

    outPoints.clear();
    outGaps.clear();
    outCaps.clear();
    if (start == goalNode) {
        outPoints.push_back(goalPos);
        outGaps.push_back(0);
        outCaps.push_back((uint8_t)NavCapability::Walk);
        return true;
    }
    std::vector<int> idx;
    for (int cur = goalNode; cur != -1; cur = nodes[cur].parent)
        idx.push_back(cur);
    std::reverse(idx.begin(), idx.end());
    for (size_t i = 1; i < idx.size(); ++i) {
        const PlanSurfaceNode& n = nodes[idx[i]];
        outPoints.push_back(n.surface.position);
        outGaps.push_back(n.gap ? 1 : 0);
        outCaps.push_back((uint8_t)n.capability);
    }
    if (outPoints.empty()) {
        outPoints.push_back(goalPos);
        outGaps.push_back(0);
        outCaps.push_back((uint8_t)NavCapability::Walk);
    }
    return true;
}

} // anonymous namespace

float npcMaxJumpHeight(const MovementConfig& m)
{
    const float g = std::fabs(m.gravityZ);
    if (m.jumpVerticalSpeed <= 0.0f || g <= 0.01f)
        return 0.0f;
    return (m.jumpVerticalSpeed * m.jumpVerticalSpeed) / (2.0f * g);
}

void NpcNavigator::pushVisited(const glm::vec3& pos, float now)
{
    recentVisited[recentVisitedHead] = {pos, now};
    recentVisitedHead = (recentVisitedHead + 1) % kRecentMax;
    if (recentVisitedCount < kRecentMax)
        ++recentVisitedCount;
}

void NpcNavigator::pushBlocked(const glm::vec3& pos, float now)
{
    recentBlocked[recentBlockedHead] = {pos, now};
    recentBlockedHead = (recentBlockedHead + 1) % kRecentMax;
    if (recentBlockedCount < kRecentMax)
        ++recentBlockedCount;
}

glm::vec3 NpcNavigator::resolveExploreTarget(const Npc& npc, const glm::vec3& hintPoint,
                                             const MovementCommitmentSettings& settings,
                                             float dt)
{
    const glm::vec3 toHint(hintPoint.x - npc.body.pos.x, hintPoint.y - npc.body.pos.y, 0.0f);
    const float hintLen = glm::length(toHint);
    glm::vec3 wantDir = hintLen > 0.001f ? toHint / hintLen : glm::vec3(0.0f);
    if (glm::length(wantDir) < 0.001f) {
        wantDir = glm::length(glm::vec3(npc.currentFacing.x, npc.currentFacing.y, 0.0f)) > 0.001f
            ? glm::normalize(glm::vec3(npc.currentFacing.x, npc.currentFacing.y, 0.0f))
            : glm::vec3(1.0f, 0.0f, 0.0f);
    }
    const float travel = std::max(15.0f, settings.travelTargetDistanceMeters);
    const float reached = std::max(2.0f, settings.travelTargetReachedMeters);
    const float hold = std::max(2.0f, settings.travelTargetHoldSeconds);
    const float minProgress = std::max(0.5f, settings.travelTargetMinProgressMeters);

    bool valid = hasTravelTarget;
    if (valid) {
        const float remaining = horizontalDistance(npc.body.pos, travelTarget);
        travelTargetTimer += dt;
        const float progress = travelTargetStartDistance - remaining;
        const bool reachedNow = remaining <= reached;
        // Retarget only when reached or genuinely stalled. A heading reversal is
        // deliberately ignored: reacting to it flipped the target 180 degrees and
        // made the actor oscillate.
        const bool stalled = travelTargetTimer >= hold && progress < minProgress;
        if (reachedNow || stalled)
            valid = false;
    }
    if (!valid) {
        travelTarget = npc.body.pos + wantDir * travel;
        hasTravelTarget = true;
        travelTargetTimer = 0.0f;
        travelTargetStartDistance = travel;
        travelTargetChangedThisUpdate = true;
    }
    return travelTarget;
}

glm::vec3 NpcNavigator::updateAreaEscape(const Npc& npc, const World& world,
                                         const std::vector<int>& candidates,
                                         const MovementCommitmentSettings& settings,
                                         float dt, float now, float memorySeconds,
                                         bool targetVisible)
{
    areaEscapeStartedThisUpdate = false;

    // A visible enemy owns movement; never fight combat steering.
    if (targetVisible) {
        areaEscapeActive = false;
        areaEscapeTimeRemaining = 0.0f;
        return glm::vec3(0.0f);
    }

    const float radius = std::max(1.0f, settings.areaEscapeRadiusMeters);
    if (areaEscapeActive) {
        areaEscapeTimeRemaining -= dt;
        if (areaEscapeTimeRemaining > 0.0f)
            return areaEscapeDirection;
        areaEscapeActive = false;
        areaAnchor = npc.body.pos;  // re-anchor so it does not retrigger at once
        areaTimer = 0.0f;
        return glm::vec3(0.0f);
    }

    // Accumulate time spent inside the anchor radius ONLY while the actor is
    // actively trying to move. An actor that has deliberately stopped (arrived
    // at its objective, holding, waiting) is not trapped and must not be pushed
    // away. A passing actor leaves the radius and resets; an oscillating actor
    // keeps trying to move and stays inside, so the timer grows.
    const bool tryingToMove = glm::length(npc.lastMoveInput) > 0.1f;
    if (!tryingToMove || horizontalDistance(npc.body.pos, areaAnchor) > radius) {
        areaAnchor = npc.body.pos;
        areaTimer = 0.0f;
    } else {
        areaTimer += dt;
    }
    if (areaTimer < std::max(1.0f, settings.areaEscapeSeconds))
        return glm::vec3(0.0f);

    // Breakout. Pick the open direction that maximizes distance from the trap
    // (away from the anchor) and, secondarily, heads toward the goal, so net
    // movement is maximized instead of pacing.
    glm::vec3 away = npc.body.pos - areaAnchor;
    away.z = 0.0f;
    if (glm::length(away) < 0.5f) {
        const glm::vec3 facing(npc.currentFacing.x, npc.currentFacing.y, 0.0f);
        away = glm::length(facing) > 0.001f ? glm::normalize(facing)
                                            : glm::vec3(1.0f, 0.0f, 0.0f);
    } else {
        away = glm::normalize(away);
    }
    glm::vec3 toGoal(0.0f);
    if (hasLastGoal) {
        toGoal = lastGoal - npc.body.pos;
        toGoal.z = 0.0f;
        if (glm::length(toGoal) > 0.001f) toGoal = glm::normalize(toGoal);
    }

    const float probeBlock = std::max(
        2.0f, NpcDifficultyConfig::instance().settings().wallCastDistance);
    const float maxDist = std::max(6.0f, settings.candidateDistanceMeters);
    constexpr int SAMPLES = 16;
    glm::vec3 best = away;
    float bestScore = -1e9f;
    for (int i = 0; i < SAMPLES; ++i) {
        const float ang = (float)i * (6.2831853f / (float)SAMPLES);
        const glm::vec3 dir(std::cos(ang), std::sin(ang), 0.0f);
        if (NpcNavigation::obstacleInDirection(npc, dir, probeBlock, world, candidates))
            continue;
        float open = 0.0f;
        for (float d = 2.0f; d <= maxDist; d += 2.0f) {
            if (NpcNavigation::obstacleInDirection(npc, dir, d, world, candidates))
                break;
            open = d;
        }
        const float score = open
                          + glm::dot(dir, away) * 6.0f
                          + (glm::length(toGoal) > 0.001f ? glm::dot(dir, toGoal) * 3.0f : 0.0f);
        if (score > bestScore) {
            bestScore = score;
            best = dir;
        }
    }

    areaEscapeDirection = best;
    areaEscapeActive = true;
    areaEscapeTimeRemaining = std::max(0.5f, settings.areaEscapeHoldSeconds);
    areaEscapeStartedThisUpdate = true;
    // Cancel the short-term flip-flop so the breakout is not immediately undone.
    clearLocalCorrection();
    backtrackActive = false;
    (void)now;
    (void)memorySeconds;
    return areaEscapeDirection;
}

float NpcNavigator::nearestRecentDistance(const glm::vec3& point, float now,
                                          float memorySeconds) const
{
    float best = 1e9f;
    for (int i = 0; i < recentVisitedCount; ++i) {
        const SearchPoint& s = recentVisited[i];
        if (now - s.time > memorySeconds) continue;
        best = std::min(best, horizontalDistance(point, s.pos));
    }
    for (int i = 0; i < recentBlockedCount; ++i) {
        const SearchPoint& s = recentBlocked[i];
        if (now - s.time > memorySeconds) continue;
        best = std::min(best, horizontalDistance(point, s.pos));
    }
    return best;
}

glm::vec3 NpcNavigator::chooseBestOpenDirection(
    const Npc& npc, const glm::vec3& forwardDir, const glm::vec3& targetDir,
    const World& world, const std::vector<int>& candidates,
    const MovementCommitmentSettings& settings, float now, float memorySeconds) const
{
    glm::vec3 fwd(forwardDir.x, forwardDir.y, 0.0f);
    if (glm::length(fwd) > 0.001f) fwd = glm::normalize(fwd);
    glm::vec3 tgt(targetDir.x, targetDir.y, 0.0f);
    const bool haveTarget = glm::length(tgt) > 0.001f;
    if (haveTarget) tgt = glm::normalize(tgt);

    const float probeBlock = std::max(
        2.0f, NpcDifficultyConfig::instance().settings().wallCastDistance);
    const float maxDist = std::max(2.0f, settings.candidateDistanceMeters);

    constexpr int SAMPLES = 16;
    // Per-actor tiebreak rotation instead of rotating the whole sample grid
    // (rotating the grid biased the chosen direction toward a diagonal and
    // degraded route following). This only decides between otherwise-equal
    // candidates, so a squad does not share one preferred direction while the
    // geometry-based score still dominates.
    const int tiebreakPhase = (int)(npc.id % (uint32_t)SAMPLES);
    glm::vec3 best{0.0f};
    float bestScore = -std::numeric_limits<float>::max();
    for (int i = 0; i < SAMPLES; ++i) {
        const float ang = (float)i * (6.2831853f / (float)SAMPLES);
        const glm::vec3 dir(std::cos(ang), std::sin(ang), 0.0f);
        // Never choose a direction that is immediately blocked.
        if (NpcNavigation::obstacleInDirection(npc, dir, probeBlock, world, candidates))
            continue;

        // Usable open distance: step outward until the ray hits a wall.
        float openDist = 0.0f;
        for (float d = 2.0f; d <= maxDist; d += 2.0f) {
            if (NpcNavigation::obstacleInDirection(npc, dir, d, world, candidates))
                break;
            openDist = d;
        }

        const float forwardDot = glm::dot(dir, fwd);
        const float targetDot = haveTarget ? glm::dot(dir, tgt) : 0.0f;
        float score = openDist * settings.openDistanceBias
                    + forwardDot * settings.forwardBias
                    + targetDot * settings.targetProgressBias;
        if (!settings.allowReverse && forwardDot < -0.25f)
            score -= settings.reversePenalty;
        if (settings.avoidRecentPath) {
            const glm::vec3 look = npc.body.pos + dir * std::min(maxDist, 5.0f);
            const float memDist = nearestRecentDistance(look, now, memorySeconds);
            if (memDist < settings.recentPathAvoidRadius)
                score -= (settings.recentPathAvoidRadius - memDist) * 4.0f;
        }
        // Per-actor tiebreak (rotated by actor id) instead of a fixed +x bias.
        score += (float)((i + tiebreakPhase) % SAMPLES) * 1e-3f;
        if (score > bestScore) {
            bestScore = score;
            best = dir;
        }
    }
    return best;
}

NpcCommitmentUpdate NpcNavigator::updateCommitment(
    Npc& npc, const glm::vec3& forwardDir, const glm::vec3& targetDir,
    bool targetVisible, const World& world, const std::vector<int>& candidates,
    const MovementCommitmentSettings& settings, float dt, float now,
    float memorySeconds)
{
    NpcCommitmentUpdate out;
    commitmentProgressFailed = false;
    commitmentBlocked = false;
    if (!settings.enabled) {
        commitmentActive = false;
        return out;
    }

    // A genuinely visible enemy (and a profile that allows it) owns steering
    // with combat movement. The committed direction is preserved so pursuit
    // can resume when the enemy is lost; it is never replaced while visible.
    if (targetVisible && settings.visibleEnemyAllowsCombatMovement)
    {
        clearLocalCorrection();
        return out;
    }

    // Local wall/stuck steering is an interrupt, not a new destination. Keep
    // the existing commitment intact while the temporary correction runs.
    if (localCorrectionActive)
    {
        localCorrectionTimeRemaining -= dt;
        if (localCorrectionTimeRemaining > 0.0f)
            return out;
        clearLocalCorrection();
    }

    const float probe = std::max(
        2.0f, NpcDifficultyConfig::instance().settings().wallCastDistance);
    if (commitmentActive) {
        commitmentTimeRemaining -= dt;
        progressTimer += dt;
    }

    const bool blocked = commitmentActive &&
        NpcNavigation::obstacleInDirection(npc, committedDirection, probe, world, candidates);
    commitmentBlocked = blocked;
    lastProgressDistance = commitmentActive
        ? horizontalDistance(npc.body.pos, commitmentStartPosition) : 0.0f;
    const bool progressed = lastProgressDistance >= settings.minimumProgressMeters;
    const bool progressFailed = commitmentActive &&
        progressTimer >= settings.progressCheckSeconds && !progressed;
    commitmentProgressFailed = progressFailed;
    out.progressDistance = lastProgressDistance;

    // Keep the committed direction while the actor is still making progress and
    // its hold has not expired, EVEN IF a wall is sensed ahead: the local wall
    // avoidance/local-correction layer steers around it. Replacing the direction
    // the instant a wall came within the probe distance made the actor change
    // heading every tick and oscillate.
    if (commitmentActive && !progressFailed && commitmentTimeRemaining > 0.0f) {
        out.direction = committedDirection;
        return out;
    }

    const bool wasActive = commitmentActive;
    out.previousDirection = committedDirection;
    const glm::vec3 best = chooseBestOpenDirection(
        npc, forwardDir, targetDir, world, candidates, settings, now, memorySeconds);
    // Emit the block edge only when we actually replace/abandon a blocked
    // direction, not on every tick it stays blocked.
    out.blocked = blocked;
    out.progressFailed = progressFailed;
    if (glm::length(best) > 0.001f) {
        committedDirection = best;
        commitmentActive = true;
        commitmentTimeRemaining = settings.directionCommitSeconds;
        commitmentStartPosition = npc.body.pos;
        progressTimer = 0.0f;
        out.direction = best;
        if (wasActive) out.replaced = true;
        else out.created = true;
    } else {
        commitmentActive = false;
        out.blocked = true;  // boxed in: no open direction at all
    }
    return out;
}

void NpcNavigator::reset()
{
    path.clear();
    pathGap.clear();
    pathCapability.clear();
    pathIndex = 0;
    repathTimer = 0.0f;
    hasLastGoal = false;
    goal = NpcGoal{};
    backtrackActive = false;
    backtrackDirection = glm::vec3(0.0f);
    backtrackRemaining = 0.0f;
    backtrackTimeRemaining = 0.0f;
    clearLocalCorrection();
    travelTarget = glm::vec3(0.0f);
    hasTravelTarget = false;
    areaAnchor = glm::vec3(0.0f);
    areaTimer = 0.0f;
    areaEscapeActive = false;
    areaEscapeTimeRemaining = 0.0f;
    areaEscapeDirection = glm::vec3(0.0f);
    areaEscapeStartedThisUpdate = false;
    commitmentActive = false;
    committedDirection = glm::vec3(0.0f);
    commitmentTimeRemaining = 0.0f;
    progressTimer = 0.0f;
    lastProgressDistance = 0.0f;
    commitmentStartPosition = glm::vec3(0.0f);
    commitmentProgressFailed = false;
    commitmentBlocked = false;
    recentVisitedCount = 0;
    recentVisitedHead = 0;
    recentBlockedCount = 0;
    recentBlockedHead = 0;
}

void NpcNavigator::startBacktrack(const glm::vec3& blockedDirection,
                                  float distance, float duration)
{
    glm::vec3 planar = glm::vec3(blockedDirection.x, blockedDirection.y, 0.0f);
    const float len = glm::length(planar);
    if (len < 0.001f || distance <= 0.0f || duration <= 0.0f)
        return;

    backtrackDirection = -planar / len;
    backtrackRemaining = distance;
    backtrackTimeRemaining = duration;
    backtrackActive = true;
    path.clear();
    pathGap.clear();
    pathCapability.clear();
    pathIndex = 0;
    requestRepath();
}

void NpcNavigator::startLocalCorrection(const glm::vec3& direction,
                                         float duration)
{
    glm::vec3 planar(direction.x, direction.y, 0.0f);
    const float len = glm::length(planar);
    if (len < 0.001f || duration <= 0.0f)
        return;

    const glm::vec3 newDir = planar / len;
    // Hysteresis: while a correction is fresh, do not flip it to a very
    // different direction. In a 90-degree corner the wall-avoid layer can
    // recompute an opposite side every tick; holding the current correction for
    // ~half its duration stops the rapid back-and-forth.
    if (localCorrectionActive && localCorrectionTimeRemaining > duration * 0.5f &&
        glm::dot(localCorrectionDirection, newDir) < 0.5f) {
        return;
    }

    localCorrectionDirection = newDir;
    localCorrectionTimeRemaining = duration;
    localCorrectionActive = true;
    // The original commitment is deliberately not invalidated. The correction
    // is only allowed to steer around the obstacle temporarily.
    commitmentBlocked = false;
    commitmentProgressFailed = false;
}

void NpcNavigator::clearLocalCorrection()
{
    localCorrectionActive = false;
    localCorrectionDirection = glm::vec3(0.0f);
    localCorrectionTimeRemaining = 0.0f;
}

NpcNavResult NpcNavigator::update(Npc& npc, const NpcGoal& newGoal, const World& world,
                                  const MovementConfig* movement, float dt,
                                  const NpcNavigationSettings* settings,
                                  const NpcMovementPolicy* policy,
                                  const MovementCommitmentSettings* commitment)
{
    NpcNavResult result;
    goal = newGoal;
    if (!goal.valid()) {
        path.clear();
        pathGap.clear();
        pathCapability.clear();
        pathIndex = 0;
        return result;
    }

    // Effective settings: a preset block overrides the shared defaults; without
    // one, the navigator's legacy window is used. The caller decides whether
    // the actor's optional preset block is supplied, but all executors still
    // arrive here through this one navigator.
    NpcNavigationSettings eff;
    if (settings) {
        eff = *settings;
    } else {
        eff.configured = false;
        eff.searchRadius = kPlanRadius;
    }
    const bool wantPlanning =
        eff.mode == "automatic_surface_path" && eff.automaticFromCollision;

    // ── Resolve the abstract goal to a destination point ────────────
    glm::vec3 dest{0.0f};
    bool haveDest = false;
    if (goal.kind == NpcGoalKind::Explore) {
        // A persistent far target, held until reached or the heading reverses.
        // This replaces the old body-relative patrol waypoint that changed every
        // tick and forced a target_moved replan storm.
        const MovementCommitmentSettings s = commitment
            ? *commitment : MovementCommitmentSettings{};
        travelTargetChangedThisUpdate = false;
        dest = resolveExploreTarget(npc, goal.targetPos, s, dt);
        result.travelTargetChanged = travelTargetChangedThisUpdate;
        result.travelTargetOut = travelTarget;
        haveDest = true;
    } else if (goal.kind == NpcGoalKind::ReachPosition) {
        dest = goal.targetPos;
        haveDest = true;
    } else if (npc.sensors.hasTarget) {
        glm::vec3 toT(npc.sensors.targetPos.x - npc.body.pos.x,
                      npc.sensors.targetPos.y - npc.body.pos.y, 0.0f);
        const float d = glm::length(toT);
        const glm::vec3 dir = d > 0.001f ? toT / d : glm::vec3(1.0f, 0.0f, 0.0f);
        switch (goal.kind) {
            case NpcGoalKind::FollowActor:
            case NpcGoalKind::ReachLineOfSight:
                dest = npc.sensors.targetPos;
                haveDest = true;
                break;
            case NpcGoalKind::MaintainDistance:
                dest = npc.sensors.targetPos - dir * std::max(0.5f, goal.desiredDistance);
                haveDest = true;
                break;
            case NpcGoalKind::FleeActor:
                dest = npc.body.pos - dir * std::max(2.0f, goal.desiredDistance);
                haveDest = true;
                break;
            default:
                break;
        }
    }
    if (!haveDest) {
        path.clear();
        pathGap.clear();
        pathCapability.clear();
        pathIndex = 0;
        return result;
    }

    if (backtrackActive)
    {
        const glm::vec3 moved = npc.body.pos - npc.previousPosition;
        backtrackRemaining -= glm::length(glm::vec2(moved.x, moved.y));
        backtrackTimeRemaining -= dt;
        if (backtrackRemaining <= 0.0f || backtrackTimeRemaining <= 0.0f)
        {
            backtrackActive = false;
            requestRepath();
        }
        else
        {
            result.valid = true;
            result.detour = true;
            result.destination = dest;
            result.waypoint = npc.body.pos + backtrackDirection;
            result.dir = backtrackDirection;
            return result;
        }
    }

    // ── Decide whether to (re)plan ──────────────────────────────────
    // The repath timer is a minimum interval, not a command to rebuild: a
    // valid route is followed until it empties/finishes or a real reason
    // appears (goal moved, physically blocked, or no progress). This stops the
    // actor from throwing away a good route just because a little time passed.
    if (repathTimer > 0.0f) repathTimer -= dt;
    const float goalThreshold = commitment ? commitment->goalMoveThresholdMeters
                                           : kGoalMoveThreshold;
    const float repathInterval = commitment ? commitment->repathIntervalSeconds
                                            : kRepathInterval;
    const bool justRetryDelay = repathTimer > 0.0f;
    const bool routeFinished = !path.empty() && pathIndex >= (int)path.size();
    const bool targetMoved = hasLastGoal &&
        glm::length(glm::vec3(dest - lastGoal)) > goalThreshold;
    // Replan on real lack of progress, not on the instantaneous "wall within
    // probe" flag (which would rebuild the route almost every tick).
    const bool blockedNow = NpcNavigation::isStuck(npc);
    const bool progressFailed = commitment && commitment->enabled && commitmentProgressFailed;
    const bool preservingLocalCorrection = localCorrectionActive;
    bool needPlan = false;
    const char* reason = "initial";
    if (path.empty() && !preservingLocalCorrection) {
        // A failed attempt already set a short retry delay; honor it. The very
        // first evaluation (no prior goal) plans immediately.
        if (!hasLastGoal || !justRetryDelay) {
            needPlan = true;
            reason = hasLastGoal ? "empty" : "initial";
        }
    } else if (routeFinished) {
        needPlan = true;
        reason = "finished";
    } else if (!justRetryDelay && targetMoved && !preservingLocalCorrection) {
        needPlan = true;
        reason = "target_moved";
    } else if (!justRetryDelay && blockedNow && !preservingLocalCorrection) {
        needPlan = true;
        reason = "blocked";
    } else if (!justRetryDelay && progressFailed && !preservingLocalCorrection) {
        needPlan = true;
        reason = "progress";
    }

    if (needPlan && wantPlanning && consumePlanToken()) {
        const MovementConfig& cfg = movement ? *movement
                                             : MovementJsonConfig::instance().config();
        std::vector<glm::vec3> plan;
        std::vector<uint8_t> planGaps;
        std::vector<uint8_t> planCaps;

        const float toGoal = horizontalDistance(npc.body.pos, dest);
        const float localReach = std::max(8.0f, eff.searchRadius);
        bool planned = false;

        // Long-range goals: route over the lazy global walkable graph first.
        // The graph sees the whole map (no authored points) and returns a
        // corridor; the local planner below is the fallback and fine-steering
        // owner. Chunks are built lazily, so this costs nothing until used.
        const bool graphAllowed = !commitment || commitment->useNavGraph;
        if (graphAllowed && toGoal > localReach) {
            // Rebuild the cached graph if the live tuning file changed, so
            // editing chunk/cell size while the game runs takes effect.
            static uint64_t sNavTuningRevision = ~0ull;
            if (sNavTuningRevision != NpcDifficultyConfig::instance().revision()) {
                sNavTuningRevision = NpcDifficultyConfig::instance().revision();
                NpcNavGraph::instance().invalidate();
            }
            const NpcDifficultySettings& diff = NpcDifficultyConfig::instance().settings();
            NpcNavGraphSettings gs;
            gs.chunkSize = diff.navGraphChunkSize;
            gs.cellSize = diff.navGraphCellSize;
            gs.maxRoutes = diff.navGraphMaxRoutes;
            gs.maxDropHeight = diff.navGraphMaxDropHeight;
            gs.jumpHeight = npcMaxJumpHeight(cfg);
            gs.actorRadius = std::max(0.1f, npc.body.getCapsule().r);
            gs.actorHeight = std::max(1.0f, npc.body.getCapsule().b.z -
                                             npc.body.getCapsule().a.z +
                                             2.0f * npc.body.getCapsule().r);
            gs.maxWalkableSlopeDot = eff.maxWalkableSlopeDot > 0.0f
                ? eff.maxWalkableSlopeDot : NpcNavigation::kWalkableSlopeDot;
            gs.maxStepHeight = eff.maxStepHeight;
            gs.allowJumps = eff.allowNavigationJumps &&
                (policy == nullptr ||
                 npcPolicyAllowsJump(*policy, NpcJumpReason::Navigation));
            const std::vector<NpcNavRoute> routes =
                NpcNavGraph::instance().findRoutes(world, npc.body.pos, dest, gs, npc.id);
            if (!routes.empty() && routes.front().valid()) {
                const std::vector<glm::vec3>& pts = routes.front().points;
                glm::vec3 last = npc.body.pos;
                for (std::size_t i = 1; i < pts.size(); ++i) {
                    const bool finalPoint = (i + 1 == pts.size());
                    if (finalPoint || glm::length(pts[i] - last) >= 5.0f) {
                        plan.push_back(pts[i]);
                        planGaps.push_back(0);
                        planCaps.push_back((uint8_t)NavCapability::Walk);
                        last = pts[i];
                    }
                }
                planned = !plan.empty();
            }
        }

        if (!planned) {
            planned = planLocalPath(npc, dest, world, cfg, eff, policy,
                                    plan, planGaps, planCaps) && !plan.empty();
        }

        // Recast/Detour request. In compare mode the custom plan above stays
        // authoritative and Recast is observational evidence. In recast mode a
        // successful corridor replaces the custom path (same format), so the
        // shared follow/traversal/movement execution is reused unchanged. If
        // Recast cannot route, the custom plan remains the bounded fallback.
        const RecastBackend navBackend = recastBackendMode();
        if (navBackend != RecastBackend::Custom) {
            const Capsule& capsule = npc.body.getCapsule();
            NavigationAgentProfile profile;
            profile.radius = std::max(0.05f, capsule.r);
            profile.height = std::max(0.2f, glm::length(capsule.b - capsule.a) +
                                               2.0f * capsule.r);
            profile.stepHeight = std::max(0.01f, eff.maxStepHeight);
            const float slopeDot = eff.maxWalkableSlopeDot > 0.0f
                ? eff.maxWalkableSlopeDot : NpcNavigation::kWalkableSlopeDot;
            profile.maxSlopeDegrees = glm::degrees(std::acos(
                std::clamp(slopeDot, -1.0f, 1.0f)));
            const RecastNavigationResult recast =
                RecastNavigationBackend::instance().query(world, npc.body.pos,
                                                           dest, profile);
            result.recastCompareAttempted = true;
            result.recastCompareAvailable = recast.available;
            result.recastCompareSuccess = recast.success;
            result.recastNavmeshVersion = recast.navmeshVersion;
            result.recastPolygonCount = recast.polygonCount;
            result.recastPathLength = recast.pathLength;
            result.recastQueryMilliseconds = recast.queryMilliseconds;
            result.recastStartPolyFound = recast.startPolyFound;
            result.recastDestPolyFound = recast.destPolyFound;
            result.recastStartPolyRef = recast.startPolyRef;
            result.recastDestPolyRef = recast.destPolyRef;
            result.recastNearestStart = recast.nearestStart;
            result.recastNearestDest = recast.nearestDest;
            result.recastStartProjectionDistance = recast.startProjectionDistance;
            result.recastDestProjectionDistance = recast.destProjectionDistance;
            result.recastFailure = recast.failure;

            if (navBackend == RecastBackend::Recast && recast.success &&
                !recast.points.empty()) {
                std::vector<glm::vec3> recastPlan = recast.points;
                // The straight path starts at the projected actor position;
                // drop it when it is already within arrival range.
                if (!recastPlan.empty()) {
                    const glm::vec3 first = recastPlan.front();
                    const float fdx = first.x - npc.body.pos.x;
                    const float fdy = first.y - npc.body.pos.y;
                    if (std::sqrt(fdx * fdx + fdy * fdy) <= kReachXZ)
                        recastPlan.erase(recastPlan.begin());
                }
                if (!recastPlan.empty()) {
                    plan = std::move(recastPlan);
                    planGaps.assign(plan.size(), 0);
                    planCaps.assign(plan.size(), (uint8_t)NavCapability::Walk);
                    planned = true;
                    result.recastAuthoritative = true;
                }
            }
        }

        if (planned) {
            path = std::move(plan);
            pathGap = std::move(planGaps);
            pathCapability = std::move(planCaps);
            pathIndex = 0;
            lastGoal = dest;
            hasLastGoal = true;
            repathTimer = repathInterval;
            result.planCreated = true;
            result.replan = planCount > 0;
            result.replanReason = reason;
            result.pathNodeCount = (int)path.size();
            ++planCount;
            if (reason != std::string("initial")) ++repathCount;
            const float netDz = path.back().z - npc.body.pos.z;
            Debug::log(Debug::Category::NpcMovement,
                "[NPC NAV] actor=%u goal=%s target=%u pathNodes=%d jumpCap=%.2f netDz=%.1f reason=%s\n",
                npc.id, goalKindName(goal.kind), goal.targetActorId,
                (int)path.size(), npcMaxJumpHeight(cfg), netDz, reason);
        } else {
            // No route in the local window. Keep the target memory and retry
            // after a short delay; the tail below turns or stops instead of
            // falling back to direct wall-pushing.
            path.clear();
            pathGap.clear();
            pathCapability.clear();
            pathIndex = 0;
            lastGoal = dest;
            hasLastGoal = true;
            repathTimer = kBlockedRetryInterval;
            result.planFailed = true;
            result.replanReason = reason;
        }
    }

    // ── Follow the cached route ─────────────────────────────────────
    while (!path.empty() && pathIndex < (int)path.size()) {
        const glm::vec3& wp = path[pathIndex];
        const float dx = wp.x - npc.body.pos.x;
        const float dy = wp.y - npc.body.pos.y;
        const bool horizReached = std::sqrt(dx * dx + dy * dy) <= kReachXZ;
        // Refuse arrival only when the waypoint is meaningfully ABOVE the actor
        // (a stacked floor / jump target). A walk waypoint at floor height is
        // ~1.8 m below the capsule center and must still count as reached, or
        // the actor orbits it forever and never makes net progress.
        const bool notAbove = (wp.z - npc.body.pos.z) <= kReachAbove;
        if (horizReached && notAbove) {
            ++pathIndex;
            continue;
        }
        break;
    }
    const bool pathActive = !path.empty() && pathIndex < (int)path.size();

    glm::vec3 target = pathActive ? path[pathIndex] : dest;
    result.waypoint = target;
    result.destination = dest;
    result.hasPath = pathActive;
    result.pathNodes = pathActive ? (int)path.size() - pathIndex : 0;
    result.valid = true;

    glm::vec3 to(target.x - npc.body.pos.x, target.y - npc.body.pos.y, 0.0f);
    const float len = glm::length(to);
    const float dz = target.z - npc.body.pos.z;
    result.heightDelta = dz;
    result.distance = len;

    if (pathActive) {
        if (len > 0.001f) result.dir = to / len;
        result.hasGap = pathIndex < (int)pathGap.size() && pathGap[pathIndex] != 0;
        if (pathIndex < (int)pathCapability.size())
            result.capability = (NavCapability)pathCapability[pathIndex];
        // Only treat the route as a detour when it is long or pulls away from
        // the direct line to the destination, so tactical strafing is kept in
        // open space.
        glm::vec3 toDest(dest.x - npc.body.pos.x, dest.y - npc.body.pos.y, 0.0f);
        const float dl = glm::length(toDest);
        if (dl > 0.001f) {
            result.detour = result.pathNodes > 2 ||
                            glm::dot(result.dir, toDest / dl) < 0.8f;
        }
        return result;
    }

    // ── No cached route: direct steer only when the line is open ────
    // The actor must never continuously apply movement into a confirmed
    // blocking wall; it turns, holds, and waits for the next replan instead.
    glm::vec3 toDest(dest.x - npc.body.pos.x, dest.y - npc.body.pos.y, 0.0f);
    const float goalDist = glm::length(toDest);
    if (goalDist <= goal.tolerance) {
        result.dir = glm::vec3(0.0f);
        return result;
    }
    glm::vec3 directDir = goalDist > 0.001f ? toDest / goalDist : glm::vec3(0.0f);

    // No cached route: the committed direction owns forward steering while it
    // is valid and still broadly toward the goal. This is what lets an actor
    // that spawned facing a wall keep a real forward direction instead of
    // re-deciding every tick.
    if (commitment && commitment->enabled && commitmentActive &&
        !commitmentBlocked && glm::length(committedDirection) > 0.001f &&
        glm::dot(committedDirection, directDir) > -0.2f) {
        result.valid = true;
        result.dir = committedDirection;
        return result;
    }

    const bool directMode = eff.mode == "direct";
    const bool blocked = !directMode && glm::length(directDir) > 0.001f &&
        NpcNavigation::obstacleInDirection(npc, directDir, eff.wallProbeDistance, world);
    if (!blocked) {
        result.dir = directDir;
        return result;
    }

    result.blocked = true;
    result.detour = true;
    if (eff.blockedBehavior == "turn" || eff.blockedBehavior == "turn_then_repath") {
        result.dir = bestTurnDirection(npc, directDir, world);  // zero if boxed in
    } else {
        result.dir = glm::vec3(0.0f);  // "repath": hold, do not push
    }
    return result;
}
