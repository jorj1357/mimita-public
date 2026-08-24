// 07 31 2026, 14 50
/* purpose
* Simulates persistent physics projectiles (grenades) and their explosion effects.
* Owns grenade bounce/drag/lifetime physics and the local client explosion visuals.
* Does NOT own server projectile authority or packet routing.
* Does NOT run the fixed-step simulation tick or render viewmodels.
*/
#include "persistent-physics.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "audio/audio.h"
#include "camera.h"
#include "combat/explosion-fx.h"
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
    const std::string& weaponId,
    const glm::vec3& ownerPos)
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
    obj.worldTouched = false;
    obj.ownerIgnored = false;
    obj.distFromOwner = glm::length(position - ownerPos);

    bool ownerArmed = false;
    if (cfg.armingDistance > 0.0f && obj.distFromOwner < cfg.armingDistance) {
        obj.ownerIgnored = true;
        Debug::log(Debug::Category::Weapons, "[POBJ] id=%u owner=%s ownerIgnored=YES distFromOwner=%.2f armingDistance=%.2f\n",
                   obj.id, ownerName.c_str(), obj.distFromOwner, cfg.armingDistance);
    } else {
        ownerArmed = true;
    }

    Debug::log(Debug::Category::Weapons, "[POBJ] SPAWN id=%u type=%s owner=%s pos=(%.2f %.2f %.2f) vel=(%.2f %.2f %.2f) "
               "ownerIgnored=%d armingDist=%.1f\n",
               obj.id, weaponId.c_str(), ownerName.c_str(),
               position.x, position.y, position.z,
               velocity.x, velocity.y, velocity.z,
               (int)obj.ownerIgnored, cfg.armingDistance);

    if (!cfg.spawnSound.empty()) {
        Debug::log(Debug::Category::Audio, "[POBJ] sound=spawn path=%s pos=(%.2f %.2f %.2f)\n",
                   cfg.spawnSound.c_str(), position.x, position.y, position.z);
        playWorldSound(cfg.spawnSound, position, 1.0f, 1.0f, 50.0f);
    }

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
        if (obj.cfg.armingTime > 0.0f && obj.age >= obj.cfg.armingTime) {
            if (obj.ownerIgnored) {
                obj.ownerIgnored = false;
                Debug::log(Debug::Category::Weapons, "[POBJ] id=%u armingTime elapsed (%.2fs) — owner collision ENABLED\n",
                           obj.id, obj.cfg.armingTime);
            }
        }

        if (obj.cfg.lifetime > 0.0f && obj.age >= obj.cfg.lifetime) {
            Debug::log(Debug::Category::Weapons, "[POBJ] id=%u lifetime expired (%.2fs) — exploding\n",
                       obj.id, obj.cfg.lifetime);
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

    obj.velocity.z -= obj.cfg.gravity * dt;

    obj.velocity *= (1.0f - obj.cfg.drag * dt);
    if (glm::length(obj.velocity) < 0.001f) obj.velocity = glm::vec3(0.0f);
    obj.angularVelocity *= (1.0f - obj.cfg.angularDrag * dt);

    float speed = glm::length(obj.velocity);
    if (speed > obj.cfg.maxLinearVelocity)
        obj.velocity *= obj.cfg.maxLinearVelocity / speed;
    float angSpeed = glm::length(obj.angularVelocity);
    if (angSpeed > obj.cfg.maxAngularVelocity)
        obj.angularVelocity *= obj.cfg.maxAngularVelocity / angSpeed;

    if (angSpeed > 0.0001f) {
        glm::quat delta = glm::angleAxis(angSpeed * dt, glm::normalize(obj.angularVelocity));
        obj.rotation = glm::normalize(delta * obj.rotation);
    }

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
        AABB queryBounds;
        queryBounds.min = obj.position - glm::vec3(r + 0.5f);
        queryBounds.max = obj.position + glm::vec3(r + 0.5f);
        std::vector<int> candidates;
        appendChunkTrianglesForAABB(const_cast<World&>(world), queryBounds, 0.1f, candidates, "persistentPhysicsCollide");

        for (int ti : candidates) {
            if (ti < 0 || ti >= (int)world.collisionMesh.triangles.size()) continue;
            const CollisionTriangle& tri = world.collisionMesh.triangles[ti];
            glm::vec3 closest = closestPointOnTriangleFn(obj.position, tri.a, tri.b, tri.c);
            glm::vec3 diff = obj.position - closest;
            float dist = glm::length(diff);
            if (dist < r && dist > 0.0001f) {
                glm::vec3 normal = diff / dist;
                float penetration = r - dist;
                obj.position += normal * penetration;

                float velDot = glm::dot(obj.velocity, normal);
                if (velDot < 0.0f) {
                    float speedBefore = glm::length(obj.velocity);
                    float restitution = obj.cfg.restitution;
                    float friction = obj.cfg.friction;

                    if (obj.cfg.maxBounceCount > 0 && obj.bounceCount >= obj.cfg.maxBounceCount) {
                        restitution = 0.0f;
                        friction = 1.0f;
                    }

                    // Stable bounce: separate normal and tangential; friction reduces tangent, never amplifies
                    glm::vec3 tangent = obj.velocity - normal * velDot;
                    float tangentRetention = std::clamp(1.0f - friction, 0.0f, 1.0f);
                    obj.velocity = tangent * tangentRetention - normal * velDot * restitution;

                    float speedAfter = glm::length(obj.velocity);
                    if (speedAfter > speedBefore * 0.98f)
                        obj.velocity *= (speedBefore * 0.95f) / speedAfter;

                    if (obj.cfg.minBounceSpeed > 0.0f && speedAfter < obj.cfg.minBounceSpeed) {
                        obj.velocity = glm::vec3(0.0f);
                        obj.angularVelocity = glm::vec3(0.0f);
                        obj.sleeping = true;
                    } else {
                        obj.angularVelocity = glm::vec3(0.0f);
                    }
                    obj.bounceCount++;

                    obj.worldTouched = true;

                    Debug::log(Debug::Category::Collision,
                        "[POBJ] WORLD_HIT id=%u pos=(%.2f %.2f %.2f) normal=(%.2f %.2f %.2f) "
                        "speedBefore=%.2f speedAfter=%.2f restitution=%.2f friction=%.2f bounce=%d/%d\n",
                        obj.id, obj.position.x, obj.position.y, obj.position.z,
                        normal.x, normal.y, normal.z,
                        speedBefore, speedAfter, restitution, friction,
                        obj.bounceCount, obj.cfg.maxBounceCount);

                    ReplayEffectEvent impactEvent;
                    impactEvent.type = "impact_world";
                    impactEvent.position = obj.position;
                    impactEvent.normal = normal;
                    impactEvent.direction = glm::normalize(obj.velocity);
                    impactEvent.scale = {obj.cfg.radius * 2.2f, 0.0f, 0.0f};
                    impactEvent.alpha = 0.5f;
                    impactEvent.lifetime = 0.25f;
                    captureReplayEffect(impactEvent);

                    EffectPart impact;
                    impact.position = obj.position;
                    impact.color = {0.5f, 0.5f, 0.5f};
                    impact.normal = normal;
                    impact.velocity = -normal * 0.3f;
                    impact.lifetime = 0.25f;
                    impact.maxLifetime = 0.25f;
                    impact.scale = obj.cfg.radius * 2.2f;
                    impact.endScale = 0.0f;
                    impact.alpha = 0.5f;
                    impact.replayType = "impact_indicator";
                    impact.sticky = true;
                    EffectPartSystem::instance().spawn(impact);
                }
                break;
            }
        }
    }

    if (obj.cfg.collideWithPlayer && !player.dead) {
        glm::vec3 toPlayer = player.pos - obj.position;
        float dist = glm::length(toPlayer);
        float hitDist = r + 0.5f;
        if (dist < hitDist) {
            if (obj.ownerIgnored) {
                Debug::log(Debug::Category::Weapons, "[POBJ] id=%u IGNORED player collision (owner grace period)\n", obj.id);
            } else if (obj.worldTouched) {
                Debug::log(Debug::Category::Weapons, "[POBJ] id=%u IGNORED player collision (already touched world)\n", obj.id);
            } else {
                Debug::log(Debug::Category::Weapons, "[POBJ] id=%u EXPLODE from airborne player hit dist=%.2f\n",
                           obj.id, dist);
                doExplosion(obj, world, player, npcs, nullptr);
                return;
            }
        }
    }

    if (obj.cfg.collideWithNpcs) {
        for (const Npc& npc : npcs.all()) {
            if (npc.body.dead) continue;
            glm::vec3 toNpc = npc.body.pos - obj.position;
            float dist = glm::length(toNpc);
            float hitDist = r + 0.5f;
            if (dist < hitDist) {
                if (obj.worldTouched) {
                    Debug::log(Debug::Category::Weapons, "[POBJ] id=%u IGNORED NPC collision (already touched world)\n", obj.id);
                } else {
                    Debug::log(Debug::Category::Weapons, "[POBJ] id=%u EXPLODE from airborne NPC hit dist=%.2f\n",
                               obj.id, dist);
                    doExplosion(obj, world, player, npcs, nullptr);
                    return;
                }
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
    float splashExp = obj.cfg.splashExponent;

    Debug::log(Debug::Category::Weapons, "[POBJ] EXPLOSION id=%u pos=(%.2f %.2f %.2f) radius=%.1f damage=%.0f exponent=%.1f\n",
               obj.id, pos.x, pos.y, pos.z, radius, baseDamage, splashExp);

    spawnExplosionFx(pos, obj.weaponId, obj.ownerName);

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
        float dmgScaled = baseDamage * std::exp(-std::pow(dist / radius, 2.0f) * splashExp);
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
        Debug::log(Debug::Category::Weapons, "[POBJ] npc=%u damage=%d dist=%.1f\n",
                   npc.id, finalDmg, dist);
    }

    {
        glm::vec3 toPlayer = player.pos - pos;
        float dist = glm::length(toPlayer);
        if (dist < radius) {
            glm::vec3 dir = dist > 0.001f ? toPlayer / dist : glm::vec3(0.0f, 1.0f, 0.0f);
            float dmgScaled = baseDamage * std::exp(-std::pow(dist / radius, 2.0f) * splashExp);
            int finalDmg = std::max(1, (int)std::round(dmgScaled * obj.cfg.explosionSelfDamageMul));
            Debug::log(Debug::Category::Weapons,
                "[SELF_DAMAGE] owner=%s isSelf=1 baseDmg=%.1f mul=%.2f finalDmg=%d dist=%.1f\n",
                obj.ownerName.c_str(), baseDamage, obj.cfg.explosionSelfDamageMul, finalDmg, dist);
            float t = dist / radius;
            float knockScale = (1.0f - t * t) * 0.85f + 0.15f;
            glm::vec3 knockback = dir * knockbackStrength * knockScale * obj.cfg.explosionSelfKnockbackMul;
            player.takeDamage(finalDmg, knockback, 8.0f);
            Debug::log(Debug::Category::Weapons, "[POBJ] player damage=%d dist=%.1f knockback=(%.2f %.2f %.2f)\n",
                       finalDmg, dist, knockback.x, knockback.y, knockback.z);
        }
    }

    ReplayEffectEvent explosionEvent;
    explosionEvent.type = "persistent_explosion";
    explosionEvent.position = pos;
    explosionEvent.scale = {radius, 0.0f, 0.0f};
    explosionEvent.sourceActorId = obj.ownerName;
    explosionEvent.targetActorId = obj.weaponId + "_" + std::to_string(obj.id);
    captureReplayEffect(explosionEvent);
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
    // Grenade launcher projectiles are rendered by WeaponGrenadeLauncher::update
    // with textured meshes from projectile-render.cpp. Skip the old flat cylinder.
    if (obj.weaponId == "grenade_launcher")
        return;

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
        snprintf(label, sizeof(label), "%s id=%u bounce=%d sleep=%d life=%.1f",
                 obj.weaponId.c_str(), obj.id, obj.bounceCount, (int)obj.sleeping,
                 obj.cfg.lifetime - obj.age);
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
    Debug::log(Debug::Category::Weapons, "[POBJ] system clear — %zu objects removed\n", mObjects.size());
    mObjects.clear();
}

void PersistentPhysicsSystem::destroy(uint32_t id)
{
    for (auto it = mObjects.begin(); it != mObjects.end(); ++it) {
        if (it->id == id) {
            Debug::log(Debug::Category::Weapons, "[POBJ] DESTROY id=%u type=%s lifetime=%.2f reason=explicit\n",
                       it->id, it->weaponId.c_str(), it->age);
            mObjects.erase(it);
            return;
        }
    }
}
