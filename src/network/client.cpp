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

        char buffer[4096];
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
                printf("[CLIENT] connected assigned player id=%u serverTick=%u tickRate=%.0f\n",
                       localPlayerId, welcome->header.tick, welcome->tickRate);
            }
            else if (header->type == PACKET_SNAPSHOT && bytes >= (int)sizeof(SnapshotPacket))
            {
                SnapshotPacket* snapshot = reinterpret_cast<SnapshotPacket*>(buffer);
                lastSnapshotTick = snapshot->header.tick;
                uint32_t count = std::min(snapshot->playerCount, (uint32_t)MAX_SNAPSHOT_PLAYERS);
                std::unordered_map<uint32_t, bool> seen;
                for (uint32_t i = 0; i < count; ++i)
                {
                    const SnapshotPlayer& sp = snapshot->players[i];
                    if (!sp.active || sp.playerId == 0)
                        continue;
                    Player& p = players[sp.playerId];
                    p.pos = {sp.px, sp.py, sp.pz};
                    p.vel = {sp.vx, sp.vy, sp.vz};
                    p.yaw = sp.yaw;
                    p.onGround = sp.onGround != 0;
                    p.updateProceduralAnimation(dt);
                    seen[sp.playerId] = true;
                }
                for (auto it = players.begin(); it != players.end(); )
                {
                    if (!seen[it->first])
                        it = players.erase(it);
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
        }

        renderWorld(world, camera);
        for (auto& kv : players)
            renderPlayer(kv.second, camera);

        uiBeginFrame(engine.window(), "multiplayer-debug-overlay");
        uiDrawRect({14, 78, 330, 118}, {0.0f, 0.0f, 0.0f, 0.56f}, "mp-hud-bg");
        char line[160];
        snprintf(line, sizeof(line), "MP id=%u players=%zu", localPlayerId, players.size());
        uiDrawText(line, 24, 88, 0.38f, {0.95f, 0.98f, 1.0f, 1.0f});
        snprintf(line, sizeof(line), "snapshot tick=%u sent=%llu recv=%llu", lastSnapshotTick,
                 (unsigned long long)packetsSent, (unsigned long long)packetsReceived);
        uiDrawText(line, 24, 116, 0.32f, {0.75f, 0.9f, 1.0f, 1.0f});
        if (localIt != players.end())
        {
            const Player& lp = localIt->second;
            snprintf(line, sizeof(line), "pos %.2f %.2f %.2f", lp.pos.x, lp.pos.y, lp.pos.z);
            uiDrawText(line, 24, 142, 0.32f, {0.35f, 1.0f, 0.45f, 1.0f});
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
