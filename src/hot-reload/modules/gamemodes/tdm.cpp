// 09 14 2026
/* purpose
* Hot TDM gamemode: registers the `tdm` mode descriptor and owns the match
* lifecycle policy (countdown/intermission/results durations and respawn rule)
* in the hot mode domain through the generic match.lifecycle event. No EXE enum,
* switch, or slot. Team scoring remains kernel-side for now.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"
#include "network/match-lifecycle.h"

#include <cstdint>

namespace {

const std::uint64_t kModeTdm = gameHash("tdm");
const std::uint64_t kModeDomain = gameHash("mode.tdm");
const std::uint64_t kTdmState = gameHash("TdmMatchState");

struct TdmMatchState {
    std::int32_t scoreLimit;
    std::int32_t redScore;
    std::int32_t blueScore;
};

void MIMITA_GAME_CALL tdmModeTick(void*, std::uint64_t, float) {}

void MIMITA_GAME_CALL onMatchLifecycle(void* /*host*/, const GameEventV1* event)
{
    auto* p = event ? static_cast<GameMatchLifecycleV1*>(event->payload) : nullptr;
    if (!p)
        return;
    p->handled = 1;
    p->outCountdownSeconds = 3.0f;
    p->outGoSeconds = 1.0f;
    p->outIntermissionSeconds = 15.0f;
    p->outResultsSeconds = 8.0f;
    p->outTimeLimitSeconds = 600.0f;
    p->outRespawnsEnabled = 1u;
    p->outRespawnSeconds = 5.0f;
}

} // namespace

const MimitaHotPackage::ModeRegistrar s_tdmMode{
    {kModeTdm, kModeDomain, kTdmState, gameHash("TdmMatchState.v1"), "TDM"}};
const MimitaHotPackage::SchemaRegistrar s_tdmSchema{
    {kTdmState, gameHash("TdmMatchState.v1"), sizeof(TdmMatchState), 4,
     GAME_COPY_AUTHORING, 0, "TdmMatchState", 1, 0}};
const MimitaHotPackage::EventRegistrar s_tdmLifecycle{
    {GAME_EVENT_MATCH_LIFECYCLE, 0, kModeDomain, onMatchLifecycle,
     "tdm.match-lifecycle"}};
const MimitaHotPackage::SystemRegistrar s_tdmSystem{
    {gameHash("tdm.mode-tick"), kModeDomain, 0, 0, tdmModeTick, "tdm.mode-tick"}};

#endif
