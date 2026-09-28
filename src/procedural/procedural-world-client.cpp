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

namespace {

uint32_t gDoorEntityId = 0;
uint32_t gAppliedRoom = 0xFFFFFFFFu;

void removeDoor()
{
    if (gDoorEntityId != 0)
    {
        PhysicalEntitySystem::instance().remove(gDoorEntityId);
        gDoorEntityId = 0;
        gAppliedRoom = 0xFFFFFFFFu;
    }
}

} // anonymous namespace

void clientProceduralWorldReset()
{
    // The entity store may already have been cleared (map change); only forget
    // our handle so the next tick re-creates it from replicated state.
    gDoorEntityId = 0;
    gAppliedRoom = 0xFFFFFFFFu;
}

void clientProceduralWorldTick()
{
    const MimitaNet::ProceduralWorldNetworkState& p =
        MimitaNet::CommunityMatchClient::instance().procedural();

    const bool wantDoor = p.enabled != 0 && p.exitLocked != 0 && p.currentRoom >= 1;
    if (!wantDoor)
    {
        removeDoor();
        return;
    }

    if (MimitaProcedural::proceduralWorldConfig().modes.empty())
        MimitaProcedural::loadProceduralWorldConfig();

    const MimitaProcedural::ProceduralModeDefinition* mode =
        MimitaProcedural::proceduralModeById(p.modeId);
    if (!mode)
        return;
    const MimitaProcedural::ProceduralRoomDefinition* room =
        MimitaProcedural::proceduralRoomById(mode->roomId);
    if (!room)
        return;

    const glm::vec3 exit =
        MimitaProcedural::proceduralRoomExit(*mode, *room, p.currentRoom);
    const glm::mat4 transform = glm::translate(glm::mat4(1.0f), exit);

    PhysicalEntitySystem& system = PhysicalEntitySystem::instance();
    PhysicalEntity* door = gDoorEntityId != 0 ? system.find(gDoorEntityId) : nullptr;

    if (!door)
    {
        std::vector<CollisionTriangle> triangles;
        buildBoxCollisionTriangles(triangles, glm::vec3(0.0f), mode->doorHalfExtents);
        gDoorEntityId = system.add(triangles, transform, PhysicalEntityMotion::Static);
        door = system.find(gDoorEntityId);
        if (door)
            door->halfExtents = mode->doorHalfExtents;
        gAppliedRoom = p.currentRoom;
        return;
    }

    if (gAppliedRoom != p.currentRoom)
    {
        system.moveKinematic(gDoorEntityId, transform, 0.0f);
        gAppliedRoom = p.currentRoom;
    }
    door->halfExtents = mode->doorHalfExtents;
}
