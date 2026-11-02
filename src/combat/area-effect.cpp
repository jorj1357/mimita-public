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
                     const std::vector<std::pair<uint32_t, glm::vec3>>& actorPositions,
                     const std::vector<int>& actorTeams,
                     std::vector<AreaEffectDamage>& outDamage)
{
    for (AreaEffect& effect : effects) {
        if (!effect.alive) continue;
        effect.ageTicks += 1;
        if (effect.ageTicks >= effect.durationTicks) {
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
            if (actorId == 0) continue;
            if (actorId == effect.ownerActorId && !effect.damageOwner) continue;
            if (effect.damagesEnemiesOnly) {
                const int team = i < actorTeams.size() ? actorTeams[i] : -1;
                if (effect.ownerTeam >= 0 && team == effect.ownerTeam) continue;
            }
            // "Anyone touching" is a capsule-vs-cylinder test, not a single
            // center-point test. A small actor radius/height default is used
            // because the server's actor positions are root/feet positions.
            const glm::vec3& pos = actorPositions[i].second;
            constexpr float kActorRadius = 0.4f;
            constexpr float kActorHeight = 1.8f;
            const float planarDist = glm::length(glm::vec2(
                pos.x - effect.position.x, pos.y - effect.position.y));
            if (planarDist > effect.radius + kActorRadius) continue;
            const float zMin = effect.position.z - 1.0f;
            const float zMax = effect.position.z +
                (effect.height > 0.0f ? effect.height : 1.0e9f);
            const float actorMin = pos.z - kActorRadius;
            const float actorMax = pos.z + kActorHeight;
            if (actorMax < zMin || actorMin > zMax) continue;
            outDamage.push_back({actorId, effect.damagePerTick, effect.id});
        }
    }

    effects.erase(std::remove_if(effects.begin(), effects.end(),
        [](const AreaEffect& e) { return !e.alive; }), effects.end());
}

bool areaEffectSegmentBlocked(const std::vector<AreaEffect>& effects,
                              AreaEffectKind kind,
                              const glm::vec3& from,
                              const glm::vec3& to)
{
    const glm::vec3 delta = to - from;
    const float a = delta.x * delta.x + delta.y * delta.y;
    const float zMinPad = 1.0f;
    for (const AreaEffect& effect : effects) {
        if (!effect.alive || effect.kind != kind) continue;
        const float zMin = effect.position.z - zMinPad;
        const float zMax = effect.position.z +
            (effect.height > 0.0f ? effect.height : 1.0e9f);

        float tEnter = 0.0f;
        float tExit = 1.0f;

        if (a < 1.0e-8f) {
            const float dx = from.x - effect.position.x;
            const float dy = from.y - effect.position.y;
            if (dx * dx + dy * dy > effect.radius * effect.radius) continue;
        } else {
            const float fx = from.x - effect.position.x;
            const float fy = from.y - effect.position.y;
            const float b = 2.0f * (fx * delta.x + fy * delta.y);
            const float c = fx * fx + fy * fy - effect.radius * effect.radius;
            const float disc = b * b - 4.0f * a * c;
            if (disc < 0.0f) continue;
            const float root = std::sqrt(disc);
            const float inv = 1.0f / (2.0f * a);
            tEnter = std::max(tEnter, (-b - root) * inv);
            tExit = std::min(tExit, (-b + root) * inv);
        }

        if (std::fabs(delta.z) < 1.0e-8f) {
            if (from.z < zMin || from.z > zMax) continue;
        } else {
            const float t1 = (zMin - from.z) / delta.z;
            const float t2 = (zMax - from.z) / delta.z;
            tEnter = std::max(tEnter, std::min(t1, t2));
            tExit = std::min(tExit, std::max(t1, t2));
        }

        if (tEnter <= tExit && tExit >= 0.0f && tEnter <= 1.0f)
            return true;
    }
    return false;
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
        fire.durationTicks = 600;
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
        for (int i = 0; i < 9; ++i) tickAreaEffects(effects, positions, teams, dmg);
        if (!dmg.empty()) fail("fire should not damage before the interval");

        // 10th tick: enemy inside takes damage; ally and outside do not.
        tickAreaEffects(effects, positions, teams, dmg);
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
        smoke.durationTicks = 3;
        effects.push_back(smoke);
        std::vector<AreaEffectDamage> dmg;
        for (int i = 0; i < 3; ++i) tickAreaEffects(effects, {}, {}, dmg);
        if (!effects.empty()) fail("expired effect should be removed");
        report += "expiry=ok\n";
    }

    // Smoke segment blocking: a segment through the volume is blocked; one
    // around it is not; the wrong kind is ignored.
    {
        AreaEffect smoke;
        smoke.kind = AreaEffectKind::Smoke;
        smoke.position = glm::vec3(0, 0, 0);
        smoke.radius = 5.0f;
        smoke.height = 8.0f;
        smoke.durationTicks = 600;
        std::vector<AreaEffect> effects{smoke};
        if (!areaEffectSegmentBlocked(effects, AreaEffectKind::Smoke,
                                      glm::vec3(-10, 0, 1), glm::vec3(10, 0, 1)))
            fail("smoke should block a segment through it");
        if (areaEffectSegmentBlocked(effects, AreaEffectKind::Smoke,
                                     glm::vec3(-10, 20, 1), glm::vec3(10, 20, 1)))
            fail("smoke should not block a segment outside its radius");
        if (areaEffectSegmentBlocked(effects, AreaEffectKind::Fire,
                                     glm::vec3(-10, 0, 1), glm::vec3(10, 0, 1)))
            fail("non-smoke kind should not block smoke segments");
        report += "smoke_segment=ok\n";
    }

    report += ok ? "PASS\n" : "FAIL\n";
    return ok;
}
