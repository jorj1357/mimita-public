// 2026-11-01
/* purpose
* Minimal live-play AI for the real local player used by unattended TDM runs.
* It selects a hostile replicated actor, approaches/strafes, aims, and fires
* through the normal InputFrame/ActorIntent boundary.
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
uint64_t gLastFallbackMs = 0;
int gFallbackSpawns = 0;

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

struct TargetView {
    bool valid = false;
    uint32_t id = 0;
    int team = -1;
    glm::vec3 pos{0.0f};
    float distance = 0.0f;
};

bool hostile(const Player& player, const Player& candidate)
{
    if (candidate.dead || candidate.currentHp <= 0) return false;
    int mine = player.matchTeam;
    if (mine < 0) {
        const uint8_t replicated =
            MimitaNet::CommunityMatchClient::instance().localTeam(
                MP_CONTEXT.localPlayerId);
        if (replicated != 0xFF) mine = (int)replicated;
    }
    const int theirs = candidate.matchTeam;
    if (mine < 0 || theirs < 0) return true;
    return mine != theirs;
}

TargetView chooseTarget(const Player& player)
{
    TargetView best;
    if (!gMeleeWeapon.empty() && gLatchedTargetId != 0) {
        const auto latch = [&](const std::unordered_map<uint32_t, Player>& map) {
            auto it = map.find(gLatchedTargetId);
            if (it == map.end() || !hostile(player, it->second))
                return false;
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
    float bestScore = std::numeric_limits<float>::max();
    const auto consider = [&](const Player& candidate, uint32_t id) {
        if (!hostile(player, candidate)) return;
        const glm::vec3 delta = candidate.pos - player.pos;
        const float distance = glm::length(delta);
        // A melee validation run should not chase a target across the map.
        // When no local hostile exists, the fallback spawn (and the normal
        // match AI) can bring one into range.
        if (!gMeleeWeapon.empty() && distance > 25.0f) return;
        const glm::vec3 planar(delta.x, delta.y, 0.0f);
        const float len = glm::length(planar);
        float score = distance;
        if (len > 0.001f) {
            const glm::vec3 facing(std::cos(glm::radians(player.yaw)),
                                   std::sin(glm::radians(player.yaw)), 0.0f);
            score -= 0.75f * glm::dot(planar / len, facing);
        }
        if (score < bestScore) {
            bestScore = score;
            best.valid = true;
            best.id = id;
            best.team = candidate.matchTeam;
            best.pos = candidate.pos;
            best.distance = distance;
        }
    };
    for (const auto& entry : MP_CONTEXT.remoteNpcs)
        consider(entry.second, entry.first);
    for (const auto& entry : MP_CONTEXT.remotePlayers)
        consider(entry.second, entry.first);
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
    const bool need = gRoute.empty() || gRouteTargetId != target.id ||
        goalMoved || tick >= gRoutePlanTick;
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
    static const char* closeWeapons[] = {
        "chainsaw", "swordsword", "mace", "shotgun", "revolver"};
    static const char* midWeapons[] = {
        "shotgun", "revolver", "rocket_launcher", "chainsaw"};
    static const char* farWeapons[] = {
        "revolver", "rocket_launcher", "shotgun"};
    const char* const* list = midWeapons;
    size_t count = sizeof(midWeapons) / sizeof(midWeapons[0]);
    if (target.distance < 4.0f) {
        list = closeWeapons;
        count = sizeof(closeWeapons) / sizeof(closeWeapons[0]);
    } else if (target.distance > 18.0f) {
        list = farWeapons;
        count = sizeof(farWeapons) / sizeof(farWeapons[0]);
    }
    for (size_t i = 0; i < count; ++i)
        if (usableWeapon(player, list[i])) return list[i];
    return {};
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

void maybeRequestFallback(const TargetView& target, uint32_t tick)
{
    if (gMeleeWeapon.empty() || gFallbackSpawns >= 12)
        return;
    if (target.valid && target.distance < 8.0f)
        return;
    const uint64_t now = aiNowMs();
    if (now - gLastFallbackMs < 8000)
        return;
    gLastFallbackMs = now;
    ++gFallbackSpawns;
    Terminal::instance().execute("npc_spawn_near 1.5 2");
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "player.ai.fallback_spawn", "player-ai",
        "requested a controlled NPC near the player because no close target existed",
        tick,
        {{"player_id", MP_CONTEXT.localPlayerId},
         {"weapon", gMeleeWeapon},
         {"target_valid", target.valid},
         {"target_distance", target.distance},
         {"fallback_count", gFallbackSpawns}},
        __FILE__, __LINE__, __FUNCTION__);
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
    }
}

void setMeleeWeapon(const std::string& weaponId)
{
    gMeleeWeapon = weaponId;
    gLatchedTargetId = 0;
    resetRoute();
    gFallbackSpawns = 0;
    gLastFallbackMs = 0;
}

const std::string& meleeWeapon() { return gMeleeWeapon; }

InputFrame buildFrame(const Player& player, Camera& camera,
                      const World& world)
{
    InputFrame frame;
    const uint32_t tick = (uint32_t)player.movementSimulationTick;
    if (gActive)
        checkPlayerFall(player, world, tick);
    if (!gActive || player.dead) return frame;

    const TargetView target = chooseTarget(player);
    const bool meleeMode = !gMeleeWeapon.empty();
    if (!meleeMode || !target.valid || target.distance <= 2.5f)
        resetRoute();

    const glm::vec3 eye = player.pos + glm::vec3(0.0f, 0.0f, 1.35f);

    float desiredYaw = camera.yaw;
    float desiredPitch = camera.pitch;
    glm::vec3 aimDirection(std::cos(glm::radians(camera.yaw)),
                           std::sin(glm::radians(camera.yaw)), 0.0f);
    glm::vec2 worldMove(0.0f);
    bool hasLineOfSight = false;
    bool followingRoute = false;
    bool requestJump = false;

    if (meleeMode && target.valid && target.distance > 2.5f)
        planRoute(player, target, world);

    if (target.valid) {
        const glm::vec3 aimPoint = target.pos + glm::vec3(0.0f, 0.0f, 0.85f);
        glm::vec3 toTarget = aimPoint - eye;
        if (glm::length(toTarget) > 0.001f)
            aimDirection = glm::normalize(toTarget);

        desiredYaw = glm::degrees(std::atan2(aimDirection.y, aimDirection.x));
        desiredPitch = glm::degrees(std::asin(glm::clamp(aimDirection.z, -1.0f, 1.0f)));
        desiredPitch = glm::clamp(desiredPitch, -60.0f, 60.0f);

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

        if (meleeMode) {
            glm::vec2 routeDirection(0.0f);
            followingRoute = followRoute(player, routeDirection, requestJump);
            if (followingRoute)
                worldMove = routeDirection;
        }

        hasLineOfSight = !worldBlocks(world, eye, aimPoint);
    } else {
        // No known hostile: keep moving forward so the match keeps producing
        // contact opportunities instead of standing at spawn.
        worldMove = glm::vec2(std::cos(glm::radians(camera.yaw)),
                              std::sin(glm::radians(camera.yaw)));
    }

    worldMove = avoidWalls(world, player, worldMove);
    if (meleeMode)
        worldMove = avoidUnsafeGround(world, player, worldMove, followingRoute);

    // The real camera is part of the player. Turn it smoothly toward the same
    // point the body/weapon is aiming at; otherwise engineTickCamera rebuilds
    // front from the stale yaw and syncPlayerYawFromCamera pulls the body back.
    if (target.valid)
        applySmoothAim(camera, desiredYaw, desiredPitch);

    const uint32_t playerId = MP_CONTEXT.localPlayerId;
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
        if (target.valid && target.distance > 7.0f && (tick % 83u) == 0u)
            frame.dashPressed = true;
    } else {
        if (target.valid && target.distance < 3.0f && (tick % 47u) == 0u)
            frame.jumpPressed = true;
        if (target.valid && target.distance > 7.0f && (tick % 83u) == 0u)
            frame.dashPressed = true;
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
    if (meleeMode)
        maybeRequestFallback(target, tick);

    return frame;
}

} // namespace PlayerAiController
