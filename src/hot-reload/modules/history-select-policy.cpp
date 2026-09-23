// 09 23 2026
/* purpose
* history.select: the ONE hot lag-compensation selection policy. Given the raw
* bracketing samples from the EXE-owned history store, it owns the
* interpolate / nearest / cross-generation clamp decision and the produced pose.
* Previously this ordering was hardcoded in the cold getPlayerPoseAtTick /
* getNpcPoseAtTick; moving it hot lets rewind ordering change live.
* Context-free: plain samples only; no Player/Npc/pointer types.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-history.h"

namespace {

void MIMITA_GAME_CALL onHistorySelect(void* /*host*/, const GameEventV1* event)
{
    auto* p = event ? static_cast<MimitaNet::GameHistorySelectV1*>(event->payload)
                    : nullptr;
    if (!p)
        return;

    p->handled = 0u;
    p->fraction = 0.0f;

    if (p->exactTick && p->haveA) {
        p->selection = (std::uint32_t)MimitaNet::HistorySelectionV1::ExactTick;
        for (int i = 0; i < 3; ++i) {
            p->position[i] = p->a.position[i];
            p->velocity[i] = p->a.velocity[i];
        }
        p->yaw = p->a.yaw;
        p->handled = 1u;
        return;
    }

    if (p->haveA && p->haveB) {
        // Generation boundary: never blend incompatible behavior generations;
        // clamp to the newer authoritative sample (matches the cold semantics).
        if (p->a.logicalGenerationId != p->b.logicalGenerationId) {
            p->selection =
                (std::uint32_t)MimitaNet::HistorySelectionV1::GenerationClamp;
            for (int i = 0; i < 3; ++i) {
                p->position[i] = p->b.position[i];
                p->velocity[i] = p->b.velocity[i];
            }
            p->yaw = p->b.yaw;
            p->handled = 1u;
            return;
        }
        p->selection = (std::uint32_t)MimitaNet::HistorySelectionV1::Interpolated;
        const float frac = p->b.tick > p->a.tick
            ? (float)(p->targetTick - p->a.tick) / (float)(p->b.tick - p->a.tick)
            : 0.0f;
        p->fraction = frac;
        for (int i = 0; i < 3; ++i) {
            p->position[i] =
                p->a.position[i] + (p->b.position[i] - p->a.position[i]) * frac;
            p->velocity[i] =
                p->a.velocity[i] + (p->b.velocity[i] - p->a.velocity[i]) * frac;
        }
        p->yaw = p->a.yaw + (p->b.yaw - p->a.yaw) * frac;
        p->handled = 1u;
        return;
    }

    if (p->haveA) {
        p->selection = (std::uint32_t)MimitaNet::HistorySelectionV1::NearestOlder;
        for (int i = 0; i < 3; ++i) {
            p->position[i] = p->a.position[i];
            p->velocity[i] = p->a.velocity[i];
        }
        p->yaw = p->a.yaw;
        p->handled = 1u;
        return;
    }
}

} // namespace

const MimitaHotPackage::EventRegistrar s_historySelectRegistration{
    {MimitaNet::GAME_EVENT_HISTORY_SELECT, 0, 0, onHistorySelect,
     "history.select"}};

#endif
