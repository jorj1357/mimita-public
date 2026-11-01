// 10 02 2026
/* purpose
* Generic server-authoritative area effects for grenade tools: a fire cylinder
* that damages every N fixed ticks, a smoke volume with a lifetime, and a
* dark-bang screen-effect duration. Reusable by any gamemode; presentation is
* owned by the actor preset / effect system.
* Does NOT own projectiles, throws, rendering, or client prediction.
* Does NOT create a second damage pipeline: it returns damage events the server
* applies through the shared damage path.
*/
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

enum class AreaEffectKind : uint8_t
{
    None = 0,
    Fire,        // cylinder, periodic damage
    Smoke,       // volume, blocks vision, lifetime
    DarkBang,    // blinding screen effect, duration
    Danger       // no damage; short AI hazard marker (for example frag blast)
};

// One live area effect. Server-owned; replicated presentation is derived from
// this (position/radius/kind/remaining) so clients never decide outcomes.
struct AreaEffect
{
    uint32_t id = 0;
    AreaEffectKind kind = AreaEffectKind::None;
    uint32_t ownerActorId = 0;
    int ownerTeam = -1;
    glm::vec3 position{0.0f};
    float radius = 4.0f;
    float height = 3.0f;             // fire/smoke cylinder height
    int durationTicks = 0;           // total lifetime, authoritative
    int ageTicks = 0;
    int damagePerTick = 0;           // fire only
    int damageIntervalTicks = 10;    // fire only
    int ticksSinceDamage = 0;
    bool damagesEnemiesOnly = true;  // fire team policy
    bool damageOwner = false;        // fire may burn the thrower
    bool alive = true;

    int remainingTicks() const {
        return durationTicks > ageTicks ? durationTicks - ageTicks : 0;
    }
    // Derived display value only; never store or compare seconds internally.
    float remainingSeconds() const {
        return static_cast<float>(remainingTicks()) / 60.0f;
    }
    float durationSeconds() const {
        return static_cast<float>(durationTicks) / 60.0f;
    }
};

// Advance every effect by one fixed tick. Appends damage events (actorId,
// damage) for effects that fired this tick into outDamage. Removes expired
// effects. `teamOf` maps an actor id to its team (-1 = none). Pure over the
// list + a team lookup; no world or actor mutation.
struct AreaEffectDamage { uint32_t actorId = 0; int damage = 0; uint32_t effectId = 0; };

void tickAreaEffects(std::vector<AreaEffect>& effects,
                     const std::vector<std::pair<uint32_t, glm::vec3>>& actorPositions,
                     const std::vector<int>& actorTeams,
                     std::vector<AreaEffectDamage>& outDamage);

// Is a position inside an effect's cylinder? Pure.
bool areaEffectContains(const AreaEffect& effect, const glm::vec3& position);

// Does any effect of `kind` intersect the segment from -> to? Used for smoke
// line-of-sight blocking. Pure over the effect list; no world mutation.
bool areaEffectSegmentBlocked(const std::vector<AreaEffect>& effects,
                              AreaEffectKind kind,
                              const glm::vec3& from,
                              const glm::vec3& to);

// World-independent selftest for area-effect ticking, fire cadence, and expiry.
bool areaEffectSelfTest(std::string& report);
