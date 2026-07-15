#include "network/server.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace MimitaNet {

void copyName(char (&dst)[MAX_NAME_BYTES], const std::string& name)
{
    std::memset(dst, 0, sizeof(dst));
    std::strncpy(dst, name.c_str(), sizeof(dst) - 1);
}

std::string uniquePlayerName(
    const std::unordered_map<uint32_t, ServerPlayer>& players,
    const std::string& requested,
    uint32_t ownId)
{
    const std::string base = requested.empty() ? "player" + std::to_string(ownId) : requested;
    std::string candidate = base;
    int suffix = 2;
    for (;;)
    {
        bool used = false;
        for (const auto& kv : players)
        {
            if (kv.first != ownId && kv.second.name == candidate)
            {
                used = true;
                break;
            }
        }
        if (!used)
            return candidate;
        candidate = base + "(" + std::to_string(suffix++) + ")";
    }
}

glm::vec3 closestPointTriangle(glm::vec3 p, glm::vec3 a, glm::vec3 b, glm::vec3 c)
{
    glm::vec3 ab = b - a;
    glm::vec3 ac = c - a;
    glm::vec3 ap = p - a;
    float d1 = glm::dot(ab, ap);
    float d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return a;
    glm::vec3 bp = p - b;
    float d3 = glm::dot(ab, bp);
    float d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return b;
    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
    {
        float v = d1 / (d1 - d3);
        return a + v * ab;
    }
    glm::vec3 cp = p - c;
    float d5 = glm::dot(ab, cp);
    float d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return c;
    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 >= 0.0f)
    {
        float w = d2 / (d2 - d6);
        return a + w * ac;
    }
    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
    {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + w * (c - b);
    }
    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    return a + ab * v + ac * w;
}

void resolveWorldCollision(ServerPlayer& p, const HeadlessWorld& world)
{
    p.onGround = false;

    for (int pass = 0; pass < 3; ++pass)
    {
        glm::vec3 samples[3] = {
            p.pos + glm::vec3(0, 0, -PLAYER_HEIGHT * 0.5f + PLAYER_RADIUS),
            p.pos,
            p.pos + glm::vec3(0, 0, PLAYER_HEIGHT * 0.5f - PLAYER_RADIUS)
        };

        for (glm::vec3 sample : samples)
        {
            for (const CollisionTriangle& tri : world.triangles)
            {
                glm::vec3 mn = glm::min(glm::min(tri.a, tri.b), tri.c) - glm::vec3(PLAYER_RADIUS + 0.1f);
                glm::vec3 mx = glm::max(glm::max(tri.a, tri.b), tri.c) + glm::vec3(PLAYER_RADIUS + 0.1f);
                if (sample.x < mn.x || sample.x > mx.x || sample.y < mn.y || sample.y > mx.y || sample.z < mn.z || sample.z > mx.z)
                    continue;

                glm::vec3 cp = closestPointTriangle(sample, tri.a, tri.b, tri.c);
                glm::vec3 delta = sample - cp;
                float dist = glm::length(delta);
                if (dist >= PLAYER_RADIUS || dist < 0.00001f)
                    continue;

                glm::vec3 n = delta / dist;
                if (glm::dot(n, tri.normal) < 0.0f)
                    n = -n;
                float penetration = PLAYER_RADIUS - dist;
                p.pos += n * (penetration + 0.001f);
                float into = glm::dot(p.vel, n);
                if (into < 0.0f)
                    p.vel -= n * into;
                if (n.z > 0.35f)
                    p.onGround = true;
            }
        }
    }

    if (p.pos.z < world.boundsMin.z + PLAYER_HEIGHT * 0.5f)
    {
        p.pos.z = world.boundsMin.z + PLAYER_HEIGHT * 0.5f;
        if (p.vel.z < 0.0f) p.vel.z = 0.0f;
        p.onGround = true;
    }
}

void resolvePlayerCollision(std::unordered_map<uint32_t, ServerPlayer>& players)
{
    for (auto a = players.begin(); a != players.end(); ++a)
    {
        auto b = a;
        ++b;
        for (; b != players.end(); ++b)
        {
            if (a->second.dead || b->second.dead)
                continue;
            glm::vec2 delta = glm::vec2(a->second.pos - b->second.pos);
            float dist = glm::length(delta);
            float minDist = PLAYER_RADIUS * 2.0f;
            if (dist >= minDist || dist < 0.0001f)
                continue;
            glm::vec2 n = delta / dist;
            float push = (minDist - dist) * 0.5f;
            a->second.pos += glm::vec3(n * push, 0.0f);
            b->second.pos -= glm::vec3(n * push, 0.0f);
        }
    }
}

void simulatePlayer(ServerPlayer& p, const HeadlessWorld& world)
{
    if (p.dead)
    {
        p.vel = glm::vec3(0.0f);
        p.respawnSeconds -= SERVER_DT;
        if (p.respawnSeconds <= 0.0f)
        {
            p.dead = false;
            p.health = 100;
            p.vel = glm::vec3(0.0f);
            p.input = {};
            p.clientStateUpdated = false;
            p.attackQueued = false;
            p.dashAvailable = true;
            p.onGround = false;
            p.respawnSeconds = 0.0f;
            // Use map spawnpoints if available
            if (!world.spawnPoints.empty())
            {
                size_t idx = (p.id - 1) % world.spawnPoints.size();
                p.pos = world.spawnPoints[idx].position;
            }
            else
            {
                p.pos = {1.0f + (float)(p.id - 1) * 1.5f, 5.0f, 30.0f};
            }
            ++p.transformEpoch;
            printf("%s [SERVER RESPAWN] playerId=%u position=(%.2f,%.2f,%.2f) epoch=%u spawnpoints=%zu\n",
                   serverTimestamp(), p.id, p.pos.x, p.pos.y, p.pos.z, (unsigned)p.transformEpoch,
                   world.spawnPoints.size());
        }
        return;
    }

    if (p.clientStateUpdated)
    {
        p.clientStateUpdated = false;
        resolveWorldCollision(p, world);
        return;
    }

    glm::vec2 wish = p.input.wish;
    float wishLen = glm::length(wish);
    if (wishLen > 1.0f)
        wish /= wishLen;

    const float maxSpeed = PHYS.moveSpeed;
    const float accel = p.onGround ? 55.0f : 22.0f;
    glm::vec2 horiz(p.vel.x, p.vel.y);
    glm::vec2 target = wish * maxSpeed;
    horiz += (target - horiz) * std::min(1.0f, accel * SERVER_DT);
    if (wishLen < 0.01f && p.onGround)
        horiz *= 0.82f;

    p.vel.x = horiz.x;
    p.vel.y = horiz.y;
    p.vel.z += PHYS.gravity * SERVER_DT;

    if (p.input.jumpHeld && p.onGround)
    {
        p.vel.z = PHYS.jumpStrength;
        p.onGround = false;
    }

    if (p.input.dashPressed && p.dashAvailable)
    {
        glm::vec2 dashDir = wishLen > 0.01f ? wish : glm::normalize(glm::vec2(p.input.camForward.x, p.input.camForward.y));
        if (glm::length(dashDir) > 0.01f)
        {
            p.vel.x += dashDir.x * DASH_IMPULSE;
            p.vel.y += dashDir.y * DASH_IMPULSE;
            p.dashAvailable = false;
            ++p.lastDashSerial;
        }
    }

    p.yaw = p.input.yaw;
    p.pos += p.vel * SERVER_DT;
    resolveWorldCollision(p, world);
    if (p.onGround)
        p.dashAvailable = true;
}

void pushPositionHistory(ServerPlayer& p, uint32_t tick)
{
    p.posHistory.push_back({p.pos, p.vel, tick});
    while (p.posHistory.size() > 30)
        p.posHistory.pop_front();
}

bool getPositionAtTick(const ServerPlayer& p, uint32_t targetTick, glm::vec3& outPos)
{
    if (p.posHistory.empty())
        return false;
    if (targetTick >= p.posHistory.back().tick)
    {
        outPos = p.pos;
        return true;
    }
    if (targetTick <= p.posHistory.front().tick)
    {
        outPos = p.posHistory.front().pos;
        return true;
    }
    for (int i = (int)p.posHistory.size() - 1; i > 0; --i)
    {
        if (p.posHistory[i].tick == targetTick)
        {
            outPos = p.posHistory[i].pos;
            return true;
        }
        if (p.posHistory[i].tick < targetTick)
        {
            const auto& a = p.posHistory[i];
            const auto& b = p.posHistory[i + 1];
            float frac = float(targetTick - a.tick) / float(b.tick - a.tick);
            outPos = glm::mix(a.pos, b.pos, frac);
            return true;
        }
    }
    outPos = p.posHistory.front().pos;
    return true;
}

SnapshotEntity makePlayerEntity(const ServerPlayer& player)
{
    SnapshotEntity out{};
    out.networkEntityId = player.id;
    out.entityType = ENTITY_PLAYER;
    out.active = 1;
    out.ownerClientId = player.id;
    out.px = player.pos.x; out.py = player.pos.y; out.pz = player.pos.z;
    out.vx = player.vel.x; out.vy = player.vel.y; out.vz = player.vel.z;
    out.yaw = player.yaw;
    out.health = player.health;
    out.onGround = player.onGround ? 1 : 0;
    out.equippedSlot = (int16_t)player.equippedSlot;
    out.weaponState = player.weaponState;
    out.lastDashSerial = player.lastDashSerial;
    out.transformEpoch = player.transformEpoch;
    out.aimX = player.input.camForward.x;
    out.aimY = player.input.camForward.y;
    out.aimZ = player.input.camForward.z;
    out.pingMs = player.pingMs;
    out.sizeScale = player.sizeScale;
    copyName(out.displayName, player.name);
    return out;
}

} // namespace MimitaNet
