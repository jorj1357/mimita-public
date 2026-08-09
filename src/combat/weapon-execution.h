// 07 21 2026, 21 00
/* purpose
* Declares shared weapon execution helpers for hitscan and physical-contact weapons.
* Keeps deterministic traces, contact shapes, and episode confirmation data transport-neutral.
* Provides small structs usable from server code and focused automated tests.
* Does NOT send packets, mutate ammo, apply health damage, render effects, or play audio.
* Does NOT own weapon JSON loading, projectile simulation, or client input collection.
* Does NOT specialize behavior for UDP, ICE, listen-server, or dedicated-server launch modes.
*/

#pragma once

#include "combat/pellet-pattern.h"
#include "combat/weapon-types.h"

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace WeaponExecution {

constexpr float DEFAULT_HITSCAN_RANGE = 250.0f;
constexpr float DEFAULT_BODY_RADIUS = 0.65f;
constexpr float DEFAULT_BODY_HEIGHT = 3.5f;

enum class PhysicalShapeKind : uint8_t
{
    Sphere,
    Capsule
};

enum class HitBodyPart : uint8_t
{
    Torso,
    Head,
    Leg
};

struct PlayerTarget
{
    uint32_t playerId = 0;
    uint32_t spawnGeneration = 0;
    glm::vec3 position{0.0f};
    float radius = DEFAULT_BODY_RADIUS;
    float height = DEFAULT_BODY_HEIGHT;
    bool dead = false;
    // Optional per-part hitboxes (head/torso/arms/legs) reconstructed at the
    // rewound pose. When populated the trace validates each part AABB exactly
    // like the client's beam does — what the shooter rendered is what takes
    // damage, no invisible capsule. Empty = capsule fallback.
    struct BodyPartBox {
        glm::vec3 center{0.0f};
        glm::vec3 half{0.0f};
        HitBodyPart bodyPart = HitBodyPart::Torso;
    };
    std::vector<BodyPartBox> bodyParts;
};

struct HitscanPelletHit
{
    bool hit = false;
    uint32_t targetPlayerId = 0;
    uint32_t targetSpawnGeneration = 0;
    glm::vec3 hitPosition{0.0f};
    glm::vec3 hitNormal{0.0f};
    glm::vec3 direction{1.0f, 0.0f, 0.0f};
    float distance = 0.0f;
    float damage = 0.0f;
    bool headshot = false;
    HitBodyPart bodyPart = HitBodyPart::Torso;
};

struct HitscanDamageAggregate
{
    uint32_t targetPlayerId = 0;
    uint32_t targetSpawnGeneration = 0;
    int damage = 0;
    int pelletHits = 0;
    glm::vec3 hitPosition{0.0f};
    glm::vec3 hitNormal{0.0f, 0.0f, 1.0f};
    glm::vec3 knockback{0.0f};
    bool headshot = false;
};

struct HitscanTraceConfig
{
    float maxRange = DEFAULT_HITSCAN_RANGE;
    float damage = 0.0f;
    float headshotMultiplier = 2.0f;
    int pelletCount = 1;
    float spreadDegrees = 0.0f;
    uint32_t deterministicSeed = 0;
    float worldBlockDistance = DEFAULT_HITSCAN_RANGE;
    float knockbackPerDamage = 0.08f;
    // Range falloff: factor = clamp(1 - distance/start, minFraction, 1).
    // 0 means no falloff (backward compatible with weapons that don't set it).
    float distanceFalloffStart = 0.0f;
    float minDamageFraction = 0.05f;
    // Body-part multiplier for limb hits (torso = 1x, head = headshotMultiplier).
    float limbDamageMultiplier = 0.75f;
    // Swept-sphere radius for the hit beam (0 = infinitely thin ray). The same
    // value is used by the client so online hits match what the shooter sees.
    float beamThickness = 0.0f;
    // World/occlusion radius (0 = thin world rays). Separate from beamThickness
    // so a thick entity-hit beam doesn't clip wall edges and break the pellet
    // pattern at the aim direction.
    float beamWorldThickness = 0.0f;
};

struct HitscanTraceResult
{
    int pelletCount = 0;
    HitscanPelletHit pellets[MAX_PELLETS_PER_BLAST]{};
    std::vector<HitscanDamageAggregate> aggregates;
};

struct PhysicalContactShape
{
    PhysicalShapeKind kind = PhysicalShapeKind::Sphere;
    glm::vec3 previousA{0.0f};
    glm::vec3 previousB{0.0f};
    glm::vec3 currentA{0.0f};
    glm::vec3 currentB{0.0f};
    float radius = 0.5f;
};

struct PhysicalContactHit
{
    bool hit = false;
    uint32_t targetPlayerId = 0;
    uint32_t targetSpawnGeneration = 0;
    glm::vec3 hitPosition{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    float distance = 0.0f;
};

struct PhysicalContactEpisode
{
    bool active = false;
    uint32_t targetPlayerId = 0;
    uint32_t targetSpawnGeneration = 0;
    uint32_t contactSerial = 0;
    uint32_t firstTick = 0;
    uint32_t lastSampleTick = 0;
    int accumulatedDamage = 0;
    int pendingConfirmationDamage = 0;
    int pendingHealthBefore = 0;
    int pendingHealthAfter = 0;
    bool pendingKilled = false;
    uint8_t samplesSinceConfirmation = 0;
    glm::vec3 accumulatedKnockback{0.0f};
    glm::vec3 lastHitPosition{0.0f};
    glm::vec3 lastNormal{0.0f, 0.0f, 1.0f};
};

float paramOr(const WeaponDefinition& def, const char* key, float fallback);
WeaponExecutionType executionTypeForBehavior(WeaponBehaviorType behavior);
int buildPelletDirections(const WeaponDefinition& def, const glm::vec3& aimDirection,
                          uint32_t seed, glm::vec3* outDirections, int capacity);
bool rayPlayerTarget(const glm::vec3& origin, const glm::vec3& direction,
                     const PlayerTarget& target, float maxDistance,
                     float beamRadius,
                     HitscanPelletHit& outHit);
HitscanTraceResult traceHitscan(const WeaponDefinition& def,
                                const glm::vec3& origin,
                                const glm::vec3& direction,
                                const HitscanTraceConfig& config,
                                const std::vector<PlayerTarget>& targets);

glm::vec3 physicalShapeCenter(const PhysicalContactShape& shape);
float physicalShapeTravelDistance(const PhysicalContactShape& shape);
bool testPhysicalContact(const PhysicalContactShape& shape,
                         const PlayerTarget& target,
                         PhysicalContactHit& outHit);
bool episodeShouldConfirm(const PhysicalContactEpisode& episode,
                          bool ending,
                          uint8_t batchSize);

} // namespace WeaponExecution
