// 10 02 2026
// Generic area-effect ticking. Server-authoritative; no rendering/prediction.
#include "combat/area-effect.h"

#include <algorithm>
#include <cmath>

bool areaEffectContains(const AreaEffect& effect, const glm::vec3& position)
{
    const glm::vec3 d = position - effect.position;
    const float planarSq = d.x * d.x + d.y * d.y;
    if (planarSq > effect.radius * effect.radius) return false;
    if (d.z < -1.0f) return false;
    if (effect.height > 0.0f && d.z > effect.height) return false;
    return true;
}

void tickAreaEffects(std::vector<AreaEffect>& effects,
                     float dt,
                     const std::vector<std::pair<uint32_t, glm::vec3>>& actorPositions,
                     const std::vector<int>& actorTeams,
                     std::vector<AreaEffectDamage>& outDamage)
{
    for (AreaEffect& effect : effects) {
        if (!effect.alive) continue;
        effect.ageSeconds += dt;
        if (effect.ageSeconds >= effect.durationSeconds) {
            effect.alive = false;
            continue;
        }
        if (effect.kind != AreaEffectKind::Fire || effect.damagePerTick <= 0)
            continue;

        // Fire damages on a fixed tick cadence.
        effect.ticksSinceDamage++;
        if (effect.ticksSinceDamage < std::max(1, effect.damageIntervalTicks))
            continue;
        effect.ticksSinceDamage = 0;

        for (size_t i = 0; i < actorPositions.size(); ++i) {
            const uint32_t actorId = actorPositions[i].first;
            if (actorId == 0 || actorId == effect.ownerActorId) continue;
            if (effect.damagesEnemiesOnly) {
                const int team = i < actorTeams.size() ? actorTeams[i] : -1;
                if (effect.ownerTeam >= 0 && team == effect.ownerTeam) continue;
            }
            if (!areaEffectContains(effect, actorPositions[i].second)) continue;
            outDamage.push_back({actorId, effect.damagePerTick, effect.id});
        }
    }

    effects.erase(std::remove_if(effects.begin(), effects.end(),
        [](const AreaEffect& e) { return !e.alive; }), effects.end());
}

bool areaEffectSelfTest(std::string& report)
{
    bool ok = true;
    auto fail = [&](const std::string& why) { ok = false; report += "FAIL: " + why + "\n"; };

    // Containment: inside the cylinder yes, outside radius no, above height no.
    {
        AreaEffect fire;
        fire.kind = AreaEffectKind::Fire;
        fire.position = glm::vec3(0, 0, 0);
        fire.radius = 4.0f;
        fire.height = 3.0f;
        if (!areaEffectContains(fire, glm::vec3(1, 1, 1))) fail("inside cylinder should contain");
        if (areaEffectContains(fire, glm::vec3(5, 0, 0))) fail("outside radius should not contain");
        if (areaEffectContains(fire, glm::vec3(0, 0, 5))) fail("above height should not contain");
    }

    // Fire damages every configured interval, and only enemies.
    {
        std::vector<AreaEffect> effects;
        AreaEffect fire;
        fire.id = 1;
        fire.kind = AreaEffectKind::Fire;
        fire.ownerActorId = 100;
        fire.ownerTeam = 0;
        fire.position = glm::vec3(0, 0, 0);
        fire.radius = 5.0f;
        fire.height = 3.0f;
        fire.durationSeconds = 10.0f;
        fire.damagePerTick = 10;
        fire.damageIntervalTicks = 10;
        effects.push_back(fire);

        // Actor 1 = enemy (team 1) inside; actor 2 = ally (team 0) inside;
        // actor 3 = enemy outside.
        std::vector<std::pair<uint32_t, glm::vec3>> positions = {
            {1, glm::vec3(1, 0, 0)},
            {2, glm::vec3(1, 0, 0)},
            {3, glm::vec3(50, 0, 0)},
        };
        std::vector<int> teams = {1, 0, 1};

        std::vector<AreaEffectDamage> dmg;
        // 9 ticks: no damage yet (interval 10).
        for (int i = 0; i < 9; ++i) tickAreaEffects(effects, 1.0f / 60.0f, positions, teams, dmg);
        if (!dmg.empty()) fail("fire should not damage before the interval");

        // 10th tick: enemy inside takes damage; ally and outside do not.
        tickAreaEffects(effects, 1.0f / 60.0f, positions, teams, dmg);
        if (dmg.size() != 1) fail("exactly one enemy should take fire damage");
        else {
            if (dmg[0].actorId != 1) fail("wrong actor took fire damage");
            if (dmg[0].damage != 10) fail("fire damage per tick should be 10");
        }
        report += "fire_cadence=ok\n";
    }

    // Expiry removes the effect.
    {
        std::vector<AreaEffect> effects;
        AreaEffect smoke;
        smoke.id = 2;
        smoke.kind = AreaEffectKind::Smoke;
        smoke.position = glm::vec3(0, 0, 0);
        smoke.radius = 4.0f;
        smoke.durationSeconds = 0.05f;
        effects.push_back(smoke);
        std::vector<AreaEffectDamage> dmg;
        tickAreaEffects(effects, 0.1f, {}, {}, dmg);
        if (!effects.empty()) fail("expired effect should be removed");
        report += "expiry=ok\n";
    }

    report += ok ? "PASS\n" : "FAIL\n";
    return ok;
}
