// 2026-09-28
/* purpose
* Define the shared, deterministic impact contract consumed by the single
* ImpactSystem. An ImpactEvent records what physically happened; the target
* decides what it means (damage, geometry cut, knockback, effect).
* Does NOT own projectile flight, damage authority, materials, or geometry.
* Does NOT contain crate-specific or weapon-specific fields.
*/

#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace MimitaImpact {

// Where an impact came from. The source only reports; it never applies damage.
enum class ImpactSource : uint8_t
{
    Projectile = 0,
    Hitscan,
    Melee,
    Explosion,
    Actor,
    PhysicalEntity
};

// What was hit. The target owns the response.
enum class ImpactTarget : uint8_t
{
    Actor = 0,
    PhysicalEntity,
    WorldGeometry
};

// Elementary impact shape. Only Sphere is used by the first slice.
enum class ImpactShape : uint32_t
{
    Sphere = 0
};

// One deterministic, fully self-describing impact. The same event must produce
// the same result on server, client prediction, replay, and tests.
struct ImpactEvent
{
    uint64_t eventId = 0;
    uint64_t simulationTick = 0;

    ImpactSource source = ImpactSource::Projectile;
    ImpactTarget target = ImpactTarget::PhysicalEntity;

    // sourceEntityId identifies the source object (projectile id, weapon slot,
    // actor id); targetEntityId identifies the hit PhysicalEntity.
    uint32_t sourceEntityId = 0;
    uint32_t targetEntityId = 0;

    glm::vec3 worldPoint{0.0f};
    glm::vec3 worldNormal{0.0f, 0.0f, 1.0f};
    glm::vec3 worldDirection{0.0f};

    float speed = 0.0f;          // m/s along worldDirection
    float mass = 0.0f;           // kg
    float density = 0.0f;        // kg/m^3
    float radius = 0.0f;         // source cross-section / shape radius
    float energy = 0.0f;         // computed by ImpactSystem (J)
    float damage = 0.0f;         // source-declared nominal damage
    float penetration = 0.0f;    // reserved: penetration depth

    uint32_t materialId = 0;     // target material (resolved by the target)
    uint32_t shapeId = 0;        // ImpactShape

    // Source-declared gameplay scale on destructive energy (any source may set
    // it; 1.0 = no scale). Not a crate-specific field.
    float cutScale = 1.0f;

    // Reserved for deterministic seeded irregularity (destructible-world §9).
    uint32_t seed = 0;
};

// Universal material record (destructible-world §12/§13). Objects reference a
// material instead of duplicating behavior. Values are gameplay-stable, not
// physically exact.
struct MaterialDefinition
{
    std::string id;
    float density = 700.0f;          // kg/m^3
    float strength = 100.0f;         // nominal structural strength
    float cutResistance = 1.0f;      // scales how easily a cut forms
    float fractureThreshold = 250.0f;// damage at which the object breaks
    float surfaceHardness = 1.0f;    // scales glancing resistance
    float holeEnergyScale = 1.0f;    // scales effective cut energy
    float maxCutRadius = 1.5f;       // clamp on a single cut radius
};

} // namespace MimitaImpact
