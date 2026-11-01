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

#include <glm/glm.hpp>

#include "camera.h"
#include "entities/player.h"
#include "combat/weapon-registry.h"
#include "devtools/terminal.h"
#include "input/input-frame.h"
#include "network/community-match-client.h"
#include "network/multiplayer-context.h"
#include "physics/movement/physics-collision.h"
#include "physics/config.h"
#include "terminal/terminal-state.h"
#include "debug/structured-log.h"

namespace {

bool gActive = false;

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
    float bestScore = std::numeric_limits<float>::max();
    const auto consider = [&](const Player& candidate, uint32_t id) {
        if (!hostile(player, candidate)) return;
        const glm::vec3 delta = candidate.pos - player.pos;
        const float distance = glm::length(delta);
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
    return best;
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
    Terminal::instance().execute("equipweapon " + wanted);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "player.ai.equip", "player-ai", "normal equip command",
        (uint32_t)tick,
        {{"player_id", MP_CONTEXT.localPlayerId},
         {"from", from},
         {"to", wanted},
         {"target_distance", target.distance}},
        __FILE__, __LINE__, __FUNCTION__);
}

} // namespace

namespace PlayerAiController {

bool active() { return gActive; }

void setActive(bool enabled) { gActive = enabled; }

InputFrame buildFrame(const Player& player, Camera& camera,
                      const World& world)
{
    InputFrame frame;
    if (!gActive || player.dead) return frame;

    const TargetView target = chooseTarget(player);
    const glm::vec3 eye = player.pos + glm::vec3(0.0f, 0.0f, 1.35f);

    float desiredYaw = camera.yaw;
    float desiredPitch = camera.pitch;
    glm::vec3 aimDirection(std::cos(glm::radians(camera.yaw)),
                           std::sin(glm::radians(camera.yaw)), 0.0f);
    glm::vec2 worldMove(0.0f);
    bool hasLineOfSight = false;

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
            if (target.distance > 6.0f)
                worldMove = planar + strafe * (0.35f * strafePhase);
            else if (target.distance < 2.4f)
                worldMove = -planar + strafe * (0.5f * strafePhase);
            else
                worldMove = strafe * strafePhase + planar * 0.15f;
            if (glm::length(worldMove) > 0.001f)
                worldMove = glm::normalize(worldMove);
        }

        hasLineOfSight = !worldBlocks(world, eye, aimPoint);
    } else {
        // No known hostile: keep moving forward so the match keeps producing
        // contact opportunities instead of standing at spawn.
        worldMove = glm::vec2(std::cos(glm::radians(camera.yaw)),
                              std::sin(glm::radians(camera.yaw)));
    }

    worldMove = avoidWalls(world, player, worldMove);

    // The real camera is part of the player. Turn it smoothly toward the same
    // point the body/weapon is aiming at; otherwise engineTickCamera rebuilds
    // front from the stale yaw and syncPlayerYawFromCamera pulls the body back.
    if (target.valid)
        applySmoothAim(camera, desiredYaw, desiredPitch);

    const uint32_t tick = (uint32_t)player.movementSimulationTick;
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
             {"pos", {player.pos.x, player.pos.y, player.pos.z}}},
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
    if (target.valid && target.distance < 3.0f && (tick % 47u) == 0u)
        frame.jumpPressed = true;
    if (target.valid && target.distance > 7.0f && (tick % 83u) == 0u)
        frame.dashPressed = true;

    if (target.valid && hasLineOfSight && target.distance < 45.0f) {
        const float aimError = glm::degrees(std::acos(glm::clamp(
            glm::dot(aimDirection, camera.front), -1.0f, 1.0f)));
        frame.primaryHeld = aimError < 12.0f;
        frame.secondaryHeld = target.distance < 8.0f && aimError < 18.0f;
    }

    // Reload when the equipped weapon is empty. This is the same reload input
    // a human sends; the server still owns ammo acceptance.
    auto it = player.weaponRuntimes.find(player.equippedWeaponId);
    if (it != player.weaponRuntimes.end() && it->second.currentAmmo <= 0 &&
        it->second.reserveAmmo > 0 && !it->second.isReloading)
        frame.reloadPressed = true;

    if (target.valid)
        maybeEquipWeapon(player, target);

    return frame;
}

} // namespace PlayerAiController
