// Actor adapters for the shared physical-contact weapon owner. Equipped clip
// profiles provide a fixed-tick pose; rendering and contact share attachment
// geometry, and every actor pair submits the same typed damage request.
#include "network/server.h"
#include "network/server-gamemode.h"
#include "network/network-weapons.h"
#include "combat/weapon-attachment.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-melee-config.h"
#include "combat/weapon-config.h"
#include "entities/player-avatar-animations.h"
#include "entities/player.h"
#include "debug/structured-log.h"
#include "utils/path_utils.h"
#include <algorithm>
#include <cmath>
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
    if (action && !action->animationClipId.empty()) pose.playOneShotAtTick(action->animationClipId,
        std::min((int)(tick - actor.actionStart), std::max(0, action->totalTicks() - 1)));
    else pose.clearOneShot();
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
        return def && !def->animationProfile.empty() && def->executionType == WeaponExecutionType::PhysicalContact;
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
        if (!def || def->animationProfile.empty() || def->executionType != WeaponExecutionType::PhysicalContact) continue;
        WeaponAttachmentPose attachment;
        bool valid = false;
        for (const auto& part : state.pose->physicalBody.parts) if (part.name == "rightArm") {
            valid = resolveWeaponAttachment(*def, part, attacker.scale, attachment); break;
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
                    const float force = glm::clamp(speed / std::max(0.001f,
                        WeaponExecution::paramOr(*def, "contactMaxForceSpeed", 22)), 0.0f, 1.0f) * directness;
                    const float minDamage = WeaponExecution::paramOr(*def, "contactMinDamage", 1);
                    const float maxDamage = WeaponExecution::paramOr(*def, "contactMaxDamage", def->damage);
                    const int damage = std::max(1, (int)std::lround(glm::mix(minDamage, maxDamage, force)));
                    const glm::vec3 impulse = hit.normal * glm::mix(
                        WeaponExecution::paramOr(*def, "contactMinKnockback", 0.5f),
                        WeaponExecution::paramOr(*def, "contactMaxKnockback", 40), force);
                    ServerActorDamageRequest request;
                    request.attacker = attacker.ref; request.victim = target.ref;
                    request.damage = damage; request.knockback = impulse;
                    request.source = ServerDamageSource::PhysicalContact; request.weaponId = def->id;
                    request.weaponDisplayName = def->displayName; request.serverTick = tick;
                    request.eventId = episode.serial; request.correlationId = episode.serial;
                    const auto result = applyActorDamage(players, npcs, request);
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
