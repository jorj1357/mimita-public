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

#include <algorithm>
#include <cstdio>
#include <filesystem>
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

// Whole-string positive integer, or 0 when the text is not a bare number.
int parsePositiveIndex(const std::string& text)
{
    try
    {
        size_t used = 0;
        const int value = std::stoi(text, &used);
        return (used == text.size() && value > 0) ? value : 0;
    }
    catch (...)
    {
        return 0;
    }
}

} // namespace

// Every .glb in the physics-objects folder, alphabetically. Numbered spawning
// (object_spawn <n>) and object_spawn_list both use this one list; exposed for
// a self-test that verifies numbered spawning without a live terminal.
std::vector<std::string> listPhysicsObjectGlbs()
{
    std::vector<std::string> paths;
    std::error_code ec;
    for (const auto& entry :
         std::filesystem::directory_iterator(
             "assets/objects/things/physics-objects", ec))
    {
        if (!entry.is_regular_file())
            continue;
        const std::string extension = entry.path().extension().string();
        if (extension == ".glb" || extension == ".GLB")
            paths.push_back(entry.path().generic_string());
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

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
                    "[OBJECT] usage: object_spawn <glb-path|n> [distance] [texture-path]");
                return;
            }

            // A bare number selects the nth entry from object_spawn_list; any
            // other text is a path (path-based spawning keeps working).
            std::string glbPath = args[0];
            const int index = parsePositiveIndex(args[0]);
            if (index > 0)
            {
                const std::vector<std::string> list = listPhysicsObjectGlbs();
                if (index > (int)list.size())
                {
                    char msg[160];
                    std::snprintf(msg, sizeof(msg),
                        "[OBJECT] index %d out of range (1..%zu); run object_spawn_list",
                        index, list.size());
                    Terminal::instance().addLog(msg);
                    return;
                }
                glbPath = list[(size_t)index - 1];
            }

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

    Terminal::instance().registerCommand({
        "object_spawn_list",
        "List spawnable physics-object GLBs with numbers",
        "object_spawn_list",
        [](const std::vector<std::string>&) {
            const std::vector<std::string> list = listPhysicsObjectGlbs();
            Terminal::instance().addLog("[OBJECT] physics objects:");
            if (list.empty())
            {
                Terminal::instance().addLog(
                    "[OBJECT] none found in assets/objects/things/physics-objects");
                return;
            }
            for (size_t i = 0; i < list.size(); ++i)
            {
                char line[320];
                std::snprintf(line, sizeof(line), "  %zu. %s", i + 1, list[i].c_str());
                Terminal::instance().addLog(line);
            }
            Terminal::instance().addLog("[OBJECT] spawn with: object_spawn <n>");
        }
    });
}