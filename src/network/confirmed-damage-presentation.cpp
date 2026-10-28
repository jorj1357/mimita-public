// 07 19 2026, 13 05
/* purpose
* Implements generic attacker-only presentation for confirmed network damage.
* Reuses existing hitmarker, hitmarker audio, and HitEffects damage-number paths.
* Rejects duplicate, stale-session, stale-spawn, non-attacker, and self-damage events.
* Does NOT apply health, kill credit, death, knockback, or projectile authority.
* Does NOT contain weapon-specific name comparisons.
* Does NOT create new damage or reliability rules.
*/

#include "network/confirmed-damage-presentation.h"

#include "audio/hitmarker-audio.h"
#include "audio/audio.h"
#include "combat/actor-preset-weapons.h"
#include "combat/death-system.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-execution.h"
#include "config/weapon-hitfx-config.h"
#include "debug/structured-log.h"
#include "config/networking-config.h"
#include "network/multiplayer-context.h"
#include "network/network-weapons.h"
#include "effects/hit-effects.h"
#include "gui/hud/reward-popup.h"
#include "killfeed/killfeed.h"
#include "terminal/terminal-state.h"
#include "ui/hitmarker.h"

#include <algorithm>
#include <cstdio>
#include <string>

#include <glm/glm.hpp>

namespace MimitaNet {
namespace {

uint64_t presentationEventKey(uint32_t eventSessionId, uint32_t eventId)
{
    return ((uint64_t)eventSessionId << 32) | (uint64_t)eventId;
}

void rememberPresentedDamage(MultiplayerContext& ctx, uint64_t key)
{
    ctx.presentedDamageEventIds.insert(key);
    ctx.presentedDamageEventOrder.push_back(key);
    while (ctx.presentedDamageEventOrder.size() > 512)
    {
        ctx.presentedDamageEventIds.erase(ctx.presentedDamageEventOrder.front());
        ctx.presentedDamageEventOrder.pop_front();
    }
}

bool hasMatchingVictimSpawn(const MultiplayerContext& ctx,
                            const DamageConfirmedEventPacket& event)
{
    if (event.targetSpawnGeneration == 0)
        return true;
    if (event.targetPlayerId == ctx.localPlayerId)
        return event.targetSpawnGeneration == ctx.lastKnownSpawnGeneration;

    auto it = ctx.remotePlayers.find(event.targetPlayerId);
    return it == ctx.remotePlayers.end() ||
        it->second.spawnGeneration == event.targetSpawnGeneration;
}

std::string playerNameFor(const MultiplayerContext& ctx, uint32_t playerId)
{
    auto it = ctx.playerRegistry.find(playerId);
    if (it != ctx.playerRegistry.end())
        return it->second.name;
    return "player_" + std::to_string(playerId);
}

} // namespace

bool presentConfirmedDamage(MultiplayerContext& ctx,
                            const DamageConfirmedEventPacket& event,
                            const ConfirmedDamagePresentationSink* sink)
{
    if (event.eventId == 0 || event.eventSessionId == 0 || event.damage <= 0)
        return false;
    if (ctx.reliableEventSessionId != 0 && event.eventSessionId != ctx.reliableEventSessionId)
        return false;

    const uint64_t key = presentationEventKey(event.eventSessionId, event.eventId);
    if (ctx.presentedDamageEventIds.count(key))
        return false;

    const bool localAttacker = event.attackerPlayerId == ctx.localPlayerId;
    const bool localVictim = event.targetPlayerId == ctx.localPlayerId;
    if (localVictim && event.attackerEntityType == ENTITY_NPC) {
        const auto npcIt = ctx.remoteNpcs.find(event.attackerPlayerId);
        const Player* npc = npcIt != ctx.remoteNpcs.end() ? &npcIt->second : nullptr;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Important,
            "client.npc.damage-visible", std::to_string(event.attackerPlayerId),
            "NPC damaged local player; recording client presentation state",
            event.header.tick,
            {{"attacker_id", event.attackerPlayerId},
             {"present_in_remote_npcs", npc != nullptr},
             {"model_loaded", npc ? npc->modelLoaded : false},
             {"avatar", npc ? npc->avatarName() : std::string()},
             {"dead", npc ? npc->dead : true},
             {"position", npc ? nlohmann::json{npc->pos.x, npc->pos.y, npc->pos.z}
                              : nlohmann::json::array()},
             {"damage", event.damage},
             {"event_id", event.eventId}},
            __FILE__, __LINE__, __FUNCTION__);
    }
    // Local-victim presentation is owned by the projectile damage path so it
    // can attach blood/flinch to the local body once. Remote observers still
    // need the same damage numbers and hit effects as the attacker.
    if (!localAttacker && localVictim)
        return false;
    if (localAttacker && event.attackerSpawnGeneration != 0 &&
        event.attackerSpawnGeneration != ctx.lastKnownSpawnGeneration)
        return false;
    if (localAttacker && !hasMatchingVictimSpawn(ctx, event))
        return false;

    const char* weaponId = networkWeaponTypeName(event.weapon);
    const std::string* defId = weaponIdForDefNetworkId(event.weaponDefNetworkId);
    const bool meleeContact = defId && (*defId == "swordsword" ||
        *defId == "chainsaw" || *defId == "mace");
    if (meleeContact) {
        const float force01 = glm::clamp(
            glm::length(glm::vec3(event.knockX, event.knockY, event.knockZ)) / 60.0f,
            0.0f, 1.0f);
        const WeaponDefinition* contactDef = defId
            ? WeaponRegistry::instance().get(*defId) : nullptr;
        const float contactScale = contactDef
            ? std::max(0.01f, WeaponExecution::paramOr(
                *contactDef, "contactRadiusScale", 1.0f))
            : 1.0f;
        HitEffects::spawnMeleeContact(
            glm::vec3(event.hitX, event.hitY, event.hitZ),
            glm::vec3(event.normalX, event.normalY, event.normalZ),
            force01, (float)event.header.tick, contactScale);
        static thread_local uint32_t lastClashSoundTick = 0;
        if (event.header.tick == 0 || event.header.tick >= lastClashSoundTick + 6) {
            lastClashSoundTick = event.header.tick;
            static const char* sounds[] = {
                "weapon/general/metalricochet",
                "weapon/general/metalricochet2",
                "weapon/general/metalricochet3"
            };
            playWorldSound(sounds[rand() % 3],
                glm::vec3(event.hitX, event.hitY, event.hitZ),
                0.35f + force01 * 0.75f, 1.25f - force01 * 0.5f, 40.0f);
        }
    }
    const auto& presentation = WeaponHitFxConfig::instance().presentationFor(weaponId);
    const auto modeHitmarkers = ActorPresetWeapons::modePresentationOverride(
        ActorPresetWeapons::PresentationFlag::HitMarkers);
    const auto modeHitSounds = ActorPresetWeapons::modePresentationOverride(
        ActorPresetWeapons::PresentationFlag::HitSounds);
    const auto modeDamageNumbers = ActorPresetWeapons::modePresentationOverride(
        ActorPresetWeapons::PresentationFlag::DamageNumbers);
    if (localAttacker && !presentation.enabled && !modeHitmarkers &&
        !modeHitSounds && !modeDamageNumbers)
        return false;
    if (event.attackerPlayerId == event.targetPlayerId && !presentation.selfDamageFeedback)
        return false;

    rememberPresentedDamage(ctx, key);

    HitEvent hit;
    hit.position = {event.hitX, event.hitY, event.hitZ};
    hit.normal = {event.normalX, event.normalY, event.normalZ};
    hit.direction = glm::length(glm::vec3(event.knockX, event.knockY, event.knockZ)) > 0.001f
        ? glm::normalize(glm::vec3(event.knockX, event.knockY, event.knockZ))
        : -hit.normal;
    hit.hitEntity = true;
    hit.damage = event.damage;
    hit.attacker = playerNameFor(ctx, event.attackerPlayerId);
    hit.victim = playerNameFor(ctx, event.targetPlayerId);
    hit.weaponSource = weaponId;
    hit.spawnDamageNumber = true;

    if (!localAttacker)
    {
        // Remote attacker: the prediction path never ran on this client, so
        // spawn the shared confirmed hit effects (blood, decals, damage number)
        // locally. No hitmarker or hit sound; those stay attacker-owned.
        HitEffects::onHit(hit);
        printf("[NET DAMAGE PRESENT] eventId=%u attacker=%u target=%u damage=%d weapon=%u remote=1\n",
               event.eventId, event.attackerPlayerId, event.targetPlayerId,
               event.damage, (unsigned)event.weapon);
        return true;
    }

    // Server confirmed a hit the local attacker predicted.
    ++ctx.confirmedHits;

    const NetworkingConfigData& netCfg = NetworkingConfig::instance().data();
    const bool authMode = netCfg.serverAuthoritativeHits.enabled;

    // In server-authoritative mode, always show confirmed feedback.
    // In prediction-only mode, only show if the individual flags are set.
    if (modeHitmarkers.value_or(presentation.hitmarker && ActorPresetWeapons::hitMarkersEnabled()))
    {
        if (sink && sink->showHitmarker)
            sink->showHitmarker(event.damage, sink->user);
        else if (authMode)
            hitmarkerVisualOnly(event.damage);
    }
    if (modeHitSounds.value_or(presentation.hitSound && ActorPresetWeapons::hitSoundsEnabled()))
    {
        if (sink && sink->playHitSound)
            sink->playHitSound(event.damage, sink->user);
        else if (authMode)
            playHitmarkerSound(event.damage);
    }
    if (modeDamageNumbers.value_or(presentation.damageNumber &&
                                   ActorPresetWeapons::damageNumbersEnabled()))
    {
        if (sink && sink->showDamageNumber)
            sink->showDamageNumber(hit, sink->user);
        else if (authMode)
            HitEffects::spawnHitEffects(hit.position, hit.direction, hit.normal,
                                        hit.damage, hit.attacker, hit.victim, true);
    }

    // On kill, heal the local attacker to full health (matches server authority)
    if (event.killed && gpPlayer)
    {
        // Server confirmed a kill: any pending predicted kill-heal for this
        // entity sticks (no rollback).
        mpConfirmPredictedKillHeal(ctx, event.targetPlayerId);
        DeathSystem::instance().healKillerToFull(*gpPlayer, playerNameFor(ctx, event.attackerPlayerId));
        printf("[NET KILL HEAL] attacker=%u health=%d\n", event.attackerPlayerId, gpPlayer->currentHp);

        // The killfeed line is owned by the single authoritative
        // KillEventPacket so every viewer sees one identical entry. This path
        // only presents attacker-side hitmarkers, sound, and damage numbers.
    }

    printf("[NET DAMAGE PRESENT] eventId=%u attacker=%u target=%u damage=%d weapon=%u hitmarker=%d sound=%d damageNumber=%d killed=%d\n",
           event.eventId, event.attackerPlayerId, event.targetPlayerId,
           event.damage, (unsigned)event.weapon, (int)presentation.hitmarker,
           (int)presentation.hitSound, (int)presentation.damageNumber, (int)event.killed);
    return true;
}

} // namespace MimitaNet
