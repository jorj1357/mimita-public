#include "persistent-physics.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "audio/audio.h"
#include "camera.h"
#include "config.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "physics/movement/physics-collision.h"
#include "physics/physics-types.h"
#include "renderer/renderer.h"
#include "replay/replay.h"
#include "world/world.h"

extern Renderer* gRenderer;

PersistentPhysicsSystem& PersistentPhysicsSystem::instance()
{
    static PersistentPhysicsSystem sys;
    return sys;
}

uint32_t PersistentPhysicsSystem::spawn(
    const PersistentPhysicsConfig& cfg,
    const glm::vec3& position,
    const glm::vec3& velocity,
    const glm::vec3& angularVelocity,
    uint32_t ownerId,
    const std::string& ownerName,
    const std::string& weaponId)
{
    PersistentPhysicsObject obj;
    obj.id = mNextId++;
    obj.ownerId = ownerId;
    obj.ownerName = ownerName;
    obj.cfg = cfg;
    obj.position = position;
    obj.velocity = velocity;
    obj.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    obj.angularVelocity = angularVelocity;
    obj.age = 0.0f;
    obj.bounceCount = 0;
    obj.exploded = false;
    obj.sleeping = false;
    obj.sleepTimer = 0.0f;
    obj.alive = true;
    obj.weaponId = weaponId;

    if (!cfg.spawnSound.empty())
        playWorldSound(cfg.spawnSound, position, 1.0f, 1.0f, 50.0f);

    ReplayEffectEvent spawnEvent;
    spawnEvent.type = "persistent_spawn";
    spawnEvent.position = position;
    spawnEvent.direction = velocity;
    spawnEvent.velocity = angularVelocity;
    spawnEvent.sourceActorId = ownerName;
    spawnEvent.targetActorId = weaponId + "_" + std::to_string(obj.id);
    spawnEvent.scale = {cfg.radius, cfg.height, (float)cfg.shape};
    spawnEvent.lifetime = cfg.lifetime;
    captureReplayEffect(spawnEvent);

    FILE* f = fopen("logs/persistent_physics_debug.txt", "a");
    if (f) {
        fprintf(f, "SPAWN id=%u type=%s owner=%s pos=(%.2f %.2f %.2f) vel=(%.2f %.2f %.2f) angVel=(%.2f %.2f %.2f)\n",
                obj.id, weaponId.c_str(), ownerName.c_str(),
                position.x, position.y, position.z,
                velocity.x, velocity.y, velocity.z,
                angularVelocity.x, angularVelocity.y, angularVelocity.z);
        fclose(f);
    }

    mObjects.push_back(std::move(obj));
    return mObjects.back().id;
}

void PersistentPhysicsSystem::update(float dt, const World& world, Player& player, NpcSystem& npcs, Camera* camera)
{
    for (auto it = mObjects.begin(); it != mObjects.end();) {
        PersistentPhysicsObject& obj = *it;

        if (obj.exploded) {
            it = mObjects.erase(it);
            continue;
        }

        obj.age += dt;
        if (obj.cfg.lifetime > 0.0f && obj.age >= obj.cfg.lifetime) {
            if (obj.cfg.explosionRadius > 0.0f) {
                doExplosion(obj, world, player, npcs, camera);
            }
            it = mObjects.erase(it);
            continue;
        }

        physicsStep(obj, dt, world);
        checkCollisions(obj, dt, world, player, npcs);

        if (!obj.exploded)
            ++it;
    }
}

void PersistentPhysicsSystem::physicsStep(PersistentPhysicsObject& obj, float dt, const World& world)
{
    if (obj.sleeping) return;

    // Apply gravity
    obj.velocity.z -= obj.cfg.gravity * dt;

    // Apply drag
    obj.velocity *= (1.0f - obj.cfg.drag * dt);
    if (glm::length(obj.velocity) < 0.001f) obj.velocity = glm::vec3(0.0f);
    obj.angularVelocity *= (1.0f - obj.cfg.angularDrag * dt);

    // Clamp velocities
    float speed = glm::length(obj.velocity);
    if (speed > obj.cfg.maxLinearVelocity)
        obj.velocity *= obj.cfg.maxLinearVelocity / speed;
    float angSpeed = glm::length(obj.angularVelocity);
    if (angSpeed > obj.cfg.maxAngularVelocity)
        obj.angularVelocity *= obj.cfg.maxAngularVelocity / angSpeed;

    // Apply rotation
    if (angSpeed > 0.0001f) {
        glm::quat delta = glm::angleAxis(angSpeed * dt, glm::normalize(obj.angularVelocity));
        obj.rotation = glm::normalize(delta * obj.rotation);
    }

    // Integrate position
    obj.position += obj.velocity * dt;

    // Sleep check
    if (speed < obj.cfg.sleepVelocity && angSpeed < obj.cfg.sleepAngular) {
        obj.sleepTimer += dt;
        if (obj.sleepTimer >= obj.cfg.sleepTime)
            obj.sleeping = true;
    } else {
        obj.sleepTimer = 0.0f;
    }
}

static glm::vec3 closestPointOnTriangleFn(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    glm::vec3 ab = b - a;
    glm::vec3 ac = c - a;
    glm::vec3 ap = p - a;
    float d1 = glm::dot(ab, ap);
    float d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return a;
    glm::vec3 bp = p - b;
    float d3 = glm::dot(ab, bp);
    float d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return b;
    glm::vec3 cp = p - c;
    float d5 = glm::dot(ab, cp);
    float d6 = glm::dot(ac, cp);
    if (d5 >= 0.0f && d6 >= 0.0f) return c;
    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        float v = d1 / (d1 - d3);
        return a + v * ab;
    }
    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        float w = d2 / (d2 - d6);
        return a + w * ac;
    }
    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + w * (c - b);
    }
    float denom = va + vb + vc;
    if (denom == 0.0f) return a;
    float u = va / denom;
    float v = vb / denom;
    float w = vc / denom;
    return u * a + v * b + w * c;
}

void PersistentPhysicsSystem::checkCollisions(
    PersistentPhysicsObject& obj, float dt,
    const World& world, Player& player, NpcSystem& npcs)
{
    if (obj.sleeping || obj.exploded) return;

    float r = obj.cfg.radius;
    float maxStep = std::max(r * 0.5f, 0.1f);
    int steps = std::min((int)std::ceil(glm::length(obj.velocity * dt) / maxStep) + 1, 8);
    glm::vec3 stepVel = obj.velocity * dt / (float)steps;

    for (int s = 0; s < steps && !obj.exploded; ++s) {
        obj.position += stepVel;

        if (!obj.cfg.collideWithWorld) continue;
        float radius = r;
        AABB queryBounds;
        queryBounds.min = obj.position - glm::vec3(radius + 0.5f);
        queryBounds.max = obj.position + glm::vec3(radius + 0.5f);
        std::vector<int> candidates;
        appendChunkTrianglesForAABB(const_cast<World&>(world), queryBounds, 0.1f, candidates);

        for (int ti : candidates) {
            if (ti < 0 || ti >= (int)world.collisionMesh.triangles.size()) continue;
            const CollisionTriangle& tri = world.collisionMesh.triangles[ti];
            glm::vec3 closest = closestPointOnTriangleFn(obj.position, tri.a, tri.b, tri.c);
            glm::vec3 diff = obj.position - closest;
            float dist = glm::length(diff);
            if (dist < radius && dist > 0.0001f) {
                glm::vec3 normal = diff / dist;
                float penetration = radius - dist;
                obj.position += normal * penetration;

                float velDot = glm::dot(obj.velocity, normal);
                if (velDot < 0.0f) {
                    glm::vec3 tangent = obj.velocity - normal * velDot;
                    obj.velocity -= normal * velDot * (1.0f + obj.cfg.restitution);
                    obj.velocity += tangent * obj.cfg.friction;
                    obj.bounceCount++;

                    float energyBefore = 0.5f * glm::dot(obj.velocity + normal * velDot, obj.velocity + normal * velDot);
                    float energyAfter = 0.5f * glm::dot(obj.velocity, obj.velocity);
                    float energyLost = energyBefore - energyAfter;

                    FILE* f = fopen("logs/persistent_physics_debug.txt", "a");
                    if (f) {
                        fprintf(f, "COLLISION id=%u pos=(%.2f %.2f %.2f) normal=(%.2f %.2f %.2f) "
                                   "velBefore=(%.2f %.2f %.2f) velAfter=(%.2f %.2f %.2f) "
                                   "energyLost=%.2f bounce=%d sleeping=%d\n",
                                obj.id, obj.position.x, obj.position.y, obj.position.z,
                                normal.x, normal.y, normal.z,
                                obj.velocity.x + normal.x * velDot,
                                obj.velocity.y + normal.y * velDot,
                                obj.velocity.z + normal.z * velDot,
                                obj.velocity.x, obj.velocity.y, obj.velocity.z,
                                energyLost, obj.bounceCount, (int)obj.sleeping);
                        fclose(f);
                    }
                }
                break;
            }
        }
    }

    // Player collision
    if (obj.cfg.collideWithPlayer && !player.dead) {
        glm::vec3 toPlayer = player.pos - obj.position;
        float dist = glm::length(toPlayer);
        float hitDist = r + 0.5f;
        if (dist < hitDist) {
            doExplosion(obj, world, player, npcs, nullptr);
            return;
        }
    }

    // NPC collision
    if (obj.cfg.collideWithNpcs) {
        for (const Npc& npc : npcs.all()) {
            if (npc.body.dead) continue;
            glm::vec3 toNpc = npc.body.pos - obj.position;
            float dist = glm::length(toNpc);
            float hitDist = r + 0.5f;
            if (dist < hitDist) {
                doExplosion(obj, world, player, npcs, nullptr);
                return;
            }
        }
    }
}

void PersistentPhysicsSystem::doExplosion(
    PersistentPhysicsObject& obj,
    const World& world, Player& player, NpcSystem& npcs, Camera* camera)
{
    if (obj.exploded) return;
    obj.exploded = true;

    glm::vec3 pos = obj.position;
    float radius = obj.cfg.explosionRadius;
    float baseDamage = obj.cfg.explosionDamage;
    float knockbackStrength = obj.cfg.explosionKnockback;

    if (!obj.cfg.explosionSound.empty())
        playWorldSound(obj.cfg.explosionSound, pos, 1.0f, 1.0f, 50.0f);

    {
        HitEvent ev;
        ev.position = pos;
        ev.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        ev.direction = glm::vec3(0.0f);
        ev.hitWorld = true;
        ev.hitEntity = false;
        ev.damage = 0;
        ev.attacker = obj.ownerName;
        ev.weaponSource = obj.weaponId;
        HitEffects::onHit(ev);
    }

    EffectPartSystem::instance().spawnMuzzleFlash(pos, obj.weaponId + "_explosion");
    EffectPartSystem::instance().spawnWorldDebris(pos, glm::vec3(0.0f, 0.0f, 1.0f), 3.0f);

    if (camera) {
        float distToCam = glm::length(pos - camera->pos);
        float shakeStrength = std::clamp(1.0f - distToCam / radius, 0.0f, 1.0f);
        camera->addPunch(shakeStrength * 4.0f, shakeStrength * 2.0f);
    }

    for (Npc& npc : npcs.all()) {
        if (npc.body.currentHp <= 0) continue;
        glm::vec3 toEntity = npc.body.pos - pos;
        float dist = glm::length(toEntity);
        if (dist >= radius) continue;
        glm::vec3 dir = dist > 0.001f ? toEntity / dist : glm::vec3(0.0f, 1.0f, 0.0f);
        float dmgScaled = baseDamage * std::exp(-std::pow(dist / radius, 2.0f) * 2.0f);
        int finalDmg = std::max(1, (int)std::round(dmgScaled));
        npc.body.currentHp -= finalDmg;
        if (npc.body.currentHp < 0) npc.body.currentHp = 0;
        float t = dist / radius;
        float knockScale = (1.0f - t * t) * 0.85f + 0.15f;
        npc.body.externalImpulse += dir * knockbackStrength * knockScale;
        HitEvent ev;
        ev.position = npc.body.pos;
        ev.normal = dir;
        ev.direction = -dir;
        ev.hitEntity = true;
        ev.damage = finalDmg;
        ev.attacker = obj.ownerName;
        ev.victim = npc.body.username;
        ev.weaponSource = obj.weaponId;
        HitEffects::onHit(ev);
    }

    {
        glm::vec3 toPlayer = player.pos - pos;
        float dist = glm::length(toPlayer);
        if (dist < radius) {
            glm::vec3 dir = dist > 0.001f ? toPlayer / dist : glm::vec3(0.0f, 1.0f, 0.0f);
            float dmgScaled = baseDamage * std::exp(-std::pow(dist / radius, 2.0f) * 2.0f);
            int finalDmg = std::max(1, (int)std::round(dmgScaled));
            float t = dist / radius;
            float knockScale = (1.0f - t * t) * 0.85f + 0.15f;
            glm::vec3 knockback = dir * knockbackStrength * knockScale * obj.cfg.explosionSelfKnockbackMul;
            player.takeDamage(finalDmg, knockback, 8.0f);
        }
    }

    ReplayEffectEvent explosionEvent;
    explosionEvent.type = "persistent_explosion";
    explosionEvent.position = pos;
    explosionEvent.scale = {radius, 0.0f, 0.0f};
    explosionEvent.sourceActorId = obj.ownerName;
    explosionEvent.targetActorId = obj.weaponId + "_" + std::to_string(obj.id);
    captureReplayEffect(explosionEvent);

    FILE* f = fopen("logs/persistent_physics_debug.txt", "a");
    if (f) {
        fprintf(f, "EXPLOSION id=%u pos=(%.2f %.2f %.2f) radius=%.1f damage=%.0f reason=%s\n",
                obj.id, pos.x, pos.y, pos.z, radius, baseDamage,
                "contact_or_timeout");
        fclose(f);
    }
}

void PersistentPhysicsSystem::render(const Camera& camera) const
{
    for (const PersistentPhysicsObject& obj : mObjects) {
        if (obj.exploded) continue;
        renderPrimitive(obj, camera);
    }
}

void PersistentPhysicsSystem::renderPrimitive(const PersistentPhysicsObject& obj, const Camera& camera) const
{
    float r = obj.cfg.radius;
    float h = obj.cfg.height;
    glm::vec3 pos = obj.position;
    float alpha = obj.sleeping ? 0.3f : 0.7f;
    glm::vec4 color(0.3f, 0.7f, 0.2f, alpha);

    switch (obj.cfg.shape) {
    case PersistentShape::Cylinder:
        DebugVis::drawFilledCylinder(camera, pos, glm::vec3(0.0f, 0.0f, 1.0f), r, h, color);
        break;
    case PersistentShape::Sphere:
        DebugVis::drawFilledSphere(camera, pos, r, color);
        break;
    default:
        break;
    }

    if (DebugVis::enabled() && DebugConfig::DEBUG_PERSISTENT_PHYSICS) {
        char label[128];
        snprintf(label, sizeof(label), "%s id=%u sleep=%d life=%.1f",
                 obj.weaponId.c_str(), obj.id, (int)obj.sleeping, obj.cfg.lifetime - obj.age);
        DebugVis::drawWorldLabel(pos + glm::vec3(0.0f, 0.0f, h + 0.3f), label,
                                 glm::vec4(1.0f, 0.8f, 0.2f, 1.0f));
        DebugVis::drawLine(camera, pos, pos + obj.velocity * 0.1f, glm::vec4(1.0f, 0.2f, 0.2f, 1.0f));
        if (glm::length(obj.angularVelocity) > 0.001f)
            DebugVis::drawLine(camera, pos, pos + glm::normalize(obj.angularVelocity) * 0.5f,
                               glm::vec4(0.2f, 1.0f, 0.2f, 1.0f));
    }
}

void PersistentPhysicsSystem::clear()
{
    for (const auto& obj : mObjects) {
        if (obj.alive) {
            FILE* f = fopen("logs/persistent_physics_debug.txt", "a");
            if (f) {
                fprintf(f, "DESTROY id=%u type=%s lifetime=%.2f reason=system_clear\n",
                        obj.id, obj.weaponId.c_str(), obj.age);
                fclose(f);
            }
        }
    }
    mObjects.clear();
}

void PersistentPhysicsSystem::destroy(uint32_t id)
{
    for (auto it = mObjects.begin(); it != mObjects.end(); ++it) {
        if (it->id == id) {
            FILE* f = fopen("logs/persistent_physics_debug.txt", "a");
            if (f) {
                fprintf(f, "DESTROY id=%u type=%s lifetime=%.2f reason=explicit\n",
                        it->id, it->weaponId.c_str(), it->age);
                fclose(f);
            }
            mObjects.erase(it);
            return;
        }
    }
}
