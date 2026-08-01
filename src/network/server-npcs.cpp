#include "network/server.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace MimitaNet {

static float serverRandom(unsigned int& state)
{
    state = state * 1664525u + 1013904223u;
    return (float)((state >> 8) & 0x00ffffffu) / (float)0x01000000u;
}

void simulateNpc(ServerNpc& npc, const std::unordered_map<uint32_t, ServerPlayer>& players)
{
    glm::vec3 target(1.0f, 5.0f, 30.0f);
    if (!players.empty())
    {
        // Target the nearest player so NPC behavior is sensible with several
        // clients instead of always picking the first in the map.
        float bestDist2 = std::numeric_limits<float>::max();
        for (const auto& kv : players)
        {
            const glm::vec2 d(kv.second.pos.x - npc.pos.x, kv.second.pos.y - npc.pos.y);
            const float d2 = d.x * d.x + d.y * d.y;
            if (d2 < bestDist2)
            {
                bestDist2 = d2;
                target = kv.second.pos;
            }
        }
    }

    glm::vec2 toTarget(target.x - npc.pos.x, target.y - npc.pos.y);
    float dist2d = glm::length(toTarget);
    glm::vec2 chaseDir = dist2d > 0.01f ? toTarget / dist2d : glm::vec2(1.0f, 0.0f);
    glm::vec2 lateral(-chaseDir.y, chaseDir.x);

    // Yaw toward target
    npc.yaw = glm::degrees(std::atan2(chaseDir.y, chaseDir.x));

    // Apply external knockback impulse (from explosions, hitscan, etc.)
    if (glm::length(npc.knockbackImpulse) > 0.05f)
    {
        npc.pos += npc.knockbackImpulse * SERVER_DT;
        npc.knockbackImpulse *= std::exp(-8.0f * SERVER_DT);
        if (glm::length(npc.knockbackImpulse) < 0.05f)
            npc.knockbackImpulse = glm::vec3(0.0f);
    }

    // State machine
    npc.stateTimer -= SERVER_DT;
    if (npc.stateTimer <= 0.0f)
    {
        float d = npc.difficulty / 10.0f;
        if (dist2d < 4.0f && serverRandom(reinterpret_cast<unsigned int&>(npc.phase)) < 0.3f)
        {
            npc.aiState = ServerNpcState::Retreat;
            npc.stateTimer = 0.5f + serverRandom(reinterpret_cast<unsigned int&>(npc.phase)) * 1.0f;
        }
        else if (dist2d > 15.0f)
        {
            npc.aiState = serverRandom(reinterpret_cast<unsigned int&>(npc.phase)) < 0.5f
                ? ServerNpcState::Chase
                : ServerNpcState::Orbit;
            npc.stateTimer = 1.0f + serverRandom(reinterpret_cast<unsigned int&>(npc.phase)) * 2.0f;
        }
        else
        {
            float r = serverRandom(reinterpret_cast<unsigned int&>(npc.phase));
            if (r < 0.3f + d * 0.2f)
            {
                npc.aiState = ServerNpcState::Strafe;
                npc.strafeDir = serverRandom(reinterpret_cast<unsigned int&>(npc.phase)) < 0.5f ? 1.0f : -1.0f;
            }
            else if (r < 0.6f)
            {
                npc.aiState = ServerNpcState::Orbit;
            }
            else
            {
                npc.aiState = ServerNpcState::Chase;
            }
            npc.stateTimer = 0.8f + serverRandom(reinterpret_cast<unsigned int&>(npc.phase)) * 2.0f;
        }
    }

    glm::vec2 moveDir(0.0f, 0.0f);
    float speed = 3.0f + npc.difficulty * 0.2f;

    switch (npc.aiState)
    {
        case ServerNpcState::Chase:
            moveDir = chaseDir;
            speed = 3.5f + npc.difficulty * 0.3f;
            break;

        case ServerNpcState::Strafe:
        {
            float towardBias = dist2d > 10.0f ? 0.3f : (dist2d < 5.0f ? -0.2f : 0.0f);
            moveDir = chaseDir * towardBias + lateral * npc.strafeDir;
            float len = glm::length(moveDir);
            if (len > 0.001f) moveDir /= len;
            break;
        }

        case ServerNpcState::Retreat:
            moveDir = -chaseDir;
            break;

        case ServerNpcState::Orbit:
        {
            float orbitSpeed = 1.5f + npc.difficulty * 0.15f;
            npc.orbitAngle += orbitSpeed * SERVER_DT * (npc.strafeDir);
            glm::vec3 orbitTarget = target + glm::vec3(
                std::cos(npc.orbitAngle) * 6.0f,
                std::sin(npc.orbitAngle) * 6.0f, 0.0f);
            glm::vec2 toOrbit = glm::vec2(orbitTarget.x, orbitTarget.y) - glm::vec2(npc.pos.x, npc.pos.y);
            float oLen = glm::length(toOrbit);
            if (oLen > 0.1f) moveDir = toOrbit / oLen;
            break;
        }
    }

    // Move
    npc.vel.x = moveDir.x * speed;
    npc.vel.y = moveDir.y * speed;
    npc.vel.z = -9.81f * SERVER_DT; // Simple gravity

    // Simple ground collision
    if (npc.pos.z <= 0.5f)
    {
        npc.pos.z = 0.5f;
        npc.vel.z = 0.0f;
        npc.onGround = true;
    }

    // Apply velocity
    npc.pos += npc.vel * SERVER_DT;

    // Health regen
    if (npc.health < 100)
        npc.health += 1;
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
