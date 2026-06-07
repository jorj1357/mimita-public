#include "combat/death-system.h"

#include <algorithm>

#include "camera.h"
#include "audio/audio.h"
#include "input/input-state.h"
#include "npc/npc.h"
#include "physics/physics-mini.h"
#include "physics/movement/physics-collision.h"
#include "render/render-player.h"
#include "replay/replay.h"
#include "world/world.h"

namespace {
constexpr float RESPAWN_SECONDS = 3.0f;
constexpr float BLACK_FADE_SECONDS = 0.5f;
constexpr float CORPSE_LIFETIME_SECONDS = 8.0f;
constexpr float CORPSE_DRAG = 3.0f;

void emitLifecycleEvent(const char* type,
                        const Player& actor,
                        const std::string& actorId,
                        const std::string& otherActorId)
{
    ReplayEffectEvent event;
    event.type = type;
    event.position = actor.pos;
    event.direction = actor.vel;
    event.sourceActorId = otherActorId;
    event.targetActorId = actorId;
    captureReplayEffect(event);
}
}

DeathSystem& DeathSystem::instance()
{
    static DeathSystem system;
    return system;
}

bool DeathSystem::kill(
    Player& victim,
    const std::string& actorId,
    const std::string& actorType,
    const std::string& killer,
    const glm::vec3& shotDirection,
    float lethalForce)
{
    if (victim.dead)
        return false;

    CorpseActor corpse(victim);
    corpse.id = actorId + "_corpse_" + std::to_string(++mCorpseSerial);
    corpse.name = victim.username + " corpse";
    corpse.body.username = corpse.name;
    corpse.body.dead = true;
    corpse.body.proceduralFrozen = true;
    corpse.body.currentHp = 0;
    corpse.body.externalImpulse = glm::vec3(0.0f);
    glm::vec3 direction = glm::length(shotDirection) > 0.001f
        ? glm::normalize(shotDirection)
        : glm::vec3(0.0f);
    corpse.body.vel = victim.vel + direction * lethalForce;
    corpse.body.vel.z += std::abs(lethalForce * 0.15f);
    corpse.body.syncLegacyStateToLayers();
    corpse.body.updateModelWorldTransforms();
    mCorpses.push_back(std::move(corpse));

    victim.currentHp = 0;
    victim.dead = true;
    victim.respawnTimer = RESPAWN_SECONDS;
    victim.killedBy = killer.empty() ? "unknown" : killer;

    if (actorType == "npc")
        AudioManager::instance().play(
            {"npc_death", AudioCategory::NPC, true, victim.pos, 1.0f, 0.9f, 45.0f, 0});

    emitLifecycleEvent("death", victim, actorId, killer);

    ReplayEffectEvent corpseEvent;
    corpseEvent.type = "corpse_spawn";
    corpseEvent.position = corpse.body.pos;
    corpseEvent.direction = direction;
    corpseEvent.velocity = corpse.body.vel;
    corpseEvent.sourceActorId = actorId;
    corpseEvent.targetActorId = corpse.id;
    captureReplayEffect(corpseEvent);

    return true;
}

void DeathSystem::respawn(Player& actor, const std::string& actorId)
{
    actor.pos = actor.respawnPosition;
    actor.vel = glm::vec3(0.0f);
    actor.externalImpulse = glm::vec3(0.0f);
    actor.currentHp = actor.maxHp;
    actor.dead = false;
    actor.proceduralFrozen = false;
    actor.respawnTimer = 0.0f;
    actor.killedBy.clear();
    actor.onGround = false;
    actor.syncLegacyStateToLayers();
    actor.updateModelWorldTransforms();
    emitLifecycleEvent("respawn", actor, actorId, actorId);
}

void DeathSystem::update(
    World& world,
    Player& player,
    NpcSystem& npcs,
    bool instantRespawnPressed,
    float dt)
{
    if (player.currentHp <= 0 && !player.dead) {
        kill(player, player.username, "player", "unknown", player.vel, 12.0f);
    }
    for (Npc& npc : npcs.all()) {
        if (npc.body.currentHp <= 0 && !npc.body.dead) {
            kill(
                npc.body,
                "npc_" + std::to_string(npc.id),
                "npc",
                "unknown",
                npc.body.vel,
                18.0f);
        }
    }

    for (CorpseActor& corpse : mCorpses) {
        corpse.age += dt;
        corpse.blackness = std::clamp(corpse.age / BLACK_FADE_SECONDS, 0.0f, 1.0f);
        corpse.fade = std::clamp(
            (corpse.age - BLACK_FADE_SECONDS) /
            (CORPSE_LIFETIME_SECONDS - BLACK_FADE_SECONDS),
            0.0f,
            1.0f);
        corpse.collidable = false;
        // Apply drag so corpse slides to a stop
        corpse.body.vel *= std::max(0.0f, 1.0f - CORPSE_DRAG * dt);
        // Sync corpse position from living dead body for replay accuracy
        if (corpse.id.find(player.username) == 0 && player.dead) {
            corpse.body.pos = player.pos;
            corpse.body.vel = player.vel + corpse.body.vel * 0.5f;
            corpse.body.yaw = player.yaw;
            corpse.body.onGround = player.onGround;
        }
        for (const Npc& npc : npcs.all()) {
            std::string expectedId = "npc_" + std::to_string(npc.id);
            if (corpse.id.find(expectedId) == 0 && npc.body.dead) {
                corpse.body.pos = npc.body.pos;
                corpse.body.vel = npc.body.vel;
                corpse.body.yaw = npc.body.yaw;
                corpse.body.onGround = npc.body.onGround;
                break;
            }
        }
    }

    mCorpses.erase(
        std::remove_if(mCorpses.begin(), mCorpses.end(), [](const CorpseActor& corpse) {
            return corpse.age >= CORPSE_LIFETIME_SECONDS;
        }),
        mCorpses.end());

    if (player.dead) {
        player.respawnTimer = std::max(0.0f, player.respawnTimer - dt);
        if (instantRespawnPressed || player.respawnTimer <= 0.0f)
            respawn(player, player.username);
    }

    for (Npc& npc : npcs.all()) {
        if (!npc.body.dead) continue;
        npc.body.respawnTimer = std::max(0.0f, npc.body.respawnTimer - dt);
        if (npc.body.respawnTimer <= 0.0f)
            respawn(npc.body, "npc_" + std::to_string(npc.id));
    }
}

void DeathSystem::render(const Camera& camera) const
{
}

void DeathSystem::appendReplayActors(std::vector<ReplayActorState>& actors) const
{
    for (const CorpseActor& corpse : mCorpses) {
        ReplayActorState actor;
        actor.id = corpse.id;
        actor.name = corpse.name;
        actor.type = "corpse";
        actor.modelPath = "assets/entity/player/default/mimita-char-no-animations-v4.glb";
        actor.position = corpse.body.pos;
        actor.rotation = glm::vec3(0.0f, 0.0f, corpse.body.yaw);
        actor.velocity = corpse.body.vel;
        actor.health = 0;
        actor.maxHealth = corpse.body.maxHp;
        actor.grounded = corpse.body.onGround;
        actor.collidable = corpse.collidable;
        actor.fade = corpse.fade;
        actor.blackness = corpse.blackness;
        actor.animationState = "dead";
        actor.bodyParts = captureReplayBodyParts(corpse.body);
        actors.push_back(actor);
    }
}
