// 2026-10-29
/* purpose
* Implements the shared server contact validation and application path.
* Owns victim lookup, lifecycle/generation checks, shared body-volume geometry,
* reach/occlusion checks, duplicate suppression, and the damage sink.
* Does NOT own weapon-specific traces or render/packet presentation.
*/

#include "network/server-contact.h"

#include "combat/projectile-simulation.h"
#include "network/server-gamemode.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <unordered_set>
#include <vector>

namespace MimitaNet {
namespace {

constexpr int kMaxBodyBoxes = 16;
constexpr size_t kMaxDedupeEntries = 8192;

struct BodyBox
{
    glm::vec3 center{0.0f};
    glm::vec3 half{0.0f};
};

struct ActorPose
{
    glm::vec3 pos{0.0f};
    float yaw = 0.0f;
};

bool finiteVec(const glm::vec3& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

ActorPose actorPose(const ServerPlayer& player, uint32_t tick)
{
    ActorPose pose{player.pos, player.yaw};
    if (tick != 0)
        getPlayerPoseAtTick(player, tick, pose.pos, pose.yaw);
    return pose;
}

ActorPose actorPose(const ServerNpc& npc, uint32_t tick)
{
    ActorPose pose{npc.pos, npc.yaw};
    if (tick != 0)
        getNpcPoseAtTick(npc, tick, pose.pos, pose.yaw);
    return pose;
}

int collectBodyBoxes(const ServerPlayer& player, uint32_t tick,
                     std::array<BodyBox, kMaxBodyBoxes>& out)
{
    int count = 0;
    RagdollReplicationPose ragdoll;
    if (getPlayerRagdollPoseAtTick(player, tick, ragdoll) &&
        ragdoll.active && ragdoll.count > 0)
    {
        for (int i = 0; i < ragdoll.count && i < kRagdollLimbCount &&
                        count < kMaxBodyBoxes; ++i)
        {
            const RagdollLimbState& limb = ragdoll.limbs[i];
            if (glm::length(limb.hitHalf) <= 1e-5f)
                continue;
            out[count++] = {limb.hitCenter, limb.hitHalf};
        }
        if (count > 0)
            return count;
    }

    const auto* tpl = standardPlayerBodyTemplate();
    if (!tpl)
        return 0;

    const ActorPose pose = actorPose(player, tick);
    const float c = std::cos(pose.yaw);
    const float s = std::sin(pose.yaw);
    for (const auto& part : *tpl)
    {
        if (count >= kMaxBodyBoxes)
            break;
        const glm::vec3 offset(
            part.offset.x * c - part.offset.y * s,
            part.offset.x * s + part.offset.y * c,
            part.offset.z);
        out[count++] = {pose.pos + offset, part.half};
    }
    return count;
}

int collectBodyBoxes(const ServerNpc& npc, uint32_t tick,
                     std::array<BodyBox, kMaxBodyBoxes>& out)
{
    int count = 0;
    if (tick != 0)
    {
        const ServerNpcBodyPartSample* parts = nullptr;
        uint8_t partCount = 0;
        if (getNpcBodyPartsAtTick(npc, tick, &parts, &partCount))
        {
            for (int i = 0; i < partCount && count < kMaxBodyBoxes; ++i)
            {
                out[count++] = {
                    {parts[i].cx, parts[i].cy, parts[i].cz},
                    {parts[i].hx, parts[i].hy, parts[i].hz}};
            }
            if (count > 0)
                return count;
        }
    }

    for (uint8_t i = 0; i < npc.bodyPartCount && i < npc.bodyParts.size() &&
                        count < kMaxBodyBoxes; ++i)
    {
        const ServerNpcBodyPartSample& part = npc.bodyParts[i];
        out[count++] = {
            {part.cx, part.cy, part.cz},
            {part.hx, part.hy, part.hz}};
    }
    return count;
}

void actorCapsule(const ServerPlayer& player, uint32_t tick,
                  glm::vec3& a, glm::vec3& b, float& radius)
{
    const ActorPose pose = actorPose(player, tick);
    a = pose.pos + glm::vec3(0.0f, 0.0f, -PLAYER_HEIGHT * 0.5f + PLAYER_RADIUS);
    b = pose.pos + glm::vec3(0.0f, 0.0f, PLAYER_HEIGHT * 0.5f - PLAYER_RADIUS);
    radius = PLAYER_RADIUS;
}

void actorCapsule(const ServerNpc& npc, uint32_t tick,
                  glm::vec3& a, glm::vec3& b, float& radius)
{
    const ActorPose pose = actorPose(npc, tick);
    a = pose.pos + glm::vec3(0.0f, 0.0f, -PLAYER_HEIGHT * 0.5f + PLAYER_RADIUS);
    b = pose.pos + glm::vec3(0.0f, 0.0f, PLAYER_HEIGHT * 0.5f - PLAYER_RADIUS);
    radius = PLAYER_RADIUS;
}

glm::vec3 closestPointOnAabb(const glm::vec3& p, const BodyBox& box)
{
    return glm::clamp(p, box.center - box.half, box.center + box.half);
}

glm::vec3 closestPointOnSegment(const glm::vec3& p, const glm::vec3& a,
                                const glm::vec3& b)
{
    const glm::vec3 ab = b - a;
    const float lenSq = glm::dot(ab, ab);
    if (lenSq <= 1e-8f)
        return a;
    const float t = glm::clamp(glm::dot(p - a, ab) / lenSq, 0.0f, 1.0f);
    return a + ab * t;
}

bool pointInExpandedAabb(const glm::vec3& p, const BodyBox& box, float tolerance)
{
    const glm::vec3 half = box.half + glm::vec3(std::max(0.0f, tolerance));
    return p.x >= box.center.x - half.x && p.x <= box.center.x + half.x &&
           p.y >= box.center.y - half.y && p.y <= box.center.y + half.y &&
           p.z >= box.center.z - half.z && p.z <= box.center.z + half.z;
}

bool pointInCapsule(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b,
                    float radius, float tolerance)
{
    const glm::vec3 closest = closestPointOnSegment(p, a, b);
    return glm::length(p - closest) <= radius + std::max(0.0f, tolerance);
}

template <typename Actor>
bool nearestBodyPoint(const Actor& actor, uint32_t tick, const glm::vec3& from,
                      glm::vec3& outPoint)
{
    std::array<BodyBox, kMaxBodyBoxes> boxes{};
    const int count = collectBodyBoxes(actor, tick, boxes);
    if (count > 0)
    {
        float best = std::numeric_limits<float>::max();
        for (int i = 0; i < count; ++i)
        {
            const glm::vec3 point = closestPointOnAabb(from, boxes[i]);
            const float distSq = glm::dot(point - from, point - from);
            if (distSq < best)
            {
                best = distSq;
                outPoint = point;
            }
        }
        return true;
    }

    glm::vec3 a{0.0f}, b{0.0f};
    float radius = 0.0f;
    actorCapsule(actor, tick, a, b, radius);
    const glm::vec3 closest = closestPointOnSegment(from, a, b);
    const glm::vec3 delta = from - closest;
    const float len = glm::length(delta);
    outPoint = len > 1e-4f ? closest + delta / len * radius : closest;
    return true;
}

template <typename Actor>
bool actorHitVolumeContains(const Actor& actor, uint32_t tick,
                            const glm::vec3& point, float tolerance)
{
    std::array<BodyBox, kMaxBodyBoxes> boxes{};
    const int count = collectBodyBoxes(actor, tick, boxes);
    for (int i = 0; i < count; ++i)
    {
        if (pointInExpandedAabb(point, boxes[i], tolerance))
            return true;
    }

    glm::vec3 a{0.0f}, b{0.0f};
    float radius = 0.0f;
    actorCapsule(actor, tick, a, b, radius);
    return pointInCapsule(point, a, b, radius, tolerance);
}

bool contactSegmentBlocked(const HeadlessWorld& world,
                           const glm::vec3& origin,
                           const glm::vec3& point)
{
    const glm::vec3 delta = point - origin;
    const float maxDist = glm::length(delta);
    if (maxDist < 0.05f)
        return false;

    const glm::vec3 dir = delta / maxDist;
    AABB rayBounds;
    rayBounds.min = glm::min(origin, point);
    rayBounds.max = glm::max(origin, point);
    std::vector<int> candidates;
    gatherHeadlessTrianglesForAABB(world, rayBounds, 0.05f, candidates);
    return splashRayBlockedByWall(origin, dir, maxDist, candidates,
                                  world.triangles);
}

uint64_t contactDedupeKey(const ServerContactRequest& request)
{
    uint64_t key = 1469598103934665603ull;
    auto mix = [&key](uint64_t value) {
        key ^= value + 0x9e3779b97f4a7c15ull + (key << 6) + (key >> 2);
    };
    mix(request.attacker.id);
    mix(static_cast<uint64_t>(request.attacker.kind));
    mix(request.victimId);
    mix(static_cast<uint64_t>(request.victimKind));
    mix(static_cast<uint64_t>(request.kind));
    mix(request.contactTick != 0 ? request.contactTick : request.serverTick);
    mix(request.contactId);
    return key;
}

bool duplicateContact(uint64_t key)
{
    static std::unordered_set<uint64_t> seen;
    static std::deque<uint64_t> order;
    if (seen.find(key) != seen.end())
        return true;
    seen.insert(key);
    order.push_back(key);
    while (order.size() > kMaxDedupeEntries)
    {
        seen.erase(order.front());
        order.pop_front();
    }
    return false;
}

} // namespace

bool nearestServerActorBodyPoint(
    const std::unordered_map<uint32_t, ServerPlayer>& players,
    const std::unordered_map<uint32_t, ServerNpc>& npcs,
    uint32_t victimId,
    ServerActorKind victimKind,
    const glm::vec3& from,
    uint32_t contactTick,
    glm::vec3& outPoint)
{
    if (victimKind == ServerActorKind::Npc)
    {
        auto it = npcs.find(victimId);
        if (it == npcs.end())
            return false;
        return nearestBodyPoint(it->second, contactTick, from, outPoint);
    }

    auto it = players.find(victimId);
    if (it == players.end())
        return false;
    return nearestBodyPoint(it->second, contactTick, from, outPoint);
}

ServerContactResult resolveServerContact(
    std::unordered_map<uint32_t, ServerPlayer>& players,
    std::unordered_map<uint32_t, ServerNpc>& npcs,
    const ServerContactRequest& request)
{
    ServerContactResult out;
    if (request.attacker.id == 0)
    {
        out.rejectionReason = "attacker-missing";
        return out;
    }
    if (request.victimId == 0 || request.damage <= 0)
    {
        out.rejectionReason = "victim-or-damage-invalid";
        return out;
    }

    ServerActorRef victim{};
    uint32_t serverGeneration = 0;
    bool alive = false;
    if (request.victimKind == ServerActorKind::Npc)
    {
        auto it = npcs.find(request.victimId);
        if (it == npcs.end())
        {
            out.rejectionReason = "victim-not-found";
            return out;
        }
        victim = {it->first, ServerActorKind::Npc, nullptr, &it->second};
        serverGeneration = it->second.transformEpoch;
        alive = it->second.health > 0;
    }
    else
    {
        auto it = players.find(request.victimId);
        if (it == players.end())
        {
            out.rejectionReason = "victim-not-found";
            return out;
        }
        victim = {it->first, ServerActorKind::Player, &it->second, nullptr};
        serverGeneration = it->second.spawnGeneration;
        alive = !it->second.dead && it->second.spawnState == ServerPlayer::Active;
    }

    if (!alive)
    {
        out.rejectionReason = "victim-inactive";
        return out;
    }
    if (request.victimGeneration != 0 &&
        request.victimGeneration != serverGeneration)
    {
        out.rejectionReason = "victim-generation-changed";
        return out;
    }
    if (!request.allowSelfDamage &&
        request.attacker.id == request.victimId &&
        request.attacker.kind == request.victimKind)
    {
        out.rejectionReason = "self-damage";
        return out;
    }

    const uint32_t contactTick =
        request.contactTick != 0 ? request.contactTick : request.serverTick;
    if (request.contactTick != 0 && request.serverTick != 0)
    {
        if (contactTick > request.serverTick + 2)
        {
            out.rejectionReason = "future-contact";
            return out;
        }
        if (request.maxContactAgeTicks > 0 &&
            request.serverTick > contactTick &&
            request.serverTick - contactTick > request.maxContactAgeTicks)
        {
            out.rejectionReason = "stale-contact";
            return out;
        }
    }

    if (serverFriendlyFireBlocks(
            request.attacker.id,
            request.attacker.kind == ServerActorKind::Npc,
            request.victimId,
            request.victimKind == ServerActorKind::Npc,
            players, npcs, "server-contact", request.serverTick))
    {
        out.rejectionReason = "friendly-fire";
        return out;
    }

    if (!finiteVec(request.point))
    {
        out.rejectionReason = "malformed-contact-point";
        return out;
    }
    if (request.maxDistance > 0.0f)
    {
        const glm::vec3 delta = request.point - request.origin;
        if (!finiteVec(delta) ||
            glm::length(delta) >
                request.maxDistance + std::max(0.0f, request.hitTolerance))
        {
            out.rejectionReason = "out-of-reach";
            return out;
        }
    }
    if (request.validateHitVolume)
    {
        const bool inside = request.victimKind == ServerActorKind::Npc
            ? actorHitVolumeContains(*victim.npc, contactTick, request.point,
                                     request.hitTolerance)
            : actorHitVolumeContains(*victim.player, contactTick, request.point,
                                     request.hitTolerance);
        if (!inside)
        {
            out.rejectionReason = "outside-hit-volume";
            return out;
        }
    }
    if (request.requireWorldLineOfSight && request.world &&
        contactSegmentBlocked(*request.world, request.origin, request.point))
    {
        out.rejectionReason = "world-occluded";
        return out;
    }
    if (request.dedupe && request.contactId != 0 &&
        duplicateContact(contactDedupeKey(request)))
    {
        out.rejectionReason = "duplicate-contact";
        return out;
    }

    ServerActorDamageRequest damageRequest;
    damageRequest.attacker = request.attacker;
    damageRequest.victim = victim;
    damageRequest.damage = request.damage;
    damageRequest.knockback = request.knockback;
    damageRequest.source = request.source;
    damageRequest.weaponId = request.weaponId;
    damageRequest.weaponDisplayName = request.weaponDisplayName;
    damageRequest.serverTick = request.serverTick;
    damageRequest.eventId = request.eventId;
    damageRequest.correlationId = request.contactId;

    out.damage = applyActorDamage(players, npcs, damageRequest);
    out.accepted = out.damage.applied;
    if (!out.accepted)
    {
        out.rejectionReason = out.damage.rejectionReason.empty()
            ? "damage-rejected" : out.damage.rejectionReason.c_str();
    }
    return out;
}

} // namespace MimitaNet
