#include "network/net_mode.h"

#include "network/net_common.h"
#include "network/packets.h"
#include "physics/physics-types.h"
#include "utils/path_utils.h"
#include "tinygltf/tiny_gltf.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <thread>
#include <unordered_map>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace MimitaNet {
namespace {

constexpr float SERVER_TICK_RATE = 60.0f;
constexpr float SERVER_DT = 1.0f / SERVER_TICK_RATE;
constexpr uint64_t CLIENT_TIMEOUT_MS = 5000;
constexpr float PLAYER_RADIUS = 0.65f;
constexpr float PLAYER_HEIGHT = 3.5f;

struct HeadlessWorld
{
    std::vector<CollisionTriangle> triangles;
    glm::vec3 boundsMin{0.0f};
    glm::vec3 boundsMax{0.0f};
};

struct ServerInput
{
    glm::vec2 wish{0.0f};
    glm::vec3 camForward{1.0f, 0.0f, 0.0f};
    float yaw = 0.0f;
    bool jumpHeld = false;
    bool dashPressed = false;
    bool attackPressed = false;
    bool freezeHeld = false;
    uint32_t tick = 0;
};

struct ServerPlayer
{
    uint32_t id = 0;
    std::string name;
    sockaddr_in addr{};
    glm::vec3 pos{0.0f};
    glm::vec3 vel{0.0f};
    float yaw = 0.0f;
    int health = 100;
    bool onGround = false;
    bool dashAvailable = true;
    bool attackQueued = false;
    bool dead = false;
    float respawnSeconds = 0.0f;
    uint64_t lastHeardMs = 0;
    ServerInput input;
};

struct ServerNpc
{
    uint32_t entityId = 0;
    std::string name;
    glm::vec3 pos{0.0f};
    glm::vec3 vel{0.0f};
    float yaw = 0.0f;
    int health = 100;
    bool onGround = false;
    float phase = 0.0f;
};

static char gTimestampBuf[64];

static const char* serverTimestamp()
{
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    snprintf(gTimestampBuf, sizeof(gTimestampBuf), "[%02d:%02d:%02d]",
             t->tm_hour, t->tm_min, t->tm_sec);
    return gTimestampBuf;
}

bool sameAddress(const sockaddr_in& a, const sockaddr_in& b)
{
    return a.sin_addr.s_addr == b.sin_addr.s_addr && a.sin_port == b.sin_port;
}

const unsigned char* accessorPtr(const tinygltf::Model& model, const tinygltf::Accessor& accessor)
{
    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buffer = model.buffers[view.buffer];
    return buffer.data.data() + view.byteOffset + accessor.byteOffset;
}

size_t accessorStride(const tinygltf::Model& model, const tinygltf::Accessor& accessor)
{
    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
    size_t stride = accessor.ByteStride(view);
    if (stride)
        return stride;
    return (size_t)tinygltf::GetComponentSizeInBytes(accessor.componentType) *
           (size_t)tinygltf::GetNumComponentsInType(accessor.type);
}

glm::vec3 readVec3(const tinygltf::Model& model, const tinygltf::Accessor& accessor, size_t index)
{
    const unsigned char* base = accessorPtr(model, accessor);
    const float* f = reinterpret_cast<const float*>(base + index * accessorStride(model, accessor));
    return {f[0], f[1], f[2]};
}

bool readIndex(const tinygltf::Model& model, const tinygltf::Accessor& accessor, size_t index, unsigned int& out)
{
    const unsigned char* base = accessorPtr(model, accessor);
    const unsigned char* p = base + index * accessorStride(model, accessor);
    switch (accessor.componentType)
    {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: out = *reinterpret_cast<const unsigned char*>(p); return true;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: out = *reinterpret_cast<const unsigned short*>(p); return true;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: out = *reinterpret_cast<const unsigned int*>(p); return true;
        default: return false;
    }
}

glm::mat4 nodeTransform(const tinygltf::Node& node)
{
    if (node.matrix.size() == 16)
    {
        glm::mat4 out(1.0f);
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                out[c][r] = (float)node.matrix[c * 4 + r];
        return out;
    }

    glm::vec3 t(0.0f);
    if (node.translation.size() == 3)
        t = {(float)node.translation[0], (float)node.translation[1], (float)node.translation[2]};
    glm::quat q(1, 0, 0, 0);
    if (node.rotation.size() == 4)
        q = glm::quat((float)node.rotation[3], (float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2]);
    glm::vec3 s(1.0f);
    if (node.scale.size() == 3)
        s = {(float)node.scale[0], (float)node.scale[1], (float)node.scale[2]};
    return glm::translate(glm::mat4(1.0f), t) * glm::mat4_cast(q) * glm::scale(glm::mat4(1.0f), s);
}

void addTriangle(HeadlessWorld& world, glm::vec3 a, glm::vec3 b, glm::vec3 c)
{
    glm::vec3 n = glm::cross(b - a, c - a);
    float len = glm::length(n);
    if (len < 0.000001f)
        return;

    CollisionTriangle tri;
    tri.a = a;
    tri.b = b;
    tri.c = c;
    tri.normal = n / len;
    world.triangles.push_back(tri);

    glm::vec3 mn = glm::min(glm::min(a, b), c);
    glm::vec3 mx = glm::max(glm::max(a, b), c);
    if (world.triangles.size() == 1)
    {
        world.boundsMin = mn;
        world.boundsMax = mx;
    }
    else
    {
        world.boundsMin = glm::min(world.boundsMin, mn);
        world.boundsMax = glm::max(world.boundsMax, mx);
    }
}

void appendPrimitive(const tinygltf::Model& model, const tinygltf::Primitive& prim, const glm::mat4& transform, HeadlessWorld& world)
{
    if (prim.mode != TINYGLTF_MODE_TRIANGLES)
        return;
    auto posIt = prim.attributes.find("POSITION");
    if (posIt == prim.attributes.end())
        return;

    const tinygltf::Accessor& pos = model.accessors[posIt->second];
    if (pos.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || pos.type != TINYGLTF_TYPE_VEC3 || pos.bufferView < 0)
        return;

    auto vertexAt = [&](unsigned int i) {
        return glm::vec3(transform * glm::vec4(readVec3(model, pos, i), 1.0f));
    };

    if (prim.indices >= 0)
    {
        const tinygltf::Accessor& idx = model.accessors[prim.indices];
        for (size_t i = 0; i + 2 < idx.count; i += 3)
        {
            unsigned int ia = 0, ib = 0, ic = 0;
            if (readIndex(model, idx, i + 0, ia) && readIndex(model, idx, i + 1, ib) && readIndex(model, idx, i + 2, ic) &&
                ia < pos.count && ib < pos.count && ic < pos.count)
                addTriangle(world, vertexAt(ia), vertexAt(ib), vertexAt(ic));
        }
    }
    else
    {
        for (size_t i = 0; i + 2 < pos.count; i += 3)
            addTriangle(world, vertexAt((unsigned)i), vertexAt((unsigned)i + 1), vertexAt((unsigned)i + 2));
    }
}

void walkNode(const tinygltf::Model& model, int nodeIndex, const glm::mat4& parent, HeadlessWorld& world)
{
    if (nodeIndex < 0 || nodeIndex >= (int)model.nodes.size())
        return;
    const tinygltf::Node& node = model.nodes[nodeIndex];
    glm::mat4 transform = parent * nodeTransform(node);

    if (node.mesh >= 0 && node.mesh < (int)model.meshes.size())
        for (const tinygltf::Primitive& prim : model.meshes[node.mesh].primitives)
            appendPrimitive(model, prim, transform, world);

    for (int child : node.children)
        walkNode(model, child, transform, world);
}

bool loadHeadlessWorld(const char* path, HeadlessWorld& world)
{
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err;
    std::string warn;
    std::string resolved = resolveAssetPath(path);
    bool ok = loader.LoadBinaryFromFile(&model, &err, &warn, resolved);
    if (!warn.empty()) printf("%s [SERVER WORLD WARNING] %s\n", serverTimestamp(), warn.c_str());
    if (!err.empty()) printf("%s [SERVER WORLD ERROR] %s\n", serverTimestamp(), err.c_str());
    if (!ok)
        return false;

    int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
    if (sceneIndex >= 0 && sceneIndex < (int)model.scenes.size())
        for (int node : model.scenes[sceneIndex].nodes)
            walkNode(model, node, glm::mat4(1.0f), world);

    printf("%s [SERVER WORLD] loaded map collision triangles=%zu bounds=(%.1f,%.1f,%.1f)-(%.1f,%.1f,%.1f)\n",
           serverTimestamp(), world.triangles.size(),
           world.boundsMin.x, world.boundsMin.y, world.boundsMin.z,
           world.boundsMax.x, world.boundsMax.y, world.boundsMax.z);
    return !world.triangles.empty();
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
            p.pos = {1.0f + (float)(p.id - 1) * 1.5f, 5.0f, 30.0f};
            printf("%s [SERVER RESPAWN] playerId=%u position=(%.2f,%.2f,%.2f)\n",
                   serverTimestamp(), p.id, p.pos.x, p.pos.y, p.pos.z);
        }
        return;
    }

    glm::vec2 wish = p.input.wish;
    float wishLen = glm::length(wish);
    if (wishLen > 1.0f)
        wish /= wishLen;

    const float maxSpeed = 8.0f;
    const float accel = p.onGround ? 55.0f : 22.0f;
    glm::vec2 horiz(p.vel.x, p.vel.y);
    glm::vec2 target = wish * maxSpeed;
    horiz += (target - horiz) * std::min(1.0f, accel * SERVER_DT);
    if (wishLen < 0.01f && p.onGround)
        horiz *= 0.82f;

    p.vel.x = horiz.x;
    p.vel.y = horiz.y;
    p.vel.z -= 28.0f * SERVER_DT;

    if (p.input.jumpHeld && p.onGround)
    {
        p.vel.z = 10.5f;
        p.onGround = false;
    }

    if (p.input.dashPressed && p.dashAvailable)
    {
        glm::vec2 dashDir = wishLen > 0.01f ? wish : glm::normalize(glm::vec2(p.input.camForward.x, p.input.camForward.y));
        if (glm::length(dashDir) > 0.01f)
        {
            p.vel.x += dashDir.x * 16.0f;
            p.vel.y += dashDir.y * 16.0f;
            p.dashAvailable = false;
        }
    }

    p.yaw = p.input.yaw;
    p.pos += p.vel * SERVER_DT;
    resolveWorldCollision(p, world);
    if (p.onGround)
        p.dashAvailable = true;
}

void simulateCombat(std::unordered_map<uint32_t, ServerPlayer>& players)
{
    constexpr float MAX_RANGE = 60.0f;
    constexpr float HIT_RADIUS = 0.85f;
    constexpr int DAMAGE = 25;

    for (auto& shooterEntry : players)
    {
        ServerPlayer& shooter = shooterEntry.second;
        const bool attackStarted = shooter.attackQueued;
        shooter.attackQueued = false;
        if (!attackStarted || shooter.dead)
            continue;

        glm::vec3 direction = shooter.input.camForward;
        const float directionLength = glm::length(direction);
        if (directionLength < 0.001f)
            continue;
        direction /= directionLength;

        ServerPlayer* closest = nullptr;
        float closestDistance = MAX_RANGE;
        const glm::vec3 origin = shooter.pos + glm::vec3(0.0f, 0.0f, 1.2f);
        for (auto& targetEntry : players)
        {
            ServerPlayer& target = targetEntry.second;
            if (target.id == shooter.id || target.dead)
                continue;

            const glm::vec3 toTarget =
                target.pos + glm::vec3(0.0f, 0.0f, 1.2f) - origin;
            const float alongRay = glm::dot(toTarget, direction);
            if (alongRay <= 0.0f || alongRay >= closestDistance)
                continue;
            const float distanceFromRay =
                glm::length(toTarget - direction * alongRay);
            if (distanceFromRay <= HIT_RADIUS)
            {
                closest = &target;
                closestDistance = alongRay;
            }
        }

        if (!closest)
        {
            printf("%s [SERVER COMBAT] shooter=%u miss\n",
                   serverTimestamp(), shooter.id);
            continue;
        }

        closest->health = std::max(0, closest->health - DAMAGE);
        printf("%s [SERVER COMBAT] shooter=%u target=%u damage=%d health=%d\n",
               serverTimestamp(), shooter.id, closest->id, DAMAGE, closest->health);
        if (closest->health == 0)
        {
            closest->dead = true;
            closest->respawnSeconds = 2.0f;
            closest->vel = glm::vec3(0.0f);
            printf("%s [SERVER DEATH] playerId=%u respawn=2.0s\n",
                   serverTimestamp(), closest->id);
        }
    }
}

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
    copyName(out.displayName, player.name);
    return out;
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

void logSnapshotEntity(const SnapshotEntity& entity)
{
    printf("  entityId=%u type=%s ownerClientId=%u position=(%.2f,%.2f,%.2f) "
           "rotation=%.2f health=%d\n",
           entity.networkEntityId,
           entity.entityType == ENTITY_PLAYER ? "Player" : "NPC",
           entity.ownerClientId,
           entity.px, entity.py, entity.pz,
           entity.yaw, entity.health);
}

} // namespace

int runServer(const LaunchOptions& options)
{
    setvbuf(stdout, nullptr, _IONBF, 0);

    printf("%s [SERVER] ========================================\n", serverTimestamp());
    printf("%s [SERVER] MiMITA Dedicated Server\n", serverTimestamp());
    printf("%s [SERVER] protocol version=%u\n", serverTimestamp(), PROTOCOL_VERSION);
    printf("%s [SERVER] tick rate=%.0f Hz\n", serverTimestamp(), SERVER_TICK_RATE);
    printf("%s [SERVER] max players=%d\n", serverTimestamp(), MAX_PLAYERS);
    printf("%s [SERVER] timeout=%llums\n", serverTimestamp(), (unsigned long long)CLIENT_TIMEOUT_MS);
    printf("%s [SERVER] ========================================\n", serverTimestamp());

    HeadlessWorld world;
    if (!loadHeadlessWorld("assets/maps/mimita-aabb-only-interior-small-v4.glb", world))
        printf("%s [SERVER WORLD] WARNING: headless GLB collision load failed; using floor fallback only\n", serverTimestamp());

    if (!netStartup())
    {
        printf("%s [SERVER] FATAL: WSAStartup failed\n", serverTimestamp());
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET)
    {
        printf("%s [SERVER] FATAL: socket() failed error=%d\n", serverTimestamp(), WSAGetLastError());
        netShutdown();
        return 1;
    }

    // Allow address reuse to avoid WSAEADDRINUSE (error 10048)
    int reuseAddr = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuseAddr, sizeof(reuseAddr)) == SOCKET_ERROR)
        printf("%s [SERVER] WARNING: setsockopt SO_REUSEADDR failed error=%d (non-fatal)\n", serverTimestamp(), WSAGetLastError());

    setNonBlocking(sock);

    sockaddr_in bindAddr{};
    if (!parseAddress(options.connect, bindAddr))
    {
        bindAddr.sin_family = AF_INET;
        bindAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        bindAddr.sin_port = htons(DEFAULT_PORT);
    }
    if (bind(sock, (sockaddr*)&bindAddr, sizeof(bindAddr)) == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
        printf("%s [SERVER] FATAL: bind() failed error=%d\n", serverTimestamp(), err);
        if (err == WSAEADDRINUSE)
            printf("%s [SERVER] HINT: Address %s is already in use. Is another server already running?\n",
                   serverTimestamp(), addressToString(bindAddr).c_str());
        closesocket(sock);
        netShutdown();
        return 1;
    }

    printf("%s [SERVER] bound to %s\n", serverTimestamp(), addressToString(bindAddr).c_str());
    printf("%s [SERVER] waiting for connections...\n", serverTimestamp());

    std::unordered_map<uint32_t, ServerPlayer> players;
    std::unordered_map<uint32_t, ServerNpc> npcs;
    uint32_t nextPlayerId = 1;
    uint32_t nextEntityId = 1000;
    uint32_t tick = 0;
    uint64_t lastLog = nowMs();
    uint64_t totalPacketsIn = 0;
    uint64_t totalPacketsOut = 0;

    for (int i = 0; i < 3; ++i)
    {
        ServerNpc npc;
        npc.entityId = nextEntityId++;
        npc.name = "NPC " + std::to_string(i + 1);
        npc.pos = {4.0f + i * 2.0f, 8.0f, 30.0f};
        npc.phase = i * 2.0f;
        npcs[npc.entityId] = npc;
    }

    while (true)
    {
        uint64_t frameStart = nowMs();
        char buffer[2048];
        sockaddr_in from{};
        int fromLen = sizeof(from);

        // Drain all pending packets
        for (;;)
        {
            int bytes = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&from, &fromLen);
            if (bytes <= 0)
                break;
            ++totalPacketsIn;

            PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
            if (bytes < (int)sizeof(PacketHeader) || header->magic != PROTOCOL_MAGIC || header->version != PROTOCOL_VERSION)
            {
                printf("%s [SERVER PACKET] rejected invalid header magic=0x%08x ver=%u\n",
                       serverTimestamp(), header->magic, header->version);
                continue;
            }

            if (header->type == PACKET_HELLO && bytes >= (int)sizeof(HelloPacket))
            {
                uint32_t existingId = 0;
                for (const auto& kv : players)
                    if (sameAddress(kv.second.addr, from))
                        existingId = kv.first;

                uint32_t id = existingId ? existingId : nextPlayerId++;
                ServerPlayer& p = players[id];
                p.id = id;
                p.addr = from;
                p.lastHeardMs = nowMs();
                p.name = uniquePlayerName(
                    players, reinterpret_cast<HelloPacket*>(buffer)->name, id);

                if (!existingId)
                {
                    p.pos = {1.0f + (float)(id - 1) * 1.5f, 5.0f, 30.0f};
                    printf("%s [SERVER JOIN] id=%u name=\"%s\" addr=%s\n",
                           serverTimestamp(), id, p.name.c_str(), addressToString(from).c_str());
                }

                WelcomePacket welcome{};
                welcome.header.type = PACKET_WELCOME;
                welcome.header.tick = tick;
                welcome.header.playerId = id;
                welcome.assignedPlayerId = id;
                welcome.tickRate = SERVER_TICK_RATE;
                copyName(welcome.approvedName, p.name);
                sendto(sock, (const char*)&welcome, sizeof(welcome), 0, (sockaddr*)&from, sizeof(from));
                ++totalPacketsOut;
            }
            else if (header->type == PACKET_INPUT && bytes >= (int)sizeof(InputPacket))
            {
                InputPacket* in = reinterpret_cast<InputPacket*>(buffer);
                auto it = players.find(in->header.playerId);
                if (it == players.end())
                    continue;
                ServerPlayer& p = it->second;
                p.lastHeardMs = nowMs();
                if (p.dead)
                {
                    p.input.attackPressed = false;
                    continue;
                }
                p.input.wish = {in->wishX, in->wishY};
                p.input.camForward = {in->camForwardX, in->camForwardY, in->camForwardZ};
                p.input.yaw = in->yaw;
                p.input.jumpHeld = in->jumpHeld != 0;
                p.input.dashPressed = in->dashPressed != 0;
                const bool attackPressed = in->attackPressed != 0;
                if (attackPressed && !p.input.attackPressed)
                    p.attackQueued = true;
                p.input.attackPressed = attackPressed;
                p.input.freezeHeld = in->freezeHeld != 0;
                p.input.tick = in->header.tick;
                if (in->spawnNpcPressed)
                {
                    ServerNpc npc;
                    npc.entityId = nextEntityId++;
                    npc.name = "NPC " + std::to_string(npc.entityId);
                    npc.pos = p.pos + glm::vec3(2.0f, 0.0f, 0.0f);
                    npcs[npc.entityId] = npc;
                    printf("%s [SERVER ENTITY SPAWN] entityId=%u type=NPC ownerClientId=0 position=(%.2f,%.2f,%.2f)\n",
                           serverTimestamp(), npc.entityId, npc.pos.x, npc.pos.y, npc.pos.z);
                }
            }
            else if (header->type == PACKET_DISCONNECT)
            {
                auto it = players.find(header->playerId);
                if (it != players.end())
                {
                    printf("%s [SERVER LEAVE] id=%u name=\"%s\"\n",
                           serverTimestamp(), it->second.id, it->second.name.c_str());
                    players.erase(it);
                }
            }
            else if (header->type == PACKET_SPAWN_NPC_REQUEST &&
                     bytes >= (int)sizeof(SpawnNpcRequestPacket))
            {
                SpawnNpcRequestPacket* request =
                    reinterpret_cast<SpawnNpcRequestPacket*>(buffer);
                if (players.find(request->header.playerId) == players.end())
                    continue;

                ServerNpc npc;
                npc.entityId = nextEntityId++;
                npc.name = "NPC " + std::to_string(npc.entityId);
                npc.pos = {request->px, request->py, request->pz};
                npcs[npc.entityId] = npc;
                printf("%s [SERVER ENTITY SPAWN] entityId=%u type=NPC ownerClientId=0 position=(%.2f,%.2f,%.2f)\n",
                       serverTimestamp(), npc.entityId, npc.pos.x, npc.pos.y, npc.pos.z);
            }
        }

        // Timeout disconnected clients
        for (auto it = players.begin(); it != players.end(); )
        {
            if (nowMs() - it->second.lastHeardMs > CLIENT_TIMEOUT_MS)
            {
                printf("%s [SERVER TIMEOUT] id=%u name=\"%s\"\n",
                       serverTimestamp(), it->second.id, it->second.name.c_str());
                it = players.erase(it);
            }
            else
                ++it;
        }

        // Simulate all players
        for (auto& kv : players)
            simulatePlayer(kv.second, world);
        resolvePlayerCollision(players);
        simulateCombat(players);
        for (auto& kv : npcs)
            simulateNpc(kv.second, players);

        // Build and send snapshot to every connected client
        SnapshotPacket snapshot{};
        snapshot.header.type = PACKET_SNAPSHOT;
        snapshot.header.tick = tick;
        uint32_t index = 0;
        for (const auto& kv : players)
        {
            if (index >= MAX_SNAPSHOT_ENTITIES)
                break;
            snapshot.entities[index++] = makePlayerEntity(kv.second);
            ++snapshot.playerCount;
        }
        for (const auto& kv : npcs)
        {
            if (index >= MAX_SNAPSHOT_ENTITIES)
                break;
            snapshot.entities[index++] = makeNpcEntity(kv.second);
            ++snapshot.npcCount;
        }
        snapshot.entityCount = index;

        if (tick % 60 == 0)
        {
            printf("%s [SERVER SNAPSHOT BUILD] tick=%u playersIncluded=%u npcsIncluded=%u entitiesIncluded=%u\n",
                   serverTimestamp(), tick, snapshot.playerCount, snapshot.npcCount, snapshot.entityCount);
            for (uint32_t i = 0; i < snapshot.entityCount; ++i)
                logSnapshotEntity(snapshot.entities[i]);
        }

        for (const auto& kv : players)
        {
            const int bytesSent = sendto(
                sock, (const char*)&snapshot, sizeof(snapshot), 0,
                (sockaddr*)&kv.second.addr, sizeof(kv.second.addr));
            ++totalPacketsOut;
            if (tick % 60 == 0)
                printf("%s [SERVER SNAPSHOT SEND] toClientId=%u bytes=%d entityCount=%u playerCount=%u npcCount=%u\n",
                       serverTimestamp(), kv.first, bytesSent, snapshot.entityCount,
                       snapshot.playerCount, snapshot.npcCount);
        }

        // Status log every second
        if (nowMs() - lastLog >= 1000)
        {
            printf("%s [SERVER STATUS] tick=%u players=%zu packetsIn=%llu packetsOut=%llu\n",
                   serverTimestamp(), tick, players.size(),
                   (unsigned long long)totalPacketsIn, (unsigned long long)totalPacketsOut);
            for (const auto& kv : players)
                printf("%s [SERVER PLAYER] id=%u name=\"%s\" pos=(%.1f,%.1f,%.1f)\n",
                       serverTimestamp(), kv.second.id, kv.second.name.c_str(),
                       kv.second.pos.x, kv.second.pos.y, kv.second.pos.z);
            lastLog = nowMs();
        }

        ++tick;
        uint64_t elapsed = nowMs() - frameStart;
        uint64_t targetMs = (uint64_t)(1000.0f / SERVER_TICK_RATE);
        if (elapsed < targetMs)
            std::this_thread::sleep_for(std::chrono::milliseconds(targetMs - elapsed));
    }
}

} // namespace MimitaNet
