// 07 31 2026, 14 50
/* purpose
* Sends and receives shot and pellet-blast packets, and reconstructs shot visuals.
* Replays authoritative tracer and hit effects for non-shooter clients.
* Does NOT own server weapon authority, damage validation, or socket transport.
* Does NOT run the fixed-step simulation tick or render viewmodels.
*/
#include "network/multiplayer-context.h"
#include "network/net_common.h"
#include "network/packets.h"
#include "network/network-weapons.h"
#include "network/disagreement-visuals.h"
#include "combat/weapon-fire.h"
#include "combat/death-system.h"
#include "combat/weapon-registry.h"
#include "config/ragdoll-death-config.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "entities/death-ghost.h"
#include "audio/audio.h"
#include "gui/hud/reward-popup.h"
#include "audio/hitmarker-audio.h"
#include "ui/hitmarker.h"
#include "killfeed/killfeed.h"
#include "terminal/terminal-state.h"
#include "config/networking-config.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <glm/glm.hpp>

namespace MimitaNet {

namespace {

const EntityInterpolationState* findRemoteInterpolation(
    const MultiplayerContext& ctx,
    uint32_t shooterId)
{
    auto playerIt = ctx.remotePlayerInterpolation.find(shooterId);
    if (playerIt != ctx.remotePlayerInterpolation.end())
        return &playerIt->second;
    auto npcIt = ctx.remoteNpcInterpolation.find(shooterId);
    if (npcIt != ctx.remoteNpcInterpolation.end())
        return &npcIt->second;
    return nullptr;
}

// Roll back a locally predicted death when the server says the target is alive.
// Clears the replica's death state and surfaces a disagreement indicator.
void revivePredictedDeath(Player& replica, const glm::vec3& at, const char* reason)
{
    replica.dead = false;
    replica.netPredictedDead = false;
    replica.proceduralFrozen = false;
    replica.respawnTimer = 0.0f;
    replica.deathAnim = Player::DeathAnimState{};
    Debug::log(Debug::Category::Networking,
               "[NET PREDICTED DEATH ROLLBACK] victim=%s reason=%s pos=(%.2f,%.2f,%.2f)",
               replica.username.c_str(), reason, at.x, at.y, at.z);
    if (NetworkingConfig::instance().data().disagreement.enabled)
    {
        DisagreementEvent ev;
        ev.timeMs = nowMs();
        ev.reason = DISAGREEMENT_INVALID_STATE;
        ev.position = at;
        ev.correction = glm::vec3(0.0f);
        ev.sourcePlayerId = 0;
        ev.targetPlayerId = 0;
        ev.description = reason;
        spawnDisagreementEffect(ev);
        logDisagreement(ev);
    }
}

bool visualTimelineReady(const MultiplayerContext& ctx,
                         uint32_t shooterId,
                         uint32_t visualServerTick,
                         uint64_t receivedMs)
{
    const NetworkingConfigData& cfg = NetworkingConfig::instance().data();
    if (!cfg.eventTimeline.enabled ||
        cfg.remotePlayers.directRender ||
        shooterId == 0 ||
        shooterId == ctx.localPlayerId ||
        visualServerTick == 0)
    {
        return true;
    }

    const EntityInterpolationState* interpolation =
        findRemoteInterpolation(ctx, shooterId);
    if (interpolation && interpolation->hasRendered &&
        interpolation->lastRenderedServerTick >= visualServerTick)
    {
        return true;
    }

    const uint64_t now = nowMs();
    if (receivedMs != 0 &&
        now >= receivedMs &&
        (double)(now - receivedMs) >= cfg.eventTimeline.remoteEffectMaximumHoldMs)
    {
        if (cfg.eventTimeline.logDelays)
        {
            printf("[NET EVENT TIMELINE RELEASE] shooter=%u visualTick=%u "
                   "renderTick=%u reason=max-hold holdMs=%llu\n",
                   shooterId, visualServerTick,
                   interpolation ? interpolation->lastRenderedServerTick : 0,
                   (unsigned long long)(now - receivedMs));
        }
        return true;
    }

    return false;
}

void releaseOrQueueShotEvent(MultiplayerContext& ctx, const NetworkShotEvent& event)
{
    if (visualTimelineReady(ctx, event.shooterPlayerId,
                            event.visualServerTick, event.receivedMs))
    {
        ctx.shotEvents.push_back(event);
        return;
    }

    ctx.pendingShotEvents.push_back(event);
    if (NetworkingConfig::instance().data().eventTimeline.logDelays)
    {
        printf("[NET EVENT TIMELINE HOLD] type=shot shooter=%u serial=%u "
               "visualTick=%u pending=%zu\n",
               event.shooterPlayerId, event.shotSerial,
               event.visualServerTick, ctx.pendingShotEvents.size());
    }
}

} // namespace

bool mpVisualTimelineReady(const MultiplayerContext& ctx,
                           uint32_t shooterId,
                           uint32_t visualServerTick,
                           uint64_t receivedMs)
{
    return visualTimelineReady(ctx, shooterId, visualServerTick, receivedMs);
}

void applyPelletBlastEventPacket(MultiplayerContext& ctx,
                                 const PelletBlastEventPacket* event);

void mpProcessShotEventPacket(MultiplayerContext& ctx, const ShotEventPacket* event)
{
    // Reliable delivery dedup + ACK: retransmissions of the same shot event
    // are dropped here and the server stops re-sending once we ACK.
    if (!mpAcceptReliableEventOnce(ctx, event->eventId, event->eventSessionId))
    {
        Debug::log(Debug::Category::Networking,
                   "[NET SHOT RECV] shooter=%u serial=%u skipped=duplicate-reliable eventId=%u\n",
                   event->shooterPlayerId, event->shotSerial, event->eventId);
        return;
    }

    uint32_t& lastSerial = ctx.lastReceivedShotSerial[event->shooterPlayerId];
    if (lastSerial != 0 &&
        (int32_t)(event->shotSerial - lastSerial) <= 0)
    {
        printf("[NET SHOT RECV] shooter=%u serial=%u skipped=duplicate last=%u\n",
               event->shooterPlayerId, event->shotSerial, lastSerial);
        return;
    }
    lastSerial = event->shotSerial;

    NetworkShotEvent out;
    out.shotSerial = event->shotSerial;
    out.clientTimeMs = event->clientTimeMs;
    out.receivedMs = nowMs();
    out.eventServerTick = event->header.tick;
    out.visualServerTick = event->lastServerTick != 0
        ? event->lastServerTick
        : event->header.tick;
    out.shooterPlayerId = event->shooterPlayerId;
    out.targetPlayerId = event->targetPlayerId;
    out.damage = event->damage;
    out.targetHealth = event->targetHealth;
    out.power = event->power;
    out.effectFlags = event->effectFlags;
    out.targetTransformEpoch = event->targetTransformEpoch;
    out.weapon = event->weapon;
    out.impactType = event->impactType;
    out.killed = event->killed != 0;
    out.damageConfirmed = event->damageConfirmed != 0;
    out.origin = {event->originX, event->originY, event->originZ};
    out.hit = {event->hitX, event->hitY, event->hitZ};
    out.direction = {event->dirX, event->dirY, event->dirZ};
    out.beamEnd = {event->beamEndX, event->beamEndY, event->beamEndZ};
    out.normal = {event->normalX, event->normalY, event->normalZ};
    out.knockback = {event->knockX, event->knockY, event->knockZ};
    releaseOrQueueShotEvent(ctx, out);
    {
        char msg[512];
        std::snprintf(msg, sizeof(msg),
                      "shooter=%u serial=%u weapon=%u impact=%u origin=(%.2f,%.2f,%.2f) hit=(%.2f,%.2f,%.2f) damage=%d",
                      event->shooterPlayerId, event->shotSerial,
                      (unsigned)event->weapon, (unsigned)event->impactType,
                      out.origin.x, out.origin.y, out.origin.z,
                      out.hit.x, out.hit.y, out.hit.z, event->damage);
        ::logStructured(::StructuredCategory::Network, ::StructuredLevel::Important,
                        "SHOT_EVENT_RECV",
                        "SHOT_" + std::to_string(event->shooterPlayerId) + "_" +
                            std::to_string(event->shotSerial),
                        "reliable shot event delivered", msg);
    }
    printf("[NET SHOT RECV] shooter=%u serial=%u weapon=%u impact=%u "
           "flags=0x%03x damageConfirmed=%d visualTick=%u origin=(%.2f %.2f %.2f) "
           "hit=(%.2f %.2f %.2f)\n",
           out.shooterPlayerId, out.shotSerial, out.weapon,
           out.impactType, out.effectFlags, (int)out.damageConfirmed,
           out.visualServerTick,
           out.origin.x, out.origin.y, out.origin.z,
           out.hit.x, out.hit.y, out.hit.z);
}

void mpProcessNpcDamageEventPacket(MultiplayerContext& ctx, const NpcDamageEventPacket* event)
{
    // Reliable delivery: dedup + ACK so the kill-heal/killfeed can never be
    // lost under packet loss (retransmitted until ACKed).
    if (!mpAcceptReliableEventOnce(ctx, event->eventId, event->eventSessionId))
    {
        printf("[NET NPC DAMAGE RECV] eventId=%u npc=%u accepted=0 reason=duplicate-or-stale\n",
               event->eventId, event->npcEntityId);
        return;
    }

    const bool isLocalShooter = event->shooterPlayerId == ctx.localPlayerId;

    // How long a locally predicted NPC hit keeps suppressing the server-confirm
    // duplicate hitmarker/killfeed. Covers one round trip plus rapid fire.
    constexpr uint64_t NPC_PREDICT_WINDOW_MS = 500;

    std::string shooterName;
    MimitaVip::VipAppearance shooterVipAppearance;
    MimitaVip::VipStyleDetail shooterVipStyleDetail;
    {
        auto si = ctx.playerRegistry.find(event->shooterPlayerId);
        if (si != ctx.playerRegistry.end())
        {
            shooterName = si->second.name;
            shooterVipAppearance = si->second.vipAppearance;
            shooterVipStyleDetail = si->second.vipStyleDetail;
        }
        else
        {
            shooterName = "player_" + std::to_string(event->shooterPlayerId);
        }
    }

    std::string npcName = "NPC " + std::to_string(event->npcEntityId);
    Player* npcPtr = nullptr;
    auto npcIt = ctx.remoteNpcs.find(event->npcEntityId);
    if (npcIt != ctx.remoteNpcs.end())
    {
        npcPtr = &npcIt->second;
        if (!npcPtr->username.empty())
            npcName = npcPtr->username;
        printf("[NET NPC DAMAGE RECV] npcId=%u damage=%d health=%d killed=%d\n",
               event->npcEntityId, event->damage, event->npcHealth,
               (int)event->killed);
    }
    else
    {
        printf("[NET NPC DAMAGE RECV] npcId=%u not-found\n", event->npcEntityId);
    }

    const glm::vec3 hitPos(event->hitX, event->hitY, event->hitZ);
    const glm::vec3 hitNml(event->normalX, event->normalY, event->normalZ);
    const glm::vec3 hitDir(event->dirX, event->dirY, event->dirZ);
    const char* weaponId = networkWeaponTypeName(event->weapon);

    // ── Predicted local-shooter feedback ─────────────────────────────
    // The instant hitmarker/blood were shown when the local ray connected.
    // The server confirm is authoritative for health and reconciles the
    // prediction: it suppresses the duplicate hitmarker and rolls back a
    // predicted kill the server says did not happen.
    const uint64_t now = nowMs();
    auto predIt = ctx.predictedNpcHitMs.find(event->npcEntityId);
    const bool predicted = predIt != ctx.predictedNpcHitMs.end() &&
        now - predIt->second < NPC_PREDICT_WINDOW_MS;
    const bool authMode = NetworkingConfig::instance().data().serverAuthoritativeHits.enabled;
    // In server-authoritative mode, we never showed prediction feedback,
    // so always take the server-confirmed path.
    const bool actuallyPredicted = predicted && !authMode;

    Debug::warn(Debug::Category::Weapons,
        "[GODBALL_DBG] CLIENT_SERVER_DAMAGE_EVENT npc=%u damage=%d serverHealth=%d "
        "killed=%d localShooter=%d replicaFound=%d replicaHealthBefore=%d "
        "predictedHitPresent=%d serverAuthMode=%d",
        event->npcEntityId, event->damage, event->npcHealth,
        (int)event->killed, (int)isLocalShooter, (int)(npcPtr != nullptr),
        npcPtr ? npcPtr->currentHp : -1, (int)predicted, (int)authMode);

    if (isLocalShooter && event->damage > 0)
    {
        if (actuallyPredicted)
        {
            // Server says the NPC survived a hit we predicted as lethal:
            // revive the replica and show a disagreement indicator.
            if (npcPtr && npcPtr->netPredictedDead && !event->killed)
            {
                revivePredictedDeath(*npcPtr, hitPos, "NPC SURVIVED SERVER DISAGREES");
                // Roll back the predicted kill-heal in the same tick the
                // disagreement effect above spawns.
                mpRollbackPredictedKillHeal(ctx, event->npcEntityId);
            }
            // Damage mismatch: show disagreement if server damage differs significantly
            auto dmgIt = ctx.predictedNpcDamage.find(event->npcEntityId);
            if (dmgIt != ctx.predictedNpcDamage.end() && dmgIt->second > 0)
            {
                const float ratio = std::abs((float)event->damage - (float)dmgIt->second) / (float)dmgIt->second;
                if (ratio > 0.1f)
                {
                    if (NetworkingConfig::instance().data().disagreement.enabled)
                    {
                        DisagreementEvent de;
                        de.timeMs = nowMs();
                        de.reason = DISAGREEMENT_INVALID_DAMAGE;
                        de.position = hitPos;
                        de.sourcePlayerId = ctx.localPlayerId;
                        de.targetPlayerId = event->npcEntityId;
                        de.description = "DAMAGE MISMATCH";
                        spawnDisagreementEffect(de);
                        logDisagreement(de);
                    }
                }
            }
            Debug::log(Debug::Category::Networking,
                       "[NET NPC HIT PRESENT] shooter=%u npc=%u damage=%d predicted=1 (suppressed duplicate)",
                       event->shooterPlayerId, event->npcEntityId, event->damage);
        }
        else
        {
            // Non-predicted path (melee/godball/multi-pellet, or server-authoritative
            // mode where prediction feedback was suppressed): show the
            // server-confirmed feedback.
            hitmarker(event->damage);
            HitEvent ev;
            ev.position = hitPos;
            ev.normal = glm::length(hitNml) > 0.001f
                ? glm::normalize(hitNml) : glm::vec3(0.0f, 0.0f, 1.0f);
            ev.direction = glm::length(hitDir) > 0.001f
                ? glm::normalize(hitDir) : -ev.normal;
            ev.hitEntity = true;
            ev.damage = event->damage;
            ev.attacker = shooterName;
            ev.victim = npcName;
            ev.weaponSource = weaponId;
            HitEffects::onHit(ev);
            printf("[NET NPC HIT PRESENT] shooter=%u npc=%u damage=%d\n",
                   event->shooterPlayerId, event->npcEntityId, event->damage);
        }
    }

    // Server says 0 damage (miss) but client predicted a hit: show disagreement
    if (isLocalShooter && event->damage == 0 && (predicted || authMode))
    {
        if (NetworkingConfig::instance().data().disagreement.enabled)
        {
            DisagreementEvent de;
            de.timeMs = nowMs();
            de.reason = DISAGREEMENT_REWIND_MISS;
            de.position = hitPos;
            de.sourcePlayerId = ctx.localPlayerId;
            de.targetPlayerId = event->npcEntityId;
            de.description = "HIT REJECTED: server says miss";
            spawnDisagreementEffect(de);
            logDisagreement(de);
        }
    }

    if (npcPtr)
    {
        printf("[NET GODBALL NPC DAMAGE APPLY] npcId=%u healthBefore=%d healthAfter=%d isLocal=%d\n",
               event->npcEntityId, npcPtr->currentHp, event->npcHealth, (int)isLocalShooter);
        if (isLocalShooter)
            mpConfirmPredictedDamage(ctx, event->npcEntityId, event->npcHealth,
                                     event->killed != 0, true);
        else
            npcPtr->currentHp = event->npcHealth;
        printf("[NET GODBALL NPC DAMAGE AFTER] npcId=%u currentHp=%d\n",
               event->npcEntityId, npcPtr->currentHp);
        Debug::warn(Debug::Category::Weapons,
            "[GODBALL_DBG] CLIENT_SERVER_HP_RESULT npc=%u replicaHealthAfter=%d serverHealth=%d",
            event->npcEntityId, npcPtr->currentHp, event->npcHealth);
    }
    else
    {
        printf("[NET GODBALL NPC DAMAGE SKIP] npcId=%u npcPtr=NULL not-found-in-remoteNpcs\n",
               event->npcEntityId);
    }

    if (event->killed)
    {
        // Server confirmed a kill: any pending predicted kill-heal for this
        // entity sticks (no rollback).
        mpConfirmPredictedKillHeal(ctx, event->npcEntityId);
        const bool localShooterPredictedKill =
            isLocalShooter && npcPtr && npcPtr->netPredictedDead;
        // Killing an NPC heals the local killer to full health. Reward
        // presentation is independent of the Player pointer because the
        // authoritative network event is sufficient to identify the killer.
        if (isLocalShooter && gpPlayer)
        {
            DeathSystem::instance().healKillerToFull(*gpPlayer, shooterName);
            printf("[NET KILL HEAL] shooter=%u npc=%u health=%d\n",
                   event->shooterPlayerId, event->npcEntityId, gpPlayer->currentHp);
        }
        // Show both reward lines, including +0 Gold, for the local shooter.
        if (isLocalShooter)
            RewardPopupSystem::instance().pushReward(10, 0);
        if (!localShooterPredictedKill)
        {
            // Death sound (the red-sphere ellipsoid spawns loss-proof for every
            // client via the snapshot health transition in updateRenderedReplica).
            AudioManager::instance().play(
                {"npc_death", AudioCategory::NPC, true, hitPos, 1.0f, 0.9f, 45.0f, 0});
            // Killfeed entry: killer, NPC, weapon.
            const WeaponDefinition* wdef = WeaponRegistry::instance().get(weaponId);
            std::string weaponDisplay = (wdef && !wdef->displayName.empty())
                ? wdef->displayName
                : ((weaponId && weaponId[0]) ? weaponId : "unknown");
            KillfeedManager::instance().onKillStyled(
                shooterName, shooterVipAppearance, shooterVipStyleDetail,
                npcName, MimitaVip::freeAppearance(), MimitaVip::VipStyleDetail{},
                weaponDisplay, false, event->header.tick,
                ((uint64_t)event->eventSessionId << 32) | event->eventId);
            printf("[NET NPC KILL PRESENT] shooter=%u npc=%u name=\"%s\"\n",
                   event->shooterPlayerId, event->npcEntityId, npcName.c_str());
        }
        else
        {
            Debug::log(Debug::Category::Networking,
                       "[NET NPC KILL PRESENT] shooter=%u npc=%u name=\"%s\" predicted=1 (suppressed duplicate)",
                       event->shooterPlayerId, event->npcEntityId, npcName.c_str());
        }
        if (npcPtr)
        {
            npcPtr->netPredictedDead = false;
            // Keep the dead NPC replica, but spawn the fall-over death visual
            // as a SEPARATE ghost so the NPC body itself is never pinned or
            // frozen. This reliable event is the single death presenter (ghost
            // + red ellipsoid); the snapshot health transition is skipped for
            // this life to avoid a 2nd death.
            if (npcPtr->currentHp > 0)
                npcPtr->currentHp = 0;
            if (!npcPtr->networkDeathPresented)
            {
                npcPtr->networkDeathPresented = true;
                DeathGhostSystem::instance().spawnFromPlayer(
                    *npcPtr, glm::vec3(0.0f, 0.0f, 1.0f),
                    "net_npc_" + std::to_string(event->npcEntityId),
                    event->npcEntityId);
                Debug::log(Debug::Category::Networking,
                    "[NET NPC DEATH FX] entityId=%u pos=(%.1f,%.1f,%.1f) source=reliable-event\n",
                    event->npcEntityId, npcPtr->pos.x, npcPtr->pos.y, npcPtr->pos.z);
            }
        }
    }
}

uint32_t mpSendShotEvent(
    MultiplayerContext& ctx,
    uint32_t targetPlayerId,
    int damage,
    float power,
    uint16_t effectFlags,
    uint8_t weapon,
    uint8_t impactType,
    const glm::vec3& origin,
    const glm::vec3& hit,
    const glm::vec3& direction,
    const glm::vec3& normal,
    const glm::vec3& knockbackImpulse)
{
    if (!ctx.active || !ctx.localPlayerId)
        return 0;

    ShotRequestPacket packet{};
    packet.header.type = PACKET_SHOT_REQUEST;
    packet.header.tick = ctx.tick;
    packet.header.playerId = ctx.localPlayerId;
    packet.shotSerial = ctx.nextLocalShotSerial++;
    if (ctx.nextLocalShotSerial == 0)
        ctx.nextLocalShotSerial = 1;
    packet.clientTimeMs = nowMs();
    packet.lastServerTick = mpFireRenderTick(ctx, ctx.latestServerTick);
    packet.targetPlayerId = targetPlayerId;
    packet.damage = damage;
    packet.power = power;
    packet.effectFlags = effectFlags;
    packet.weapon = weapon;
    packet.impactType = impactType;
    packet.originX = origin.x; packet.originY = origin.y; packet.originZ = origin.z;
    packet.hitX = hit.x; packet.hitY = hit.y; packet.hitZ = hit.z;
    packet.dirX = direction.x; packet.dirY = direction.y; packet.dirZ = direction.z;
    packet.normalX = normal.x; packet.normalY = normal.y; packet.normalZ = normal.z;
    packet.knockX = knockbackImpulse.x; packet.knockY = knockbackImpulse.y; packet.knockZ = knockbackImpulse.z;
    mpSendPacket(ctx, &packet, sizeof(packet));
    printf("[NET SHOT SEND] shooter=%u serial=%u weapon=%u impact=%u "
           "flags=0x%03x target=%u damage=%d origin=(%.2f %.2f %.2f) "
           "hit=(%.2f %.2f %.2f)\n",
           ctx.localPlayerId, packet.shotSerial, weapon, impactType,
           effectFlags, targetPlayerId, damage,
           origin.x, origin.y, origin.z, hit.x, hit.y, hit.z);
    return packet.shotSerial;
}

void mpSendNpcDamageRequest(MultiplayerContext& ctx, uint32_t npcEntityId, int damage,
    const glm::vec3& origin, const glm::vec3& hit, const glm::vec3& direction,
    const glm::vec3& normal, const glm::vec3& knockback, uint16_t effectFlags, uint8_t weapon)
{
    if (!ctx.active || !ctx.localPlayerId)
        return;

    NpcDamageRequestPacket packet{};
    packet.header.type = PACKET_NPC_DAMAGE_REQUEST;
    packet.header.tick = ctx.tick;
    packet.header.playerId = ctx.localPlayerId;
    packet.npcEntityId = npcEntityId;
    packet.damage = std::clamp(damage, 1, 200);
    packet.originX = origin.x; packet.originY = origin.y; packet.originZ = origin.z;
    packet.hitX = hit.x; packet.hitY = hit.y; packet.hitZ = hit.z;
    packet.dirX = direction.x; packet.dirY = direction.y; packet.dirZ = direction.z;
    packet.normalX = normal.x; packet.normalY = normal.y; packet.normalZ = normal.z;
    packet.knockX = knockback.x; packet.knockY = knockback.y; packet.knockZ = knockback.z;
    packet.effectFlags = effectFlags;
    packet.weapon = weapon;
    packet.impactType = SHOT_IMPACT_ENTITY;
    mpSendPacket(ctx, &packet, sizeof(packet));
    printf("[NET NPC DAMAGE SEND] npcId=%u damage=%d origin=(%.2f,%.2f,%.2f)\n",
           npcEntityId, damage, origin.x, origin.y, origin.z);
}

void mpSendPelletBlastRequest(MultiplayerContext& ctx, uint8_t weapon,
    const glm::vec3& origin, const glm::vec3& baseDirection, uint32_t spreadSeed)
{
    if (!ctx.active || !ctx.localPlayerId)
        return;

    PelletBlastRequestPacket packet{};
    packet.header.type = PACKET_PELLET_BLAST_REQUEST;
    packet.header.tick = ctx.tick;
    packet.header.playerId = ctx.localPlayerId;
    packet.shotSerial = ctx.nextLocalShotSerial++;
    if (ctx.nextLocalShotSerial == 0)
        ctx.nextLocalShotSerial = 1;
    packet.clientTimeMs = nowMs();
    packet.lastServerTick = mpFireRenderTick(ctx, ctx.latestServerTick);
    packet.spreadSeed = spreadSeed;
    packet.weapon = weapon;
    packet.originX = origin.x; packet.originY = origin.y; packet.originZ = origin.z;
    const glm::vec3 dir = glm::normalize(baseDirection);
    packet.baseDirX = dir.x; packet.baseDirY = dir.y; packet.baseDirZ = dir.z;
    mpSendPacket(ctx, &packet, sizeof(packet));
    printf("[PELLET BLAST CLIENT SEND] shooter=%u serial=%u weapon=%s "
           "origin=(%.2f,%.2f,%.2f) baseDirection=(%.3f,%.3f,%.3f) "
           "seed=%u lastServerTick=%u\n",
           ctx.localPlayerId, packet.shotSerial,
           networkWeaponTypeName(weapon),
           origin.x, origin.y, origin.z,
           dir.x, dir.y, dir.z,
           spreadSeed, packet.lastServerTick);
}

void mpProcessPelletBlastEventPacket(MultiplayerContext& ctx, const PelletBlastEventPacket* event)
{
    // Reliable delivery dedup + ACK (retransmissions dropped here).
    if (!mpAcceptReliableEventOnce(ctx, event->eventId, event->eventSessionId))
    {
        Debug::log(Debug::Category::Networking,
                   "[PELLET BLAST CLIENT RECV] shooter=%u serial=%u duplicate-reliable=1 skipped=1\n",
                   event->shooterPlayerId, event->shotSerial);
        return;
    }

    // Deduplicate by shooter + serial
    uint64_t dedupKey = ((uint64_t)event->shooterPlayerId << 32) | event->shotSerial;
    if (ctx.processedPelletBlastSerials.count(dedupKey))
    {
        printf("[PELLET BLAST CLIENT RECV] shooter=%u serial=%u duplicate=1 skipped=1\n",
               event->shooterPlayerId, event->shotSerial);
        return;
    }
    ctx.processedPelletBlastSerials.insert(dedupKey);
    if (ctx.processedPelletBlastSerials.size() > 256)
        ctx.processedPelletBlastSerials.clear();

    const uint64_t receivedMs = nowMs();
    const uint32_t visualServerTick = event->lastServerTick != 0
        ? event->lastServerTick
        : event->header.tick;
    if (!visualTimelineReady(ctx, event->shooterPlayerId,
                             visualServerTick, receivedMs))
    {
        MultiplayerContext::PendingPelletBlastEvent pending;
        pending.packet = *event;
        pending.receivedMs = receivedMs;
        ctx.pendingPelletBlastEvents.push_back(pending);
        if (NetworkingConfig::instance().data().eventTimeline.logDelays)
        {
            printf("[NET EVENT TIMELINE HOLD] type=pellet shooter=%u serial=%u "
                   "visualTick=%u pending=%zu\n",
                   event->shooterPlayerId, event->shotSerial,
                   visualServerTick, ctx.pendingPelletBlastEvents.size());
        }
        return;
    }

    applyPelletBlastEventPacket(ctx, event);
}

void applyPelletBlastEventPacket(MultiplayerContext& ctx,
                                 const PelletBlastEventPacket* event)
{
    const bool isLocalShooter = event->shooterPlayerId == ctx.localPlayerId;
    const glm::vec3 origin(event->originX, event->originY, event->originZ);
    const glm::vec3 baseDir(event->baseDirX, event->baseDirY, event->baseDirZ);
    const char* weaponName = networkWeaponTypeName(event->weapon);

    // Re-base visuals onto the shooter's rendered replica body so the blast
    // visibly originates from where the body is drawn, not the ghost position.
    const glm::vec3 visualDelta =
        mpRemoteShooterRenderDelta(ctx, event->shooterPlayerId);
    const glm::vec3 visualOrigin = origin + visualDelta;
    const bool continueAfterHit = NetworkingConfig::instance().data()
        .combat.beamContinueAfterHit;

    printf("[PELLET BLAST CLIENT RECV] shooter=%u serial=%u weapon=%s "
           "pelletCount=%u origin=(%.2f,%.2f,%.2f) localShooter=%d\n",
           event->shooterPlayerId, event->shotSerial, weaponName,
           event->pelletCount, origin.x, origin.y, origin.z, (int)isLocalShooter);

    // For non-shooter: play sound and muzzle flash once
    if (!isLocalShooter)
    {
        playWorldSound("shotgunshoot", visualOrigin, 1.0f, 1.0f, 80.0f);
        EffectPartSystem::instance().spawnMuzzleFlash(visualOrigin, "", 1.0f, weaponName);
    }

    // Render each pellet tracer + impact
    for (uint8_t i = 0; i < event->pelletCount && i < MAX_NETWORK_PELLETS; ++i)
    {
        const NetworkPelletResult& pellet = event->pellets[i];
        const glm::vec3 hitPos(pellet.hitX, pellet.hitY, pellet.hitZ);
        const glm::vec3 hitNml(pellet.normalX, pellet.normalY, pellet.normalZ);

        // Only render tracers for non-shooter (shooter already predicted)
        if (!isLocalShooter)
        {
            // Start at the shooter's rendered body. With beam_continue_after_hit
            // each pellet's tracer continues to the weapon's max range past the
            // first hit; otherwise it ends at the confirmed hit point.
            glm::vec3 tracerEnd = hitPos;
            if (continueAfterHit && event->maxRange > 0.0f)
            {
                const glm::vec3 d = glm::length(hitPos - origin) > 0.001f
                    ? glm::normalize(hitPos - origin)
                    : glm::normalize(baseDir);
                tracerEnd = visualOrigin + d * event->maxRange;
            }
            EffectPartSystem::instance().spawnTracer(visualOrigin, tracerEnd, weaponName, 1.0f, weaponName);
        }

        if (pellet.impactType == PELLET_IMPACT_WORLD)
        {
            if (!isLocalShooter)
            {
                HitEvent ev;
                ev.position = hitPos;
                ev.normal = hitNml;
                ev.direction = -hitNml;
                ev.hitWorld = true;
                ev.damage = 0;
                ev.attacker = weaponName;
                ev.weaponSource = "pellet_blast";
                HitEffects::onHit(ev);
                EffectPartSystem::instance().spawnImpactSphereTickCfg(hitPos);
            }
        }
        else if (pellet.impactType == PELLET_IMPACT_PLAYER && !isLocalShooter)
        {
            HitEvent ev;
            ev.position = hitPos;
            ev.normal = hitNml;
            ev.direction = -hitNml;
            ev.hitEntity = true;
            ev.damage = 0;
            ev.attacker = weaponName;
            ev.weaponSource = "pellet_blast";
            HitEffects::onHit(ev);

            auto remote = ctx.remotePlayers.find(pellet.targetPlayerId);
            if (remote != ctx.remotePlayers.end())
            {
                printf("[PELLET HIT] player=%u damage=pending\n", pellet.targetPlayerId);
            }
        }
    }

    // ── Apply authoritative target results (knockback + death) ──────
    const char* shooterName = "";
    {
        auto si = ctx.playerRegistry.find(event->shooterPlayerId);
        if (si != ctx.playerRegistry.end())
            shooterName = si->second.name.c_str();
    }
    const glm::vec3 baseDirection(event->baseDirX, event->baseDirY, event->baseDirZ);

    for (uint8_t t = 0; t < event->targetCount && t < MAX_PELLET_BLAST_TARGETS; ++t)
    {
        const PelletBlastTargetResult& targetRes = event->targets[t];
        const bool isLocalTarget = targetRes.targetPlayerId == ctx.localPlayerId;

        // ── Knockback ────────────────────────────────────────────────
        glm::vec3 knockback(
            (float)targetRes.knockX,
            (float)targetRes.knockY,
            (float)targetRes.knockZ);

        if (glm::length(knockback) > 0.001f)
        {
            if (isLocalTarget)
            {
                ctx.pendingKnockbacks.push_back({
                    knockback, targetRes.targetSpawnGeneration, 0, weaponName});
                printf("[PELLET KNOCKBACK APPLY] victim=local "
                       "impulse=(%.2f,%.2f,%.2f) source=%s\n",
                       knockback.x, knockback.y, knockback.z, weaponName);
            }
            else
            {
                auto remote = ctx.remotePlayers.find(targetRes.targetPlayerId);
                if (remote != ctx.remotePlayers.end())
                {
                    remote->second.externalImpulse += knockback;
                    printf("[PELLET KNOCKBACK APPLY] player=%u "
                           "impulse=(%.2f,%.2f,%.2f)\n",
                           targetRes.targetPlayerId,
                           knockback.x, knockback.y, knockback.z);
                }
            }
        }

        // ── Death effect for killed targets (shooter and others) ─────
        if (targetRes.killed && !isLocalTarget)
        {
            NetworkShotEvent deathEvent;
            deathEvent.shotSerial = event->shotSerial;
            deathEvent.receivedMs = nowMs();
            deathEvent.eventServerTick = event->header.tick;
            deathEvent.visualServerTick = event->lastServerTick != 0
                ? event->lastServerTick
                : event->header.tick;
            deathEvent.shooterPlayerId = event->shooterPlayerId;
            deathEvent.targetPlayerId = targetRes.targetPlayerId;
            deathEvent.damage = targetRes.totalDamage;
            deathEvent.targetHealth = targetRes.healthAfter;
            deathEvent.weapon = event->weapon;
            deathEvent.impactType = SHOT_IMPACT_ENTITY;
            deathEvent.killed = true;
            deathEvent.damageConfirmed = true;
            deathEvent.direction = glm::length(baseDirection) > 0.001f
                ? glm::normalize(baseDirection) : glm::vec3(0.0f, 0.0f, -1.0f);
            deathEvent.knockback = knockback;
            ctx.shotEvents.push_back(deathEvent);

            printf("[PELLET DEATH] shooter=%u target=%u serial=%u\n",
                   event->shooterPlayerId, targetRes.targetPlayerId, event->shotSerial);
        }

    }
}

void mpReleaseTimelineEvents(MultiplayerContext& ctx)
{
    for (auto it = ctx.pendingShotEvents.begin(); it != ctx.pendingShotEvents.end(); )
    {
        if (!visualTimelineReady(ctx, it->shooterPlayerId,
                                 it->visualServerTick, it->receivedMs))
        {
            ++it;
            continue;
        }

        ctx.shotEvents.push_back(*it);
        if (NetworkingConfig::instance().data().eventTimeline.logDelays)
        {
            printf("[NET EVENT TIMELINE RELEASE] type=shot shooter=%u serial=%u "
                   "visualTick=%u\n",
                   it->shooterPlayerId, it->shotSerial, it->visualServerTick);
        }
        it = ctx.pendingShotEvents.erase(it);
    }

    for (auto it = ctx.pendingPelletBlastEvents.begin();
         it != ctx.pendingPelletBlastEvents.end(); )
    {
        const PelletBlastEventPacket& packet = it->packet;
        const uint32_t visualServerTick = packet.lastServerTick != 0
            ? packet.lastServerTick
            : packet.header.tick;
        if (!visualTimelineReady(ctx, packet.shooterPlayerId,
                                 visualServerTick, it->receivedMs))
        {
            ++it;
            continue;
        }

        applyPelletBlastEventPacket(ctx, &packet);
        if (NetworkingConfig::instance().data().eventTimeline.logDelays)
        {
            printf("[NET EVENT TIMELINE RELEASE] type=pellet shooter=%u serial=%u "
                   "visualTick=%u\n",
                   packet.shooterPlayerId, packet.shotSerial,
                   visualServerTick);
        }
        it = ctx.pendingPelletBlastEvents.erase(it);
    }
}

} // namespace MimitaNet
