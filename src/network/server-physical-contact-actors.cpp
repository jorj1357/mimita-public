// Actor adapters for the shared physical-contact weapon owner. Equipped clip
// profiles provide a fixed-tick pose; rendering and contact share attachment
// geometry, and every actor pair submits the same typed damage request.
#include "network/server.h"
#include "network/server-gamemode.h"
#include "network/network-weapons.h"
#include "network/packets.h"
#include "combat/weapon-attachment.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-melee-config.h"
#include "combat/weapon-config.h"
#include "entities/player-avatar-animations.h"
#include "entities/player.h"
#include "debug/structured-log.h"
#include "devtools/scenario-server-playback.h"
#include "physics/movement/movement-step.h"
#include "utils/path_utils.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <unordered_map>
#include <glm/gtc/matrix_transform.hpp>

extern nlohmann::json gAvatarBodypartOverrides;
extern bool gAvatarUseModelTransforms;
extern bool gAvatarCanonicalizePlayerModel;

namespace MimitaNet {
namespace {
struct Episode {
    uint32_t generation = 0, lastTick = 0, serial = 0, samples = 0;
    int damage = 0, before = 0, after = 0;
    bool killed = false;
    glm::vec3 impulse{0}, hit{0}, normal{0,0,1};
    float force = 0;
};
struct ActorContactRuntime {
    std::unique_ptr<Player> pose;
    std::string avatar, definition;
    uint32_t generation = 0, epoch = 0, lastTick = 0, nextSerial = 1, lastDiagnostic = 0;
    bool previousValid = false;
    bool positionHistoryValid = false;
    glm::vec3 previousPosition{0};
    WeaponExecution::PhysicalContactShape shape;
    WeaponExecution::PlayerTarget target;
    glm::vec3 minimum{0}, maximum{0};
    std::unordered_map<uint64_t, Episode> episodes;
    std::unordered_map<uint64_t, uint32_t> lastClashTick;
};
struct ActorView {
    uint64_t key = 0;
    ServerActorRef ref;
    ActorContactRuntime* runtime = nullptr;
    const WeaponDefinition* weapon = nullptr;
    glm::vec3 position{0}, velocity{0}, aim{1,0,0};
    float yaw = 0, scale = 1;
    uint32_t generation = 0;
    bool actionActive = false;
    std::string actionId;
    uint32_t actionStart = 0;
};
thread_local std::unordered_map<uint64_t, ActorContactRuntime> runtimes;
thread_local std::vector<ActorView> actors;

static const WeaponData::MeleeAction* currentAction(
    const ActorView& actor, uint32_t tick, const char** phase,
    int* phaseTick, int* phaseTicks)
{
    if (!actor.actionActive || !actor.weapon)
        return nullptr;
    const auto* set = WeaponData::MeleeMoveSetRegistry::instance().get(actor.weapon->id);
    const WeaponData::MeleeAction* action = set ? set->find(actor.actionId) : nullptr;
    if (!action)
        return nullptr;
    const int elapsed = std::max(0, (int)(tick - actor.actionStart));
    const int windup = action->windup.ticks;
    const int active = action->active.ticks;
    const char* p = "windup";
    int pt = elapsed;
    int pts = windup;
    if (elapsed >= windup + active) {
        p = "recovery";
        pt = elapsed - windup - active;
        pts = action->recovery.ticks;
    } else if (elapsed >= windup) {
        p = "active";
        pt = elapsed - windup;
        pts = active;
    }
    if (phase) *phase = p;
    if (phaseTick) *phaseTick = pt;
    if (phaseTicks) *phaseTicks = pts;
    return action;
}

static float pointSegmentDistance(const glm::vec3& p, const glm::vec3& a,
                                  const glm::vec3& b)
{
    const glm::vec3 ab = b - a;
    const float denom = glm::dot(ab, ab);
    if (denom < 0.00001f)
        return glm::length(p - a);
    const float t = glm::clamp(glm::dot(p - a, ab) / denom, 0.0f, 1.0f);
    return glm::length(p - (a + ab * t));
}

static float segmentSegmentDistance(const glm::vec3& a0, const glm::vec3& a1,
                                    const glm::vec3& b0, const glm::vec3& b1)
{
    // Closest-point approximation adequate for the blade-vs-blade block test.
    float best = std::min(
        std::min(pointSegmentDistance(a0, b0, b1), pointSegmentDistance(a1, b0, b1)),
        std::min(pointSegmentDistance(b0, a0, a1), pointSegmentDistance(b1, a0, a1)));
    const int steps = 8;
    for (int i = 0; i <= steps; ++i) {
        const glm::vec3 p = glm::mix(a0, a1, (float)i / (float)steps);
        best = std::min(best, pointSegmentDistance(p, b0, b1));
    }
    return best;
}

static bool blocksMeleeContact(const ActorView& target, uint32_t tick,
                               const WeaponExecution::PhysicalContactShape& attackerShape,
                               const WeaponExecution::PhysicalContactHit& hit)
{
    const char* phase = nullptr;
    const WeaponData::MeleeAction* action =
        currentAction(target, tick, &phase, nullptr, nullptr);
    if (!action || !action->blocksMelee)
        return false;

    glm::vec3 forward = target.aim;
    if (!std::isfinite(forward.x) || !std::isfinite(forward.y) ||
        !std::isfinite(forward.z) || glm::length(forward) < 0.001f) {
        forward = glm::vec3(std::cos(target.yaw), std::sin(target.yaw), 0.0f);
    }
    forward = glm::normalize(forward);
    glm::vec3 toHit = hit.hitPosition - target.position;
    if (glm::length(toHit) > 0.001f) {
        const float dot = glm::clamp(glm::dot(forward, glm::normalize(toHit)),
                                     -1.0f, 1.0f);
        const float halfArc = glm::radians(std::max(0.0f, action->blockArcDegrees)) * 0.5f;
        if (dot < std::cos(halfArc))
            return false;
    }

    // The blade has to physically meet the incoming contact. The target shape
    // is the current/previous animated weapon capsule in its runtime.
    const auto& targetShape = target.runtime->shape;
    if (!target.runtime->previousValid)
        return false;
    const float distance = segmentSegmentDistance(
        attackerShape.currentA, attackerShape.currentB,
        targetShape.currentA, targetShape.currentB);
    return distance <= attackerShape.radius + targetShape.radius + 0.12f;
}

static void broadcastWeaponClash(
    SOCKET sock, std::unordered_map<uint32_t, ServerPlayer>& players,
    const ServerActorRef& attacker, const glm::vec3& hit, const glm::vec3& normal,
    float force, uint64_t& packets)
{
    MeleeHitEventPacket event{};
    event.header.type = PACKET_MELEE_HIT_EVENT;
    event.header.playerId = attacker.id;
    event.attackerPlayerId = attacker.id;
    event.targetPlayerId = 0;
    event.attackType = 3;
    event.damage = 0;
    event.hitX = hit.x; event.hitY = hit.y; event.hitZ = hit.z;
    event.normalX = normal.x; event.normalY = normal.y; event.normalZ = normal.z;
    event.knockX = normal.x * force; event.knockY = normal.y * force;
    event.knockZ = normal.z * force;
    event.weapon = NETWORK_WEAPON_SWORDSWORD;
    for (auto& entry : players) {
        ServerPlayer& p = entry.second;
        if (p.transport) {
            p.transport->send(&event, sizeof(event));
        } else {
            sendto(sock, (const char*)&event, sizeof(event), 0,
                   (sockaddr*)&p.addr, sizeof(p.addr));
        }
        ++packets;
    }
}

bool preparePose(ActorView& actor, const std::string& avatar, uint32_t tick)
{
    auto& runtime = *actor.runtime;
    if (!runtime.pose || runtime.avatar != avatar) {
        runtime.pose = std::make_unique<Player>(false);
        runtime.avatar = avatar;
        std::string model = "assets/entity/player/default/mimita-char-no-animations-v4.glb";
        nlohmann::json definition;
        if (!avatar.empty()) {
            std::ifstream file(resolveAssetPath("assets/avatars/" + avatar + "/avatar.json"));
            try { if (file) definition = nlohmann::json::parse(file, nullptr, true, true); }
            catch (...) { definition = nullptr; }
            if (definition.is_object()) model = definition.value("player_model", model);
        }
        const bool oldModel = gAvatarUseModelTransforms, oldCanonical = gAvatarCanonicalizePlayerModel;
        const auto oldOverrides = gAvatarBodypartOverrides;
        gAvatarUseModelTransforms = definition.is_object() && definition.value("body_transform_source", "model") == "model";
        gAvatarCanonicalizePlayerModel = gAvatarUseModelTransforms &&
            definition.value("body_frame_policy", "player_canonical") == "player_canonical";
        gAvatarBodypartOverrides = definition.is_object() && definition.contains("bodyparts") ? definition["bodyparts"] : nlohmann::json();
        const bool loaded = runtime.pose->loadModelColliders(model.c_str());
        gAvatarUseModelTransforms = oldModel;
        gAvatarCanonicalizePlayerModel = oldCanonical;
        gAvatarBodypartOverrides = oldOverrides;
        runtime.pose->setAvatarName(avatar);
        runtime.previousValid = false;
        StructuredLogger::instance().writeEvent(StructuredCategory::Weapons,
            loaded ? StructuredLevel::Important : StructuredLevel::Errors,
            "weapon.contact.pose_bound", std::to_string(actor.ref.id), "bound shared CPU actor pose", tick,
            {{"actor", actor.ref.id}, {"actor_type", (int)actor.ref.kind}, {"avatar", avatar},
             {"model", model}, {"loaded", loaded}, {"default_model", !definition.is_object()}});
        if (!loaded) return false;
    }
    Player& pose = *runtime.pose;
    pose.pos = actor.position; pose.vel = actor.velocity; pose.yaw = actor.yaw;
    pose.sizeScale = actor.scale; pose.currentHp = actor.ref.player ? actor.ref.player->health : actor.ref.npc->health;
    pose.dead = false;
    pose.username = "contact_actor_" + std::to_string(actor.ref.id);
    pose.equippedWeaponId = actor.weapon ? actor.weapon->id : "";
    pose.hasValidWeapon = actor.weapon != nullptr;
    pose.equippedSlot = actor.weapon ? actor.weapon->slot : 0;
    pose.ground.onGround = actor.ref.player ? actor.ref.player->onGround : actor.ref.npc->onGround;
    pose.ground.stableOnGround = pose.ground.onGround;
    pose.movementSimulationTick = tick;
    const auto* set = actor.weapon ? WeaponData::MeleeMoveSetRegistry::instance().get(actor.weapon->id) : nullptr;
    const auto* action = set && actor.actionActive ? set->find(actor.actionId) : nullptr;
    pose.proceduralMeleePoseActive = false;
    if (action && !action->animationClipId.empty()) {
        pose.playOneShotAtTick(action->animationClipId,
            std::min((int)(tick - actor.actionStart), std::max(0, action->totalTicks() - 1)));
    } else {
        pose.clearOneShot();
        if (action && action->hasProceduralPose) {
            const int elapsed = std::max(0, std::min((int)(tick - actor.actionStart),
                std::max(0, action->totalTicks() - 1)));
            const int windup = action->windup.ticks;
            const int active = action->active.ticks;
            const char* phase = "windup";
            int phaseTick = elapsed;
            int phaseTicks = windup;
            if (elapsed >= windup + active) {
                phase = "recovery";
                phaseTick = elapsed - windup - active;
                phaseTicks = action->recovery.ticks;
            } else if (elapsed >= windup) {
                phase = "active";
                phaseTick = elapsed - windup;
                phaseTicks = active;
            }
            WeaponData::MeleeProceduralPose procedural;
            if (WeaponData::resolveMeleeProceduralPose(*action, phase,
                    phaseTick, phaseTicks, procedural)) {
                pose.proceduralMeleeRightArmRotation = procedural.rightArmRotation;
                pose.proceduralMeleeRightArmTranslation = procedural.rightArmTranslation;
                pose.proceduralMeleeTorsoRotation = procedural.torsoRotation;
                pose.proceduralMeleePoseActive = true;
            }
        }
    }
    pose.updateProceduralAnimation(SERVER_DT, actor.aim, actor.position, glm::length(actor.velocity) > 0.1f);

    auto& target = runtime.target;
    target.playerId = actor.ref.id; target.spawnGeneration = actor.generation;
    target.position = actor.position; target.dead = false;
    target.bodyParts.clear();
    runtime.minimum = glm::vec3(std::numeric_limits<float>::max());
    runtime.maximum = -runtime.minimum;
    auto append = [&](const glm::vec3& center, const glm::vec3& half) {
        target.bodyParts.push_back({center, half, WeaponExecution::HitBodyPart::Torso});
        runtime.minimum = glm::min(runtime.minimum, center - half);
        runtime.maximum = glm::max(runtime.maximum, center + half);
    };
    // Replay-controlled proxies carry the recorded authoritative collision
    // pose. Use it instead of re-deriving a pose from the live animation so the
    // contact query evaluates the same body geometry that was recorded.
    if (actor.ref.npc && actor.ref.npc->replayControlled &&
        actor.ref.npc->bodyPartCount > 0) {
        for (uint8_t i = 0; i < actor.ref.npc->bodyPartCount; ++i) {
            const auto& part = actor.ref.npc->bodyParts[i];
            append(glm::vec3(part.cx, part.cy, part.cz),
                   glm::vec3(part.hx, part.hy, part.hz));
        }
        return !target.bodyParts.empty();
    }
    if (actor.ref.player && actor.ref.player->hasRagdollPose &&
        !actor.ref.player->ragdollHistory.empty() && !pose.usesMeleeAuthoredPose()) {
        const auto& replicated = actor.ref.player->ragdollHistory.back().pose;
        for (int i = 0; i < replicated.count && i < kRagdollLimbCount; ++i)
            if (glm::length(replicated.limbs[i].hitHalf) > 0.00001f)
                append(replicated.limbs[i].hitCenter, replicated.limbs[i].hitHalf);
    }
    if (target.bodyParts.empty()) for (const auto& part : pose.physicalBody.parts) {
        glm::vec3 low(std::numeric_limits<float>::max()), high = -low;
        for (int corner = 0; corner < 8; ++corner) {
            const glm::vec3 local((corner & 1) ? part.collider.localMax.x : part.collider.localMin.x,
                (corner & 2) ? part.collider.localMax.y : part.collider.localMin.y,
                (corner & 4) ? part.collider.localMax.z : part.collider.localMin.z);
            const glm::vec3 world = glm::vec3(part.worldTransform * glm::vec4(local, 1));
            low = glm::min(low, world); high = glm::max(high, world);
        }
        append((low + high) * 0.5f, (high - low) * 0.5f);
    }
    return !target.bodyParts.empty();
}

void confirm(SOCKET sock, std::unordered_map<uint32_t, ServerPlayer>& players,
    const ActorView& attacker, const ActorView& target, Episode& episode,
    uint32_t tick, uint64_t& packets)
{
    if (episode.damage <= 0 || !attacker.weapon) return;
    if (sock == INVALID_SOCKET) {
        episode.damage = 0; episode.samples = 0;
        episode.impulse = glm::vec3(0); episode.force = 0;
        return;
    }
    const bool npcAttacker = attacker.ref.kind == ServerActorKind::Npc;
    ServerDamageResult result;
    result.applied = true; result.killed = episode.killed;
    result.healthBefore = episode.before;
    result.healthAfter = target.ref.player ? target.ref.player->health : target.ref.npc->health;
    if (target.ref.player) queueServerDamageConfirmedEvent(sock, players, tick, packets,
        npcAttacker ? 0 : attacker.ref.id, *target.ref.player, episode.damage, result,
        episode.hit, episode.normal, episode.impulse, ServerDamageSource::PhysicalContact,
        networkWeaponTypeForDefinition(*attacker.weapon), episode.serial, 0,
        npcAttacker ? attacker.ref.id : 0, attacker.weapon->id, episode.force);
    else broadcastNpcDamageEvent(sock, players, tick, packets, attacker.ref.id, *target.ref.npc,
        episode.damage, episode.killed, attacker.position, episode.hit,
        glm::normalize(episode.normal), episode.normal, networkWeaponTypeForDefinition(*attacker.weapon),
        (uint8_t)attacker.ref.kind, episode.force);
    episode.damage = 0; episode.samples = 0; episode.impulse = glm::vec3(0); episode.force = 0;
}

void flushPending(SOCKET sock, std::unordered_map<uint32_t, ServerPlayer>& players,
    std::unordered_map<uint32_t, ServerNpc>& npcs, uint64_t attackerKey,
    ActorContactRuntime& runtime, uint32_t tick, uint64_t& packets)
{
    ActorView attacker;
    attacker.ref.id = (uint32_t)attackerKey;
    attacker.ref.kind = (ServerActorKind)(attackerKey >> 32);
    attacker.weapon = WeaponRegistry::instance().get(runtime.definition);
    attacker.position = runtime.previousPosition;
    for (auto& pair : runtime.episodes) {
        ActorView target;
        target.ref.id = (uint32_t)pair.first;
        target.ref.kind = (ServerActorKind)(pair.first >> 32);
        if (target.ref.kind == ServerActorKind::Player) {
            auto it = players.find(target.ref.id);
            if (it == players.end() || it->second.spawnGeneration != pair.second.generation) continue;
            target.ref.player = &it->second;
        } else {
            auto it = npcs.find(target.ref.id);
            if (it == npcs.end() || it->second.transformEpoch != pair.second.generation) continue;
            target.ref.npc = &it->second;
        }
        confirm(sock, players, attacker, target, pair.second, tick, packets);
    }
}
}

void tickServerAnimatedPhysicalContactWeapons(SOCKET sock,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    std::unordered_map<uint32_t, ServerNpc>& npcs, uint32_t tick, uint64_t& packets,
    std::vector<ServerPhysicalContactResult>* localResults)
{
    actors.clear();
    auto configured = [](const std::string& id) {
        const auto* def = WeaponRegistry::instance().get(id);
        return def && def->id != "nothing";
    };
    bool needed = false;
    for (const auto& p : players) needed = needed || (!p.second.dead && configured(p.second.equippedWeaponId));
    for (const auto& n : npcs) needed = needed || (n.second.health > 0 && configured(n.second.equippedWeaponId));
    if (!needed) {
        for (auto& pair : runtimes) flushPending(sock, players, npcs, pair.first, pair.second, tick, packets);
        runtimes.clear(); return;
    }
    WeaponConfig::instance().pollHotReload();
    AvatarAnimationRegistry::instance().pollReload(tick);
    auto add = [&](ServerActorRef ref, const std::string& avatar, const std::string& weaponId,
        const glm::vec3& position, const glm::vec3& velocity, const glm::vec3& aim, float yaw,
        float scale, uint32_t generation, bool actionActive, const std::string& actionId, uint32_t start) {
        const uint64_t key = ((uint64_t)ref.kind << 32) | ref.id;
        auto& state = runtimes[key];
        const uint32_t epoch = ref.player ? ref.player->transformEpoch : ref.npc->transformEpoch;
        if (state.generation != generation || state.epoch != epoch ||
            state.lastTick > tick || state.definition != weaponId) {
            flushPending(sock, players, npcs, key, state, tick, packets);
            state.previousValid = false; state.positionHistoryValid = false; state.episodes.clear();
            state.generation = generation; state.epoch = epoch; state.definition = weaponId;
        }
        state.lastTick = tick;
        ActorView view{key, ref, &state, WeaponRegistry::instance().get(weaponId), position, velocity,
            aim, yaw, scale, generation, actionActive, actionId, start};
        if (preparePose(view, avatar, tick)) actors.push_back(std::move(view));
    };
    for (auto& entry : players) {
        auto& p = entry.second;
        if (p.dead || p.connectionStale || p.spawnState != ServerPlayer::Active) continue;
        add({p.id, ServerActorKind::Player, &p, nullptr}, p.avatarName, p.equippedWeaponId,
            p.pos, p.vel, p.input.camForward, p.yaw, p.sizeScale, p.spawnGeneration,
            p.meleeActionActive, p.meleeActionId, p.meleeActionStartTick);
    }
    for (auto& entry : npcs) {
        auto& n = entry.second;
        if (n.health <= 0) continue;
        add({n.entityId, ServerActorKind::Npc, nullptr, &n}, n.avatarName, n.equippedWeaponId,
            n.pos, n.vel, n.aim, n.yaw, n.sizeScale, n.transformEpoch,
            n.meleeActionActive, n.meleeActionId, n.meleeActionStartTick);
    }
    std::sort(actors.begin(), actors.end(), [](const auto& a, const auto& b) { return a.key < b.key; });
    for (const auto& attacker : actors) {
        if ((attacker.ref.player && attacker.ref.player->dead) ||
            (attacker.ref.npc && attacker.ref.npc->health <= 0)) continue;
        auto& state = *attacker.runtime;
        const auto* def = attacker.weapon;
        if (!def || def->id == "nothing") continue;
        const bool damaging = def->executionType == WeaponExecutionType::PhysicalContact;
        glm::vec3 actionRotation(0.0f), actionOffset(0.0f);
        if (const auto* set = WeaponData::MeleeMoveSetRegistry::instance().get(def->id)) {
            std::string actionId = attacker.actionId;
            const WeaponData::MeleeAction* action = actionId.empty() ? nullptr : set->find(actionId);
            if (!action) {
                for (const char* suffix : {"_windup", "_active", "_recovery"}) {
                    const size_t suffixLen = std::strlen(suffix);
                    if (actionId.size() > suffixLen &&
                        actionId.compare(actionId.size() - suffixLen, suffixLen, suffix) == 0) {
                        action = set->find(actionId.substr(0, actionId.size() - suffixLen));
                        if (action) break;
                    }
                }
            }
            if (action && action->hasWeaponPose) {
                actionRotation = action->weaponPoseRotation;
                actionOffset = action->weaponPoseOffset;
            }
        }
        WeaponAttachmentPose attachment;
        bool valid = attacker.ref.npc && attacker.ref.npc->replayControlled &&
            attacker.ref.npc->hasReplayWeaponShape;
        if (!valid) for (const auto& part : state.pose->physicalBody.parts) if (part.name == "rightArm") {
            valid = resolveWeaponAttachment(*def, part, attacker.scale, attachment,
                                            actionRotation, actionOffset); break;
        }
        if (!valid) {
            if (tick - state.lastDiagnostic >= 60) {
                state.lastDiagnostic = tick;
                StructuredLogger::instance().writeEvent(StructuredCategory::Weapons, StructuredLevel::Errors,
                    "weapon.contact.pose_unavailable", std::to_string(attacker.ref.id),
                    "blade contact skipped: arm or model unavailable", tick, {{"weapon", def->id}});
            }
            state.previousValid = false; continue;
        }
        WeaponExecution::PhysicalContactShape shape;
        shape.kind = WeaponExecution::PhysicalShapeKind::Capsule;
        shape.currentA = attachment.bladeBase; shape.currentB = attachment.bladeTip;
        shape.previousA = state.previousValid ? state.shape.currentA : shape.currentA;
        shape.previousB = state.previousValid ? state.shape.currentB : shape.currentB;
        shape.radius = attachment.bladeRadius;
        if (attacker.ref.npc && attacker.ref.npc->replayControlled &&
            attacker.ref.npc->hasReplayWeaponShape) {
            shape.previousA = attacker.ref.npc->replayBladePreviousA;
            shape.previousB = attacker.ref.npc->replayBladePreviousB;
            shape.currentA = attacker.ref.npc->replayBladeCurrentA;
            shape.currentB = attacker.ref.npc->replayBladeCurrentB;
            shape.radius = attacker.ref.npc->replayBladeRadius;
        }
        if (attacker.ref.npc) {
            attacker.ref.npc->lastPhysicalWeaponShape = shape;
            attacker.ref.npc->hasLastPhysicalWeaponShape = true;
        } else if (attacker.ref.player) {
            attacker.ref.player->lastPhysicalWeaponShape = shape;
            attacker.ref.player->hasLastPhysicalWeaponShape = true;
        }
        if (tick - state.lastDiagnostic >= 60) {
            state.lastDiagnostic = tick;
            StructuredLogger::instance().writeEvent(StructuredCategory::Weapons, StructuredLevel::Important,
                "weapon.contact.shape", std::to_string(attacker.ref.id), "sampled equipped model blade", tick,
                {{"actor", attacker.ref.id}, {"actor_type", (int)attacker.ref.kind}, {"weapon", def->id},
                 {"action", attacker.actionId}, {"radius", shape.radius},
                 {"base", {shape.currentA.x,shape.currentA.y,shape.currentA.z}},
                 {"tip", {shape.currentB.x,shape.currentB.y,shape.currentB.z}}, {"always_active", true}});
        }
        const glm::vec3 low = glm::min(glm::min(shape.previousA, shape.previousB),
            glm::min(shape.currentA, shape.currentB)) - glm::vec3(shape.radius);
        const glm::vec3 high = glm::max(glm::max(shape.previousA, shape.previousB),
            glm::max(shape.currentA, shape.currentB)) + glm::vec3(shape.radius);
        for (const auto& target : actors) {
            if (target.key == attacker.key) continue;
            auto episodeIt = state.episodes.find(target.key);
            const int attackerTeam = attacker.ref.player ? attacker.ref.player->matchTeam : attacker.ref.npc->matchTeam;
            const int targetTeam = target.ref.player ? target.ref.player->matchTeam : target.ref.npc->matchTeam;
            // Candidate policy filter; the shared damage owner still validates
            // roster fallbacks when either mirror has no resolved team yet.
            if (!serverFriendlyFireEnabled() && attackerTeam >= 0 && targetTeam >= 0 && attackerTeam == targetTeam) {
                if (episodeIt != state.episodes.end()) {
                    confirm(sock, players, attacker, target, episodeIt->second, tick, packets);
                    state.episodes.erase(episodeIt);
                }
                continue;
            }
            bool touching = false;
            const glm::vec3 targetDelta = target.runtime->positionHistoryValid
                ? target.position - target.runtime->previousPosition : glm::vec3(0);
            const glm::vec3 targetLow = glm::min(target.runtime->minimum, target.runtime->minimum - targetDelta);
            const glm::vec3 targetHigh = glm::max(target.runtime->maximum, target.runtime->maximum - targetDelta);
            if (glm::all(glm::lessThanEqual(low, targetHigh)) &&
                glm::all(glm::greaterThanEqual(high, targetLow))) {
                auto relative = shape;
                if (target.runtime->positionHistoryValid) {
                    relative.previousA += targetDelta; relative.previousB += targetDelta;
                }
                WeaponExecution::PhysicalContactHit hit;
                touching = WeaponExecution::testPhysicalContact(relative, target.runtime->target, hit);
                if (touching) {
                    const uint64_t claimKey =
                        (static_cast<uint64_t>(target.ref.id) << 32) | tick;
                    if (attacker.ref.player &&
                        attacker.ref.player->meleeClaimedTargetTicks.count(claimKey))
                    {
                        // A lenient client claim already applied this target/tick.
                        continue;
                    }
                    auto& episode = state.episodes[target.key];
                    if (episode.generation != target.generation) episode = Episode{};
                    episode.generation = target.generation; episode.lastTick = tick;
                    const bool beginning = episode.serial == 0;
                    if (beginning) episode.serial = state.nextSerial++;
                    const glm::vec3 blade = shape.currentB - shape.currentA;
                    const float along = glm::clamp(glm::dot(hit.hitPosition - shape.currentA, blade) /
                        std::max(glm::dot(blade, blade), 0.00001f), 0.0f, 1.0f);
                    const glm::vec3 velocity = (glm::mix(shape.currentA, shape.currentB, along) -
                        glm::mix(shape.previousA, shape.previousB, along)) / SERVER_DT - target.velocity;
                    const float speed = glm::length(velocity);
                    const float directness = speed > 0.00001f ? std::abs(glm::dot(velocity / speed, hit.normal)) : 0;
                    const char* actionPhase = nullptr;
                    int actionPhaseTick = 0, actionPhaseTicks = 0;
                    const WeaponData::MeleeAction* contactAction = currentAction(
                        attacker, tick, &actionPhase, &actionPhaseTick, &actionPhaseTicks);
                    const bool actionActive = contactAction && actionPhase &&
                        std::string(actionPhase) == "active";
                    const float force = glm::clamp(speed / std::max(0.001f,
                        contactAction && contactAction->contactMaxForceSpeed >= 0.0f
                            ? contactAction->contactMaxForceSpeed
                            : WeaponExecution::paramOr(*def, "contactMaxForceSpeed", 22)),
                        0.0f, 1.0f) * directness;
                    float minDamage = WeaponExecution::paramOr(*def, "contactMinDamage", 1);
                    float maxDamage = WeaponExecution::paramOr(*def, "contactMaxDamage", def->damage);
                    float minKnockback = WeaponExecution::paramOr(*def, "contactMinKnockback", 0.5f);
                    float maxKnockback = WeaponExecution::paramOr(*def, "contactMaxKnockback", 40);
                    if (actionActive && contactAction) {
                        if (contactAction->contactMinDamage >= 0.0f)
                            minDamage = contactAction->contactMinDamage;
                        if (contactAction->contactMaxDamage >= 0.0f)
                            maxDamage = contactAction->contactMaxDamage;
                        if (contactAction->contactMinKnockback >= 0.0f)
                            minKnockback = contactAction->contactMinKnockback;
                        if (contactAction->contactMaxKnockback >= 0.0f)
                            maxKnockback = contactAction->contactMaxKnockback;
                    }
                    float actionMultiplier = 1.0f;
                    if (actionActive && contactAction)
                        actionMultiplier = std::max(0.0f, contactAction->contactDamageMultiplier);
                    float revMultiplier = 1.0f;
                    {
                        const float revMax = WeaponExecution::paramOr(*def, "revDamageMultiplier", 1.0f);
                        const float revDecay = WeaponExecution::paramOr(*def, "revDecayTicks", 0.0f);
                        if (revMax > 1.0f && revDecay > 0.0f && attacker.actionStart != 0) {
                            const float t = glm::clamp(
                                (float)(tick - attacker.actionStart) / revDecay, 0.0f, 1.0f);
                            revMultiplier = 1.0f + (revMax - 1.0f) * (1.0f - t);
                        }
                    }
                    const int damage = damaging
                        ? WeaponExecution::computeMeleeContactDamage(
                              minDamage, maxDamage, force, actionMultiplier,
                              revMultiplier, def->meleeContact.damageScale)
                        : 0;
                    const glm::vec3 impulse =
                        WeaponExecution::computeMeleeContactKnockback(
                            hit.normal, minKnockback, maxKnockback, force,
                            def->meleeContact.forceScale);

                    if (blocksMeleeContact(target, tick, relative, hit)) {
                        const float blockForce = std::max(1.5f, force * 12.0f);
                        const glm::vec3 blockImpulse = -hit.normal * blockForce;
                        if (attacker.ref.player) {
                            attacker.ref.player->vel += blockImpulse;
                            recordServerMovementExternalImpulse(*attacker.ref.player, blockImpulse);
                        } else if (attacker.ref.npc) {
                            attacker.ref.npc->knockbackImpulse += blockImpulse;
                        }
                        broadcastWeaponClash(sock, players, attacker.ref, hit.hitPosition,
                            hit.normal, blockForce, packets);
                        StructuredLogger::instance().writeEvent(StructuredCategory::Weapons,
                            StructuredLevel::Important, "weapon.block", std::to_string(attacker.ref.id),
                            "front-arc melee block stopped contact", tick,
                            {{"attacker", attacker.ref.id}, {"target", target.ref.id},
                             {"weapon", def->id}, {"target_weapon", target.weapon ? target.weapon->id : ""},
                             {"force", force}});
                        if (episodeIt != state.episodes.end()) {
                            confirm(sock, players, attacker, target, episodeIt->second, tick, packets);
                            state.episodes.erase(episodeIt);
                        }
                        continue;
                    }

                    if (!damaging) {
                        if (target.ref.player) {
                            target.ref.player->vel += impulse;
                            recordServerMovementExternalImpulse(*target.ref.player, impulse);
                        } else if (target.ref.npc) {
                            target.ref.npc->knockbackImpulse += impulse;
                        }
                        if (beginning)
                            broadcastWeaponClash(sock, players, attacker.ref,
                                hit.hitPosition, hit.normal, glm::length(impulse), packets);
                    } else {
                    ServerActorDamageRequest request;
                    request.attacker = attacker.ref; request.victim = target.ref;
                    request.damage = damage; request.knockback = impulse;
                    request.source = ServerDamageSource::PhysicalContact; request.weaponId = def->id;
                    request.weaponDisplayName = def->displayName; request.serverTick = tick;
                    request.eventId = episode.serial; request.correlationId = episode.serial;
                    const auto result = applyActorDamage(players, npcs, request);
                    if (ScenarioServerPlayback::instance().active()) {
                        StructuredLogger::instance().writeEvent(
                            StructuredCategory::Weapons,
                            result.applied ? StructuredLevel::Important : StructuredLevel::Errors,
                            "scenario.contact.query", std::to_string(attacker.ref.id),
                            "scenario melee contact query and authoritative decision",
                            tick,
                            nlohmann::json{
                                {"attacker", attacker.ref.id},
                                {"attacker_kind", (int)attacker.ref.kind},
                                {"target", target.ref.id},
                                {"target_kind", (int)target.ref.kind},
                                {"attacker_generation", attacker.generation},
                                {"target_generation", target.generation},
                                {"weapon", def->id},
                                {"action", attacker.actionId},
                                {"action_start_tick", attacker.actionStart},
                                {"blade_base", {shape.currentA.x, shape.currentA.y,
                                                shape.currentA.z}},
                                {"blade_tip", {shape.currentB.x, shape.currentB.y,
                                               shape.currentB.z}},
                                {"blade_radius", shape.radius},
                                {"contact", true},
                                {"contact_position", {hit.hitPosition.x, hit.hitPosition.y,
                                                      hit.hitPosition.z}},
                                {"damage", damage},
                                {"knockback", {impulse.x, impulse.y, impulse.z}},
                                {"accepted", result.applied},
                                {"rejection_reason", result.rejectionReason},
                                {"health_before", result.healthBefore},
                                {"health_after", result.healthAfter},
                                {"killed", result.killed},
                                {"target_health_reference", target.ref.player
                                    ? target.ref.player->health : target.ref.npc->health}},
                            __FILE__, __LINE__, __FUNCTION__);
                    }
                    if (result.applied) {
                        if (localResults) localResults->push_back({request, result, hit.hitPosition, force,
                            beginning || episode.samples + 1 >= 4 || result.killed});
                        if (episode.samples == 0) episode.before = result.healthBefore;
                        episode.after = result.healthAfter; episode.damage += damage;
                        episode.impulse += impulse; episode.hit = hit.hitPosition; episode.normal = hit.normal;
                        episode.killed = result.killed; episode.force = std::max(episode.force, force); ++episode.samples;
                        if (episode.samples >= 4 || result.killed) confirm(sock, players, attacker, target, episode, tick, packets);
                        if (result.killed && target.ref.npc && sock != INVALID_SOCKET) serverGamemodeRecordKill(sock, players, &npcs,
                            attacker.ref.id, (uint8_t)attacker.ref.kind, target.ref.id, (uint8_t)target.ref.kind,
                            def->id, def->displayName, episode.serial, attacker.position, target.position, tick, packets);
                        if (result.killed || beginning) {
                            StructuredLogger::instance().writeEvent(StructuredCategory::Weapons, StructuredLevel::Important,
                                "weapon.contact.result", std::to_string(attacker.ref.id), "shared actor blade contact applied", tick,
                                {{"attacker", attacker.ref.id}, {"attacker_type", (int)attacker.ref.kind},
                                 {"target", target.ref.id}, {"target_type", (int)target.ref.kind}, {"weapon", def->id},
                                 {"speed", speed}, {"directness", directness}, {"force", force}, {"damage", damage},
                                 {"health_before", result.healthBefore}, {"health_after", result.healthAfter}, {"killed", result.killed},
                                 {"blade_base", {shape.currentA.x,shape.currentA.y,shape.currentA.z}},
                                 {"blade_tip", {shape.currentB.x,shape.currentB.y,shape.currentB.z}},
                                 {"action", attacker.actionId}, {"attack_required", false}});
                        }
                    }
                    }
                }
            }
            // Weapon-vs-weapon contact. This is separate from actor-body
            // contact: two blades can meet without either body being touched.
            if (target.weapon && target.weapon->id != "nothing" &&
                target.runtime->previousValid) {
                const float weaponDistance = segmentSegmentDistance(
                    shape.currentA, shape.currentB,
                    target.runtime->shape.currentA, target.runtime->shape.currentB);
                if (weaponDistance <= shape.radius + target.runtime->shape.radius + 0.12f) {
                    uint32_t& lastClash = state.lastClashTick[target.key];
                    if (lastClash == 0 || tick - lastClash >= 6) {
                        lastClash = tick;
                        const glm::vec3 attackerBladeVelocity =
                            (shape.currentB - shape.previousB) / SERVER_DT;
                        const glm::vec3 targetBladeVelocity =
                            (target.runtime->shape.currentB -
                             target.runtime->shape.previousB) / SERVER_DT;
                        const float relativeSpeed = glm::length(
                            attackerBladeVelocity - targetBladeVelocity);
                        glm::vec3 normal = target.position - attacker.position;
                        if (glm::length(normal) < 0.001f)
                            normal = glm::vec3(0.0f, 0.0f, 1.0f);
                        normal = glm::normalize(normal);
                        const float force = glm::clamp(relativeSpeed / 25.0f, 0.0f, 1.0f);
                        const float strength = 8.0f + force * 55.0f;
                        const glm::vec3 attackerImpulse = -normal * strength;
                        const glm::vec3 targetImpulse = normal * strength;
                        if (attacker.ref.player) {
                            attacker.ref.player->vel += attackerImpulse;
                            recordServerMovementExternalImpulse(*attacker.ref.player, attackerImpulse);
                        } else if (attacker.ref.npc) {
                            attacker.ref.npc->knockbackImpulse += attackerImpulse;
                        }
                        if (target.ref.player) {
                            target.ref.player->vel += targetImpulse;
                            recordServerMovementExternalImpulse(*target.ref.player, targetImpulse);
                        } else if (target.ref.npc) {
                            target.ref.npc->knockbackImpulse += targetImpulse;
                        }
                        const glm::vec3 contactPoint = glm::mix(shape.currentB,
                            target.runtime->shape.currentB, 0.5f);
                        broadcastWeaponClash(sock, players, attacker.ref, contactPoint,
                            normal, strength, packets);
                        StructuredLogger::instance().writeEvent(
                            StructuredCategory::Weapons, StructuredLevel::Important,
                            "weapon.clash", std::to_string(attacker.ref.id),
                            "melee weapon contact applied opposing impulses", tick,
                            {{"attacker", attacker.ref.id},
                             {"attacker_type", (int)attacker.ref.kind},
                             {"target", target.ref.id},
                             {"target_type", (int)target.ref.kind},
                             {"attacker_weapon", def->id},
                             {"target_weapon", target.weapon->id},
                             {"relative_speed", relativeSpeed},
                             {"force", force},
                             {"impulse", strength},
                             {"contact", {contactPoint.x, contactPoint.y, contactPoint.z}}});
                        if (ScenarioServerPlayback::instance().active()) {
                            StructuredLogger::instance().writeEvent(
                                StructuredCategory::Weapons, StructuredLevel::Important,
                                "scenario.contact.weapon", std::to_string(attacker.ref.id),
                                "scenario weapon-versus-weapon contact resolved", tick,
                                nlohmann::json{
                                    {"attacker", attacker.ref.id},
                                    {"attacker_kind", (int)attacker.ref.kind},
                                    {"target", target.ref.id},
                                    {"target_kind", (int)target.ref.kind},
                                    {"attacker_weapon", def->id},
                                    {"target_weapon", target.weapon->id},
                                    {"distance", weaponDistance},
                                    {"threshold", shape.radius +
                                        target.runtime->shape.radius + 0.12f},
                                    {"relative_speed", relativeSpeed},
                                    {"force", force},
                                    {"impulse", strength},
                                    {"contact", {contactPoint.x, contactPoint.y,
                                                 contactPoint.z}},
                                    {"throttle_ticks", 6}},
                                __FILE__, __LINE__, __FUNCTION__);
                        }
                    }
                }
            }
            if (!touching && episodeIt != state.episodes.end()) {
                confirm(sock, players, attacker, target, episodeIt->second, tick, packets);
                state.episodes.erase(episodeIt);
            }
        }
        for (auto it = state.episodes.begin(); it != state.episodes.end(); ) {
            if (it->second.lastTick != tick) it = state.episodes.erase(it); else ++it;
        }
        state.shape = shape; state.previousValid = true;
    }
    for (const auto& actor : actors) {
        actor.runtime->previousPosition = actor.position;
        actor.runtime->positionHistoryValid = true;
    }
    for (auto it = runtimes.begin(); it != runtimes.end(); ) {
        if (it->second.lastTick != tick) {
            flushPending(sock, players, npcs, it->first, it->second, tick, packets);
            it = runtimes.erase(it);
        } else ++it;
    }
}
}
