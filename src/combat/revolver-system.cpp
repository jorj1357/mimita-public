#include "combat/revolver-system.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <limits>

#include "audio/audio.h"
#include "camera.h"
#include "config/player-settings.h"
#include "debug/debug-visuals.h"
#include "devtools/terminal.h"
#include "effects/effect-part.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "physics/config.h"
#include "world/world.h"

namespace {
bool rayTriangle(glm::vec3 origin, glm::vec3 direction, const CollisionTriangle& tri, float& distance)
{
    glm::vec3 e1 = tri.b - tri.a;
    glm::vec3 e2 = tri.c - tri.a;
    glm::vec3 p = glm::cross(direction, e2);
    float det = glm::dot(e1, p);
    if (std::fabs(det) < 0.000001f) return false;
    float inv = 1.0f / det;
    glm::vec3 t = origin - tri.a;
    float u = glm::dot(t, p) * inv;
    if (u < 0.0f || u > 1.0f) return false;
    glm::vec3 q = glm::cross(t, e1);
    float v = glm::dot(direction, q) * inv;
    if (v < 0.0f || u + v > 1.0f) return false;
    distance = glm::dot(e2, q) * inv;
    return distance > 0.0f;
}

bool rayAabb(glm::vec3 origin, glm::vec3 direction, glm::vec3 mn, glm::vec3 mx, float& distance, glm::vec3& normal)
{
    float tmin = 0.0f;
    float tmax = 1000.0f;
    normal = glm::vec3(0.0f);
    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(direction[axis]) < 0.000001f) {
            if (origin[axis] < mn[axis] || origin[axis] > mx[axis]) return false;
            continue;
        }
        float inv = 1.0f / direction[axis];
        float a = (mn[axis] - origin[axis]) * inv;
        float b = (mx[axis] - origin[axis]) * inv;
        float sign = -1.0f;
        if (a > b) { std::swap(a, b); sign = 1.0f; }
        if (a > tmin) {
            tmin = a;
            normal = glm::vec3(0.0f);
            normal[axis] = sign;
        }
        tmax = std::min(tmax, b);
        if (tmin > tmax) return false;
    }
    distance = tmin;
    return distance >= 0.0f;
}

float pointBlankDamage(const std::string& part, float height)
{
    if (part == "head") return 100.0f;
    if (part == "torso") return height >= 0.5f ? 50.0f : 30.0f;
    if (part.find("Arm") != std::string::npos) return height >= 0.5f ? 20.0f : 10.0f;
    if (part.find("Leg") != std::string::npos) return height >= 0.5f ? 25.0f : 15.0f;
    return 10.0f;
}
}

void RevolverSystem::update(const Camera& camera, Player& player, float dt)
{
    const PlayerSettings& cfg = GetPlayerSettings();
    mTime += dt;
    glm::vec3 right = glm::normalize(glm::cross(camera.front, glm::vec3(0,0,1)));
    glm::vec3 up = glm::normalize(glm::cross(right, camera.front));
    float speed = glm::length(player.vel);
    float sway = cfg.weaponSwayStrength * (0.25f + std::min(speed / 20.0f, 2.0f));
    glm::vec3 targetForward = glm::normalize(camera.front + right * std::sin(mTime * 5.0f) * sway * 0.12f
                                             + up * (std::cos(mTime * 7.0f) * sway * 0.08f + mRecoil * 0.03f));
    float follow = 1.0f - std::exp(-cfg.weaponAimFollowSpeed * dt / std::max(cfg.weaponWeight, 0.1f));
    mForward = glm::normalize(glm::mix(mForward, targetForward, std::clamp(follow, 0.0f, 1.0f)));
    glm::vec3 targetPos = camera.pos + camera.front * 0.75f + right * 0.42f - up * 0.35f
                        - mForward * (mRecoil * 0.025f + mDisturbance * 0.05f);
    mPosition = glm::mix(mPosition, targetPos, std::clamp(follow, 0.0f, 1.0f));
    mMuzzle = mPosition + mForward * 0.72f;
    DebugVis::recordMovement(player.pos, player.externalImpulse * 0.1f, "weapon-recoil-impulse");
    
    // Apply recoil to player procedural animation (additive)
    if (player.equippedSlot == 1 && mRecoil > 0.01f) {
        for (PhysicalBodyPart& part : player.physicalBody.parts) {
            if (part.name == "rightArm") {
                // Additive recoil on top of procedural pose
                part.pose.rotationEuler.x -= mRecoil * 4.0f;
                part.pose.translation.y -= mRecoil * 0.015f;
                part.pose.translation.z += mRecoil * 0.02f;
                break;
            }
            if (part.name == "leftArm") {
                part.pose.rotationEuler.x -= mRecoil * 2.0f;
                part.pose.translation.z += mRecoil * 0.01f;
                break;
            }
        }
    }
    mRecoil = std::max(0.0f, mRecoil - dt * 7.0f);
    mDisturbance = std::max(0.0f, mDisturbance - dt * 4.0f);
}

void RevolverSystem::render(const Camera& camera, const World& world) const
{
    glm::vec3 center = mPosition + mForward * 0.32f;
    DebugVis::drawWireBox(camera, center, {0.12f, 0.12f, 0.42f}, {0.55f,0.55f,0.58f,1.0f});
    DebugVis::drawWireSphere(camera, mMuzzle, 0.10f, {1.0f,1.0f,1.0f,1.0f});
    float nearest = 100.0f;
    for (const CollisionTriangle& tri : world.collisionMesh.triangles) {
        float distance = 0.0f;
        if (rayTriangle(mMuzzle, mForward, tri, distance))
            nearest = std::min(nearest, distance);
    }
    DebugVis::drawLine(camera, mMuzzle, mMuzzle + mForward * nearest, {1.0f,0.25f,0.1f,0.85f});
}

RevolverShotResult RevolverSystem::fire(Player& shooter, NpcSystem& npcs, const World& world)
{
    RevolverShotResult result;
    if (shooter.equippedSlot != 1 || shooter.revolverCylinder <= 0)
        return result;
    result.fired = true;
    result.start = mMuzzle;
    shooter.revolverCylinder--;
    // Random pitch ±1% and volume ±1% to avoid robotic identical playback
    float rndPitch = 1.0f + ((rand() % 201 - 100) / 10000.0f);  // 0.99 - 1.01
    float rndVolume = 1.0f + ((rand() % 201 - 100) / 10000.0f); // 0.99 - 1.01
    playWorldSound("revolvershoot", mMuzzle, rndVolume, rndPitch, 80.0f);

    float nearest = 100.0f;
    for (const CollisionTriangle& tri : world.collisionMesh.triangles) {
        float distance = 0.0f;
        if (rayTriangle(mMuzzle, mForward, tri, distance))
            nearest = std::min(nearest, distance);
    }

    Npc* victim = nullptr;
    std::string hitPart;
    glm::vec3 hitNormal{0.0f};
    float localHeight = 0.5f;
    for (Npc& npc : npcs.all()) {
        if (npc.body.currentHp <= 0) continue;
        npc.body.updateModelWorldTransforms();
        for (const PhysicalBodyPart& part : npc.body.physicalBody.parts) {
            glm::vec3 localCenter = (part.collider.localMin + part.collider.localMax) * 0.5f;
            glm::vec3 center = glm::vec3(part.worldTransform * glm::vec4(localCenter, 1.0f));
            glm::vec3 half = (part.collider.localMax - part.collider.localMin) * 0.5f;
            half = glm::max(half, glm::vec3(0.12f));
            float distance = 0.0f;
            glm::vec3 normal;
            if (rayAabb(mMuzzle, mForward, center - half, center + half, distance, normal) && distance < nearest) {
                nearest = distance;
                victim = &npc;
                hitPart = part.name;
                hitNormal = normal;
                glm::vec3 hit = mMuzzle + mForward * distance;
                localHeight = std::clamp((hit.z - (center.z - half.z)) / (half.z * 2.0f), 0.0f, 1.0f);
            }
        }
    }

    result.end = mMuzzle + mForward * nearest;
    if (victim) {
        float base = pointBlankDamage(hitPart, localHeight);
        float distanceFactor = std::clamp(1.0f - nearest / 110.0f, 0.10f, 1.0f);
        float angleFactor = std::clamp(std::fabs(glm::dot(-mForward, hitNormal)), 0.15f, 1.0f);
        float damage = std::min(base, std::max(base * distanceFactor * angleFactor, nearest >= 100.0f ? 10.0f : 1.0f));
        int rounded = std::max(1, (int)std::round(damage));
        float knockback = damage * distanceFactor * (0.08f + angleFactor * 0.12f);
        victim->body.currentHp = std::max(0, victim->body.currentHp - rounded);
        victim->body.vel += mForward * knockback + glm::vec3(0,0,knockback * 0.12f);
        result.hitEntity = true;
        result.bodyPart = hitPart;
        result.damage = (float)rounded;

        EffectPartSystem::instance().spawnDamage(result.end, victim->body.username, rounded);
        // Use projected blood instead of old sticky blood
        EffectPartSystem::instance().spawnProjectedBlood(result.end, mForward, rounded, nearest, hitPart, world);
        float hurt01 = std::clamp(damage / 100.0f, 0.0f, 1.0f);
        playWorldSound("gethurt", result.end, 0.35f + hurt01 * 0.65f, 1.35f - hurt01 * 0.55f, 35.0f);

        char debug[320];
        snprintf(debug, sizeof(debug),
                 "[DAMAGE] part=%s localHeight=%.2f distance=%.2fm angleFactor=%.2f damage=%d knockback=%.2f",
                 hitPart.c_str(), localHeight, nearest, angleFactor, rounded, knockback);
        if (GetPlayerSettings().debugCombat)
            Terminal::instance().addLog(debug);

        if (victim->body.currentHp <= 0) {
            std::string line = shooter.username + " killed " + victim->body.username + " with Revolver";
            addKill(line);
            Terminal::instance().addLog(line);
        }
    } else {
        EffectPartSystem::instance().spawnWorldImpact(result.end, -mForward);
    }

    const PlayerSettings& cfg = GetPlayerSettings();
    float recoil = cfg.weaponRecoilStrength;
    // shooter.externalImpulse -= mForward * recoil;
    // dont push me in air so much 6 6 2026 when shooting 
    glm::vec3 recoilDir =
        glm::vec3(
            -mForward.x,
            -mForward.y,
            0.0f
        );

    if (glm::length(recoilDir) > 0.001f)
        recoilDir = glm::normalize(recoilDir);

    shooter.externalImpulse += recoilDir * recoil;
    
    mRecoil = std::min(mRecoil + recoil * 0.12f, 5.0f);
    mDisturbance += 0.6f;
    return result;
}

void RevolverSystem::disturb(float amount)
{
    mDisturbance = std::max(mDisturbance, amount);
}

void RevolverSystem::addKill(const std::string& line)
{
    mKillfeed.push_back(line);
    if (mKillfeed.size() > 20)
        mKillfeed.erase(mKillfeed.begin());
}
