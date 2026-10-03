// 09 28 2026, 00 00
/* purpose
* Implements the client-side procedural-world door barrier.
* Reads replicated state (CommunityMatchClient) and mirrors the current room's
* locked exit as a static PhysicalEntity. Collision comes from the entity's
* local box triangles; visuals come from the shared physical-entity renderer.
* Does NOT own room generation or authoritative room lifecycle.
*/

#include "procedural/procedural-world-client.h"

#include <algorithm>
#include <cmath>
#include <vector>
#include <cstdio>
#include <cstring>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "physics/physical-entity.h"
#include "camera.h"
#include "debug/debug-visuals.h"
#include "entities/player.h"
#include "gui/ui-system.h"
#include "procedural/procedural-world.h"
#include "network/community-match-client.h"
#include "world/world-gltf-loader.h"

namespace {

uint32_t gDoorEntityId = 0;
uint32_t gAppliedRoom = 0xFFFFFFFFu;
World gRoomTemplate;
std::string gRoomTemplatePath;
bool gRoomTemplateLoaded = false;
bool gRoomInstancesApplied = false;
uint32_t gAppliedGeneratedRooms = 0;
uint32_t gAppliedStateVersion = 0;
size_t gBaseVertexCount = 0;
size_t gBaseBatchCount = 0;
size_t gBaseCollisionCount = 0;
uint16_t gTeleportShieldTicks = 0;

void removeDoor()
{
    if (gDoorEntityId != 0)
    {
        PhysicalEntitySystem::instance().remove(gDoorEntityId);
        gDoorEntityId = 0;
        gAppliedRoom = 0xFFFFFFFFu;
    }
}

void removeRoomInstances(World& world)
{
    if (!gRoomInstancesApplied)
        return;
    truncateWorldInstances(world, gBaseVertexCount, gBaseBatchCount,
                            gBaseCollisionCount);
    gRoomInstancesApplied = false;
    gAppliedGeneratedRooms = 0;
    gAppliedStateVersion = 0;
}

bool ensureRoomTemplate(const std::string& path)
{
    if (gRoomTemplateLoaded && gRoomTemplatePath == path)
        return true;
    if (!loadWorldTemplate(path.c_str(), gRoomTemplate))
    {
        gRoomTemplateLoaded = false;
        gRoomTemplatePath.clear();
        return false;
    }
    gRoomTemplateLoaded = true;
    gRoomTemplatePath = path;
    return true;
}

} // anonymous namespace

void clientProceduralWorldReset()
{
    // The entity store may already have been cleared (map change); only forget
    // our handle so the next tick re-creates it from replicated state.
    gDoorEntityId = 0;
    gAppliedRoom = 0xFFFFFFFFu;
    gRoomInstancesApplied = false;
    gAppliedGeneratedRooms = 0;
    gBaseVertexCount = 0;
    gBaseBatchCount = 0;
    gBaseCollisionCount = 0;
    gTeleportShieldTicks = 0;
}

void appendStreamingBlocks(World& world,
                           const MimitaProcedural::ProceduralModeDefinition& mode,
                           uint32_t seed, uint32_t roomSlot)
{
    if (!mode.streamingEnabled || mode.chunkSize <= 0.0f)
        return;
    const glm::vec3 center = MimitaProcedural::proceduralTransformPoint(
        MimitaProcedural::proceduralRoomTransform(mode, roomSlot),
        glm::vec3(0.0f));
    const int32_t centerX = static_cast<int32_t>(std::floor(
        (center.x - mode.origin.x) / mode.chunkSize));
    const int32_t centerZ = static_cast<int32_t>(std::floor(
        (center.z - mode.origin.z) / mode.chunkSize));
    const int32_t radius = static_cast<int32_t>(mode.streamRadiusChunks);
    for (int32_t z = centerZ - radius; z <= centerZ + radius; ++z)
    {
        for (int32_t x = centerX - radius; x <= centerX + radius; ++x)
        {
            for (const glm::vec3& position :
                 MimitaProcedural::proceduralChunkBlockPositions(
                     mode, seed, x, z))
            {
                appendWorldInstance(
                    world, gRoomTemplate,
                    glm::translate(glm::mat4(1.0f), position));
            }
        }
    }
}

void clientProceduralTeleportShieldStart()
{
    gTeleportShieldTicks = 60;
}

void clientProceduralTeleportShieldTick()
{
    if (gTeleportShieldTicks > 0)
        --gTeleportShieldTicks;
}

void clientProceduralTeleportShieldRender(const Player& player,
                                          const Camera& camera)
{
    if (gTeleportShieldTicks == 0)
        return;

    DebugVis::drawFilledSphere(
        camera, player.pos + glm::vec3(0.0f, 0.0f, 0.9f), 0.9f,
        glm::vec4(0.45f, 1.0f, 0.92f, 0.28f),
        glm::vec3(1.15f, 1.15f, 2.2f));
}

void clientProceduralTeleportShieldRenderUi()
{
    if (gTeleportShieldTicks == 0)
        return;

    char text[64];
    std::snprintf(text, sizeof(text), "TELEPORT SHIELD: %u",
                  (unsigned)gTeleportShieldTicks);
    const float scale = 0.36f;
    const float width = static_cast<float>(std::strlen(text)) * scale * 8.0f;
    uiDrawText(text, uiScreenW() * 0.5f - width * 0.5f, 54.0f, scale,
               glm::vec4(0.45f, 1.0f, 0.92f, 1.0f));
}

void clientProceduralWorldTick(World& world)
{
    const MimitaNet::ProceduralWorldNetworkState& p =
        MimitaNet::CommunityMatchClient::instance().procedural();

    if (p.enabled == 0 || p.generatedRooms == 0)
    {
        removeRoomInstances(world);
        removeDoor();
        return;
    }

    bool configChanged = false;
    if (MimitaProcedural::proceduralWorldConfig().modes.empty())
        MimitaProcedural::loadProceduralWorldConfig();
    else
        configChanged = MimitaProcedural::reloadProceduralWorldConfigIfChanged();
    if (configChanged)
        removeDoor();

    const MimitaProcedural::ProceduralModeDefinition* mode =
        MimitaProcedural::proceduralModeById(p.modeId);
    if (!mode)
        return;
    const MimitaProcedural::ProceduralRoomDefinition* room =
        MimitaProcedural::proceduralRoomById(mode->roomId);
    if (!room)
        return;

    if (!ensureRoomTemplate(room->geometryPath))
        return;

    // A state version marks a coarse streaming boundary. Rebuild the bounded
    // window once, never from the render loop's per-frame geometry path.
    if (gRoomInstancesApplied && gAppliedStateVersion != p.stateVersion)
        removeRoomInstances(world);

    const bool firstApplication = !gRoomInstancesApplied;
    if (firstApplication)
    {
        gBaseVertexCount = world.mesh.verts.size();
        gBaseBatchCount = world.mesh.batches.size();
        gBaseCollisionCount = world.collisionMesh.triangles.size();
    }

    if (firstApplication)
    {
        appendWorldInstance(world, gRoomTemplate,
                            MimitaProcedural::proceduralRoomTransform(*mode, 0));
        const uint32_t radius = mode->loadedRoomRadius;
        const uint32_t first = p.currentRoom > radius ? p.currentRoom - radius : 1;
        const uint32_t last = std::min(p.generatedRooms,
                                       p.currentRoom + radius);
        for (uint32_t slot = first; slot <= last; ++slot)
        {
            appendWorldInstance(
                world, gRoomTemplate,
                MimitaProcedural::proceduralRoomTransform(*mode, slot));
        }
        appendStreamingBlocks(world, *mode, p.seed, p.currentRoom);
        gRoomInstancesApplied = true;
        gAppliedGeneratedRooms = p.generatedRooms;
        gAppliedStateVersion = p.stateVersion;
    }

    const bool wantDoor = p.exitLocked != 0 && p.currentRoom >= 1;
    if (!wantDoor)
    {
        removeDoor();
        return;
    }

    const glm::mat4 transform = MimitaProcedural::proceduralRoomDoorTransform(
        *mode, *room, p.currentRoom);
    const glm::vec3 half = MimitaProcedural::proceduralRoomDoorHalfExtents(
        *mode, *room);

    // A room owns its own door. Do not move the previous room's entity to the
    // new room: remove room N's door, then create a fresh room N+1 door.
    if (gAppliedRoom != 0xFFFFFFFFu && gAppliedRoom != p.currentRoom)
        removeDoor();

    PhysicalEntitySystem& system = PhysicalEntitySystem::instance();
    PhysicalEntity* door = gDoorEntityId != 0 ? system.find(gDoorEntityId) : nullptr;

    if (!door)
    {
        std::vector<CollisionTriangle> triangles;
        buildBoxCollisionTriangles(triangles, glm::vec3(0.0f), half);
        gDoorEntityId = system.add(triangles, transform, PhysicalEntityMotion::Static);
        door = system.find(gDoorEntityId);
        if (door)
            door->halfExtents = half;
        gAppliedRoom = p.currentRoom;
        return;
    }

    // Keep the existing door fixed for this room. A new room takes the branch
    // above and receives a new physical entity instead of a fast-moving door.
    gAppliedRoom = p.currentRoom;
    door->halfExtents = half;
}
