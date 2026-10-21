#include "combat/weapon-contact-local.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-melee-config.h"
#include "combat/weapon-audio.h"
#include "combat/death-system.h"
#include "network/server.h"
#include "entities/player.h"
#include "npc/npc.h"
#include <algorithm>

void tickLocalPhysicalContactWeapons(Player& player, NpcSystem& npcSystem, uint32_t tick)
{
    using namespace MimitaNet;
    constexpr uint32_t localId = 0xfffffffeu;
    static std::unordered_map<uint32_t, ServerPlayer> players;
    static std::unordered_map<uint32_t, ServerNpc> npcs;
    static std::vector<ServerPhysicalContactResult> results;
    static std::vector<std::pair<uint64_t, Player*>> bodies;
    uint64_t packets = 0;
    results.clear(); bodies.clear();
    auto configured = [](const Player& body) {
        const auto* def = WeaponRegistry::instance().get(body.equippedWeaponId);
        return !body.dead && def && !def->animationProfile.empty() &&
            def->executionType == WeaponExecutionType::PhysicalContact;
    };
    bool needed = configured(player);
    for (const auto& npc : npcSystem.all()) needed = needed || configured(npc.body);
    if (!needed) {
        players.clear(); npcs.clear();
        tickServerAnimatedPhysicalContactWeapons(INVALID_SOCKET, players, npcs, tick, packets, &results);
        return;
    }

    auto& human = players[localId];
    human.id = localId; human.name = player.username; human.avatarName = player.avatarName();
    human.pos = player.pos; human.vel = player.vel; human.yaw = player.yaw;
    human.health = player.currentHp; human.maxHealth = player.maxHp; human.dead = player.dead;
    human.spawnState = ServerPlayer::Active; human.matchTeam = player.matchTeam;
    human.sizeScale = player.sizeScale; human.spawnGeneration = player.spawnGeneration;
    human.equippedWeaponId = player.equippedWeaponId; human.equippedSlot = player.equippedSlot;
    human.input.camForward = player.aimDirection; human.onGround = player.ground.onGround;
    human.meleeActionActive = false;
    if (const auto* set = WeaponData::MeleeMoveSetRegistry::instance().get(player.equippedWeaponId))
        for (const auto& action : set->actions)
            if (!player.lockedClip.empty() && action.animationClipId == player.lockedClip) {
                human.meleeActionActive = true; human.meleeActionId = action.id;
                human.meleeActionStartTick = tick - (uint32_t)std::max(0, player.lockedClipSampleTick);
                break;
            }
    bodies.emplace_back(((uint64_t)ServerActorKind::Player << 32) | localId, &player);
    for (Npc& npc : npcSystem.all()) {
        auto& record = npcs[npc.id];
        if (record.transformEpoch != npc.transformEpoch || record.equippedWeaponId != npc.body.equippedWeaponId) {
            record.meleeActionActive = false; record.meleeActionId.clear(); record.meleeNextAttackTick = 0;
        }
        record.entityId = npc.id; record.name = npc.body.username; record.avatarName = npc.avatarName;
        record.transformEpoch = npc.transformEpoch; record.pos = npc.body.pos; record.vel = npc.body.vel;
        record.yaw = npc.body.yaw; record.aim = npc.currentFacing; record.health = npc.body.currentHp;
        record.matchTeam = npc.body.matchTeam; record.onGround = npc.body.ground.onGround;
        record.sizeScale = npc.body.sizeScale; record.equippedWeaponId = npc.body.equippedWeaponId;
        record.equippedSlot = npc.body.equippedSlot; record.knockbackImpulse = glm::vec3(0);
        bodies.emplace_back(((uint64_t)ServerActorKind::Npc << 32) | npc.id, &npc.body);
    }
    std::sort(bodies.begin(), bodies.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    auto findBody = [&](ServerActorRef ref) -> Player* {
        const uint64_t key = ((uint64_t)ref.kind << 32) | ref.id;
        auto it = std::lower_bound(bodies.begin(), bodies.end(), key,
            [](const auto& body, uint64_t value) { return body.first < value; });
        return it != bodies.end() && it->first == key ? it->second : nullptr;
    };
    for (auto it = npcs.begin(); it != npcs.end(); ) {
        if (!findBody({it->first, ServerActorKind::Npc, nullptr, nullptr})) it = npcs.erase(it); else ++it;
    }
    tickServerWeaponAnimationActions(players, npcs, tick);
    for (Npc& npc : npcSystem.all()) {
        const auto& record = npcs.at(npc.id);
        const auto* set = WeaponData::MeleeMoveSetRegistry::instance().get(record.equippedWeaponId);
        if (!set || !set->animationOnly) continue;
        const auto* action = record.meleeActionActive ? set->find(record.meleeActionId) : nullptr;
        if (action) npc.body.playOneShotAtTick(action->animationClipId,
            std::min((int)(tick - record.meleeActionStartTick), action->totalTicks() - 1));
        else npc.body.clearOneShot();
        npc.body.updateProceduralAnimation(SERVER_DT, npc.currentFacing, npc.body.pos,
            glm::length(npc.body.vel) > 0.1f);
    }
    tickServerAnimatedPhysicalContactWeapons(INVALID_SOCKET, players, npcs, tick, packets, &results);
    for (const auto& contact : results) {
        Player* victim = findBody(contact.request.victim);
        Player* attacker = findBody(contact.request.attacker);
        if (!victim || !attacker || victim->dead) continue;
        victim->takeDamage(contact.request.damage, contact.request.knockback, 0.0f, contact.hit);
        victim->externalImpulse += contact.request.knockback;
        if (contact.presentImpact)
            if (const auto* def = WeaponRegistry::instance().get(contact.request.weaponId))
                WeaponAudio::playContactSound(*def, contact.hit, contact.force);
        if (contact.result.killed) DeathSystem::instance().kill(*victim,
            contact.request.victim.kind == ServerActorKind::Npc
                ? "npc_" + std::to_string(contact.request.victim.id) : victim->username,
            contact.request.victim.kind == ServerActorKind::Npc ? "npc" : "player",
            attacker->username, contact.request.knockback, glm::length(contact.request.knockback));
    }
}
