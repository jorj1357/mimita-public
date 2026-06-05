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
    uint64_t lastHeardMs = 0;
    ServerInput input;
};

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
    if (!warn.empty()) printf("[SERVER WORLD WARNING] %s\n", warn.c_str());
    if (!err.empty()) printf("[SERVER WORLD ERROR] %s\n", err.c_str());
    if (!ok)
        return false;

    int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
    if (sceneIndex >= 0 && sceneIndex < (int)model.scenes.size())
        for (int node : model.scenes[sceneIndex].nodes)
            walkNode(model, node, glm::mat4(1.0f), world);

    printf("[SERVER WORLD] collision triangles=%zu bounds min=(%.2f %.2f %.2f) max=(%.2f %.2f %.2f)\n",
           world.triangles.size(), world.boundsMin.x, world.boundsMin.y, world.boundsMin.z,
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
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
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

} // namespace

int runServer(const LaunchOptions&)
{
    printf("[SERVER] starting localhost UDP server on port %u\n", DEFAULT_PORT);
    printf("[SERVER] tick rate %.0f Hz\n", SERVER_TICK_RATE);

    HeadlessWorld world;
    if (!loadHeadlessWorld("assets/maps/mimita-aabb-only-interior-small-v4.glb", world))
        printf("[SERVER WARNING] headless GLB collision load failed; using floor fallback only\n");

    if (!netStartup())
        return 1;

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET)
    {
        printf("[SERVER] socket failed error=%d\n", WSAGetLastError());
        netShutdown();
        return 1;
    }
    setNonBlocking(sock);

    sockaddr_in bindAddr{};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bindAddr.sin_port = htons(DEFAULT_PORT);
    if (bind(sock, (sockaddr*)&bindAddr, sizeof(bindAddr)) == SOCKET_ERROR)
    {
        printf("[SERVER] bind failed error=%d\n", WSAGetLastError());
        closesocket(sock);
        netShutdown();
        return 1;
    }

    std::unordered_map<uint32_t, ServerPlayer> players;
    uint32_t nextPlayerId = 1;
    uint32_t tick = 0;
    uint64_t lastLog = nowMs();

    while (true)
    {
        uint64_t frameStart = nowMs();
        char buffer[2048];
        sockaddr_in from{};
        int fromLen = sizeof(from);
        for (;;)
        {
            int bytes = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&from, &fromLen);
            if (bytes <= 0)
                break;

            PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
            if (bytes < (int)sizeof(PacketHeader) || header->magic != PROTOCOL_MAGIC || header->version != PROTOCOL_VERSION)
                continue;

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
                p.name = reinterpret_cast<HelloPacket*>(buffer)->name;
                if (!existingId)
                {
                    p.pos = {1.0f + (float)(id - 1) * 1.5f, 5.0f, 30.0f};
                    printf("[SERVER] client connected id=%u name=%s addr=%s\n", id, p.name.c_str(), addressToString(from).c_str());
                }

                WelcomePacket welcome{};
                welcome.header.type = PACKET_WELCOME;
                welcome.header.tick = tick;
                welcome.header.playerId = id;
                welcome.assignedPlayerId = id;
                welcome.tickRate = SERVER_TICK_RATE;
                sendto(sock, (const char*)&welcome, sizeof(welcome), 0, (sockaddr*)&from, sizeof(from));
            }
            else if (header->type == PACKET_INPUT && bytes >= (int)sizeof(InputPacket))
            {
                InputPacket* in = reinterpret_cast<InputPacket*>(buffer);
                auto it = players.find(in->header.playerId);
                if (it == players.end())
                    continue;
                ServerPlayer& p = it->second;
                p.lastHeardMs = nowMs();
                p.input.wish = {in->wishX, in->wishY};
                p.input.camForward = {in->camForwardX, in->camForwardY, in->camForwardZ};
                p.input.yaw = in->yaw;
                p.input.jumpHeld = in->jumpHeld != 0;
                p.input.dashPressed = in->dashPressed != 0;
                p.input.attackPressed = in->attackPressed != 0;
                p.input.freezeHeld = in->freezeHeld != 0;
                p.input.tick = in->header.tick;
                if ((tick % 60) == 0 || p.input.dashPressed || p.input.attackPressed)
                    printf("[SERVER] input id=%u tick=%u wish=(%.2f %.2f) jump=%d dash=%d attack=%d\n",
                           p.id, in->header.tick, p.input.wish.x, p.input.wish.y,
                           p.input.jumpHeld ? 1 : 0, p.input.dashPressed ? 1 : 0, p.input.attackPressed ? 1 : 0);
            }
            else if (header->type == PACKET_DISCONNECT)
            {
                auto it = players.find(header->playerId);
                if (it != players.end())
                {
                    printf("[SERVER] client disconnected id=%u name=%s\n", it->second.id, it->second.name.c_str());
                    players.erase(it);
                }
            }
        }

        for (auto it = players.begin(); it != players.end(); )
        {
            if (nowMs() - it->second.lastHeardMs > CLIENT_TIMEOUT_MS)
            {
                printf("[SERVER] client timed out id=%u name=%s\n", it->second.id, it->second.name.c_str());
                it = players.erase(it);
            }
            else
                ++it;
        }

        for (auto& kv : players)
            simulatePlayer(kv.second, world);
        resolvePlayerCollision(players);

        SnapshotPacket snapshot{};
        snapshot.header.type = PACKET_SNAPSHOT;
        snapshot.header.tick = tick;
        snapshot.playerCount = (uint32_t)std::min((size_t)MAX_SNAPSHOT_PLAYERS, players.size());
        uint32_t index = 0;
        for (const auto& kv : players)
        {
            if (index >= MAX_SNAPSHOT_PLAYERS)
                break;
            const ServerPlayer& p = kv.second;
            SnapshotPlayer& out = snapshot.players[index++];
            out.playerId = p.id;
            out.px = p.pos.x; out.py = p.pos.y; out.pz = p.pos.z;
            out.vx = p.vel.x; out.vy = p.vel.y; out.vz = p.vel.z;
            out.yaw = p.yaw;
            out.health = p.health;
            out.onGround = p.onGround ? 1 : 0;
            out.active = 1;
        }

        for (const auto& kv : players)
            sendto(sock, (const char*)&snapshot, sizeof(snapshot), 0, (sockaddr*)&kv.second.addr, sizeof(kv.second.addr));

        if (nowMs() - lastLog >= 1000)
        {
            printf("[SERVER] tick=%u rate=%.0f players=%zu\n", tick, SERVER_TICK_RATE, players.size());
            for (const auto& kv : players)
                printf("[SERVER] player id=%u pos=(%.2f %.2f %.2f) vel=(%.2f %.2f %.2f)\n",
                       kv.second.id, kv.second.pos.x, kv.second.pos.y, kv.second.pos.z,
                       kv.second.vel.x, kv.second.vel.y, kv.second.vel.z);
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
