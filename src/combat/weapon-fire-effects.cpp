#include "combat/weapon-fire.h"
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include "config/player-settings.h"
#include "debug/debug-log.h"
#include "effects/hit-effects.h"
#include "combat/death-system.h"
#include "devtools/terminal.h"
#include "entities/player.h"
#include "npc/npc.h"

namespace WeaponFire {

static float partBaseDamage(const std::string& part, float height) {
    if (part == "head") return 100.0f;
    if (part == "torso") return height >= 0.5f ? 50.0f : 30.0f;
    if (part.find("Arm") != std::string::npos) return height >= 0.5f ? 20.0f : 10.0f;
    if (part.find("Leg") != std::string::npos) return height >= 0.5f ? 25.0f : 15.0f;
    return 10.0f;
}

void applyRecoil(Player& shooter, const WeaponDefinition& def,
                 const glm::vec3& shotDirection, float& inOutRecoil, float dt) {
    const PlayerSettings& cfg = GetPlayerSettings();
    float recoilStrength = cfg.weaponRecoilStrength;
    glm::vec3 recoilDir(
        -shotDirection.x,
        -shotDirection.y,
        0.0f
    );
    if (glm::length(recoilDir) > 0.001f)
        recoilDir = glm::normalize(recoilDir);
    shooter.externalImpulse += recoilDir * recoilStrength;
    shooter.externalImpulse.z += recoilStrength * cfg.weaponRecoilUpKick * 0.01f;
    inOutRecoil = std::min(inOutRecoil + def.recoil * 0.25f, 8.0f);

    if (DebugConfig::DEBUG_RECOIL)
        Debug::log(Debug::Category::General,
            "[RECOIL] impulse=(%.3f %.3f %.3f) mag=%.1f\n",
            recoilDir.x * recoilStrength,
            recoilDir.y * recoilStrength,
            recoilStrength * cfg.weaponRecoilUpKick * 0.01f,
            recoilStrength);
}

int applyDamageToEntity(const DamageContext& ctx, Npc& victim,
                         const WeaponDefinition& def, Player& shooter,
                         NpcSystem& npcs, const glm::vec3& muzzlePos,
                         const glm::vec3& shotDirection) {
    float base = partBaseDamage(ctx.bodyPart, ctx.hitPosition.z);
    float distanceFalloffStart = 110.0f;
    auto it = def.customParams.find("distanceFalloffStart");
    if (it != def.customParams.end()) distanceFalloffStart = it->second;

    float minDamageFrac = 0.10f;
    it = def.customParams.find("minDamageFraction");
    if (it != def.customParams.end()) minDamageFrac = it->second;

    float minAngleFrac = 0.15f;
    it = def.customParams.find("minAngleFactor");
    if (it != def.customParams.end()) minAngleFrac = it->second;

    float distanceFactor = std::clamp(1.0f - ctx.distance / distanceFalloffStart, minDamageFrac, 1.0f);
    float angleFactor = std::clamp(std::fabs(glm::dot(-shotDirection, ctx.hitNormal)), minAngleFrac, 1.0f);
    float damage = std::min(base, std::max(base * distanceFactor * angleFactor, ctx.distance >= 100.0f ? 10.0f : 1.0f));
    int rounded = std::max(1, (int)std::round(damage));
    float knockback = damage * distanceFactor * (0.08f + angleFactor * 0.12f);

    victim.body.currentHp = std::max(0, victim.body.currentHp - rounded);
    victim.body.vel += shotDirection * knockback + glm::vec3(0, 0, knockback * 0.12f);
    victim.hitReactionTimer = 0.3f;

    {
        HitEvent ev;
        ev.position = ctx.hitPosition;
        ev.normal = ctx.hitNormal;
        ev.direction = -shotDirection;
        ev.hitEntity = true;
        ev.damage = rounded;
        ev.attacker = shooter.username;
        ev.victim = victim.body.username;
        ev.weaponSource = "weaponfire";
        HitEffects::onHit(ev);
    }

    if (GetPlayerSettings().debugCombat) {
        char debug[320];
        snprintf(debug, sizeof(debug),
                 "[DAMAGE] part=%s distance=%.2fm angleFactor=%.2f damage=%d knockback=%.2f",
                 ctx.bodyPart.c_str(), ctx.distance, angleFactor, rounded, knockback);
        Terminal::instance().addLog(debug);
    }

    if (victim.body.currentHp <= 0) {
        DeathSystem::instance().kill(
            victim.body,
            "npc_" + std::to_string(victim.id),
            "npc",
            shooter.username,
            shotDirection,
            18.0f);
        std::string line = shooter.username + " killed " + victim.body.username + " with " + def.displayName;
        Terminal::instance().addLog(line);
    }

    return rounded;
}

} // namespace WeaponFire
