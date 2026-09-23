// 09 23 2026
/* purpose
* Implements the generic EXE-owned history lookup and the `history.query`
// capability. The kernel exposes raw samples; hot `history.select` owns the
* decision (interpolate / nearest / cross-generation clamp).
* Does NOT own selection policy, collision, or the socket.
*/
#include "network/server-history.h"

#include <cmath>
#include <unordered_map>

#include "network/server-context.h"
#include "network/server.h"
#include "live-code/live-behavior.h"

namespace MimitaNet {

namespace {

// Fill the two samples that bracket `targetTick` from a player history deque.
template <typename Deque>
void bracketSamples(const Deque& history, std::uint32_t targetTick,
                    GameHistorySelectV1& out)
{
    out.haveA = 0;
    out.haveB = 0;
    out.exactTick = 0;
    if (history.empty())
        return;
    if (targetTick <= history.front().tick) {
        out.a.tick = history.front().tick;
        out.a.logicalGenerationId = history.front().logicalGenerationId;
        out.a.position[0] = history.front().pos.x;
        out.a.position[1] = history.front().pos.y;
        out.a.position[2] = history.front().pos.z;
        out.a.velocity[0] = history.front().vel.x;
        out.a.velocity[1] = history.front().vel.y;
        out.a.velocity[2] = history.front().vel.z;
        out.a.yaw = history.front().yaw;
        out.haveA = 1;
        return;
    }
    if (targetTick >= history.back().tick) {
        out.a.tick = history.back().tick;
        out.a.logicalGenerationId = history.back().logicalGenerationId;
        out.a.position[0] = history.back().pos.x;
        out.a.position[1] = history.back().pos.y;
        out.a.position[2] = history.back().pos.z;
        out.a.velocity[0] = history.back().vel.x;
        out.a.velocity[1] = history.back().vel.y;
        out.a.velocity[2] = history.back().vel.z;
        out.a.yaw = history.back().yaw;
        out.haveA = 1;
        return;
    }
    for (std::size_t i = history.size() - 1; i > 0; --i) {
        if (history[i].tick == targetTick) {
            out.a.tick = history[i].tick;
            out.a.logicalGenerationId = history[i].logicalGenerationId;
            out.a.position[0] = history[i].pos.x;
            out.a.position[1] = history[i].pos.y;
            out.a.position[2] = history[i].pos.z;
            out.a.velocity[0] = history[i].vel.x;
            out.a.velocity[1] = history[i].vel.y;
            out.a.velocity[2] = history[i].vel.z;
            out.a.yaw = history[i].yaw;
            out.haveA = 1;
            out.exactTick = 1;
            return;
        }
        if (history[i].tick < targetTick) {
            const auto& older = history[i];
            const auto& newer = history[i + 1];
            out.a.tick = older.tick;
            out.a.logicalGenerationId = older.logicalGenerationId;
            out.a.position[0] = older.pos.x;
            out.a.position[1] = older.pos.y;
            out.a.position[2] = older.pos.z;
            out.a.velocity[0] = older.vel.x;
            out.a.velocity[1] = older.vel.y;
            out.a.velocity[2] = older.vel.z;
            out.a.yaw = older.yaw;
            out.b.tick = newer.tick;
            out.b.logicalGenerationId = newer.logicalGenerationId;
            out.b.position[0] = newer.pos.x;
            out.b.position[1] = newer.pos.y;
            out.b.position[2] = newer.pos.z;
            out.b.velocity[0] = newer.vel.x;
            out.b.velocity[1] = newer.vel.y;
            out.b.velocity[2] = newer.vel.z;
            out.b.yaw = newer.yaw;
            out.haveA = 1;
            out.haveB = 1;
            return;
        }
    }
    // Unreachable given the front/back guards, but keep a safe default.
    out.a.tick = history.front().tick;
    out.haveA = 1;
}

} // namespace

bool serverHistoryRawSamples(std::uint32_t domain, std::uint64_t entityId,
                             std::uint32_t targetTick,
                             GameHistorySelectV1& out)
{
    out = GameHistorySelectV1{};
    out.targetTick = targetTick;
    ServerContextV1* context = activeServerContext();
    if (!context)
        return false;
    if (domain == (std::uint32_t)HistoryDomainV1::Player) {
        if (!context->players)
            return false;
        auto& players = *static_cast<std::unordered_map<std::uint32_t, ServerPlayer>*>(
            context->players);
        auto it = players.find((std::uint32_t)entityId);
        if (it == players.end())
            return false;
        bracketSamples(it->second.posHistory, targetTick, out);
        return true;
    }
    if (domain == (std::uint32_t)HistoryDomainV1::Npc) {
        if (!context->npcs)
            return false;
        auto& npcs = *static_cast<std::unordered_map<std::uint32_t, ServerNpc>*>(
            context->npcs);
        auto it = npcs.find((std::uint32_t)entityId);
        if (it == npcs.end())
            return false;
        bracketSamples(it->second.posHistory, targetTick, out);
        return true;
    }
    return false;
}

std::uint32_t serverHistoryQuery(void* /*host*/, GameHistoryQueryV1* query)
{
    if (!query)
        return 0;
    query->found = 0;
    query->handled = 0;
    GameHistorySelectV1 select{};
    if (!serverHistoryRawSamples(query->domain, query->entityId, query->tick, select))
        return 0;
    select.currentTick = query->currentTick;

    // Hot policy owns the selection; the kernel default (nearest older) is the
    // fallback when no handler responds.
    bool hotHandled = false;
    if (LiveBehavior::dispatchGameplayEvent64(
            GAME_EVENT_HISTORY_SELECT, &select, sizeof(select), query->currentTick, 0,
            0) &&
        select.handled)
        hotHandled = true;

    if (!hotHandled) {
        // Cold default: exact -> interpolate -> nearest older. This matches the
        // legacy getPlayerPoseAtTick/getNpcPoseAtTick semantics.
        if (select.exactTick && select.haveA) {
            select.selection = (std::uint32_t)HistorySelectionV1::ExactTick;
            select.fraction = 0.0f;
            for (int i = 0; i < 3; ++i) {
                select.position[i] = select.a.position[i];
                select.velocity[i] = select.a.velocity[i];
            }
            select.yaw = select.a.yaw;
        } else if (select.haveA && select.haveB) {
            if (select.a.logicalGenerationId != select.b.logicalGenerationId) {
                select.selection = (std::uint32_t)HistorySelectionV1::GenerationClamp;
                select.fraction = 0.0f;
                for (int i = 0; i < 3; ++i) {
                    select.position[i] = select.b.position[i];
                    select.velocity[i] = select.b.velocity[i];
                }
                select.yaw = select.b.yaw;
            } else {
                select.selection = (std::uint32_t)HistorySelectionV1::Interpolated;
                const float frac = select.b.tick > select.a.tick
                    ? (float)(query->tick - select.a.tick) /
                        (float)(select.b.tick - select.a.tick)
                    : 0.0f;
                select.fraction = frac;
                for (int i = 0; i < 3; ++i) {
                    select.position[i] =
                        select.a.position[i] +
                        (select.b.position[i] - select.a.position[i]) * frac;
                    select.velocity[i] =
                        select.a.velocity[i] +
                        (select.b.velocity[i] - select.a.velocity[i]) * frac;
                }
                select.yaw = select.a.yaw + (select.b.yaw - select.a.yaw) * frac;
            }
        } else if (select.haveA) {
            select.selection = (std::uint32_t)HistorySelectionV1::NearestOlder;
            select.fraction = 0.0f;
            for (int i = 0; i < 3; ++i) {
                select.position[i] = select.a.position[i];
                select.velocity[i] = select.a.velocity[i];
            }
            select.yaw = select.a.yaw;
        } else {
            return 0;
        }
    }

    query->handled = 1;
    query->found = 1;
    query->selection = select.selection;
    query->sampleTickA = select.a.tick;
    query->sampleTickB = select.b.tick;
    query->fraction = select.fraction;
    for (int i = 0; i < 3; ++i) {
        query->position[i] = select.position[i];
        query->velocity[i] = select.velocity[i];
    }
    query->yaw = select.yaw;
    query->generationA = select.a.logicalGenerationId;
    query->generationB = select.b.logicalGenerationId;
    return 1;
}

} // namespace MimitaNet
