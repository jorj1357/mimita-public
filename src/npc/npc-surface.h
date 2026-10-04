// 10 04 2026
/* purpose
* Pure classification of world collision triangles into navigation surfaces
* (floor / ramp / step / too-steep / ceiling) and the capability tags a
* navigation connection can carry for future movement types.
* No world query and no NPC dependency: callers pass a triangle normal and the
* shared walkable-slope limit, so the same rule is unit-testable off-device.
* Does NOT build grids, path, steer, or apply physics.
*/
#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

// How a single collision triangle reads as a standable surface.
enum class NavSurfaceKind : uint8_t
{
    Floor = 0,   // near-horizontal, walkable
    Ramp,        // walkable but sloped
    Step,        // walkable, assigned by the grid when surfaces differ in height
    TooSteep,    // upward-facing but steeper than the walkable limit
    Ceiling,     // downward-facing
};

// How a navigation connection is traversed. Only Walk/Jump/Drop are produced
// today; the rest are reserved so future movement types can reuse connections.
enum class NavCapability : uint8_t
{
    Walk = 0,
    Jump,
    Drop,
    Crawl,
    Fly,
    Roll,
    Teleport,
};

// One standable surface detected inside a grid column. Multiple surfaces may
// share the same X/Y for stacked floors, bridges, and upper platforms.
struct NavigationSurface
{
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    float height = 0.0f;
    bool walkable = false;
    NavSurfaceKind kind = NavSurfaceKind::Floor;
};

const char* navSurfaceKindName(NavSurfaceKind kind);
const char* navCapabilityName(NavCapability capability);

// Classify one triangle normal against the shared walkable-slope dot product.
// `walkableDot` is cos(maxWalkableSlopeDegrees); the face is walkable when its
// upward normal is at least that value.
NavSurfaceKind classifySurface(const glm::vec3& normal, float walkableDot);

// True when a normal is walkable ground: upward facing and within the limit.
bool isWalkableNormal(const glm::vec3& normal, float walkableDot);

// World-independent selftest for classification and capability naming.
bool npcSurfaceSelfTest(std::string& report);
