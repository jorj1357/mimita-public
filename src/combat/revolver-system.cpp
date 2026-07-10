#include "combat/revolver-system.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <limits>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "audio/audio.h"
#include "combat/death-system.h"
#include "camera.h"
#include "config/player-settings.h"
#include "debug/debug-visuals.h"
#include "debug/debug-diag.h"
#include "debug/debug-log.h"
#include "devtools/terminal.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "entities/player.h"
#include "map/map_loader.h"
#include "npc/npc.h"
#include "physics/config.h"
#include "renderer/renderer.h"
#include "replay/replay.h"
#include "world/texture-store.h"
#include "world/world.h"

#include "ui/hitmarker.h"

extern Renderer* gRenderer;
extern TextureStore gTextures;

namespace {
constexpr const char* REVOLVER_MODEL_PATH = "assets/objects/weapons/mimita-revolver-v1.glb";
constexpr float MAX_SHOT_DISTANCE = 100.0f;

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

RevolverSystem::RevolverSystem() = default;

void RevolverSystem::loadHeldModel()
{
    if (mModelLoadAttempted)
        return;
    mModelLoadAttempted = true;
    mHeldMesh = loadGLB(REVOLVER_MODEL_PATH);
    if (mHeldMesh.verts.empty()) {
        printf("[REVOLVER] held model failed to load: %s\n", REVOLVER_MODEL_PATH);
        return;
    }

    glm::vec3 boundsMin = mHeldMesh.verts.front().pos;
    glm::vec3 boundsMax = boundsMin;
    for (const Vertex& vertex : mHeldMesh.verts) {
        boundsMin = glm::min(boundsMin, vertex.pos);
        boundsMax = glm::max(boundsMax, vertex.pos);
    }
    glm::vec3 size = boundsMax - boundsMin;
    int axis = size.y > size.x ? 1 : 0;
    if (size.z > size[axis])
        axis = 2;
    glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
    mModelGrip = center;
    mModelMuzzle = center;
    mModelGrip[axis] = boundsMin[axis];
    mModelMuzzle[axis] = boundsMax[axis];

    glGenVertexArrays(1, &mVao);
    glGenBuffers(1, &mVbo);
    glBindVertexArray(mVao);
    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    glBufferData(GL_ARRAY_BUFFER, mHeldMesh.verts.size() * sizeof(Vertex), mHeldMesh.verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
    glBindVertexArray(0);
    printf("[REVOLVER] held model loaded verts=%zu\n", mHeldMesh.verts.size());
}

void RevolverSystem::update(const Camera& camera, Player& player, float dt)
{
    loadHeldModel();
    mTime += dt;
    player.updateModelWorldTransforms();
    for (const PhysicalBodyPart& part : player.physicalBody.parts) {
        if (part.name != "rightArm")
            continue;

        glm::vec3 boundsSize = part.collider.localMax - part.collider.localMin;
        int axis = boundsSize.y > boundsSize.x ? 1 : 0;
        if (boundsSize.z > boundsSize[axis])
            axis = 2;
        glm::vec3 handPoint = (part.collider.localMin + part.collider.localMax) * 0.5f;
        float minDistance = std::fabs(part.collider.localMin[axis]);
        float maxDistance = std::fabs(part.collider.localMax[axis]);
        handPoint[axis] = maxDistance >= minDistance ? part.collider.localMax[axis] : part.collider.localMin[axis];
        glm::vec3 handDirection(0.0f);
        handDirection[axis] = handPoint[axis] >= 0.0f ? 1.0f : -1.0f;

        glm::vec3 modelDirection = glm::normalize(mModelMuzzle - mModelGrip);
        glm::quat gripRotation = glm::rotation(modelDirection, handDirection);
        mWeaponTransform = part.worldTransform *
                           glm::translate(glm::mat4(1.0f), handPoint) *
                           glm::mat4_cast(gripRotation) *
                           glm::translate(glm::mat4(1.0f), -mModelGrip);
        mMuzzle = glm::vec3(mWeaponTransform * glm::vec4(mModelMuzzle, 1.0f));
        mForward = glm::normalize(glm::vec3(mWeaponTransform * glm::vec4(modelDirection, 0.0f)));
        break;
    }
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
    mRecoil = std::max(0.0f, mRecoil - dt * 15.0f);
    mDisturbance = std::max(0.0f, mDisturbance - dt * 8.0f);
}

void RevolverSystem::render(const Camera& camera, const Player& player) const
{
    if (player.equippedSlot != 1 || !gRenderer || !gRenderer->shaderProgram || !mVao || mHeldMesh.verts.empty())
        return;

    const unsigned int shader = gRenderer->shaderProgram;
    glm::mat4 view = camera.getView();
    glm::mat4 projection = camera.getProj((float)gRenderer->width, (float)gRenderer->height);
    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, &projection[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, &mWeaponTransform[0][0]);
    glUniform1i(glGetUniformLocation(shader, "uUseColor"), 0);
    glUniform1i(glGetUniformLocation(shader, "uTex"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(mVao);
    for (const Mesh::Batch& batch : mHeldMesh.batches) {
        glBindTexture(GL_TEXTURE_2D, batch.texture ? batch.texture : gTextures.get("default"));
        glDrawArrays(GL_TRIANGLES, (GLint)batch.first, (GLsizei)batch.count);
        diagRenderCountWeaponDraw();
    }
    glBindVertexArray(0);
}

RevolverShotResult RevolverSystem::fire(const Camera& camera, Player& shooter, NpcSystem& npcs, const World& world)
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

    float cameraNearest = MAX_SHOT_DISTANCE;
    for (const CollisionTriangle& tri : world.collisionMesh.triangles) {
        float distance = 0.0f;
        if (rayTriangle(camera.pos, camera.front, tri, distance))
            cameraNearest = std::min(cameraNearest, distance);
    }
    for (Npc& npc : npcs.all()) {
        if (npc.body.currentHp <= 0) continue;
        npc.body.updateModelWorldTransforms();
        for (const PhysicalBodyPart& part : npc.body.physicalBody.parts) {
            glm::vec3 localCenter = (part.collider.localMin + part.collider.localMax) * 0.5f;
            glm::vec3 center = glm::vec3(part.worldTransform * glm::vec4(localCenter, 1.0f));
            glm::vec3 half = glm::max((part.collider.localMax - part.collider.localMin) * 0.5f, glm::vec3(0.12f));
            float distance = 0.0f;
            glm::vec3 normal;
            if (rayAabb(camera.pos, camera.front, center - half, center + half, distance, normal))
                cameraNearest = std::min(cameraNearest, distance);
        }
    }
    glm::vec3 cameraTarget = camera.pos + camera.front * cameraNearest;
    glm::vec3 shotDirection = cameraTarget - mMuzzle;
    if (glm::length(shotDirection) <= 0.001f)
        shotDirection = camera.front;
    shotDirection = glm::normalize(shotDirection);
    mForward = shotDirection;

    float nearest = MAX_SHOT_DISTANCE;
    bool hitWorld = false;
    glm::vec3 worldNormal = -shotDirection;
    for (const CollisionTriangle& tri : world.collisionMesh.triangles) {
        float distance = 0.0f;
        if (rayTriangle(mMuzzle, shotDirection, tri, distance) && distance < nearest) {
            nearest = distance;
            hitWorld = true;
            worldNormal = tri.normal;
        }
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
            if (rayAabb(mMuzzle, shotDirection, center - half, center + half, distance, normal) && distance < nearest) {
                nearest = distance;
                hitWorld = false;
                victim = &npc;
                hitPart = part.name;
                hitNormal = normal;
                glm::vec3 hit = mMuzzle + shotDirection * distance;
                localHeight = std::clamp((hit.z - (center.z - half.z)) / (half.z * 2.0f), 0.0f, 1.0f);
            }
        }
    }

    result.end = mMuzzle + shotDirection * nearest;
    ReplayEffectEvent gunshotEvent;
    gunshotEvent.type = "gunshot";
    gunshotEvent.position = mMuzzle;
    gunshotEvent.direction = shotDirection;
    gunshotEvent.from = mMuzzle;
    gunshotEvent.to = result.end;
    gunshotEvent.sourceActorId = shooter.username;
    captureReplayEffect(gunshotEvent);

    EffectPartSystem::instance().spawnMuzzleFlash(mMuzzle, shooter.username, shooter.sizeScale);
    EffectPartSystem::instance().spawnTracer(mMuzzle, result.end, shooter.username, shooter.sizeScale);
    if (victim) {
        float base = pointBlankDamage(hitPart, localHeight);
        float distanceFactor = std::clamp(1.0f - nearest / 110.0f, 0.10f, 1.0f);
        float angleFactor = std::clamp(std::fabs(glm::dot(-shotDirection, hitNormal)), 0.15f, 1.0f);
        float damage = std::min(base, std::max(base * distanceFactor * angleFactor, nearest >= 100.0f ? 10.0f : 1.0f));
        int rounded = std::max(1, (int)std::round(damage));
        float knockback = damage * distanceFactor * (0.08f + angleFactor * 0.12f);
        victim->body.currentHp = std::max(0, victim->body.currentHp - rounded);
        victim->body.externalImpulse += shotDirection * knockback + glm::vec3(0,0,knockback * 0.12f);
        victim->hitReactionTimer = 0.3f;
        result.hitEntity = true;
        hitmarker(rounded);
        result.bodyPart = hitPart;
        result.damage = (float)rounded;
        result.knockbackImpulse = shotDirection * knockback + glm::vec3(0, 0, knockback * 0.12f);
        {
            HitEvent ev;
            ev.position = result.end;
            ev.normal = hitNormal;
            ev.direction = shotDirection;
            ev.hitEntity = true;
            ev.damage = rounded;
            ev.attacker = shooter.username;
            ev.victim = "npc_" + std::to_string(victim->id);
            ev.weaponSource = "revolver";
            HitEffects::onHit(ev);
        }
        {
            float dist = glm::length(result.end - audioListenerPosition());
            float headMul = (hitPart == "head") ? 2.0f : 1.0f;
            float severity = std::clamp(angleFactor * ((float)rounded / 100.0f) * headMul, 0.0f, 1.0f);
            float vol, pit;
            computeImpactAudio(1.2f, dist, severity, vol, pit);
            playWorldSound("player_hurt", result.end, vol, pit, 60.0f);
            Debug::log(Debug::Category::Audio, "[HIT AUDIO] event=player_hurt dist=%.1f damage=%d severity=%.2f pitch=%.2f volume=%.2f\n",
                       dist, rounded, severity, pit, vol);
        }

        char debug[320];
        snprintf(debug, sizeof(debug),
                 "[DAMAGE] part=%s localHeight=%.2f distance=%.2fm angleFactor=%.2f damage=%d knockback=%.2f",
                 hitPart.c_str(), localHeight, nearest, angleFactor, rounded, knockback);
        if (GetPlayerSettings().debugCombat)
            Terminal::instance().addLog(debug);

        if (victim->body.currentHp <= 0) {
            DeathSystem::instance().kill(
                victim->body,
                "npc_" + std::to_string(victim->id),
                "npc",
                shooter.username,
                shotDirection,
                18.0f);
            std::string line = shooter.username + " killed " + victim->body.username + " with Revolver";
            addKill(line);
            Terminal::instance().addLog(line);
        }
    } else if (hitWorld) {
        {
            HitEvent ev;
            ev.position = result.end;
            ev.normal = worldNormal;
            ev.direction = shotDirection;
            ev.hitWorld = true;
            ev.damage = 0;
            ev.attacker = shooter.username;
            ev.weaponSource = "revolver";
            HitEffects::onHit(ev);
        }
        EffectPartSystem::instance().spawnWorldDebris(result.end, worldNormal, 1.0f, shooter.sizeScale);
        {
            float dist = glm::length(result.end - audioListenerPosition());
            float directness = std::abs(glm::dot(-shotDirection, worldNormal));
            float severity = std::clamp(directness, 0.0f, 1.0f);
            float vol, pit;
            computeImpactAudio(1.2f, dist, severity, vol, pit);
            playWorldSound("hitworld", result.end, vol, pit, 60.0f);
            Debug::log(Debug::Category::Audio, "[WORLD IMPACT AUDIO] dist=%.1f severity=%.2f pitch=%.2f volume=%.2f\n",
                       dist, severity, pit, vol);
        }
    }

    const PlayerSettings& cfg = GetPlayerSettings();
    float recoil = cfg.weaponRecoilStrength;
    // shooter.externalImpulse -= mForward * recoil;
    // dont push me in air so much 6 6 2026 when shooting 
    glm::vec3 recoilDir =
        glm::vec3(
            -shotDirection.x,
            -shotDirection.y,
            0.0f
        );

    if (glm::length(recoilDir) > 0.001f)
        recoilDir = glm::normalize(recoilDir);

    shooter.externalImpulse += recoilDir * recoil;
    
    mRecoil = std::min(mRecoil + recoil * 0.25f, 8.0f);
    mDisturbance += 1.2f;
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
