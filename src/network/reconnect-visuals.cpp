// 08 08 2026, 12 00
/* purpose
* Implements observer-facing disconnect/reconnect effects for remote players.
* Spawns red pulsing beams + a per-frame ticking "connection lost" billboard
* label above a frozen body, and a green "reconnected" burst on recovery.
* Uses the pooled EffectPart system (same style as server-disagreement visuals).
* Does NOT own the local player's connection state machine or notifications.
* Does NOT run server-side, send packets, or mutate gameplay simulation.
*/

#include "network/reconnect-visuals.h"
#include "effects/effect-part.h"
#include "debug/debug-log.h"

#include <cstdio>
#include <unordered_map>
#include <vector>

namespace MimitaNet {

namespace {

constexpr float kReconnectBeamLifetime = 0.5f;
constexpr float kReconnectLabelLifetime = 0.4f;
constexpr float kReconnectGreenLifetime = 1.5f;
constexpr glm::vec3 kRed(1.0f, 0.22f, 0.22f);
constexpr glm::vec3 kGreen(0.25f, 1.0f, 0.35f);

std::string remotePlayerName(const MultiplayerContext& ctx, uint32_t playerId)
{
    auto it = ctx.playerRegistry.find(playerId);
    if (it != ctx.playerRegistry.end() && !it->second.name.empty())
        return it->second.name;
    return "player_" + std::to_string(playerId);
}

void spawnRedBeam(const glm::vec3& position, uint32_t playerId)
{
    EffectPart beam;
    beam.position = position;
    beam.endPosition = position + glm::vec3(0.0f, 0.0f, 2.6f);
    beam.color = kRed;
    beam.maxLifetime = kReconnectBeamLifetime;
    beam.scale = 0.05f;
    beam.endScale = 0.02f;
    beam.alpha = 0.5f;
    beam.beam = true;
    beam.ownerId = playerId;
    beam.replayType = "reconnect_beam";
    EffectPartSystem::instance().spawn(beam);
}

void spawnTickingLabel(const glm::vec3& position, const std::string& name,
                       double elapsedSeconds, uint32_t playerId)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "%s connection lost... %.2fs",
             name.c_str(), elapsedSeconds);
    EffectPart text;
    text.position = position + glm::vec3(0.0f, 0.0f, 3.0f);
    text.color = kRed;
    text.maxLifetime = kReconnectLabelLifetime;
    text.label = buf;
    text.scale = 0.028f;
    text.billboardText = true;
    text.ownerId = playerId;
    text.replayType = "reconnect_label";
    EffectPartSystem::instance().spawn(text);
}

void spawnGreenReconnected(const glm::vec3& position, const std::string& name)
{
    EffectPart pulse;
    pulse.position = position;
    pulse.color = kGreen;
    pulse.maxLifetime = kReconnectGreenLifetime;
    pulse.scale = 0.15f;
    pulse.endScale = 1.2f;
    pulse.alpha = 0.5f;
    pulse.billboardText = false;
    pulse.replayType = "reconnected_pulse";
    EffectPartSystem::instance().spawn(pulse);

    EffectPart text;
    text.position = position + glm::vec3(0.0f, 0.0f, 2.9f);
    text.color = kGreen;
    text.maxLifetime = kReconnectGreenLifetime;
    text.label = name + " reconnected";
    text.scale = 0.028f;
    text.billboardText = true;
    text.replayType = "reconnected_label";
    EffectPartSystem::instance().spawn(text);
}

} // namespace

void mpNoteRemotePlayerDisconnected(MultiplayerContext& ctx, uint32_t playerId,
                                    uint64_t serverDisconnectedAtMs)
{
    (void)serverDisconnectedAtMs;
    if (playerId == 0 || playerId == ctx.localPlayerId)
        return;

    RemoteConnectionState& st = ctx.remoteConnectionStates[playerId];
    st.disconnectedSinceMs = nowMs();  // client-clock elapsed ticker
    st.reconnectedNotified = false;

    auto it = ctx.remotePlayers.find(playerId);
    const glm::vec3 pos = it != ctx.remotePlayers.end() ? it->second.pos : glm::vec3(0.0f);
    spawnRedBeam(pos, playerId);
    spawnTickingLabel(pos, remotePlayerName(ctx, playerId), 0.0, playerId);

    Debug::warn(Debug::Category::Networking,
                "[PEER LOST] id=%u name=\"%s\" pos=(%.1f,%.1f,%.1f) — red effect\n",
                playerId, remotePlayerName(ctx, playerId).c_str(),
                pos.x, pos.y, pos.z);
}

void mpNoteRemotePlayerReconnected(MultiplayerContext& ctx, uint32_t playerId)
{
    if (playerId == 0 || playerId == ctx.localPlayerId)
        return;

    auto it = ctx.remoteConnectionStates.find(playerId);
    if (it == ctx.remoteConnectionStates.end())
        return;

    const bool wasDisconnected = it->second.disconnectedSinceMs != 0;
    if (!wasDisconnected)
        return;

    auto playerIt = ctx.remotePlayers.find(playerId);
    const glm::vec3 pos =
        playerIt != ctx.remotePlayers.end() ? playerIt->second.pos : glm::vec3(0.0f);
    spawnGreenReconnected(pos, remotePlayerName(ctx, playerId));

    ctx.remoteConnectionStates.erase(it);
    Debug::warn(Debug::Category::Networking,
                "[PEER RECONNECTED] id=%u name=\"%s\" — green effect\n",
                playerId, remotePlayerName(ctx, playerId).c_str());
}

void mpUpdateReconnectVisuals(MultiplayerContext& ctx, float dt)
{
    (void)dt;
    if (ctx.remoteConnectionStates.empty())
        return;

    const uint64_t now = nowMs();
    std::vector<uint32_t> toErase;
    for (auto& kv : ctx.remoteConnectionStates)
    {
        const uint32_t playerId = kv.first;
        RemoteConnectionState& st = kv.second;
        if (st.disconnectedSinceMs == 0)
            continue;

        // The frozen body may have been removed by the missing-entity tracker
        // after the 60s grace; drop the indicator with it.
        if (ctx.remotePlayers.find(playerId) == ctx.remotePlayers.end())
        {
            toErase.push_back(playerId);
            continue;
        }

        const glm::vec3 pos = ctx.remotePlayers[playerId].pos;
        const double elapsed = now >= st.disconnectedSinceMs
            ? (double)(now - st.disconnectedSinceMs) / 1000.0 : 0.0;

        // Red beam refreshes every half-second so it stays visibly pulsing.
        static std::unordered_map<uint32_t, uint64_t> lastBeamMs;
        const uint64_t& last = lastBeamMs[playerId];
        if (now - last >= (uint64_t)(kReconnectBeamLifetime * 1000.0 * 0.9))
        {
            lastBeamMs[playerId] = now;
            spawnRedBeam(pos, playerId);
        }

        // Fast-ticking label refreshes every frame.
        spawnTickingLabel(pos, remotePlayerName(ctx, playerId), elapsed, playerId);
    }

    for (uint32_t id : toErase)
    {
        ctx.remoteConnectionStates.erase(id);
        Debug::warn(Debug::Category::Networking,
                    "[PEER LOST EFFECT] cleared id=%u (entity removed)\n", id);
    }
}

void mpClearRemoteReconnectVisual(MultiplayerContext& ctx, uint32_t playerId)
{
    ctx.remoteConnectionStates.erase(playerId);
}

} // namespace MimitaNet
