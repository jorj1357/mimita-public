#include "weapon-godball.h"
#include "weapon-audio.h"
#include "weapon-types.h"
#include "weapon-execution.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <filesystem>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "audio/audio.h"
#include "camera.h"
#include "combat/death-system.h"
#include "config.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "devtools/terminal.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "ui/hitmarker.h"
#include "world/world.h"
#include "config/networking-config.h"
#include "network/multiplayer-context.h"

extern MimitaNet::MultiplayerContext* gpMpContext;

namespace {

// ── Godball debug file logger ──────────────────────────────────
FILE* gGodballLogFile = nullptr;
bool gGodballLogOpened = false;

void openGodballLog()
{
    if (gGodballLogOpened) return;
    gGodballLogOpened = true;

    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &t);
#else
    localtime_r(&t, &local);
#endif
    char dateBuf[16], timeBuf[16];
    std::strftime(dateBuf, sizeof(dateBuf), "%m-%d-%Y", &local);
    std::strftime(timeBuf, sizeof(timeBuf), "%H%M%S", &local);

    std::string dir = "logs/" + std::string(dateBuf);
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);

    std::string path = dir + "/Weapons_log_" + std::string(timeBuf) + ".txt";
    gGodballLogFile = fopen(path.c_str(), "w");
    if (gGodballLogFile) {
        fprintf(gGodballLogFile, "==================================================\n");
        fprintf(gGodballLogFile, " GODBALL NPC DAMAGE LOG\n");
        fprintf(gGodballLogFile, " Start: %s\n", std::string(timeBuf).c_str());
        fprintf(gGodballLogFile, " File: %s\n", path.c_str());
        fprintf(gGodballLogFile, "==================================================\n\n");
        fflush(gGodballLogFile);
    }
    Debug::warn(Debug::Category::Weapons, "[GODBALL_DBG] Log opened: %s\n", path.c_str());
}

void closeGodballLog()
{
    if (gGodballLogFile) {
        fprintf(gGodballLogFile, "\n==================================================\n");
        fprintf(gGodballLogFile, " LOG CLOSED\n");
        fprintf(gGodballLogFile, "==================================================\n");
        fclose(gGodballLogFile);
        gGodballLogFile = nullptr;
    }
}

void godballLog(const char* fmt, ...)
{
    openGodballLog();
    Debug::warn(Debug::Category::Weapons, "[GODBALL_DBG] ");
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Debug::warn(Debug::Category::Weapons, "%s\n", buf);
    if (gGodballLogFile) {
        fprintf(gGodballLogFile, "[GODBALL_DBG] %s\n", buf);
        fflush(gGodballLogFile);
    }
}

bool serverAuthHits()
{
    if (!gpMpContext || !gpMpContext->active) return false;
    return NetworkingConfig::instance().data().serverAuthoritativeHits.enabled;
}

} // anonymous namespace

namespace WeaponGodball {

static constexpr float MAX_GODBALL_SPEED = 120.0f;
static constexpr float GODBALL_GRAVITY = 15.0f;

static void tickImpactEvents(GodballPhysics& phys, float dt) {
    for (auto it = phys.impactEvents.begin(); it != phys.impactEvents.end(); ) {
        it->age += dt;
        if (it->age > 1.0f) {
            it = phys.impactEvents.erase(it);
        } else {
            ++it;
        }
    }
}

void updatePhysics(GodballPhysics& phys, const WeaponDefinition& def,
                    WeaponRuntime& runtime, Player& owner,
                    const Camera& camera, float dt) {
    if (!phys.active) return;

    if (phys.hitstopTimer > 0.0f) {
        phys.hitstopTimer -= dt;
        return;
    }

    float safeDt = std::min(dt, 0.033f);

    phys.prevPosition = phys.position;

    glm::vec3 handPos = getHandPosition(owner);
    glm::vec3 handVel(0.0f);
    if (phys.hasPrevHandPos && safeDt > 0.0001f) {
        handVel = (handPos - phys.prevHandPos) / safeDt;
        float hSpeed = glm::length(handVel);
        if (hSpeed > 120.0f) handVel *= 120.0f / hSpeed;
    }
    phys.prevHandPos = handPos;
    phys.hasPrevHandPos = true;
    float handSpeed = glm::length(handVel);

    glm::vec3 toHand = handPos - phys.position;
    float dist = glm::length(toHand);
    glm::vec3 ropeDir(0.0f, 0.0f, 1.0f);
    if (dist > 0.001f) ropeDir = toHand / dist;
    bool ropeTaut = dist >= phys.ropeLength * 0.95f;

    phys.velocity.z -= GODBALL_GRAVITY * safeDt;

    if (ropeTaut && handSpeed > 0.5f) {
        float dragFactor = phys.ropeDamping * 0.015f;
        phys.velocity += handVel * std::min(dragFactor, 0.05f);
    }

    phys.position += phys.velocity * safeDt;

    toHand = handPos - phys.position;
    dist = glm::length(toHand);
    phys.constraintDist = dist;

    if (dist > phys.ropeLength && dist > 0.001f) {
        glm::vec3 dir = toHand / dist;
        float overshoot = dist - phys.ropeLength;
        phys.ropeTension = overshoot * phys.ropeStiffness;

        phys.position = handPos - dir * phys.ropeLength;

        glm::vec3 correction = dir * overshoot;
        phys.velocity += correction / safeDt;

        float radialVel = glm::dot(phys.velocity, dir);
        if (radialVel < 0.0f) {
            phys.velocity -= dir * radialVel;
        }
    } else {
        phys.ropeTension = 0.0f;
    }

    toHand = handPos - phys.position;
    dist = glm::length(toHand);
    float speed = glm::length(phys.velocity);
    if (dist > 0.1f) {
        ropeDir = toHand / dist;
        phys.angularVelocity = glm::cross(ropeDir, phys.velocity) / dist;
        float angSpeed = glm::length(phys.angularVelocity);
        constexpr float MAX_ANG_SPEED = 80.0f;
        if (angSpeed > MAX_ANG_SPEED) {
            phys.angularVelocity = (phys.angularVelocity / angSpeed) * MAX_ANG_SPEED;
        }
        glm::vec3 radialComp = ropeDir * glm::dot(phys.velocity, ropeDir);
        phys.tangentialSpeed = glm::length(phys.velocity - radialComp);
        phys.radialVel = glm::dot(ropeDir, phys.velocity);
    } else {
        phys.angularVelocity = glm::vec3(0.0f);
        phys.tangentialSpeed = speed;
        phys.radialVel = 0.0f;
    }

    if (speed > MAX_GODBALL_SPEED) {
        phys.velocity = (phys.velocity / speed) * MAX_GODBALL_SPEED;
    }

    glm::vec3 ballToOwner = phys.position - owner.pos;
    float ownerDist = glm::length(ballToOwner);
    constexpr float MIN_OWNER_DIST = 0.7f;
    if (ownerDist < MIN_OWNER_DIST && ownerDist > 0.001f) {
        glm::vec3 pushDir = ballToOwner / ownerDist;
        phys.position = owner.pos + pushDir * MIN_OWNER_DIST;
        float velToward = glm::dot(phys.velocity, pushDir);
        if (velToward < 0.0f) {
            phys.velocity -= pushDir * velToward;
        }
    }

    runtime.godball.ballPosition = phys.position;
    runtime.godball.ballVelocity = phys.velocity;

    phys.handedEnergy = handSpeed;
    phys.constraintDist = dist;

    if (DebugConfig::DEBUG_GODBALL) {
        printf("[GODBALL] speed=%.2f tang=%.2f radial=%.2f "
               "dist=%.2f/%.1f tension=%.1f hand=%.1f\n",
               speed, phys.tangentialSpeed, phys.radialVel,
               phys.constraintDist, phys.ropeLength, phys.ropeTension,
               phys.handedEnergy);
    }

    godballLog("PHYSICS prev=(%.2f,%.2f,%.2f) curr=(%.2f,%.2f,%.2f) "
               "vel=(%.2f,%.2f,%.2f) speed=%.2f "
               "hand=(%.2f,%.2f,%.2f) handSpeed=%.2f "
               "ropeTaut=%d tension=%.2f ownerDist=%.2f",
               phys.prevPosition.x, phys.prevPosition.y, phys.prevPosition.z,
               phys.position.x, phys.position.y, phys.position.z,
               phys.velocity.x, phys.velocity.y, phys.velocity.z, speed,
               handPos.x, handPos.y, handPos.z, handSpeed,
               (int)ropeTaut, phys.ropeTension, ownerDist);
}

bool sweptSphereOverlap(const glm::vec3& prevPos, const glm::vec3& currPos,
                                 float radius, const glm::vec3& targetPos, float targetRadius,
                                 glm::vec3& hitPoint, glm::vec3& hitNormal,
                                 glm::vec3* outClosest) {
    glm::vec3 seg = currPos - prevPos;
    float segLen = glm::length(seg);

    if (segLen < 0.001f) {
        glm::vec3 diff = targetPos - currPos;
        float dist = glm::length(diff);
        if (dist < radius + targetRadius) {
            if (dist > 0.001f) {
                hitNormal = diff / dist;
            } else {
                hitNormal = glm::vec3(0.0f, 0.0f, 1.0f);
            }
            hitPoint = currPos + hitNormal * radius;
            if (outClosest) *outClosest = currPos;
            return true;
        }
        if (outClosest) *outClosest = currPos;
        return false;
    }

    glm::vec3 segDir = seg / segLen;
    glm::vec3 toTarget = targetPos - prevPos;

    float t = glm::dot(toTarget, segDir);
    t = std::clamp(t, 0.0f, segLen);

    glm::vec3 closestOnSeg = prevPos + segDir * t;
    if (outClosest) *outClosest = closestOnSeg;

    glm::vec3 diff = targetPos - closestOnSeg;
    float dist = glm::length(diff);
    float overlap = (radius + targetRadius) - dist;

    if (overlap > 0.0f) {
        if (dist > 0.001f) {
            hitNormal = diff / dist;
        } else {
            hitNormal = -segDir;
        }
        hitPoint = closestOnSeg + hitNormal * radius;
        return true;
    }
    return false;
}

void checkOverlaps(GodballPhysics& phys, const WeaponDefinition& def,
                   WeaponRuntime& runtime, Player& owner,
                   NpcSystem& npcs,
                   std::unordered_map<uint32_t, Player>* remoteNpcs,
                   const Camera& camera, float dt) {
    if (!phys.active) return;

    phys.lastFrameHit = false;
    phys.lastHitNormal = glm::vec3(0.0f);
    phys.npcCollisions.clear();
    tickImpactEvents(phys, dt);

    float safeDt = std::min(dt, 0.05f);
    float tickInterval = def.customParams.count("damageTickInterval")
        ? def.customParams.at("damageTickInterval") : 0.1f;

    runtime.godball.overlapDamageTimer -= safeDt;
    bool tickReady = runtime.godball.overlapDamageTimer <= 0.0f;
    if (tickReady) {
        runtime.godball.overlapDamageTimer = tickInterval;
    }

    const float damageRadius = phys.radius * 1.25f;
    glm::vec3 currPos = phys.position;
    glm::vec3 prevPos = phys.prevPosition;
    float ballSpeed = glm::length(phys.velocity);

    const float sweepThreshold = WeaponExecution::paramOr(def, "sweepSubstepThreshold", 20.0f);
    const int maxSubsteps = (int)WeaponExecution::paramOr(def, "maxSweepSubsteps", 8.0f);
    const float combinedRadius = damageRadius + 0.5f;
    const float maxStepDist = combinedRadius * 0.8f;
    const float moveDist = ballSpeed * safeDt;
    const int substeps = (moveDist > sweepThreshold && maxStepDist > 0.001f)
        ? std::clamp((int)std::ceil(moveDist / maxStepDist), 1, maxSubsteps)
        : 1;

    godballLog("CHECK active=1 tickReady=%d timer=%.4f interval=%.3f dt=%.4f "
               "ballPos=(%.2f,%.2f,%.2f) prevPos=(%.2f,%.2f,%.2f) speed=%.2f "
               "damageRadius=%.3f npcs=%zu remoteNpcs=%s/%zu substeps=%d",
               (int)tickReady, runtime.godball.overlapDamageTimer, tickInterval, safeDt,
               currPos.x, currPos.y, currPos.z,
               prevPos.x, prevPos.y, prevPos.z,
               ballSpeed, damageRadius, npcs.all().size(),
               remoteNpcs ? "ready" : "null",
               remoteNpcs ? remoteNpcs->size() : 0u, substeps);

    if (tickReady) {
        godballLog("TARGET_COLLECTION localNpcs=%zu remoteNpcs=%s/%zu "
                   "ball=(%.2f,%.2f,%.2f) prev=(%.2f,%.2f,%.2f) "
                   "radius=%.3f speed=%.2f",
                   npcs.all().size(), remoteNpcs ? "ready" : "null",
                   remoteNpcs ? remoteNpcs->size() : 0u,
                   currPos.x, currPos.y, currPos.z,
                   prevPos.x, prevPos.y, prevPos.z,
                   damageRadius, ballSpeed);
    }

    for (Npc& npc : npcs.all()) {
        if (npc.body.currentHp <= 0) continue;

        npc.body.updateModelWorldTransforms();

        uint32_t npcId = npc.id;

        GodballPhysics::NpcCollisionDebug cd;
        cd.npcId = npcId;
        cd.ballSpeed = ballSpeed;
        cd.npcPos = npc.body.pos;

        glm::vec3 toTarget = npc.body.pos - currPos;
        cd.distanceToTarget = glm::length(toTarget);
        if (cd.distanceToTarget > 0.001f) {
            cd.angleDot = glm::dot(toTarget / cd.distanceToTarget,
                ballSpeed > 0.001f ? glm::normalize(phys.velocity) : glm::vec3(0.0f, 1.0f, 0.0f));
        }

        float cooldownVal = 0.0f;
        auto& cooldowns = runtime.godball.targetCooldowns;
        auto cooldownIt = cooldowns.find(npcId);
        if (cooldownIt != cooldowns.end()) cooldownVal = cooldownIt->second;

        godballLog("NPC id=%u hp=%d pos=(%.2f,%.2f,%.2f) dist=%.3f cooldown=%.3f "
                   "bodyParts=%zu",
                   npcId, npc.body.currentHp,
                   npc.body.pos.x, npc.body.pos.y, npc.body.pos.z,
                   cd.distanceToTarget, cooldownVal,
                   npc.body.physicalBody.parts.size());

        if (cooldownIt != cooldowns.end() && cooldownIt->second > 0.0f) {
            cd.cooldownActive = true;
            cd.rejected = true;
            cd.rejectReason = "cooldown";
            godballLog("  SKIP id=%u reason=cooldown cd=%.3f", npcId, cooldownIt->second);
            if (DebugConfig::DEBUG_GODBALL) {
                printf("[GODBALL] npc=%u cooldown=%.3f -> skip\n",
                       npcId, cooldownIt->second);
            }
            phys.npcCollisions.push_back(cd);
            continue;
        }
        cd.cooldownActive = false;

        if (!tickReady) {
            cd.rejected = true;
            cd.rejectReason = "notTickReady";
            godballLog("  SKIP id=%u reason=tickReady", npcId);
            phys.npcCollisions.push_back(cd);
            continue;
        }

        bool hit = false;
        glm::vec3 hitPoint, hitNormal, closestOnSeg;
        std::string hitPartName;
        float bestDist = 999999.0f;

        for (const PhysicalBodyPart& part : npc.body.physicalBody.parts) {
            glm::vec3 localCenter = (part.collider.localMin + part.collider.localMax) * 0.5f;
            glm::vec3 worldCenter = glm::vec3(part.worldTransform * glm::vec4(localCenter, 1.0f));
            glm::vec3 halfSize = glm::max((part.collider.localMax - part.collider.localMin) * 0.5f, glm::vec3(0.12f));
            float partRadius = glm::length(halfSize) * 1.25f;

            for (int s = 0; s < substeps; s++) {
                float t0 = (float)s / substeps;
                float t1 = (float)(s + 1) / substeps;
                glm::vec3 subPrev = prevPos + (currPos - prevPos) * t0;
                glm::vec3 subCurr = prevPos + (currPos - prevPos) * t1;

                glm::vec3 oc;
                if (sweptSphereOverlap(subPrev, subCurr, damageRadius,
                                       worldCenter, partRadius,
                                       hitPoint, hitNormal, &oc)) {
                    hit = true;
                    hitPartName = part.name;
                    break;
                }
                float d = glm::length(worldCenter - oc);
                if (d < bestDist) bestDist = d;
            }
            if (hit) break;
        }

        cd.overlapCheck = true;
        cd.sweptHit = hit;
        cd.hitPoint = hitPoint;
        cd.hitNormal = hitNormal;
        cd.sweepClosest = closestOnSeg;
        cd.overlapAmount = damageRadius + 0.5f - cd.distanceToTarget;

        if (!hit) {
            cd.rejected = true;
            cd.rejectReason = "noIntersection";
            godballLog("  MISS id=%u dist=%.3f bestDist=%.3f combinedR=%.3f",
                       npcId, cd.distanceToTarget, bestDist, damageRadius + 0.5f);
            if (DebugConfig::DEBUG_GODBALL) {
                printf("[GODBALL] npc=%u noIntersection dist=%.2f\n",
                       npcId, cd.distanceToTarget);
            }
            phys.npcCollisions.push_back(cd);
            continue;
        }

        phys.lastFrameHit = true;
        phys.lastHitNormal = hitNormal;

        float damage = computeDamage(phys, def, owner, npc.body, hitPoint);
        int rounded = std::max(1, (int)std::round(damage));
        cd.computedDamage = damage;

        printf("[GODBALL HIT] speed=%.2f damage=%d part=%s target=%s\n",
               ballSpeed, rounded, hitPartName.c_str(), npc.body.username.c_str());

        glm::vec3 kbDir;
        if (ballSpeed > 0.5f) {
            glm::vec3 velDir = glm::normalize(phys.velocity);
            float velInfluence = std::min(ballSpeed / 15.0f, 1.0f);
            kbDir = glm::normalize(glm::mix(hitNormal, velDir, velInfluence));
        } else {
            kbDir = hitNormal;
        }
        if (glm::length(kbDir) < 0.001f)
            kbDir = glm::vec3(0.0f, 1.0f, 0.0f);

        float impactSpeed = std::max(ballSpeed, 1.0f);
        float knockbackBase = damage * 0.02f;
        float speedScale = impactSpeed / 10.0f;
        float knockbackForce = knockbackBase * (0.5f + 0.5f * speedScale);

        npc.body.currentHp = std::max(0, npc.body.currentHp - rounded);
        npc.body.externalImpulse += kbDir * knockbackForce + glm::vec3(0, 0, knockbackForce * 0.4f);
        npc.hitReactionTimer = 0.25f + std::min(ballSpeed * 0.005f, 0.15f);

        godballLog("  HIT id=%u name=%s part=%s damage=%.1f rounded=%d "
                   "speed=%.2f hitPt=(%.2f,%.2f,%.2f) hitNormal=(%.2f,%.2f,%.2f) "
                   "kbDir=(%.2f,%.2f,%.2f) kbForce=%.2f "
                   "hpBefore=%d hpAfter=%d",
                   npcId, npc.body.username.c_str(), hitPartName.c_str(),
                   damage, rounded, ballSpeed,
                   hitPoint.x, hitPoint.y, hitPoint.z,
                   hitNormal.x, hitNormal.y, hitNormal.z,
                   kbDir.x, kbDir.y, kbDir.z, knockbackForce,
                   npc.body.currentHp + rounded, npc.body.currentHp);

        {
            GodballPhysics::ImpactEvent ev;
            ev.position = npc.body.pos + glm::vec3(0, 0, 0.8f);
            ev.normal = kbDir;
            ev.damage = (float)rounded;
            ev.velocity = ballSpeed;
            ev.age = 0.0f;
            phys.impactEvents.push_back(ev);
            if (phys.impactEvents.size() > 16)
                phys.impactEvents.erase(phys.impactEvents.begin());
        }

        if (DebugConfig::DEBUG_GODBALL_HITSTOP) {
            phys.hitstopTimer = 0.08f;
        }

        glm::vec3 hitPos = npc.body.pos + glm::vec3(0, 0, 0.8f);
        {
            HitEvent ev;
            ev.position = hitPos;
            ev.normal = kbDir;
            ev.direction = kbDir;
            ev.hitEntity = true;
            ev.damage = rounded;
            ev.attacker = owner.username;
            ev.victim = "npc_" + std::to_string(npcId);
            ev.weaponSource = "godball";
            if (!serverAuthHits()) HitEffects::onHit(ev);
        }

        {
            float maxPossibleDamage = def.customParams.count("maxDamageCap")
                ? def.customParams.at("maxDamageCap") : 200.0f;
            float damageFraction = std::clamp((float)rounded / maxPossibleDamage, 0.0f, 1.0f);
            WeaponAudio::playGodballImpact(hitPos, damageFraction);
        }

        cooldowns[npcId] = tickInterval;
        if (!serverAuthHits()) hitmarker(rounded);

        if (npc.body.currentHp <= 0) {
            DeathSystem::instance().kill(
                npc.body, "npc_" + std::to_string(npcId), "npc",
                owner.username, kbDir, 8.0f + ballSpeed * 0.15f);
            std::string line = owner.username + " killed " + npc.body.username + " with Godball";
            Terminal::instance().addLog(line);
        }

        phys.npcCollisions.push_back(cd);
    }

    // ── Remote NPC overlap detection (server NPCs via mpContext) ──
    // Runs at 60Hz fixed-step, independent of frame rate.
    {
        static double gRemoteOverlapAccum = 0.0;
        constexpr double REMOTE_OVERLAP_DT = 1.0 / 60.0;
        gRemoteOverlapAccum += (double)safeDt;
        while (gRemoteOverlapAccum >= REMOTE_OVERLAP_DT) {
            gRemoteOverlapAccum -= REMOTE_OVERLAP_DT;

            if (!remoteNpcs) {
                godballLog("REMOTE_NPC_SCAN skipped reason=null-target-map");
                continue;
            }

            auto& remoteCooldowns = runtime.godball.targetCooldowns;
            for (auto& entry : *remoteNpcs) {
                const uint32_t npcId = entry.first;
                Player& remote = entry.second;
                if (remote.dead || remote.currentHp <= 0) continue;
                if (remote.physicalBody.parts.empty()) continue;

                // Per-NPC cooldown: 1 tick (1/60s)
                auto cdIt = remoteCooldowns.find(npcId);
                if (cdIt != remoteCooldowns.end() && cdIt->second > 0.0f) continue;

                remote.updateModelWorldTransforms();
                if (tickReady) {
                    const float targetDistance = glm::length(remote.pos - currPos);
                    godballLog("NPC_CANDIDATE id=%u name=%s hp=%d pos=(%.2f,%.2f,%.2f) "
                               "parts=%zu ballDistance=%.3f cooldown=%.3f",
                               npcId, remote.username.c_str(), remote.currentHp,
                               remote.pos.x, remote.pos.y, remote.pos.z,
                               remote.physicalBody.parts.size(), targetDistance,
                               cdIt != remoteCooldowns.end() ? cdIt->second : 0.0f);
                }
                bool hit = false;
                glm::vec3 hitPt, hitNm;

                for (const PhysicalBodyPart& part : remote.physicalBody.parts) {
                    glm::vec3 localCenter = (part.collider.localMin + part.collider.localMax) * 0.5f;
                    glm::vec3 worldCenter = glm::vec3(part.worldTransform * glm::vec4(localCenter, 1.0f));
                    glm::vec3 halfSize = glm::max((part.collider.localMax - part.collider.localMin) * 0.5f, glm::vec3(0.12f));
                    float partRadius = glm::length(halfSize) * 1.25f;

                    for (int s = 0; s < substeps; s++) {
                        float t0 = (float)s / substeps;
                        float t1 = (float)(s + 1) / substeps;
                        glm::vec3 subPrev = prevPos + (currPos - prevPos) * t0;
                        glm::vec3 subCurr = prevPos + (currPos - prevPos) * t1;

                        if (sweptSphereOverlap(subPrev, subCurr, damageRadius,
                                               worldCenter, partRadius,
                                               hitPt, hitNm)) {
                            hit = true;
                            break;
                        }
                    }
                    if (hit) break;
                }

                if (!hit) continue;

                const int hpBeforePrediction = remote.currentHp;
                float damage = computeDamage(phys, def, owner, remote, hitPt);
                int rounded = std::max(1, (int)std::round(damage));

                godballLog("REMOTE_NPC_HIT id=%u name=%s damage=%.1f rounded=%d "
                           "speed=%.2f hitPt=(%.2f,%.2f,%.2f)",
                           npcId, remote.username.c_str(), damage, rounded, ballSpeed,
                           hitPt.x, hitPt.y, hitPt.z);

                godballLog("CLIENT_DAMAGE_DECISION attacker=%s targetNpc=%u part=unknown "
                           "ballPrev=(%.2f,%.2f,%.2f) ballCurr=(%.2f,%.2f,%.2f) "
                           "hitPoint=(%.2f,%.2f,%.2f) hitNormal=(%.2f,%.2f,%.2f) "
                           "speed=%.2f damageComputed=%.2f damageRounded=%d hpBefore=%d "
                           "serverAuthMode=%d",
                           owner.username.c_str(), npcId,
                           prevPos.x, prevPos.y, prevPos.z,
                           currPos.x, currPos.y, currPos.z,
                           hitPt.x, hitPt.y, hitPt.z,
                           hitNm.x, hitNm.y, hitNm.z,
                           ballSpeed, damage, rounded, hpBeforePrediction,
                           (int)serverAuthHits());

                // Visual feedback — damage numbers, blood, hitmarker
                if (!serverAuthHits()) {
                    hitmarker(rounded);
                    {
                        HitEvent ev;
                        ev.position = hitPt;
                        ev.normal = hitNm;
                        ev.direction = glm::length(phys.velocity) > 0.001f
                            ? glm::normalize(phys.velocity) : hitNm;
                        ev.hitEntity = true;
                        ev.damage = rounded;
                        ev.hitDistance = 0.0f;
                        ev.attacker = owner.username;
                        ev.victim = remote.username;
                        ev.weaponSource = "godball";
                        HitEffects::onHit(ev);
                    }
                    {
                        float dist = glm::length(hitPt - audioListenerPosition());
                        float severity = std::clamp((float)rounded / 100.0f, 0.0f, 1.0f);
                        float vol, pit;
                        computeImpactAudio(1.0f, dist, severity, vol, pit);
                        playWorldSound("godballhit", hitPt, vol, pit, 60.0f);
                    }
                }

                // Predict damage locally (instant feedback)
                const uint64_t nowMsVal = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count());
                gpMpContext->predictedNpcHitMs[npcId] = nowMsVal;
                gpMpContext->predictedNpcDamage[npcId] = rounded;
                MimitaNet::mpApplyPredictedDamage(*gpMpContext, npcId, rounded, true);
                godballLog("CLIENT_PREDICTED_HP targetNpc=%u hpBefore=%d hpAfter=%d damage=%d",
                           npcId, hpBeforePrediction, remote.currentHp, rounded);

                // Predict knockback
                glm::vec3 kbDir = ballSpeed > 0.5f
                    ? glm::normalize(glm::mix(hitNm, glm::normalize(phys.velocity), std::min(ballSpeed / 15.0f, 1.0f)))
                    : hitNm;
                float knockbackForce = damage * 0.02f * (0.5f + 0.5f * ballSpeed / 10.0f);
                remote.externalImpulse += kbDir * knockbackForce + glm::vec3(0, 0, knockbackForce * 0.4f);

                // Send hit claim to server
                MimitaNet::GodballHitClaimPacket claim{};
                claim.header.type = MimitaNet::PACKET_GODBALL_HIT_CLAIM;
                claim.header.tick = gpMpContext->tick;
                claim.header.playerId = gpMpContext->localPlayerId;
                claim.attackerId = gpMpContext->localPlayerId;
                claim.targetId = npcId;
                claim.contactSerial = 0;
                claim.simulationTick = gpMpContext->tick;
                claim.damage = (float)rounded;
                claim.ballSpeed = ballSpeed;
                claim.hitX = hitPt.x;
                claim.hitY = hitPt.y;
                claim.hitZ = hitPt.z;
                claim.normalX = hitNm.x;
                claim.normalY = hitNm.y;
                claim.normalZ = hitNm.z;
                claim.spawnGeneration = gpMpContext->lastKnownSpawnGeneration;
                godballLog("CLIENT_CLAIM_SENT attacker=%u targetNpc=%u damage=%.1f "
                           "ballSpeed=%.2f simulationTick=%u spawnGeneration=%u "
                           "contactSerial=%u hit=(%.2f,%.2f,%.2f) normal=(%.2f,%.2f,%.2f)",
                           claim.attackerId, claim.targetId, claim.damage,
                           claim.ballSpeed, claim.simulationTick,
                           claim.spawnGeneration, claim.contactSerial,
                           claim.hitX, claim.hitY, claim.hitZ,
                           claim.normalX, claim.normalY, claim.normalZ);
                const bool claimSent = MimitaNet::mpSendPacket(
                    *gpMpContext, &claim, sizeof(claim));
                godballLog("CLIENT_CLAIM_RESULT targetNpc=%u damage=%d sent=%d",
                           claim.targetId, rounded, (int)claimSent);

                // 1-tick cooldown (next 60Hz step can hit again)
                remoteCooldowns[npcId] = 1.0f / 60.0f;
            }
        }
    }

    if (tickReady) {
        for (auto& pair : runtime.godball.targetCooldowns) {
            if (pair.second > 0.0f) pair.second -= tickInterval;
        }
    }
}

} // namespace WeaponGodball
