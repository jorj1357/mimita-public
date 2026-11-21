// 2026-11-01
/* purpose
* Minimal live-play AI for the real local player used by unattended TDM runs.
* It selects a hostile replicated actor, approaches/strafes, aims, and fires
* through the normal InputFrame/ActorIntent boundary. With no hostile it
* explores the map instead of spawning fallback NPCs.
* Does NOT own collision, damage, weapon formulas, or server authority.
*/
#include "actor/player-ai.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "camera.h"
#include "entities/player.h"
#include "combat/weapon-registry.h"
#include "devtools/terminal.h"
#include "input/input-frame.h"
#include "network/community-match-client.h"
#include "network/community-server-config.h"
#include "network/multiplayer-context.h"
#include "npc/npc-difficulty-config.h"
#include "npc/npc-nav-graph.h"
#include "npc/npc-navigation.h"
#include "physics/movement/physics-collision.h"
#include "physics/config.h"
#include "terminal/terminal-state.h"
#include "debug/structured-log.h"

namespace {

bool gActive = false;
std::string gMeleeWeapon;
std::vector<glm::vec3> gRoute;
size_t gRouteIndex = 0;
uint32_t gRouteTargetId = 0;
glm::vec3 gRouteGoal{0.0f};
uint32_t gRoutePlanTick = 0;
uint32_t gLastSpawnGeneration = 0;
uint32_t gLatchedTargetId = 0;
uint64_t gLastFallLogMs = 0;
glm::vec3 gExploreGoal{0.0f};
uint32_t gExplorePlanTick = 0;
std::string gWeaponVisit;
uint32_t gWeaponVisitEndsTick = 0;
uint32_t gNextProgressCheckTick = 0;
glm::vec3 gProgressCheckPosition{0.0f};
int gNoProgressSamples = 0;
uint32_t gRecoveryUntilTick = 0;
glm::vec2 gRecoveryDirection{0.0f};
uint32_t gFreezeReleaseTick = 0;

uint64_t aiNowMs()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

void resetRoute()
{
    gRoute.clear();
    gRouteIndex = 0;
    gRouteTargetId = 0;
    gRouteGoal = glm::vec3(0.0f);
    gRoutePlanTick = 0;
}

bool worldBlocks(const World& world, const glm::vec3& from, const glm::vec3& to);

struct TargetView {
    bool valid = false;
    uint32_t id = 0;
    int team = -1;
    glm::vec3 pos{0.0f};
    float distance = 0.0f;
};

bool hostile(const Player& player, const Player& candidate,
             uint32_t candidateId)
{
    if (candidate.dead || candidate.currentHp <= 0) return false;
    const std::string& mode =
        MimitaNet::CommunityMatchClient::instance().mode();
    const bool teamMode = mode == "tdm" || mode == "juggernaut";
    if (!teamMode)
        return true;

    int mine = player.matchTeam;
    if (mine < 0) {
        const uint8_t replicated =
            MimitaNet::CommunityMatchClient::instance().localTeam(
                MP_CONTEXT.localPlayerId);
        if (replicated != 0xFF) mine = (int)replicated;
    }
    int theirs = candidate.matchTeam;
    if (theirs < 0) {
        const uint8_t replicated =
            MimitaNet::CommunityMatchClient::instance().teamForActor(
                candidateId);
        if (replicated != 0xFF) theirs = (int)replicated;
    }
    // Never guess hostility from an unknown team. In TDM an unresolved actor
    // must not make the local AI chase or attack a teammate.
    return mine >= 0 && theirs >= 0 && mine != theirs;
}

bool targetVisible(const World& world, const Player& player,
                   const Player& candidate)
{
    const glm::vec3 eye = player.pos + glm::vec3(0.0f, 0.0f, 1.35f);
    const glm::vec3 aim = candidate.pos + glm::vec3(0.0f, 0.0f, 0.85f);
    return !worldBlocks(world, eye, aim);
}

TargetView chooseTarget(const Player& player, const World& world)
{
    TargetView best;
    if (!gMeleeWeapon.empty() && gLatchedTargetId != 0) {
        const auto latch = [&](const std::unordered_map<uint32_t, Player>& map) {
            auto it = map.find(gLatchedTargetId);
            if (it == map.end() || !hostile(player, it->second, it->first))
                return false;
            if (!targetVisible(world, player, it->second)) {
                gLatchedTargetId = 0;
                return false;
            }
            best.valid = true;
            best.id = it->first;
            best.team = it->second.matchTeam;
            best.pos = it->second.pos;
            best.distance = glm::length(it->second.pos - player.pos);
            // Keep a close target latched, but allow a freshly spawned near
            // target to take over when the latched one has drifted away. Six
            // metres is outside every focused melee weapon's contact volume.
            return best.distance <= 6.0f;
        };
        if (latch(MP_CONTEXT.remoteNpcs) || latch(MP_CONTEXT.remotePlayers))
            return best;
        gLatchedTargetId = 0;
    }

    struct Candidate {
        const Player* actor = nullptr;
        uint32_t id = 0;
        float distance = 0.0f;
        float score = 0.0f;
    };
    std::vector<Candidate> candidates;
    const glm::vec3 mapExtent = world.collisionMesh.boundsMax -
                                world.collisionMesh.boundsMin;
    const float maxRange = std::max(glm::length(mapExtent), 60.0f);
    const auto consider = [&](const Player& candidate, uint32_t id) {
        if (id == MP_CONTEXT.localPlayerId) return;
        if (!hostile(player, candidate, id)) return;
        const glm::vec3 delta = candidate.pos - player.pos;
        const float distance = glm::length(delta);
        if (distance > maxRange) return;
        const glm::vec3 planar(delta.x, delta.y, 0.0f);
        const float len = glm::length(planar);
        float score = distance;
        if (len > 0.001f) {
            const glm::vec3 facing(std::cos(glm::radians(player.yaw)),
                                   std::sin(glm::radians(player.yaw)), 0.0f);
            score -= 0.75f * glm::dot(planar / len, facing);
        }
        candidates.push_back({&candidate, id, distance, score});
    };
    for (const auto& entry : MP_CONTEXT.remoteNpcs)
        consider(entry.second, entry.first);
    for (const auto& entry : MP_CONTEXT.remotePlayers)
        consider(entry.second, entry.first);
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) {
                  return a.score < b.score;
              });

    // Cast only against the best few candidates; the nearest actor is often
    // behind cover and should not cause a per-tick raycast over every actor.
    constexpr size_t kMaxVisibilityChecks = 8;
    size_t checked = 0;
    for (const Candidate& candidate : candidates) {
        if (checked++ >= kMaxVisibilityChecks) break;
        if (!targetVisible(world, player, *candidate.actor)) continue;
        best.valid = true;
        best.id = candidate.id;
        best.team = candidate.actor->matchTeam;
        best.pos = candidate.actor->pos;
        best.distance = candidate.distance;
        break;
    }
    // Keep the nearest known hostile as a pursuit goal when all candidates
    // are behind cover. buildFrame still blocks ranged fire until that target
    // has a clear world ray, so occlusion changes navigation, not authority.
    if (!best.valid && !candidates.empty()) {
        const Candidate& pursuit = candidates.front();
        best.valid = true;
        best.id = pursuit.id;
        best.team = pursuit.actor->matchTeam;
        best.pos = pursuit.actor->pos;
        best.distance = pursuit.distance;
    }
    if (!gMeleeWeapon.empty())
        gLatchedTargetId = best.valid && best.distance <= 15.0f ? best.id : 0;
    return best;
}

bool groundAhead(const World& world, const Player& player,
                 const glm::vec2& direction, float maxDrop, float& outGroundZ)
{
    if (glm::length(direction) < 0.001f) return false;
    const glm::vec3 forward =
        glm::normalize(glm::vec3(direction.x, direction.y, 0.0f));
    const glm::vec3 probe =
        player.pos + forward * 1.6f + glm::vec3(0.0f, 0.0f, 0.25f);
    outGroundZ = NpcNavigation::groundHeightAt(world, probe, 6.0f, 0.25f);
    return outGroundZ > -1.0e5f && outGroundZ >= player.pos.z - maxDrop;
}

glm::vec2 avoidUnsafeGround(const World& world, const Player& player,
                            glm::vec2 desired, bool allowRouteDrop)
{
    if (!player.ground.onGround)
        return glm::vec2(0.0f);
    if (glm::length(desired) < 0.001f)
        return desired;
    desired = glm::normalize(desired);
    const float maxDrop = allowRouteDrop ? 8.0f : 3.0f;
    float groundZ = 0.0f;
    if (groundAhead(world, player, desired, maxDrop, groundZ))
        return desired;
    static const float angles[] = {25.0f, -25.0f, 50.0f, -50.0f,
                                   75.0f, -75.0f, 105.0f, -105.0f,
                                   140.0f, -140.0f, 180.0f};
    for (float degrees : angles) {
        const float radians = glm::radians(degrees);
        const glm::vec2 candidate(
            desired.x * std::cos(radians) - desired.y * std::sin(radians),
            desired.x * std::sin(radians) + desired.y * std::cos(radians));
        if (groundAhead(world, player, candidate, maxDrop, groundZ))
            return candidate;
    }
    return glm::vec2(0.0f);
}

NpcNavGraphSettings playerNavGraphSettings()
{
    const auto& difficulty = NpcDifficultyConfig::instance().settings();
    NpcNavGraphSettings settings;
    settings.chunkSize = difficulty.navGraphChunkSize;
    settings.cellSize = difficulty.navGraphCellSize;
    settings.maxRoutes = std::max(1, difficulty.navGraphMaxRoutes);
    settings.maxDropHeight = difficulty.navGraphMaxDropHeight;
    return settings;
}

void planRoute(const Player& player, const TargetView& target,
               const World& world)
{
    if (!target.valid) {
        resetRoute();
        return;
    }
    const uint32_t tick = (uint32_t)player.movementSimulationTick;
    const bool goalMoved =
        glm::length(glm::vec2(target.pos.x - gRouteGoal.x,
                              target.pos.y - gRouteGoal.y)) > 2.5f;
    const bool need = gRouteTargetId != target.id || goalMoved ||
        tick >= gRoutePlanTick;
    if (!need) return;

    const auto start = std::chrono::steady_clock::now();
    const std::vector<NpcNavRoute> routes = NpcNavGraph::instance().findRoutes(
        world, player.pos, target.pos, playerNavGraphSettings(),
        MP_CONTEXT.localPlayerId, tick);
    const double milliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();

    gRoute.clear();
    gRouteIndex = 0;
    gRouteTargetId = target.id;
    gRouteGoal = target.pos;
    gRoutePlanTick = tick + 30u;
    if (!routes.empty() && routes.front().valid()) {
        gRoute = routes.front().points;
        if (gRoute.size() > 1 &&
            glm::length(glm::vec2(gRoute[0].x - player.pos.x,
                                  gRoute[0].y - player.pos.y)) < 1.5f)
            gRouteIndex = 1;
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "player.ai.navplan", "player-ai",
        gRoute.empty() ? "no route; using local steering" : "route planned",
        tick, {{"target_id", target.id},
               {"target_distance", target.distance},
               {"points", gRoute.size()},
               {"plan_ms", milliseconds},
               {"route_index", gRouteIndex}},
        __FILE__, __LINE__, __FUNCTION__);
}

bool chooseExploreGoal(const Player& player, const World& world,
                       glm::vec3& outGoal)
{
    const auto& mesh = world.collisionMesh;
    if (mesh.triangles.empty()) return false;
    const glm::vec3 min = mesh.boundsMin;
    const glm::vec3 max = mesh.boundsMax;
    const float width = max.x - min.x;
    const float depth = max.y - min.y;
    const float height = max.z - min.z;
    if (width <= 1.0f || depth <= 1.0f) return false;
    const float insetX = std::clamp(width * 0.08f, 1.0f, 20.0f);
    const float insetY = std::clamp(depth * 0.08f, 1.0f, 20.0f);
    const glm::vec2 points[9] = {
        {min.x + insetX, min.y + insetY},
        {max.x - insetX, min.y + insetY},
        {min.x + insetX, max.y - insetY},
        {max.x - insetX, max.y - insetY},
        {(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f},
        {(min.x + max.x) * 0.5f, min.y + insetY},
        {(min.x + max.x) * 0.5f, max.y - insetY},
        {min.x + insetX, (min.y + max.y) * 0.5f},
        {max.x - insetX, (min.y + max.y) * 0.5f},
    };
    float bestDistance = 6.0f;
    bool found = false;
    for (const glm::vec2& point : points) {
        const glm::vec3 probe(point.x, point.y, max.z + 10.0f);
        const float groundZ = NpcNavigation::groundHeightAt(
            world, probe, height + 60.0f, 2.0f);
        if (groundZ < -1.0e5f) continue;
        const glm::vec3 goal(point.x, point.y, groundZ + 0.1f);
        const float distance = glm::length(goal - player.pos);
        if (distance > bestDistance) {
            bestDistance = distance;
            outGoal = goal;
            found = true;
        }
    }
    return found;
}

bool planExploreRoute(const Player& player, const World& world)
{
    constexpr uint32_t kExploreRouteId = 0xFFFFFFFFu;
    const uint32_t tick = (uint32_t)player.movementSimulationTick;
    static uint32_t sLastExplorePlanTick = 0;
    if (sLastExplorePlanTick != 0 && tick >= sLastExplorePlanTick &&
        tick - sLastExplorePlanTick < 120u)
        return !gRoute.empty();
    // A plan is valid for its interval even if the route is empty. Without
    // this hard rate limit, an unreachable goal can replan and log every tick.
    if (gExplorePlanTick != 0 && tick < gExplorePlanTick)
        return !gRoute.empty();
    const bool goalReached = glm::length(player.pos - gExploreGoal) < 5.0f;
    const bool routeConsumed = !gRoute.empty() && gRouteIndex >= gRoute.size();
    const bool need = gRouteTargetId != kExploreRouteId ||
        tick >= gExplorePlanTick ||
        goalReached ||
        routeConsumed ||
        glm::length(gExploreGoal) < 0.001f;
    if (!need)
        return !gRoute.empty();

    if (goalReached || glm::length(gExploreGoal) < 0.001f) {
        glm::vec3 nextGoal{0.0f};
        if (!chooseExploreGoal(player, world, nextGoal)) {
            resetRoute();
            return false;
        }
        gExploreGoal = nextGoal;
    }

    const auto start = std::chrono::steady_clock::now();
    const std::vector<NpcNavRoute> routes = NpcNavGraph::instance().findRoutes(
        world, player.pos, gExploreGoal, playerNavGraphSettings(),
        MP_CONTEXT.localPlayerId, tick);
    const double milliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();

    gRoute.clear();
    gRouteIndex = 0;
    gRouteTargetId = kExploreRouteId;
    gRouteGoal = gExploreGoal;
    gRoutePlanTick = tick + 120u;
    sLastExplorePlanTick = tick;
    if (!routes.empty() && routes.front().valid()) {
        gRoute = routes.front().points;
        if (gRoute.size() > 1 &&
            glm::length(glm::vec2(gRoute[0].x - player.pos.x,
                                  gRoute[0].y - player.pos.y)) < 1.5f)
            gRouteIndex = 1;
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "player.ai.explore", "player-ai",
        gRoute.empty() ? "no exploration route; using local steering"
                       : "exploration route planned",
        tick,
        {{"player_id", MP_CONTEXT.localPlayerId},
         {"goal", {gExploreGoal.x, gExploreGoal.y, gExploreGoal.z}},
         {"distance", glm::length(gExploreGoal - player.pos)},
         {"points", gRoute.size()},
         {"plan_ms", milliseconds}},
        __FILE__, __LINE__, __FUNCTION__);
    return !gRoute.empty();
}

bool followRoute(const Player& player, glm::vec2& outDirection, bool& outJump)
{
    while (gRouteIndex < gRoute.size()) {
        const glm::vec3& waypoint = gRoute[gRouteIndex];
        const float dx = waypoint.x - player.pos.x;
        const float dy = waypoint.y - player.pos.y;
        const float planar = std::sqrt(dx * dx + dy * dy);
        if (planar <= 1.6f && waypoint.z - player.pos.z <= 1.5f) {
            ++gRouteIndex;
            continue;
        }
        if (planar > 0.001f) outDirection = glm::vec2(dx, dy) / planar;
        outJump = waypoint.z - player.pos.z > 0.9f;
        return true;
    }
    return false;
}

int meleeLogicalSlot(const std::string& weaponId)
{
    auto& config = MimitaNet::CommunityServerConfig::instance();
    if (config.weaponSets().empty()) config.load();
    const int weaponSetId = MP_CONTEXT.communityWeaponSetId > 0
        ? MP_CONTEXT.communityWeaponSetId : 1;
    return config.slotForWeapon(weaponSetId, weaponId);
}

bool worldBlocks(const World& world, const glm::vec3& from, const glm::vec3& to)
{
    glm::vec3 delta = to - from;
    const float distance = glm::length(delta);
    if (distance < 0.5f) return false;
    delta /= distance;
    float hitDistance = distance;
    glm::vec3 normal(0.0f);
    if (!rayTraverseGridCells(world, from, delta, distance, hitDistance, &normal))
        return false;
    if (hitDistance <= 0.1f || hitDistance >= distance - 0.3f)
        return false;
    if (normal.z < 0.0f)
        normal = -normal;
    // A walkable ramp is terrain, not a wall. Only a non-walkable face blocks.
    return normal.z <= MAX_WALKABLE_SLOPE_DOT;
}

glm::vec2 avoidWalls(const World& world, const Player& player,
                     glm::vec2 desired)
{
    if (glm::length(desired) < 0.001f) return desired;
    desired = glm::normalize(desired);
    const auto blockedAt = [&](glm::vec2 direction, float z) {
        const glm::vec3 origin(player.pos.x, player.pos.y, z);
        const glm::vec3 to = origin + glm::vec3(direction.x, direction.y, 0.0f) * 2.5f;
        return worldBlocks(world, origin, to);
    };
    const auto clear = [&](glm::vec2 direction) {
        // A low walkable lip/ramp must not read as a full wall. A tall wall
        // blocks both the foot-level and chest-level probes.
        return !(blockedAt(direction, player.pos.z - 1.2f) &&
                 blockedAt(direction, player.pos.z + 0.4f));
    };
    if (clear(desired)) return desired;
    static const float angles[] = {25.0f, -25.0f, 50.0f, -50.0f,
                                   75.0f, -75.0f, 105.0f, -105.0f,
                                   140.0f, -140.0f};
    for (float degrees : angles) {
        const float radians = glm::radians(degrees);
        const glm::vec2 candidate(
            desired.x * std::cos(radians) - desired.y * std::sin(radians),
            desired.x * std::sin(radians) + desired.y * std::cos(radians));
        if (clear(candidate)) return candidate;
    }
    // Every candidate sees walkable terrain or a low lip; let the shared
    // movement solver climb it instead of reversing or oscillating.
    return desired;
}

void applySmoothAim(Camera& camera, float targetYaw, float targetPitch)
{
    static auto last = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - last).count();
    last = now;
    dt = std::clamp(dt, 0.0f, 0.05f);
    if (dt <= 0.0f) return;

    float deltaYaw = targetYaw - camera.yaw;
    while (deltaYaw > 180.0f) deltaYaw -= 360.0f;
    while (deltaYaw < -180.0f) deltaYaw += 360.0f;
    constexpr float kMaxTurnDegreesPerSecond = 720.0f;
    const float maxStep = kMaxTurnDegreesPerSecond * dt;
    const float yawStep = std::clamp(deltaYaw, -maxStep, maxStep);
    const float pitchStep = std::clamp(targetPitch - camera.pitch, -maxStep, maxStep);
    camera.yaw += yawStep;
    camera.pitch = std::clamp(camera.pitch + pitchStep, -89.0f, 89.0f);
    camera.updateVectors();
}

bool usableWeapon(const Player& player, const std::string& id)
{
    if (id.empty() || id == "nothing") return false;
    auto it = player.weaponRuntimes.find(id);
    if (it == player.weaponRuntimes.end()) return false;
    const WeaponDefinition* def = WeaponRegistry::instance().get(id);
    if (!def) return false;
    if (def->magazineSize <= 0) return true; // continuous-contact melee
    return it->second.currentAmmo > 0 || it->second.reserveAmmo > 0;
}

std::string chooseWeapon(const Player& player, const TargetView& target)
{
    if (!gMeleeWeapon.empty())
        return usableWeapon(player, gMeleeWeapon) ? gMeleeWeapon : std::string();

    // Visit the actor's resolved weapon runtimes rather than a hard-coded
    // subset. The server still owns equip, ammo, cadence, and damage checks;
    // this only ensures the AI actually tries every usable weapon in its set.
    std::vector<std::string> available;
    available.reserve(player.weaponRuntimes.size());
    for (const auto& entry : player.weaponRuntimes) {
        if (usableWeapon(player, entry.first))
            available.push_back(entry.first);
    }
    std::sort(available.begin(), available.end());
    if (available.empty()) {
        gWeaponVisit.clear();
        return {};
    }

    const uint32_t tick = static_cast<uint32_t>(player.movementSimulationTick);
    const bool visitExpired = gWeaponVisit.empty() ||
        tick >= gWeaponVisitEndsTick || !usableWeapon(player, gWeaponVisit);
    if (!visitExpired)
        return gWeaponVisit;

    const auto current = std::find(available.begin(), available.end(),
                                   player.equippedWeaponId);
    size_t start = current == available.end()
        ? 0u : (static_cast<size_t>(current - available.begin()) + 1u) % available.size();
    // Prefer legal weapons that fit the current range when several are
    // available, while continuing through the complete set over time.
    for (size_t offset = 0; offset < available.size(); ++offset) {
        const std::string& candidate = available[(start + offset) % available.size()];
        const bool explosive = candidate.find("rocket") != std::string::npos ||
                               candidate.find("grenade") != std::string::npos;
        if (target.distance < 5.0f && explosive)
            continue;
        gWeaponVisit = candidate;
        gWeaponVisitEndsTick = tick + 180u;
        return gWeaponVisit;
    }
    gWeaponVisit = available[start];
    gWeaponVisitEndsTick = tick + 180u;
    return gWeaponVisit;
}

void maybeEquipWeapon(const Player& player, const TargetView& target)
{
    static uint64_t lastAttemptTick = 0;
    const uint64_t tick = player.movementSimulationTick;
    if (tick < lastAttemptTick + 90u) return;
    const std::string from = player.equippedWeaponId;
    const std::string wanted = chooseWeapon(player, target);
    if (wanted.empty() || wanted == from) return;
    lastAttemptTick = tick;
    const int logicalSlot = meleeLogicalSlot(wanted);
    if (logicalSlot > 0 && !gMeleeWeapon.empty())
        Terminal::instance().execute("equipslot " + std::to_string(logicalSlot));
    else
        Terminal::instance().execute("equipweapon " + wanted);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "player.ai.equip", "player-ai", "normal equip command",
        (uint32_t)tick,
        {{"player_id", MP_CONTEXT.localPlayerId},
         {"from", from},
         {"to", wanted},
         {"logical_slot", logicalSlot},
         {"forced_melee", !gMeleeWeapon.empty()},
         {"target_distance", target.distance}},
        __FILE__, __LINE__, __FUNCTION__);
}

void checkPlayerFall(const Player& player, const World& world, uint32_t tick)
{
    const uint32_t generation = MP_CONTEXT.lastKnownSpawnGeneration;
    const bool hadGeneration = gLastSpawnGeneration != 0 && generation != 0;
    const bool respawned = hadGeneration && generation != gLastSpawnGeneration;
    const bool belowMap = !world.collisionMesh.triangles.empty() &&
        player.pos.z < world.collisionMesh.boundsMin.z - 2.0f;
    if (gLastSpawnGeneration == 0 && generation != 0)
        gLastSpawnGeneration = generation;
    const uint64_t now = aiNowMs();
    if ((respawned || belowMap || player.dead) && now - gLastFallLogMs >= 2000)
    {
        gLastFallLogMs = now;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Errors,
            "player.ai.fall_detected", "player-ai",
            belowMap ? "player below the map floor"
                     : (respawned ? "player respawned during AI mode"
                                  : "player died during AI mode"),
            tick,
            {{"player_id", MP_CONTEXT.localPlayerId},
             {"reason", belowMap ? "below_map"
                                 : (respawned ? "respawned" : "dead")},
             {"position", {player.pos.x, player.pos.y, player.pos.z}},
             {"spawn_generation", generation},
             {"previous_spawn_generation", gLastSpawnGeneration},
             {"dead", player.dead},
             {"weapon", gMeleeWeapon}},
            __FILE__, __LINE__, __FUNCTION__);
    }
    gLastSpawnGeneration = generation;
}

} // namespace

namespace PlayerAiController {

bool active() { return gActive; }

void setActive(bool enabled)
{
    gActive = enabled;
    if (!enabled) {
        gMeleeWeapon.clear();
        gLatchedTargetId = 0;
        resetRoute();
        gWeaponVisit.clear();
        gWeaponVisitEndsTick = 0;
        gNextProgressCheckTick = 0;
        gNoProgressSamples = 0;
        gRecoveryUntilTick = 0;
        gFreezeReleaseTick = 0;
        gExploreGoal = glm::vec3(0.0f);
        gExplorePlanTick = 0;
    }
}

void setMeleeWeapon(const std::string& weaponId)
{
    gMeleeWeapon = weaponId;
    gLatchedTargetId = 0;
    resetRoute();
    gExploreGoal = glm::vec3(0.0f);
    gExplorePlanTick = 0;
    gWeaponVisit.clear();
    gWeaponVisitEndsTick = 0;
}

const std::string& meleeWeapon() { return gMeleeWeapon; }

InputFrame buildFrame(const Player& player, Camera& camera,
                      const World& world)
{
    InputFrame frame;
    const uint32_t tick = (uint32_t)player.movementSimulationTick;
    const uint32_t playerId = MP_CONTEXT.localPlayerId;
    if (gActive)
        checkPlayerFall(player, world, tick);
    if (!gActive || player.dead) return frame;

    const TargetView target = chooseTarget(player, world);
    const WeaponDefinition* equippedDefinition =
        WeaponRegistry::instance().get(player.equippedWeaponId);
    const bool equippedMelee = equippedDefinition &&
        (equippedDefinition->behaviorType == WeaponBehaviorType::Melee ||
         equippedDefinition->behaviorType == WeaponBehaviorType::Swordsword ||
         equippedDefinition->behaviorType == WeaponBehaviorType::QuickHit ||
         equippedDefinition->behaviorType == WeaponBehaviorType::SpyKnife);
    const bool meleeMode = !gMeleeWeapon.empty() || equippedMelee;
    if (target.valid && target.distance <= 2.5f)
        resetRoute();

    const glm::vec3 eye = player.pos + glm::vec3(0.0f, 0.0f, 1.35f);
    const glm::vec3 aimPoint = target.pos + glm::vec3(0.0f, 0.0f, 0.85f);
    bool hasLineOfSight = target.valid && !worldBlocks(world, eye, aimPoint);

    float desiredYaw = camera.yaw;
    float desiredPitch = camera.pitch;
    glm::vec3 aimDirection(std::cos(glm::radians(camera.yaw)),
                           std::sin(glm::radians(camera.yaw)), 0.0f);
    glm::vec2 worldMove(0.0f);
    bool followingRoute = false;
    bool requestJump = false;

    if (target.valid && target.distance > 2.5f &&
        (meleeMode || !hasLineOfSight))
        planRoute(player, target, world);

    // Pursuit may use a known hostile position to route around cover, but the
    // client AI should not stare at a live through-wall position. Keep its
    // current heading until the target is visible, just as ranged fire is
    // already gated by line of sight below.
    if (target.valid) {
        if (hasLineOfSight) {
            glm::vec3 toTarget = aimPoint - eye;
            if (glm::length(toTarget) > 0.001f)
                aimDirection = glm::normalize(toTarget);

            desiredYaw = glm::degrees(std::atan2(aimDirection.y, aimDirection.x));
            desiredPitch = glm::degrees(std::asin(glm::clamp(aimDirection.z, -1.0f, 1.0f)));
            desiredPitch = glm::clamp(desiredPitch, -60.0f, 60.0f);
        }

        glm::vec2 planar(target.pos.x - player.pos.x,
                         target.pos.y - player.pos.y);
        const float planarDistance = glm::length(planar);
        if (planarDistance > 0.001f) {
            planar /= planarDistance;
            const glm::vec2 strafe(-planar.y, planar.x);
            const float strafePhase =
                std::sin((float)player.movementSimulationTick * 0.035f);
            if (meleeMode) {
                // Mode 1: close into the weapon volume and stay there instead
                // of holding a ranged stand-off distance.
                if (target.distance > 1.8f)
                    worldMove = planar;
                else if (target.distance < 0.8f)
                    worldMove = -planar * 0.6f;
                else
                    worldMove = strafe * (0.6f * strafePhase);
            } else if (target.distance > 6.0f) {
                worldMove = planar + strafe * (0.35f * strafePhase);
            } else if (target.distance < 2.4f) {
                worldMove = -planar + strafe * (0.5f * strafePhase);
            } else {
                worldMove = strafe * strafePhase + planar * 0.15f;
            }
            if (glm::length(worldMove) > 0.001f)
                worldMove = glm::normalize(worldMove);
        }

        if (meleeMode || !hasLineOfSight) {
            glm::vec2 routeDirection(0.0f);
            followingRoute = followRoute(player, routeDirection, requestJump);
            if (followingRoute)
                worldMove = routeDirection;
        }
    } else {
        // No known hostile: cover ground toward the farthest reachable region
        // instead of walking into the same wall forever. This is exploration,
        // not combat authority; weapons stay disarmed by the missing target.
        planExploreRoute(player, world);
        glm::vec2 routeDirection(0.0f);
        followingRoute = followRoute(player, routeDirection, requestJump);
        if (followingRoute) {
            worldMove = routeDirection;
        } else {
            worldMove = glm::vec2(std::cos(glm::radians(camera.yaw)),
                                  std::sin(glm::radians(camera.yaw)));
        }
    }

    worldMove = avoidWalls(world, player, worldMove);
    if (meleeMode)
        worldMove = avoidUnsafeGround(world, player, worldMove, followingRoute);

    const bool airborne = !player.ground.onGround;
    bool airStrafe = false;
    if (airborne && target.valid) {
        const glm::vec2 towardTarget(target.pos.x - player.pos.x,
                                     target.pos.y - player.pos.y);
        const float distance = glm::length(towardTarget);
        if (distance > 0.001f) {
            const glm::vec2 forward = towardTarget / distance;
            const glm::vec2 side(-forward.y, forward.x);
            const float phase = std::sin(
                static_cast<float>(tick) * 0.045f +
                static_cast<float>(MP_CONTEXT.localPlayerId % 7u));
            worldMove += side * (phase >= 0.0f ? 0.8f : -0.8f);
            if (glm::length(worldMove) > 0.001f)
                worldMove = glm::normalize(worldMove);
            airStrafe = true;
        }
    }

    // Detect sustained lack of progress and deliberately break the steering
    // symmetry that can keep the controller grinding against a corner. Every
    // recovery direction still passes the same world and ground probes as
    // ordinary navigation; jump/dash remain normal shared inputs.
    if (gNextProgressCheckTick == 0) {
        gNextProgressCheckTick = tick + 60u;
        gProgressCheckPosition = player.pos;
    } else if (tick >= gNextProgressCheckTick) {
        const float progress = glm::length(glm::vec2(
            player.pos.x - gProgressCheckPosition.x,
            player.pos.y - gProgressCheckPosition.y));
        if (glm::length(worldMove) > 0.1f && progress < 0.35f &&
            !player.freeze.freezeActive)
            ++gNoProgressSamples;
        else
            gNoProgressSamples = 0;

        gProgressCheckPosition = player.pos;
        gNextProgressCheckTick = tick + 60u;
        if (gNoProgressSamples >= 2) {
            const float angles[] = {90.0f, -90.0f, 135.0f, -135.0f, 180.0f};
            glm::vec2 base = glm::length(worldMove) > 0.001f
                ? glm::normalize(worldMove)
                : glm::vec2(std::cos(glm::radians(camera.yaw)),
                            std::sin(glm::radians(camera.yaw)));
            bool foundEscape = false;
            for (const float degrees : angles) {
                const float radians = glm::radians(degrees);
                const glm::vec2 candidate(
                    base.x * std::cos(radians) - base.y * std::sin(radians),
                    base.x * std::sin(radians) + base.y * std::cos(radians));
                const glm::vec3 eye(player.pos.x, player.pos.y,
                                    player.pos.z + 0.4f);
                const glm::vec3 end = eye +
                    glm::vec3(candidate.x, candidate.y, 0.0f) * 2.5f;
                if (worldBlocks(world, eye, end))
                    continue;
                float groundZ = 0.0f;
                if (player.ground.onGround &&
                    !groundAhead(world, player, candidate, 3.0f, groundZ))
                    continue;
                gRecoveryDirection = candidate;
                foundEscape = true;
                break;
            }
            if (!foundEscape)
                gRecoveryDirection = -base;
            gRecoveryUntilTick = tick + 45u;
            gNoProgressSamples = 0;
            requestJump = true;
        }
    }
    const bool progressRecovery = tick < gRecoveryUntilTick;
    if (progressRecovery && glm::length(gRecoveryDirection) > 0.001f) {
        worldMove = gRecoveryDirection;
        followingRoute = false;
        requestJump = true;
    }

    // Keep a little space from known teammates so they can take distinct
    // approach lanes. This uses the replicated roster only and never assumes
    // an actor with unknown team data is an ally or an enemy.
    int localTeam = player.matchTeam;
    if (localTeam < 0) {
        const uint8_t replicated =
            MimitaNet::CommunityMatchClient::instance().localTeam(playerId);
        if (replicated != 0xFF)
            localTeam = static_cast<int>(replicated);
    }
    glm::vec2 teammateSpacing(0.0f);
    int nearbyTeammates = 0;
    const auto addTeammateSpacing = [&](const Player& actor, uint32_t actorId) {
        if (actorId == playerId || actor.dead || actor.currentHp <= 0 ||
            localTeam < 0)
            return;
        int actorTeam = actor.matchTeam;
        if (actorTeam < 0) {
            const uint8_t replicated =
                MimitaNet::CommunityMatchClient::instance().teamForActor(actorId);
            if (replicated != 0xFF)
                actorTeam = static_cast<int>(replicated);
        }
        if (actorTeam != localTeam)
            return;
        const glm::vec2 away(player.pos.x - actor.pos.x,
                             player.pos.y - actor.pos.y);
        const float distance = glm::length(away);
        if (distance <= 0.001f || distance >= 3.5f)
            return;
        teammateSpacing += (away / distance) * ((3.5f - distance) / 3.5f);
        ++nearbyTeammates;
    };
    for (const auto& entry : MP_CONTEXT.remotePlayers)
        addTeammateSpacing(entry.second, entry.first);
    for (const auto& entry : MP_CONTEXT.remoteNpcs)
        addTeammateSpacing(entry.second, entry.first);
    if (!meleeMode && nearbyTeammates > 0 && glm::length(worldMove) > 0.001f) {
        worldMove += teammateSpacing * 0.35f;
        if (glm::length(worldMove) > 0.001f)
            worldMove = glm::normalize(worldMove);
        worldMove = avoidWalls(world, player, worldMove);
    }

    // The real camera is part of the player. Turn it smoothly toward the same
    // point the body/weapon is aiming at; otherwise engineTickCamera rebuilds
    // front from the stale yaw and syncPlayerYawFromCamera pulls the body back.
    if (target.valid)
        applySmoothAim(camera, desiredYaw, desiredPitch);

    const bool exploring = !target.valid && followingRoute;

    if (((tick + playerId) % 60u) == 0u) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "player.ai.state", "player-ai", target.valid ? "has_target" : "seeking",
            tick,
            {{"player_id", playerId},
             {"team", player.matchTeam},
             {"target_id", target.id},
             {"target_distance", target.distance},
             {"has_line_of_sight", hasLineOfSight},
             {"move", {worldMove.x, worldMove.y}},
             {"aim_yaw", desiredYaw},
             {"camera_yaw", camera.yaw},
             {"aim_pitch", desiredPitch},
             {"camera_pitch", camera.pitch},
             {"body_yaw", player.yaw},
             {"camera_body_error_deg", std::remainder(camera.yaw - player.yaw, 360.0f)},
             {"pos", {player.pos.x, player.pos.y, player.pos.z}},
             {"melee_weapon", gMeleeWeapon},
             {"following_route", followingRoute},
             {"exploring", exploring},
             {"explore_goal", {gExploreGoal.x, gExploreGoal.y, gExploreGoal.z}},
             {"route_nodes", gRoute.size() > gRouteIndex
                                 ? gRoute.size() - gRouteIndex : 0},
             {"on_ground", player.ground.onGround}},
            __FILE__, __LINE__, __FUNCTION__);
    }

    frame.moveX = worldMove.x;
    frame.moveY = worldMove.y;
    frame.movementPressed = glm::length(worldMove) > 0.001f;
    frame.movementJustPressed = frame.movementPressed;
    frame.lookYaw = camera.yaw;
    frame.lookPitch = camera.pitch;
    frame.camForward = camera.front;

    const glm::vec3 forward(std::cos(glm::radians(camera.yaw)),
                            std::sin(glm::radians(camera.yaw)), 0.0f);
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 0, 1)));
    frame.wishForward = glm::dot(glm::vec3(worldMove.x, worldMove.y, 0.0f), forward);
    frame.wishSide = glm::dot(glm::vec3(worldMove.x, worldMove.y, 0.0f), right);

    // Jump/dash are legal shared actions; use them sparingly to clear contact.
    if (meleeMode) {
        if (requestJump)
            frame.jumpPressed = true;
        if (target.valid && target.distance > 7.0f &&
            ((tick + playerId) % 83u) == 0u)
            frame.dashPressed = true;
    } else {
        if (requestJump ||
            (target.valid && target.distance < 3.0f &&
             ((tick + playerId) % 47u) == 0u))
            frame.jumpPressed = true;
        if (target.valid && target.distance > 7.0f &&
            ((tick + playerId) % 83u) == 0u)
            frame.dashPressed = true;
    }

    if (!target.valid && frame.movementPressed &&
        player.dash.dashAvailable && ((tick + playerId) % 97u) == 0u)
        frame.dashPressed = true;

    if (airborne && target.valid && target.distance > 5.0f &&
        player.vel.z < -1.0f && ((tick + playerId) % 37u) == 0u)
        frame.downDashPressed = true;
    if (!target.valid && airborne && player.vel.z < -1.0f &&
        ((tick + playerId) % 53u) == 0u)
        frame.downDashPressed = true;

    const float planarSpeed = std::sqrt(player.vel.x * player.vel.x +
                                        player.vel.y * player.vel.y);
    if (gFreezeReleaseTick > tick) {
        frame.freezeHeld = true;
    } else if (target.valid && player.ground.onGround &&
               planarSpeed < 2.0f && target.distance < 18.0f &&
               player.freeze.freezeAvailable &&
               ((tick + playerId) % 211u) == 0u) {
        // Freeze intentionally stops motion, so only spend it while already
        // nearly still and in a grounded firing engagement.
        frame.freezePressed = true;
        frame.freezeHeld = true;
        gFreezeReleaseTick = tick + 12u;
    } else if (!target.valid && progressRecovery && player.ground.onGround &&
               player.freeze.freezeAvailable &&
               ((tick + playerId) % 251u) == 0u) {
        // Use freeze only to break a stuck/blocked direction during
        // exploration; it is not a movement speed boost.
        frame.freezePressed = true;
        frame.freezeHeld = true;
        gFreezeReleaseTick = tick + 12u;
    }

    float aimError = 180.0f;
    if (target.valid && hasLineOfSight) {
        aimError = glm::degrees(std::acos(glm::clamp(
            glm::dot(aimDirection, camera.front), -1.0f, 1.0f)));
    }
    if (target.valid && hasLineOfSight && target.distance < 45.0f) {
        frame.primaryHeld = aimError < 12.0f;
        frame.secondaryHeld = target.distance < 8.0f && aimError < 18.0f;
    }
    if (meleeMode) {
        frame.primaryHeld = target.valid && hasLineOfSight &&
            target.distance < 4.8f && aimError < 20.0f;
        frame.secondaryHeld = false;
    }

    // Reload when the equipped weapon is empty. This is the same reload input
    // a human sends; the server still owns ammo acceptance.
    auto it = player.weaponRuntimes.find(player.equippedWeaponId);
    if (it != player.weaponRuntimes.end() && it->second.currentAmmo <= 0 &&
        it->second.reserveAmmo > 0 && !it->second.isReloading)
        frame.reloadPressed = true;

    if (target.valid)
        maybeEquipWeapon(player, target);

    if (((tick + playerId) % 60u) == 0u || frame.jumpPressed ||
        frame.dashPressed || frame.downDashPressed || frame.freezePressed) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "player.ai.action", "player-ai",
            progressRecovery ? "stuck recovery" : "player-like movement and combat intent",
            tick,
            {{"player_id", playerId},
             {"target_id", target.id},
             {"weapon", player.equippedWeaponId},
             {"move", {worldMove.x, worldMove.y}},
             {"jump", frame.jumpPressed},
             {"dash", frame.dashPressed},
             {"down_dash", frame.downDashPressed},
             {"freeze_pressed", frame.freezePressed},
             {"freeze_held", frame.freezeHeld},
             {"airborne", airborne},
             {"air_strafe", airStrafe},
             {"progress_recovery", progressRecovery},
             {"exploring", exploring},
             {"explore_goal", {gExploreGoal.x, gExploreGoal.y, gExploreGoal.z}},
             {"nearby_teammates", nearbyTeammates},
             {"target_distance", target.distance},
             {"position", {player.pos.x, player.pos.y, player.pos.z}},
             {"velocity", {player.vel.x, player.vel.y, player.vel.z}}},
            __FILE__, __LINE__, __FUNCTION__);
    }

    return frame;
}

} // namespace PlayerAiController
