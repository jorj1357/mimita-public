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

#include "npc/npc.h"
#include "npc/npc-navigation.h"
#include "npc/npc-difficulty-config.h"
#include "config/movement-config.h"
#include "world/world.h"
#include "physics/physics-types.h"
#include "physics/movement/physics-collision.h"
#include "debug/debug-log.h"

namespace {

constexpr float kPlanRadius = 12.5f;   // half-extent of the local plan window
constexpr float kCell = 2.5f;          // ground-grid cell size
constexpr float kUpProbe = 4.0f;       // probe start above the actor
constexpr float kProbeDepth = 12.0f;   // probe reach below the actor
constexpr float kStepUp = 0.65f;       // walk-up without jumping
constexpr float kReachXZ = 1.0f;       // waypoint arrival (planar)
constexpr float kReachZ = 1.3f;        // waypoint arrival (vertical)
constexpr float kRepathInterval = 0.9f;
constexpr float kGoalMoveThreshold = 2.5f;
constexpr int kMaxPlansPerSecond = 16;
constexpr float kWalkableNormalZ = 0.7f;
constexpr float kNotFound = -1e6f;

const int kGridN = (int)std::floor((2.0f * kPlanRadius) / kCell) + 1;

struct LocalNode
{
    bool valid = false;
    float z = 0.0f;
    float g = 1e18f;
    float f = 1e18f;
    int parent = -1;
    bool closed = false;
};

const char* goalKindName(NpcGoalKind kind)
{
    switch (kind) {
        case NpcGoalKind::ReachPosition:     return "reach";
        case NpcGoalKind::FollowActor:       return "pursue";
        case NpcGoalKind::MaintainDistance:  return "maintain";
        case NpcGoalKind::FleeActor:         return "flee";
        case NpcGoalKind::ReachLineOfSight:  return "los";
        case NpcGoalKind::None:              return "none";
    }
    return "none";
}

float maxJumpHeight(const MovementConfig& m)
{
    const float g = std::fabs(m.gravityZ);
    if (m.jumpVerticalSpeed <= 0.0f || g <= 0.01f)
        return 0.0f;
    return (m.jumpVerticalSpeed * m.jumpVerticalSpeed) / (2.0f * g);
}

// Highest walkable surface below `fromZ` at (x,y), tested against a window
// candidate set gathered once per plan (no per-probe broadphase query).
float probeGround(const World& world, const std::vector<int>& candidates,
                  float x, float y, float fromZ, float depth)
{
    const glm::vec3 origin(x, y, fromZ);
    const glm::vec3 down(0.0f, 0.0f, -1.0f);
    float best = kNotFound;
    const auto& tris = world.collisionMesh.triangles;
    for (int ti : candidates) {
        if (ti < 0 || ti >= (int)tris.size()) continue;
        const CollisionTriangle& tri = tris[ti];
        if (tri.normal.z < kWalkableNormalZ) continue;
        float t = 0.0f;
        if (NpcNavigation::rayTriangle(origin, down, tri, depth, t)) {
            const float z = fromZ - t;
            if (z > best) best = z;
        }
    }
    return best;
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

// Grid A* over the local window. Returns false when no route exists.
bool planLocalPath(const Npc& npc, const glm::vec3& goalPos, const World& world,
                   const MovementConfig& movement, std::vector<glm::vec3>& outPoints)
{
    const int N = kGridN;
    const int half = N / 2;
    const float c = kCell;
    const float ox = std::floor(npc.body.pos.x / c) * c;
    const float oy = std::floor(npc.body.pos.y / c) * c;

    AABB win;
    win.min = glm::vec3(ox - half * c - c, oy - half * c - c,
                        npc.body.pos.z - kProbeDepth);
    win.max = glm::vec3(ox + half * c + c, oy + half * c + c,
                        npc.body.pos.z + kUpProbe);
    static thread_local std::vector<int> candidates;
    candidates.clear();
    appendChunkTrianglesForAABB(world, win, 0.0f, candidates, "npcNavPlan");

    std::vector<LocalNode> nodes((size_t)N * N);
    const float fromZ = npc.body.pos.z + kUpProbe;
    const float depth = kProbeDepth + kUpProbe;
    for (int gy = 0; gy < N; ++gy) {
        for (int gx = 0; gx < N; ++gx) {
            const float x = ox + (gx - half) * c;
            const float y = oy + (gy - half) * c;
            const float z = probeGround(world, candidates, x, y, fromZ, depth);
            if (z > kNotFound) {
                LocalNode& n = nodes[(size_t)gy * N + gx];
                n.valid = true;
                n.z = z;
            }
        }
    }

    auto valid = [&](int gx, int gy) {
        return gx >= 0 && gy >= 0 && gx < N && gy < N &&
               nodes[(size_t)gy * N + gx].valid;
    };
    auto dist2 = [&](int a, int b) {
        const int ax = a % N, ay = a / N;
        const int bx = b % N, by = b / N;
        const float dx = (ax - bx) * c, dy = (ay - by) * c;
        return std::sqrt(dx * dx + dy * dy);
    };
    auto nearestValid = [&](const glm::vec3& p) {
        int best = -1;
        float bestD = 1e18f;
        for (int i = 0; i < N * N; ++i) {
            if (!nodes[i].valid) continue;
            const int gx = i % N, gy = i / N;
            const float x = ox + (gx - half) * c;
            const float y = oy + (gy - half) * c;
            const float dx = x - p.x, dy = y - p.y, dz = nodes[i].z - p.z;
            const float d = dx * dx + dy * dy + dz * dz;
            if (d < bestD) { bestD = d; best = i; }
        }
        return best;
    };

    const int start = nearestValid(npc.body.pos);
    const int goalNode = nearestValid(goalPos);
    if (start < 0 || goalNode < 0)
        return false;

    const float jump = maxJumpHeight(movement);

    nodes[start].g = 0.0f;
    nodes[start].f = dist2(start, goalNode);
    std::vector<int> open;
    open.push_back(start);

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

        const int cx = cur % N, cy = cur / N;
        for (int d = 0; d < 8; ++d) {
            const int nx = cx + DX[d], ny = cy + DY[d];
            if (!valid(nx, ny)) continue;
            const int nb = ny * N + nx;
            if (nodes[nb].closed) continue;

            // Do not cut diagonal corners through invalid cells.
            if (DX[d] != 0 && DY[d] != 0) {
                if (!valid(cx + DX[d], cy) || !valid(cx, cy + DY[d]))
                    continue;
            }

            const float dz = nodes[nb].z - nodes[cur].z;
            if (dz > jump) continue;                 // too tall to reach at all
            const bool needJump = dz > kStepUp;

            const float planar = (DX[d] != 0 && DY[d] != 0) ? c * 1.4142f : c;
            const float ng = nodes[cur].g + planar + (needJump ? 0.8f : 0.0f);
            if (ng < nodes[nb].g) {
                nodes[nb].g = ng;
                nodes[nb].parent = cur;
                nodes[nb].f = ng + dist2(nb, goalNode);
                open.push_back(nb);
            }
        }
    }

    if (nodes[goalNode].g >= 1e17f)
        return false;

    outPoints.clear();
    if (start == goalNode) {
        outPoints.push_back(goalPos);
        return true;
    }
    std::vector<int> idx;
    for (int cur = goalNode; cur != -1; cur = nodes[cur].parent)
        idx.push_back(cur);
    std::reverse(idx.begin(), idx.end());
    for (size_t i = 1; i < idx.size(); ++i) {
        const int id = idx[i];
        const int gx = id % N, gy = id / N;
        outPoints.push_back(glm::vec3(ox + (gx - half) * c,
                                      oy + (gy - half) * c,
                                      nodes[id].z));
    }
    if (outPoints.empty())
        outPoints.push_back(goalPos);
    return true;
}

} // anonymous namespace

void NpcNavigator::reset()
{
    path.clear();
    pathIndex = 0;
    repathTimer = 0.0f;
    hasLastGoal = false;
    goal = NpcGoal{};
}

NpcNavResult NpcNavigator::update(Npc& npc, const NpcGoal& newGoal, const World& world,
                                  const MovementConfig* movement, float dt)
{
    NpcNavResult result;
    goal = newGoal;
    if (!goal.valid()) {
        path.clear();
        pathIndex = 0;
        return result;
    }

    // ── Resolve the abstract goal to a destination point ────────────
    glm::vec3 dest{0.0f};
    bool haveDest = false;
    if (goal.kind == NpcGoalKind::ReachPosition) {
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
        pathIndex = 0;
        return result;
    }

    // ── Decide whether to (re)plan ──────────────────────────────────
    bool needPlan = path.empty() || pathIndex >= (int)path.size();
    const char* reason = "initial";
    if (!needPlan) {
        repathTimer -= dt;
        if (repathTimer <= 0.0f) {
            needPlan = true;
            reason = "timer";
        } else if (hasLastGoal &&
                   glm::length(glm::vec3(dest - lastGoal)) > kGoalMoveThreshold) {
            needPlan = true;
            reason = "target_moved";
        }
    }
    if (!needPlan && NpcNavigation::isStuck(npc)) {
        needPlan = true;
        reason = "stuck";
    }

    if (needPlan && consumePlanToken()) {
        const MovementConfig& cfg = movement ? *movement
                                             : MovementJsonConfig::instance().config();
        std::vector<glm::vec3> plan;
        if (planLocalPath(npc, dest, world, cfg, plan) && !plan.empty()) {
            path = std::move(plan);
            pathIndex = 0;
            lastGoal = dest;
            hasLastGoal = true;
            repathTimer = kRepathInterval;
            ++planCount;
            if (reason != std::string("initial")) ++repathCount;
            const float netDz = path.back().z - npc.body.pos.z;
            Debug::log(Debug::Category::NpcMovement,
                "[NPC NAV] actor=%u goal=%s target=%u pathNodes=%d jumpCap=%.2f netDz=%.1f reason=%s\n",
                npc.id, goalKindName(goal.kind), goal.targetActorId,
                (int)path.size(), maxJumpHeight(cfg), netDz, reason);
        } else {
            // No route in the local window: fall back to direct steering and
            // retry soon (target may have moved into a reachable cell).
            path.clear();
            pathIndex = 0;
            repathTimer = 0.4f;
        }
    }

    // ── Follow the cached route ─────────────────────────────────────
    while (!path.empty() && pathIndex < (int)path.size()) {
        const glm::vec3& wp = path[pathIndex];
        const float dx = wp.x - npc.body.pos.x;
        const float dy = wp.y - npc.body.pos.y;
        if (std::sqrt(dx * dx + dy * dy) <= kReachXZ &&
            std::fabs(wp.z - npc.body.pos.z) <= kReachZ) {
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

    glm::vec3 to(target.x - npc.body.pos.x, target.y - npc.body.pos.y, 0.0f);
    const float len = glm::length(to);
    const float dz = target.z - npc.body.pos.z;

    if (pathActive) {
        if (len > 0.001f) result.dir = to / len;
        // Only treat the route as a detour when it is long or pulls away from
        // the direct line to the destination, so tactical strafing is kept in
        // open space.
        glm::vec3 toDest(dest.x - npc.body.pos.x, dest.y - npc.body.pos.y, 0.0f);
        const float dl = glm::length(toDest);
        if (dl > 0.001f) {
            result.detour = result.pathNodes > 2 ||
                            glm::dot(result.dir, toDest / dl) < 0.8f;
        }
        if (dz > kStepUp && len < kCell * 1.5f && npc.sensors.touchFloor)
            result.wantJump = true;
        if (dz < -kStepUp)
            result.wantDownDash = true;
        return result;
    }

    // Direct steering to the destination. Stop when close enough.
    const float goalDist = glm::length(glm::vec3(
        dest.x - npc.body.pos.x, dest.y - npc.body.pos.y, 0.0f));
    if (goalDist > goal.tolerance && len > 0.001f)
        result.dir = to / len;
    if (goal.kind == NpcGoalKind::FollowActor && dz > kStepUp &&
        goalDist < 3.0f && npc.sensors.touchFloor)
        result.wantJump = true;
    return result;
}
