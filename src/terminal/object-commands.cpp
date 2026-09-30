// 2026-09-30
/* purpose
* Register the object_spawn terminal command: spawn a destructible moving
* object from a closed GLB. The GLB is imported GL-free, validated as a closed
* manifold, and given the same boolean cut behavior as a crate. Non-watertight
* models are rejected with the exact reason instead of entering gameplay.
* Does NOT upload the GLB's own textures (the caller passes a texture path),
* own physics, or implement rendering.
*/

#include "terminal/object-commands.h"

#include <cstdio>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "camera.h"
#include "config/material-config.h"
#include "devtools/terminal.h"
#include "entities/player.h"
#include "impact/destructible-mesh-loader.h"
#include "impact/impact-system.h"
#include "physics/physical-entity.h"
#include "terminal/terminal-state.h"

namespace {

bool parseFloat(const std::string& text, float& out)
{
    try
    {
        size_t used = 0;
        out = std::stof(text, &used);
        return used == text.size();
    }
    catch (...)
    {
        return false;
    }
}

} // namespace

void registerObjectCommands()
{
    Terminal::instance().registerCommand({
        "object_spawn",
        "Spawn a destructible object from a watertight GLB",
        "object_spawn <glb-path> [distance] [texture-path]",
        [](const std::vector<std::string>& args) {
            if (args.empty())
            {
                Terminal::instance().addLog(
                    "[OBJECT] usage: object_spawn <glb-path> [distance] [texture-path]");
                return;
            }

            const std::string glbPath = args[0];
            float distance = 6.0f;
            if (args.size() >= 2 && !parseFloat(args[1], distance))
                distance = 6.0f;
            if (distance < 0.1f)
                distance = 0.1f;
            const std::string texturePath =
                args.size() >= 3 ? args[2] : std::string("assets/textureshq/clouds11.png");

            MimitaImpact::DestructibleMeshLoad load =
                MimitaImpact::loadDestructibleMeshFromGLB(glbPath);
            if (!load.success)
            {
                const std::string message = "[OBJECT] import failed: " + load.error;
                Terminal::instance().addLog(message.c_str());
                return;
            }

            MimitaImpact::MaterialConfig& materials =
                MimitaImpact::MaterialConfig::instance();
            if (materials.revision() == 0)
                materials.load();
            const uint32_t materialId = MimitaImpact::materialIdForName("wood");
            const MimitaImpact::MaterialDefinition& material = materials.find(materialId);

            glm::vec3 dir = THE_CAMERA.front;
            if (glm::dot(dir, dir) < 1e-6f)
                dir = glm::vec3(1.0f, 0.0f, 0.0f);
            dir = glm::normalize(dir);
            const glm::vec3 spawnPos = THE_PLAYER.pos + dir * distance;

            PhysicalEntitySystem& system = PhysicalEntitySystem::instance();
            const uint32_t id = system.add(
                std::vector<CollisionTriangle>{},
                glm::translate(glm::mat4(1.0f), spawnPos),
                PhysicalEntityMotion::Dynamic, materialId);

            size_t triangleCount = 0;
            float volume = 0.0f;
            if (PhysicalEntity* e = system.find(id))
            {
                e->shape = PhysicalEntityShape::TriangleMesh;
                e->halfExtents = load.halfExtents;
                e->density = 1.0f;
                e->friction = 0.7f;
                e->restitution = 0.0f;
                e->linearDamping = 0.15f;
                e->angularDamping = 2.5f;
                e->maxAngularSpeed = 6.0f;
                e->rightingStrength = 12.0f;
                e->sleepRequiredTicks = 18;
                e->materialId = materialId;
                e->strength = material.strength;
                e->modelPath = glbPath;
                e->texturePath = texturePath;
                e->persistenceId = "object-" + std::to_string(id);

                MimitaImpact::ImpactSystem::instance().initializeEntityFromMesh(
                    *e, std::move(load.mesh), load.halfExtents, materialId);
                triangleCount = e->localTriangles.size();
                volume = e->destructible.remainingVolume;
            }

            char buf[320];
            std::snprintf(buf, sizeof(buf),
                "[OBJECT] spawned id=%u model=%s at (%.2f %.2f %.2f) tris=%zu vol=%.4f "
                "half=(%.2f %.2f %.2f) mass=%.2f",
                id, glbPath.c_str(), spawnPos.x, spawnPos.y, spawnPos.z,
                triangleCount, volume, load.halfExtents.x, load.halfExtents.y,
                load.halfExtents.z,
                system.find(id) ? system.find(id)->mass : 0.0f);
            Terminal::instance().addLog(buf);
        }
    });
}