#include "network/net_mode.h"

#include "network/net_common.h"
#include "network/packets.h"
#include "engine/engine.h"
#include "world/world.h"
#include "world/world-gltf-loader.h"
#include "entities/player.h"
#include "camera.h"
#include "input/input-state.h"
#include "input/input-poll.h"
#include "render/render-world.h"
#include "render/render-player.h"
#include "audio/audio.h"
#include "gui/ui-system.h"
#include "gui/gui-layout.h"
#include "gui/font-stuff/font-loader.h"
#include "debug/debug-visuals.h"
#include "debug/debug-log.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <unordered_map>

namespace MimitaNet {
namespace {

void copyName(char (&dst)[MAX_NAME_BYTES], const std::string& name)
{
    std::memset(dst, 0, sizeof(dst));
    std::strncpy(dst, name.c_str(), sizeof(dst) - 1);
}

} // namespace

int runClient(const LaunchOptions& options)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    printf("[CLIENT] connecting name=%s target=%s\n", options.name.c_str(), options.connect.c_str());

    if (!netStartup())
        return 1;

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET)
    {
        printf("[CLIENT] socket failed error=%d\n", WSAGetLastError());
        netShutdown();
        return 1;
    }
    setNonBlocking(sock);

    sockaddr_in serverAddr{};
    if (!parseAddress(options.connect, serverAddr))
    {
        printf("[CLIENT] invalid --connect address: %s\n", options.connect.c_str());
        closesocket(sock);
        netShutdown();
        return 1;
    }

    Engine engine;
    engine.init(800, 600, ("mimita.exe multiplayer - " + options.name).c_str());
    if (!engine.window())
    {
        closesocket(sock);
        netShutdown();
        return 1;
    }

    glfwSetInputMode(engine.window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    fontInit();
    uiInit(engine.window());
    DebugVis::init(engine.window());
    Debug::startupReport();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);

    World world;
    loadWorldFromGLB(world, "assets/maps/mimita-aabb-only-interior-small-v4.glb");

    Camera camera;
    engine.bindCamera(&camera);
    glfwSetWindowUserPointer(engine.window(), &camera);

    uint32_t localPlayerId = 0;
    uint32_t clientTick = 0;
    uint64_t lastHelloMs = 0;
    uint64_t lastLogMs = nowMs();
    uint64_t packetsSent = 0;
    uint64_t packetsReceived = 0;
    uint32_t lastSnapshotTick = 0;
    std::unordered_map<uint32_t, Player> players;
    std::unordered_map<uint32_t, Player> npcs;
    std::string approvedName = options.name;

    while (engine.running())
    {
        float dt = engine.beginFrame();
        audioUpdate(dt);
        DebugVis::update();

        uint64_t currentMs = nowMs();
        if (!localPlayerId && currentMs - lastHelloMs > 500)
        {
            HelloPacket hello{};
            hello.header.type = PACKET_HELLO;
            hello.header.tick = clientTick;
            copyName(hello.name, options.name);
            sendto(sock, (const char*)&hello, sizeof(hello), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
            ++packetsSent;
            lastHelloMs = currentMs;
            printf("[CLIENT] hello sent to %s\n", options.connect.c_str());
        }

        char buffer[16384];
        for (;;)
        {
            sockaddr_in from{};
            int fromLen = sizeof(from);
            int bytes = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&from, &fromLen);
            if (bytes <= 0)
                break;
            ++packetsReceived;

            PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
            if (bytes < (int)sizeof(PacketHeader) || header->magic != PROTOCOL_MAGIC || header->version != PROTOCOL_VERSION)
                continue;

            if (header->type == PACKET_WELCOME && bytes >= (int)sizeof(WelcomePacket))
            {
                WelcomePacket* welcome = reinterpret_cast<WelcomePacket*>(buffer);
                localPlayerId = welcome->assignedPlayerId;
                approvedName = welcome->approvedName;
                printf("[CLIENT] connected assigned player id=%u serverTick=%u tickRate=%.0f\n",
                       localPlayerId, welcome->header.tick, welcome->tickRate);
            }
            else if (header->type == PACKET_SNAPSHOT && bytes >= (int)sizeof(SnapshotPacket))
            {
                SnapshotPacket* snapshot = reinterpret_cast<SnapshotPacket*>(buffer);
                lastSnapshotTick = snapshot->header.tick;
                uint32_t count = std::min(snapshot->entityCount, (uint32_t)MAX_SNAPSHOT_ENTITIES);
                const bool logSnapshot = snapshot->header.tick % 60 == 0;
                if (logSnapshot)
                    printf("[CLIENT SNAPSHOT RECV] localClientId=%u bytes=%d entityCount=%u playerCount=%u npcCount=%u\n",
                           localPlayerId, bytes, snapshot->entityCount,
                           snapshot->playerCount, snapshot->npcCount);
                std::unordered_map<uint32_t, bool> seenPlayers;
                std::unordered_map<uint32_t, bool> seenNpcs;
                for (uint32_t i = 0; i < count; ++i)
                {
                    const SnapshotEntity& entity = snapshot->entities[i];
                    if (!entity.active || entity.networkEntityId == 0)
                    {
                        printf("[CLIENT ENTITY SKIP] entityId=%u reason=inactive-or-zero-id\n",
                               entity.networkEntityId);
                        continue;
                    }

                    std::unordered_map<uint32_t, Player>* replicas = nullptr;
                    std::unordered_map<uint32_t, bool>* seen = nullptr;
                    const char* typeName = nullptr;
                    if (entity.entityType == ENTITY_PLAYER)
                    {
                        replicas = &players;
                        seen = &seenPlayers;
                        typeName = "Player";
                    }
                    else if (entity.entityType == ENTITY_NPC)
                    {
                        replicas = &npcs;
                        seen = &seenNpcs;
                        typeName = "NPC";
                    }
                    else
                    {
                        printf("[CLIENT ENTITY SKIP] entityId=%u reason=unknown-entity-type-%u\n",
                               entity.networkEntityId, entity.entityType);
                        continue;
                    }

                    bool existsBefore = replicas->find(entity.networkEntityId) != replicas->end();
                    Player& p = (*replicas)[entity.networkEntityId];
                    p.pos = {entity.px, entity.py, entity.pz};
                    p.vel = {entity.vx, entity.vy, entity.vz};
                    p.yaw = entity.yaw;
                    p.ground.onGround = entity.onGround != 0;
                    p.currentHp = entity.health;
                    p.username = entity.displayName;
                    p.updateProceduralAnimation(dt);
                    (*seen)[entity.networkEntityId] = true;
                    if (!existsBefore || logSnapshot)
                        printf("[CLIENT ENTITY APPLY] entityId=%u type=%s isLocal=%d existsBefore=%d "
                               "createdNow=%d position=(%.2f,%.2f,%.2f) renderRegistered=1\n",
                               entity.networkEntityId, typeName,
                               (int)(entity.ownerClientId == localPlayerId),
                               (int)existsBefore, (int)!existsBefore,
                               entity.px, entity.py, entity.pz);
                }
                for (auto it = players.begin(); it != players.end(); )
                {
                    if (!seenPlayers[it->first])
                        it = players.erase(it);
                    else
                        ++it;
                }
                for (auto it = npcs.begin(); it != npcs.end(); )
                {
                    if (!seenNpcs[it->first])
                        it = npcs.erase(it);
                    else
                        ++it;
                }
            }
        }

        InputState input = pollInput(engine.window(), camera);
        if (localPlayerId)
        {
            InputPacket in{};
            in.header.type = PACKET_INPUT;
            in.header.tick = clientTick;
            in.header.playerId = localPlayerId;
            in.wishX = input.wishMoveXY.x;
            in.wishY = input.wishMoveXY.y;
            in.camForwardX = input.camForward.x;
            in.camForwardY = input.camForward.y;
            in.camForwardZ = input.camForward.z;
            in.yaw = camera.yaw;
            in.jumpHeld = input.jumpHeld ? 1 : 0;
            in.dashPressed = input.dashPressed ? 1 : 0;
            in.attackPressed = glfwGetMouseButton(engine.window(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS ? 1 : 0;
            in.freezeHeld = input.freezeHeld ? 1 : 0;
            sendto(sock, (const char*)&in, sizeof(in), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
            ++packetsSent;
        }

        auto localIt = players.find(localPlayerId);
        if (localIt != players.end())
        {
            camera.updateVectors();
            camera.follow(localIt->second.pos);
            camera.smoothCollision(localIt->second.pos, world.collisionMesh.triangles, 1.0f / 60.0f);
        }

        renderWorld(world, camera);
        for (auto& kv : players)
            renderPlayer(kv.second, camera);
        for (auto& kv : npcs)
            renderPlayer(kv.second, camera);

        uiBeginFrame(engine.window(), "multiplayer-debug-overlay");
        GuiLayout& mpLayout = GuiLayoutManager::instance().getLayout("config/gui/client-hud.json");
        auto mpText = [&](const std::string& id, const std::string& text) {
            const GuiElement* el = mpLayout.get(id);
            if (!el) return;
            float s = el->fontSize > 0.0f ? el->fontSize : 0.32f;
            uiDrawText(text.c_str(), uiScaleX(el->x), uiScaleY(el->y), s, el->getTextColorVec());
        };
        char line[160];
        snprintf(line, sizeof(line), "MP id=%u name=%s players=%zu npcs=%zu",
                 localPlayerId, approvedName.c_str(), players.size(), npcs.size());
        mpText("mpLine1", line);
        snprintf(line, sizeof(line), "snapshot tick=%u sent=%llu recv=%llu", lastSnapshotTick,
                 (unsigned long long)packetsSent, (unsigned long long)packetsReceived);
        mpText("mpLine2", line);
        if (localIt != players.end())
        {
            const Player& lp = localIt->second;
            snprintf(line, sizeof(line), "pos %.2f %.2f %.2f", lp.pos.x, lp.pos.y, lp.pos.z);
            mpText("mpLine3", line);
        }
        uiRenderFrameDebugOverlay(engine.window(), "MULTIPLAYER", true);
        uiEndFrame();

        if (currentMs - lastLogMs >= 1000)
        {
            if (localIt != players.end())
                printf("[CLIENT] id=%u snapshot=%u packets sent=%llu recv=%llu local pos=(%.2f %.2f %.2f)\n",
                       localPlayerId, lastSnapshotTick, (unsigned long long)packetsSent, (unsigned long long)packetsReceived,
                       localIt->second.pos.x, localIt->second.pos.y, localIt->second.pos.z);
            else
                printf("[CLIENT] waiting for snapshot id=%u packets sent=%llu recv=%llu\n",
                       localPlayerId, (unsigned long long)packetsSent, (unsigned long long)packetsReceived);
            lastLogMs = currentMs;
        }

        if (glfwGetKey(engine.window(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(engine.window(), GLFW_TRUE);

        engine.endFrame();
        ++clientTick;
    }

    if (localPlayerId)
    {
        DisconnectPacket bye{};
        bye.header.type = PACKET_DISCONNECT;
        bye.header.playerId = localPlayerId;
        bye.header.tick = clientTick;
        sendto(sock, (const char*)&bye, sizeof(bye), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
    }

    engine.shutdown();
    closesocket(sock);
    netShutdown();
    printf("[CLIENT] shutdown complete\n");
    return 0;
}

} // namespace MimitaNet
