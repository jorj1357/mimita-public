#include "network/server.h"

#include <cstdio>

namespace MimitaNet {

void simulateNpc(ServerNpc& npc, const std::unordered_map<uint32_t, ServerPlayer>& players)
{
    npc.phase += SERVER_DT * 0.65f;
    glm::vec3 target(1.0f, 5.0f, 30.0f);
    if (!players.empty())
        target = players.begin()->second.pos;

    glm::vec2 delta(target.x - npc.pos.x, target.y - npc.pos.y);
    if (glm::length(delta) > 2.5f)
    {
        glm::vec2 direction = glm::normalize(delta);
        npc.vel.x = direction.x * 3.5f;
        npc.vel.y = direction.y * 3.5f;
        npc.yaw = glm::degrees(std::atan2(direction.y, direction.x));
    }
    else
    {
        npc.vel.x = std::cos(npc.phase) * 1.5f;
        npc.vel.y = std::sin(npc.phase) * 1.5f;
    }
    npc.pos += npc.vel * SERVER_DT;
}

SnapshotEntity makeNpcEntity(const ServerNpc& npc)
{
    SnapshotEntity out{};
    out.networkEntityId = npc.entityId;
    out.entityType = ENTITY_NPC;
    out.active = 1;
    out.ownerClientId = 0;
    out.px = npc.pos.x; out.py = npc.pos.y; out.pz = npc.pos.z;
    out.vx = npc.vel.x; out.vy = npc.vel.y; out.vz = npc.vel.z;
    out.yaw = npc.yaw;
    out.health = npc.health;
    out.onGround = npc.onGround ? 1 : 0;
    copyName(out.displayName, npc.name);
    return out;
}

} // namespace MimitaNet
