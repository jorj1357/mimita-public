// 2026-09-27
/* purpose
* Registers the crate_spawn / crate_clear terminal commands so a generic moving
* physical entity can be created in-game for collision and carry testing.
* The crate is placed `distance` metres along the local player's camera look
* direction (default 5 m) and floats as a kinematic entity.
* Does NOT implement entity physics, rendering, or the collision solver.
* Does NOT modify NPC, weapon, damage, or network behavior.
*/

#include "terminal/crate-commands.h"

#include <cstdio>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "devtools/terminal.h"
#include "terminal/terminal-state.h"
#include "physics/physical-entity.h"
#include "config/collision-config.h"
#include "config/material-config.h"
#include "impact/impact-system.h"

namespace {

bool parseFloat(const std::string& text, float& out)
{
    try {
        size_t used = 0;
        out = std::stof(text, &used);
        return used == text.size();
    } catch (...) {
        return false;
    }
}

float crateVolume(const PhysicalEntity& e)
{
    const glm::vec3 dimensions = e.halfExtents * 2.0f;
    return dimensions.x * dimensions.y * dimensions.z;
}

float clampCrateDensity(float density)
{
    if (!std::isfinite(density))
        return 1.0f;
    return std::clamp(density, 0.001f, 1000000000.0f);
}

void logCrateInfo(const PhysicalEntity& e)
{
    const glm::vec3 pos = glm::vec3(e.transform[3]);
    const glm::vec3 eulerDegrees = glm::degrees(glm::eulerAngles(e.orientation));
    const glm::vec3 dimensions = e.halfExtents * 2.0f;
    char buf[512];
    std::snprintf(buf, sizeof(buf),
        "[CRATE] id=%u persistence=%s pos=(%.2f %.2f %.2f) vel=(%.2f %.2f %.2f) "
        "rot=(%.1f %.1f %.1f deg) angVel=(%.2f %.2f %.2f) dim=(%.2f %.2f %.2f) "
        "volume=%.2f m3 density=%.3f kg/m3 mass=%.2f kg sleeping=%d sleepTicks=%u",
        e.id, e.persistenceId.c_str(), pos.x, pos.y, pos.z,
        e.velocity.x, e.velocity.y, e.velocity.z,
        eulerDegrees.x, eulerDegrees.y, eulerDegrees.z,
        e.angularVelocity.x, e.angularVelocity.y, e.angularVelocity.z,
        dimensions.x, dimensions.y, dimensions.z, crateVolume(e), e.density,
        e.mass, (int)e.sleeping, (unsigned)e.sleepTicks);
    Terminal::instance().addLog(buf);
}

template <typename Fn>
void forEachActiveCrate(Fn&& fn)
{
    for (PhysicalEntity& e : PhysicalEntitySystem::instance().entities())
    {
        if (e.shape == PhysicalEntityShape::Box && e.persistenceId.rfind("crate-", 0) == 0)
            fn(e);
    }
}

} // namespace

void registerCrateCommands()
{
    Terminal::instance().registerCommand({
        "crate_spawn",
        "Spawn a physical crate 5m ahead (optional velocity)",
        "crate_spawn [distance] [vx vy vz]",
        [](const std::vector<std::string>& args) {
            Player& player = THE_PLAYER;
            Camera& camera = THE_CAMERA;

            float distance = 5.0f;
            if (!args.empty() && !parseFloat(args[0], distance))
                distance = 5.0f;
            if (distance < 0.1f)
                distance = 0.1f;

            glm::vec3 dir = camera.front;
            if (glm::dot(dir, dir) < 1e-6f)
                dir = glm::vec3(1.0f, 0.0f, 0.0f);
            dir = glm::normalize(dir);

            glm::vec3 velocity(0.0f);
            if (args.size() >= 4)
            {
                float vx = 0.0f, vy = 0.0f, vz = 0.0f;
                parseFloat(args[1], vx);
                parseFloat(args[2], vy);
                parseFloat(args[3], vz);
                velocity = glm::vec3(vx, vy, vz);
            }

            const glm::vec3 spawnPos = player.pos + dir * distance;

            std::vector<CollisionTriangle> crateTriangles;
            constexpr float kCrateHalfExtent = 2.5f;
            buildBoxCollisionTriangles(crateTriangles, glm::vec3(0.0f),
                                       glm::vec3(kCrateHalfExtent));

            // Material table owns density/health/cut behavior. Load lazily.
            MimitaImpact::MaterialConfig& materials = MimitaImpact::MaterialConfig::instance();
            if (materials.revision() == 0)
                materials.load();
            const uint32_t materialId = MimitaImpact::materialIdForName("wood");
            const MimitaImpact::MaterialDefinition& material = materials.find(materialId);

            PhysicalEntitySystem& system = PhysicalEntitySystem::instance();
            const uint32_t id = system.add(
                crateTriangles,
                glm::translate(glm::mat4(1.0f), spawnPos),
                PhysicalEntityMotion::Dynamic,
                materialId);
            if (PhysicalEntity* e = system.find(id))
            {
                e->velocity = velocity;
                e->shape = PhysicalEntityShape::Box;
                e->halfExtents = glm::vec3(kCrateHalfExtent);
                // Gameplay density, not the material's physical density: the
                // crate must stay light enough for a player push. `crate_density`
                // / `crate_mass` still override this and always re-derive mass
                // from density * volume.
                e->density = 1.0f;
                e->mass = e->density * crateVolume(*e);
                e->friction = 0.7f;
                e->restitution = 0.0f;
                e->linearDamping = 0.15f;
                e->angularDamping = 2.5f;
                e->maxAngularSpeed = 6.0f;
                e->rightingStrength = 28.0f;
                e->sleepRequiredTicks = 18;
                e->materialId = materialId;
                e->strength = material.strength;
                e->modelPath = "generated:crate-box";
                e->texturePath = "assets/textureshq/clouds11.png";
                e->persistenceId = "crate-" + std::to_string(id);
                // Build the authoritative destructible record (box minus cuts)
                // and replace the collision mesh with the generated triangles.
                MimitaImpact::ImpactSystem::instance().initializeEntity(
                    *e, materialId, glm::vec3(kCrateHalfExtent));
            }

            char buf[192];
            std::snprintf(buf, sizeof(buf),
                "[CRATE] spawned id=%u at (%.2f %.2f %.2f) vel=(%.2f %.2f %.2f) total=%zu",
                id, spawnPos.x, spawnPos.y, spawnPos.z,
                velocity.x, velocity.y, velocity.z, system.entities().size());
            Terminal::instance().addLog(buf);

            if (!CollisionConfig::instance().actorTriangleSolver())
                Terminal::instance().addLog(
                    "[CRATE] note: actorTriangleSolver is off; collides once enabled in config/collision.json");
        }
    });

    Terminal::instance().registerCommand({
        "crate_info",
        "Print active crate rigid-body state",
        "crate_info",
        [](const std::vector<std::string>&) {
            size_t count = 0;
            forEachActiveCrate([&](PhysicalEntity& e) {
                logCrateInfo(e);
                ++count;
            });
            if (count == 0)
                Terminal::instance().addLog("[CRATE] no active crates");
        }
    });

    Terminal::instance().registerCommand({
        "crate_material",
        "Set the impact material for every active crate (e.g. wood, steel)",
        "crate_material <name>",
        [](const std::vector<std::string>& args) {
            if (args.empty())
            {
                Terminal::instance().addLog("[CRATE] usage: crate_material <name>");
                return;
            }
            MimitaImpact::MaterialConfig& materials = MimitaImpact::MaterialConfig::instance();
            if (materials.revision() == 0)
                materials.load();
            const uint32_t materialId = MimitaImpact::materialIdForName(args[0]);
            const MimitaImpact::MaterialDefinition& material = materials.find(materialId);
            forEachActiveCrate([&](PhysicalEntity& e) {
                e.materialId = materialId;
                e.destructible.materialId = materialId;
                e.strength = material.strength;
                e.destructible.maxHealth = material.strength;
                e.destructible.health = material.strength;
            });
            Terminal::instance().addLog(
                "[CRATE] material set to " + material.id +
                " (density=" + std::to_string(material.density) + ")");
        }
    });

    Terminal::instance().registerCommand({
        "crate_mass",
        "Set mass in kg for every active crate and derive density",
        "crate_mass [kg]",
        [](const std::vector<std::string>& args) {
            float desiredMass = 0.0f;
            if (args.empty() || !parseFloat(args[0], desiredMass) || desiredMass <= 0.0f)
            {
                Terminal::instance().addLog("[CRATE] usage: crate_mass [kg]");
                return;
            }
            forEachActiveCrate([&](PhysicalEntity& e) {
                e.density = clampCrateDensity(desiredMass /
                                              std::max(crateVolume(e), 0.0001f));
                e.mass = e.density * crateVolume(e);
                e.sleeping = false;
                e.sleepTicks = 0;
            });
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                "[CRATE] mass set to %.3f kg; density recomputed from volume", desiredMass);
            Terminal::instance().addLog(buf);
        }
    });

    Terminal::instance().registerCommand({
        "crate_density",
        "Set density in kg/m3 for every active crate and derive mass",
        "crate_density [kg/m3]",
        [](const std::vector<std::string>& args) {
            float desiredDensity = 0.0f;
            if (args.empty() || !parseFloat(args[0], desiredDensity) || desiredDensity <= 0.0f)
            {
                Terminal::instance().addLog("[CRATE] usage: crate_density [kg/m3]");
                return;
            }
            forEachActiveCrate([&](PhysicalEntity& e) {
                e.density = clampCrateDensity(desiredDensity);
                e.mass = e.density * crateVolume(e);
                e.sleeping = false;
                e.sleepTicks = 0;
            });
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                "[CRATE] density set to %.3f kg/m3; mass recomputed from volume",
                desiredDensity);
            Terminal::instance().addLog(buf);
        }
    });

    Terminal::instance().registerCommand({
        "crate_bounce",
        "Set crate restitution for world, player, and crate impacts",
        "crate_bounce [0..2]",
        [](const std::vector<std::string>& args) {
            float bounce = 0.0f;
            if (args.empty() || !parseFloat(args[0], bounce) || !std::isfinite(bounce))
            {
                Terminal::instance().addLog("[CRATE] usage: crate_bounce [0..2]");
                return;
            }
            bounce = std::clamp(bounce, 0.0f, 2.0f);
            forEachActiveCrate([&](PhysicalEntity& e) {
                e.restitution = bounce;
                e.sleeping = false;
                e.sleepTicks = 0;
            });
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                          "[CRATE] restitution set to %.3f", bounce);
            Terminal::instance().addLog(buf);
        }
    });

    Terminal::instance().registerCommand({
        "crate_sleep",
        "Sleep every active crate",
        "crate_sleep",
        [](const std::vector<std::string>&) {
            forEachActiveCrate([](PhysicalEntity& e) {
                e.velocity = glm::vec3(0.0f);
                e.angularVelocity = glm::vec3(0.0f);
                e.sleeping = true;
                e.sleepTicks = e.sleepRequiredTicks;
            });
            Terminal::instance().addLog("[CRATE] active crates forced asleep");
        }
    });

    Terminal::instance().registerCommand({
        "crate_wake",
        "Wake every active crate",
        "crate_wake",
        [](const std::vector<std::string>&) {
            forEachActiveCrate([](PhysicalEntity& e) {
                e.sleeping = false;
                e.sleepTicks = 0;
            });
            Terminal::instance().addLog("[CRATE] active crates awakened");
        }
    });

    Terminal::instance().registerCommand({
        "crate_clear",
        "Remove all spawned crates",
        "crate_clear",
        [](const std::vector<std::string>&) {
            PhysicalEntitySystem::instance().clear();
            Terminal::instance().addLog("[CRATE] cleared all crates");
        }
    });
}
