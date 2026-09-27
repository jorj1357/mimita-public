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
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "devtools/terminal.h"
#include "terminal/terminal-state.h"
#include "physics/physical-entity.h"
#include "config/collision-config.h"

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
            buildBoxCollisionTriangles(crateTriangles, glm::vec3(0.0f),
                                       glm::vec3(0.5f));

            PhysicalEntitySystem& system = PhysicalEntitySystem::instance();
            const uint32_t id = system.add(
                crateTriangles,
                glm::translate(glm::mat4(1.0f), spawnPos),
                PhysicalEntityMotion::Kinematic,
                /*materialId=*/1u);
            if (PhysicalEntity* e = system.find(id))
                e->velocity = velocity;

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
        "crate_clear",
        "Remove all spawned crates",
        "crate_clear",
        [](const std::vector<std::string>&) {
            PhysicalEntitySystem::instance().clear();
            Terminal::instance().addLog("[CRATE] cleared all crates");
        }
    });
}
