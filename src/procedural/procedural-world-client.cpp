// 09 28 2026, 00 00
/* purpose
* Implements the client-side procedural-world door barrier.
* Reads replicated state (CommunityMatchClient) and mirrors the current room's
* locked exit as a static PhysicalEntity. Collision comes from the entity's
* local box triangles; visuals come from the shared physical-entity renderer.
* Does NOT own room generation or authoritative room lifecycle.
*/

#include "procedural/procedural-world-client.h"

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "physics/physical-entity.h"
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
size_t gBaseVertexCount = 0;
size_t gBaseBatchCount = 0;
size_t gBaseCollisionCount = 0;

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

    if (!gRoomInstancesApplied)
    {
        gBaseVertexCount = world.mesh.verts.size();
        gBaseBatchCount = world.mesh.batches.size();
        gBaseCollisionCount = world.collisionMesh.triangles.size();
        gRoomInstancesApplied = true;
    }

    // Slot 0 is the lobby and combat room N is slot N on the server. Render
    // the same inclusive range so client geometry matches the authoritative
    // teleport coordinates.
    while (gAppliedGeneratedRooms <= p.generatedRooms)
    {
        appendWorldInstance(
            world, gRoomTemplate,
            MimitaProcedural::proceduralRoomTransform(
                *mode, gAppliedGeneratedRooms));
        ++gAppliedGeneratedRooms;
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

    system.moveKinematic(gDoorEntityId, transform, 0.0f);
    gAppliedRoom = p.currentRoom;
    door->halfExtents = half;
}
