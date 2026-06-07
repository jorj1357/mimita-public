// C:\important\quiet\n\mimita-priv-v7\src\combat\weapon-hit.cpp
// mar 8 2026
/**
 * purpose
 * expose ONE function:
 * weaponHit(args)
 *
 * this file DOES:
 * - hold the one-weapon combat entry point skeleton
 *
 * this file DOES NOT:
 * - own combo timer
 * - own wall impact damage
 */

#include "weapon-hit.h"
#include "combat/death-system.h"
#include "entities/player.h"
#include "debug/debug-log.h"
#include "audio/audio.h"
#include "effects/effect-part.h"
#include <cstdio>
#include <glm/glm.hpp>

// NPC melee attack on player
void weaponHit(Player& attacker, Player& target)
{
    const float MELEE_RANGE = 2.5f;
    const float MELEE_DAMAGE = 25.0f;
    const float MELEE_KNOCKBACK = 15.0f;
    
    float distance = glm::length(target.pos - attacker.pos);
    if (distance > MELEE_RANGE) {
        if (DebugConfig::DEBUG_COMMANDS) {
            Debug::log(Debug::Category::General, "[MELEE MISS] distance=%.2f range=%.2f\n", distance, MELEE_RANGE);
        }
        return;
    }
    
    // Check if target is in front of attacker (within 90 degree cone)
    glm::vec3 toTarget = glm::normalize(target.pos - attacker.pos);
    glm::vec3 attackerForward = attacker.movementCapsule.rotation * glm::vec3(0, 1, 0);
    float dot = glm::dot(toTarget, attackerForward);
    
    if (dot < 0.0f) { // Behind attacker
        if (DebugConfig::DEBUG_COMMANDS) {
            Debug::log(Debug::Category::General, "[MELEE MISS] behind attacker dot=%.2f\n", dot);
        }
        return;
    }
    
    // Hit!
    glm::vec3 knockbackDir = toTarget;
    knockbackDir.z = 0.3f; // Slight upward
    
    target.takeDamage((int)MELEE_DAMAGE, knockbackDir, MELEE_KNOCKBACK);
    EffectPartSystem::instance().spawnEntityImpact(
        target.pos, -toTarget, attacker.username, target.username);
    EffectPartSystem::instance().spawnBloodSpurt(
        target.pos, toTarget, attacker.username, target.username);
    if (target.currentHp <= 0) {
        DeathSystem::instance().kill(
            target, target.username, "player", attacker.username, toTarget, MELEE_KNOCKBACK);
    }
    
    if (DebugConfig::DEBUG_COMMANDS) {
        Debug::log(Debug::Category::General, "[MELEE HIT] damage=%.0f knockback=%.1f\n", MELEE_DAMAGE, MELEE_KNOCKBACK);
    }
}
