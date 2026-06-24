#include "debug/debug-visuals.h"

#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "camera.h"
#include "physics/config.h"
#include "entities/player.h"
#include "renderer/renderer.h"
#include "world/world.h"
#include "debug/debug-log.h"
#include "config.h"

extern Renderer* gRenderer;

extern void flushDebugTris(const Camera& camera);
extern void flushDebugLines(const Camera& camera);
extern void drawDebugLabels(const Camera& camera);
extern void drawLine(const Camera& camera, glm::vec3 a, glm::vec3 b, glm::vec4 color);
extern void drawPointCross(const Camera& camera, glm::vec3 p, float size, glm::vec4 color);
extern void drawBox(const Camera& camera, glm::vec3 center, glm::vec3 half, glm::vec4 color);
extern void drawOrientedBounds(const Camera& camera, const glm::mat4& transform, glm::vec3 localMin, glm::vec3 localMax, glm::vec4 color);
extern void drawWireSphere(const Camera& camera, glm::vec3 center, float radius, glm::vec4 color);
extern void drawWorldLabel(glm::vec3 worldPos, const char* text, glm::vec4 color);
extern bool projectToScreen(const Camera& camera, glm::vec3 worldPos, float& x, float& y);
extern void drawCapsuleApprox(const Player& player, const Camera& camera);
extern void drawPlayerArchitectureDebug(const Player& player, const Camera& camera);
extern void drawCollisionEvents(const Camera& camera);
extern void drawCapsuleWire(const Camera& camera, const Capsule& c, glm::vec4 color);
extern void drawTransformAxes(const Camera& camera, const glm::mat4& transform, float scale);

void drawDebugStuff(const Player& player, const Camera& camera, const World& world)
{
    if (!DebugVis::enabled()) return;

    if (DebugVis::physics()) {
        drawCapsuleApprox(player, camera);
        drawLine(camera, player.pos, player.pos + player.vel * 0.25f, {0.0f,1.0f,0.2f,1.0f});
        drawLine(camera, player.pos, player.pos + glm::vec3(0,0,-3), {0.2f,0.5f,1.0f,1.0f});
        drawLine(camera, player.pos, player.pos + glm::vec3(0,0,2), {1.0f,1.0f,0.0f,1.0f});
        for (const Collider& collider : player.bodyColliders) {
            auto it = std::find_if(player.nodes.begin(), player.nodes.end(), [&](const TransformNode& node) {
                return node.name == collider.name;
            });
            if (it != player.nodes.end())
            {
                drawOrientedBounds(camera, it->worldTransform, collider.localMin, collider.localMax, {1.0f,0.2f,0.9f,0.85f});
                glm::vec3 origin = glm::vec3(it->worldTransform[3]);
                glm::vec3 xAxis = glm::normalize(glm::vec3(it->worldTransform[0])) * 0.25f;
                glm::vec3 yAxis = glm::normalize(glm::vec3(it->worldTransform[1])) * 0.25f;
                glm::vec3 zAxis = glm::normalize(glm::vec3(it->worldTransform[2])) * 0.25f;
                drawLine(camera, origin, origin + xAxis, {1.0f,0.1f,0.1f,1.0f});
                drawLine(camera, origin, origin + yAxis, {0.1f,1.0f,0.1f,1.0f});
                drawLine(camera, origin, origin + zAxis, {0.1f,0.4f,1.0f,1.0f});
            }
        }
    }

    if (DebugVis::playerArchitecture()) {
        drawPlayerArchitectureDebug(player, camera);
    }

    if (DebugVis::collision()) {
        drawCollisionEvents(camera);
    }

    if (DebugConfig::DEBUG_COLLISION_PLAYER) {
        Capsule cap = player.getCapsule();
        glm::vec4 capColor = player.ground.stableOnGround
            ? glm::vec4(0.0f, 1.0f, 0.0f, 0.6f)
            : glm::vec4(1.0f, 0.3f, 0.0f, 0.6f);
        drawWireSphere(camera, cap.a, cap.r, capColor);
        drawWireSphere(camera, cap.b, cap.r, capColor);
        for (int i = 0; i <= 8; i++) {
            float t = (float)i / 8.0f;
            glm::vec3 pA = cap.a + glm::vec3(cos(t * 6.2832f) * cap.r, sin(t * 6.2832f) * cap.r, 0.0f);
            glm::vec3 pB = cap.b + glm::vec3(cos(t * 6.2832f) * cap.r, sin(t * 6.2832f) * cap.r, 0.0f);
            drawLine(camera, pA, pB, capColor);
        }

        char info[128];
        if (player.ground.stableOnGround) {
            snprintf(info, sizeof(info), "GROUNDED stable=%d lostTimer=%.3f",
                     (int)player.ground.onGround, player.ground.groundLostTimer);
        } else {
            snprintf(info, sizeof(info), "AIR vel.z=%.2f",
                     player.vel.z);
        }
        drawWorldLabel(player.pos + glm::vec3(0, 0, PLAYER_HEIGHT + 0.8f), info, capColor);
    }

    if (DebugConfig::DEBUG_COLLISION_LIMB) {
        const glm::vec4 limbColor{0.8f, 0.4f, 0.1f, 0.8f};
        for (const Collider& collider : player.bodyColliders)
        {
            auto it = std::find_if(player.nodes.begin(), player.nodes.end(), [&](const TransformNode& node) {
                return node.name == collider.name;
            });
            if (it == player.nodes.end()) continue;

            const glm::mat4& xform = it->worldTransform;
            glm::vec3 localCenter = (collider.localMin + collider.localMax) * 0.5f;
            glm::vec3 localExtents = (collider.localMax - collider.localMin) * 0.5f;
            float axisLen = glm::length(localExtents);
            glm::vec3 axisDir(0.0f, 0.0f, 1.0f);
            if (axisLen > 0.001f)
                axisDir = localExtents / axisLen;
            glm::vec3 worldCenter = glm::vec3(xform * glm::vec4(localCenter, 1.0f));
            float radius = std::min(localExtents.x, localExtents.y) * 1.5f;
            radius = std::max(radius, BODY_SAMPLE_RADIUS);
            radius = std::min(radius, 0.35f);
            glm::vec3 worldA = worldCenter - glm::vec3(xform * glm::vec4(axisDir * axisLen, 0.0f));
            glm::vec3 worldB = worldCenter + glm::vec3(xform * glm::vec4(axisDir * axisLen, 0.0f));

            if (glm::length(worldB - worldA) > 0.001f) {
                drawWireSphere(camera, worldA, radius, limbColor);
                drawWireSphere(camera, worldB, radius, limbColor);
                for (int i = 0; i <= 6; i++) {
                    float t2 = (float)i / 6.0f;
                    glm::vec3 pA = worldA + glm::vec3(cos(t2 * 6.2832f) * radius, sin(t2 * 6.2832f) * radius, 0.0f);
                    glm::vec3 pB = worldB + glm::vec3(cos(t2 * 6.2832f) * radius, sin(t2 * 6.2832f) * radius, 0.0f);
                    drawLine(camera, pA, pB, limbColor);
                }
            } else {
                drawWireSphere(camera, worldCenter, radius, limbColor);
            }
            drawWorldLabel(worldCenter + glm::vec3(0.0f, 0.0f, radius + 0.3f),
                          collider.name.c_str(), limbColor);
        }

        if (player.collision.hasWeaponCollisionCapsule) {
            const glm::vec4 weaponColor{1.0f, 0.85f, 0.15f, 0.9f};
            const Capsule& weaponCap = player.weaponCollisionCapsule;
            drawCapsuleWire(camera, weaponCap, weaponColor);
            glm::vec3 center = (weaponCap.a + weaponCap.b) * 0.5f;
            drawWorldLabel(center + glm::vec3(0.0f, 0.0f, weaponCap.r + 0.3f),
                           player.weaponCollisionName.empty()
                               ? "weapon"
                               : player.weaponCollisionName.c_str(),
                           weaponColor);
        }

        if (DebugConfig::DEBUG_COLLISION_BODY_PUSH)
        {
            glm::vec3 bodyPush = player.debugBodyCollisionPush;
            float bodyPushLen = glm::length(bodyPush);
            if (bodyPushLen > 0.001f) {
                glm::vec3 start = player.pos + glm::vec3(0.0f, 0.0f, PLAYER_HEIGHT * 0.5f);
                glm::vec3 end = start + bodyPush * (1.0f / bodyPushLen) * std::min(bodyPushLen * 5.0f, 2.0f);
                drawLine(camera, start, end, glm::vec4(1.0f, 0.3f, 0.1f, 0.9f));
                drawPointCross(camera, end, 0.1f, glm::vec4(1.0f, 0.3f, 0.1f, 0.9f));
            }

            glm::vec3 weaponPush = player.debugWeaponCollisionPush;
            float weaponPushLen = glm::length(weaponPush);
            if (weaponPushLen > 0.001f) {
                glm::vec3 start = player.pos + glm::vec3(0.0f, 0.0f, PLAYER_HEIGHT * 0.5f);
                glm::vec3 end = start + weaponPush * (1.0f / weaponPushLen) * std::min(weaponPushLen * 5.0f, 2.0f);
                drawLine(camera, start, end, glm::vec4(0.85f, 1.0f, 0.15f, 0.9f));
                drawPointCross(camera, end, 0.1f, glm::vec4(0.85f, 1.0f, 0.15f, 0.9f));
            }
        }
    }

    if (DebugConfig::DEBUG_COLLISION_SYSTEM) {
        drawCollisionEvents(camera);
    }

    if (DebugConfig::DEBUG_COLLISION_GRID) {
        int drawn = 0;
        int capped = 0;
        for (const CollisionTriangle& tri : world.collisionMesh.triangles) {
            drawLine(camera, tri.a, tri.b, {1.0f, 0.3f, 0.0f, 0.55f});
            drawLine(camera, tri.b, tri.c, {1.0f, 0.3f, 0.0f, 0.55f});
            drawLine(camera, tri.c, tri.a, {1.0f, 0.3f, 0.0f, 0.55f});
            if (++drawn >= 8192) { capped = 1; break; }
        }
        if (capped) {
            char msg[128];
            snprintf(msg, sizeof(msg), "COLLISION TRIANGLE DRAW CAPPED at %d (total %zu)", drawn, world.collisionMesh.triangles.size());
            drawWorldLabel(player.pos + glm::vec3(0.0f, 0.0f, PLAYER_HEIGHT + 1.2f), msg, {1.0f, 0.5f, 0.0f, 1.0f});
        }
    }

    if (DebugVis::render()) {
        drawLine(camera, camera.pos, camera.pos + camera.front * 5.0f, {0.2f,0.8f,1.0f,1.0f});

        for (size_t i = 0; i < world.spawnPoints.size(); ++i)
        {
            const SpawnPoint& spawn = world.spawnPoints[i];
            const bool selected = (int)i == world.selectedSpawnIndex;
            const glm::vec4 color = selected
                ? glm::vec4(1.0f, 0.85f, 0.1f, 1.0f)
                : glm::vec4(0.2f, 1.0f, 0.55f, 1.0f);
            const float radius = selected ? 0.55f : 0.35f;

            DebugVis::drawWireSphere(camera, spawn.position, radius, color);
            DebugVis::drawPointCross(camera, spawn.position, radius * 1.4f, color);
            DebugVis::drawLine(
                camera, spawn.position,
                spawn.position + glm::vec3(0.0f, 0.0f, 1.5f), color);

            const glm::vec3 forward = spawn.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
            DebugVis::drawLine(
                camera, spawn.position,
                spawn.position + forward * 1.2f,
                {0.25f, 0.65f, 1.0f, 1.0f});

            char label[192];
            snprintf(label, sizeof(label),
                     "SPAWN %zu%s %s (%.2f %.2f %.2f)",
                     i, selected ? " SELECTED" : "", spawn.tag.c_str(),
                     spawn.position.x, spawn.position.y, spawn.position.z);
            DebugVis::drawWorldLabel(
                spawn.position + glm::vec3(0.0f, 0.0f, 1.7f),
                label, color);
        }
    }

    if (DebugVis::bounds()) {
        drawBox(camera, player.pos + glm::vec3(0,0,1), {0.55f,0.55f,1.0f}, {1.0f,0.0f,1.0f,1.0f});
        int drawn = 0;
        if (!world.collisionMesh.empty()) {
            if (!world.collisionChunks.empty() && world.collisionChunkSize > 0.001f) {
                glm::ivec3 pc(
                    (int)std::floor(player.pos.x / world.collisionChunkSize),
                    (int)std::floor(player.pos.y / world.collisionChunkSize),
                    (int)std::floor(player.pos.z / world.collisionChunkSize)

                );
                int chunkDrawn = 0;

                for (int x = pc.x - 1; x <= pc.x + 1; ++x)
                for (int y = pc.y - 1; y <= pc.y + 1; ++y)
                for (int z = pc.z - 1; z <= pc.z + 1; ++z) {
                    if (world.collisionChunks.find(glm::ivec3(x, y, z)) == world.collisionChunks.end())
                        continue;
                    glm::vec3 center = (glm::vec3(x, y, z) + glm::vec3(0.5f)) * world.collisionChunkSize;
                    drawBox(camera, center, glm::vec3(world.collisionChunkSize * 0.5f), {0.0f,1.0f,0.3f,0.35f});
                    if (++chunkDrawn >= 16)
                        break;
                }
            }
            for (const CollisionTriangle& tri : world.collisionMesh.triangles) {
                drawLine(camera, tri.a, tri.b, {1.0f,0.85f,0.0f,0.65f});
                drawLine(camera, tri.b, tri.c, {1.0f,0.85f,0.0f,0.65f});
                drawLine(camera, tri.c, tri.a, {1.0f,0.85f,0.0f,0.65f});

                if (DebugVis::normals()) {
                    glm::vec3 center = (tri.a + tri.b + tri.c) / 3.0f;
                    drawLine(camera, center, center + tri.normal * 0.35f, {0.2f,1.0f,1.0f,0.8f});
                }
                if (++drawn >= 96) break;
            }
        } else {
            for (const Block& b : world.blocks) {
                drawBox(camera, b.pos, b.size * 0.5f, {1.0f,1.0f,0.0f,0.85f});
                if (++drawn >= 24) break;
            }
        }
    }

    if (gRenderer && gRenderer->shaderProgram) {
        glUseProgram(gRenderer->shaderProgram);

        glUniform1i(glGetUniformLocation(gRenderer->shaderProgram, "uUseColor"), 0);
    }
    flushDebugTris(camera);
    flushDebugLines(camera);
    drawDebugLabels(camera);
}

namespace DebugVis {

void drawNpcDebugStuff(const std::vector<NpcDebugInfo>& npcs,
                             const Camera& camera)
{
    if (!DebugVis::masterEnabled() || !DebugConfig::DEBUG_NPC)
        return;

    const glm::vec4 awarenessColor{1.0f, 0.35f, 0.05f, 0.28f};
    const glm::vec4 moveColor{0.15f, 1.0f, 0.35f, 0.95f};
    const glm::vec4 targetColor{1.0f, 0.12f, 0.12f, 0.9f};
    const glm::vec4 pathColor{0.2f, 0.6f, 1.0f, 0.95f};
    const glm::vec4 groundedColor{0.1f, 1.0f, 0.25f, 1.0f};
    const glm::vec4 airborneColor{1.0f, 0.25f, 0.1f, 1.0f};

    for (const NpcDebugInfo& npc : npcs)
    {
        ::drawWireSphere(camera, npc.position, npc.awarenessRadius, awarenessColor);
        ::drawLine(camera, npc.position, npc.position + npc.velocity * 0.20f, {0.0f, 1.0f, 1.0f, 0.9f});
        ::drawLine(camera, npc.position, npc.position + npc.acceleration * 0.02f, {1.0f, 0.2f, 1.0f, 0.9f});
        ::drawLine(camera, npc.position, npc.position + npc.moveDirection * 2.0f, moveColor);
        ::drawLine(camera, npc.position, npc.pathTarget, pathColor);
        ::drawWireSphere(
            camera,
            npc.position + glm::vec3(0.0f, 0.0f, 0.15f),
            0.18f,
            npc.grounded ? groundedColor : airborneColor
        );

        if (npc.hasTarget)
            ::drawLine(camera, npc.position + glm::vec3(0.0f, 0.0f, 1.2f),
                     npc.targetPosition + glm::vec3(0.0f, 0.0f, 1.2f),
                     targetColor);

        char label[160];
        snprintf(label, sizeof(label), "NPC d=%.1f %s speed=%.1f %s",
                 npc.difficulty,
                 npc.action.c_str(),
                 npc.finalSpeed,
                 npc.grounded ? "grounded" : "airborne");
        ::drawWorldLabel(
            npc.position + glm::vec3(0.0f, 0.0f, 2.25f),
            label,
            npc.grounded ? groundedColor : airborneColor
        );
    }
}

} // namespace DebugVis
