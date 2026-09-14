// 09 14 2026
/* purpose
* A gamemode identity unknown when mimita.exe started. It declares its own
* match-state schema (HotTestMatchState), scores kills with a mode-defined
* multiplier, and owns its win condition through generic match capabilities.
* Proves live gamemode creation, selection, editing, and schema migration with
* no EXE enum, switch, or slot.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

#include <cstdint>

namespace {

const std::uint64_t kModeHotTest = gameHash("gamemode.hot-test");
const std::uint64_t kModeDomain = gameHash("mode.hottest");
const std::uint64_t kHotTestState = gameHash("HotTestMatchState");

struct HotTestMatchState {
    std::int32_t scoreLimit;      // kills (weighted) required to win
    std::int32_t score;           // accumulated weighted score
    float weirdMultiplier;        // mode-defined scoring rule
};

std::uint64_t currentMatch(GameplayContextV1* ctx)
{
    std::uint64_t match = 0;
    if (ctx && ctx->matchCurrent)
        ctx->matchCurrent(ctx->host, &match);
    return match;
}

bool readState(GameplayContextV1* ctx, std::uint64_t match, HotTestMatchState& out)
{
    if (!ctx || !ctx->dynamicReadComponent || match == 0)
        return false;
    return ctx->dynamicReadComponent(ctx->host, match, kHotTestState, &out, sizeof(out));
}

void writeState(GameplayContextV1* ctx, std::uint64_t match, const HotTestMatchState& state)
{
    if (ctx && ctx->dynamicWriteComponent && match != 0)
        ctx->dynamicWriteComponent(ctx->host, match, kHotTestState, &state, sizeof(state));
}

void ensureDefaults(HotTestMatchState& state)
{
    if (state.scoreLimit <= 0)
        state.scoreLimit = 5;
    if (state.weirdMultiplier <= 0.0f)
        state.weirdMultiplier = 2.0f;
}

void MIMITA_GAME_CALL onActorKilled(void* host, const GameEventV1* event)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    auto* killed = event ? static_cast<GameActorKilledV1*>(event->payload) : nullptr;
    if (!ctx || !killed)
        return;
    const std::uint64_t match = currentMatch(ctx);
    HotTestMatchState state{};
    if (!readState(ctx, match, state))
        ensureDefaults(state);
    ensureDefaults(state);
    int weight = static_cast<int>(state.weirdMultiplier);
    if (weight < 1)
        weight = 1;
    state.score += weight;
    writeState(ctx, match, state);
    killed->handled = 1;  // this mode owns scoring for the kill
    if (state.score >= state.scoreLimit && ctx->matchFinish)
        ctx->matchFinish(ctx->host, 1 /*actor*/, killed->killerId, 0 /*score*/);
}

void MIMITA_GAME_CALL onMatchEvaluate(void* host, const GameEventV1* event)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    auto* evaluate = event ? static_cast<GameMatchEvaluateV1*>(event->payload) : nullptr;
    if (!ctx || !evaluate)
        return;
    evaluate->handled = 1;
    HotTestMatchState state{};
    if (!readState(ctx, currentMatch(ctx), state))
        return;
    ensureDefaults(state);
    if (state.score >= state.scoreLimit) {
        evaluate->outEndMatch = 1;
        evaluate->outWinnerKind = 1;
        evaluate->outWinnerId = 1;  // mode-defined; the mode owns its rules
        evaluate->outVictoryType = 0;
    }
}

void MIMITA_GAME_CALL hotTestModeTick(void*, std::uint64_t, float) {}

} // namespace

const MimitaHotPackage::ModeRegistrar s_hotTestMode{
    {kModeHotTest, kModeDomain, kHotTestState, gameHash("HotTestMatchState.v1"),
     "Hot Test"}};
const MimitaHotPackage::EventRegistrar s_hotTestKilled{
    {gameHash("actor.killed"), gameHash("actor.killed.v1"), kModeDomain,
     onActorKilled, "hot-test.actor-killed"}};
const MimitaHotPackage::EventRegistrar s_hotTestEvaluate{
    {gameHash("match.evaluate"), gameHash("match.evaluate.v1"), kModeDomain,
     onMatchEvaluate, "hot-test.match-evaluate"}};
const MimitaHotPackage::SchemaRegistrar s_hotTestSchema{
    {kHotTestState, gameHash("HotTestMatchState.v1"), sizeof(HotTestMatchState), 4,
     GAME_COPY_AUTHORING, 0, "HotTestMatchState", 1, 0}};
const MimitaHotPackage::SystemRegistrar s_hotTestSystem{
    {gameHash("hot-test.mode-tick"), kModeDomain, 0, 0, hotTestModeTick,
     "hot-test.mode-tick"}};

#endif
