// 09 14 2026
/* purpose
* Hot FFA gamemode: authoritative scoring and score-limit win condition live in
* a package system/event set, not in the kernel. Registers a mode descriptor, a
* match-state dynamic schema, domain-scoped events, and the score-snapshot
* capability the legacy packet bridge reads. No EXE enum, switch, or slot.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"
#include "network/match-lifecycle.h"

#include <cstdint>
#include <cstdio>

namespace {

const std::uint64_t kModeFfa = gameHash("ffa");
const std::uint64_t kModeDomain = gameHash("mode.ffa");
const std::uint64_t kFfaState = gameHash("FfaMatchState");
constexpr int kMaxFfaActors = 32;

struct FfaEntry {
    std::uint32_t playerId;
    std::int32_t score;
    std::int32_t deaths;
};

struct FfaMatchState {
    std::int32_t scoreLimit;
    std::int32_t count;
    FfaEntry entries[kMaxFfaActors];
};

std::uint64_t currentMatch(GameplayContextV1* ctx)
{
    std::uint64_t match = 0;
    if (ctx && ctx->matchCurrent)
        ctx->matchCurrent(ctx->host, &match);
    return match;
}

bool readState(GameplayContextV1* ctx, std::uint64_t match, FfaMatchState& out)
{
    if (!ctx || !ctx->dynamicReadComponent || match == 0)
        return false;
    return ctx->dynamicReadComponent(ctx->host, match, kFfaState, &out, sizeof(out));
}

void writeState(GameplayContextV1* ctx, std::uint64_t match, const FfaMatchState& state)
{
    if (ctx && ctx->dynamicWriteComponent && match != 0)
        ctx->dynamicWriteComponent(ctx->host, match, kFfaState, &state, sizeof(state));
}

int findEntry(const FfaMatchState& state, std::uint32_t id)
{
    for (int i = 0; i < state.count; ++i) {
        if (state.entries[i].playerId == id)
            return i;
    }
    return -1;
}

int ensureEntry(FfaMatchState& state, std::uint32_t id)
{
    int index = findEntry(state, id);
    if (index >= 0)
        return index;
    if (state.count >= kMaxFfaActors)
        return -1;
    index = state.count++;
    state.entries[index] = FfaEntry{id, 0, 0};
    return index;
}

void ensureDefaults(FfaMatchState& state)
{
    if (state.scoreLimit <= 0)
        state.scoreLimit = 20;
    if (state.count < 0 || state.count > kMaxFfaActors)
        state.count = 0;
}

void MIMITA_GAME_CALL onActorKilled(void* host, const GameEventV1* event)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    auto* killed = event ? static_cast<GameActorKilledV1*>(event->payload) : nullptr;
    if (!ctx || !killed)
        return;
    const std::uint64_t match = currentMatch(ctx);
    FfaMatchState state{};
    if (!readState(ctx, match, state))
        ensureDefaults(state);
    ensureDefaults(state);
    const int killer = ensureEntry(state, killed->killerId);
    if (killer >= 0)
        state.entries[killer].score += 1;
    const int victim = ensureEntry(state, killed->victimId);
    if (victim >= 0)
        state.entries[victim].deaths += 1;
    writeState(ctx, match, state);
    killed->handled = 1;  // FFA owns scoring for this kill
    if (killer >= 0 && state.entries[killer].score >= state.scoreLimit &&
        ctx->matchFinish) {
        ctx->matchFinish(ctx->host, 1 /*actor*/, killed->killerId, 0 /*score*/);
    }
}

void MIMITA_GAME_CALL onMatchEvaluate(void* host, const GameEventV1* event)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    auto* evaluate = event ? static_cast<GameMatchEvaluateV1*>(event->payload) : nullptr;
    if (!ctx || !evaluate)
        return;
    evaluate->handled = 1;  // FFA owns its win decision while its domain is active
    FfaMatchState state{};
    if (!readState(ctx, currentMatch(ctx), state))
        return;
    ensureDefaults(state);
    int best = -1;
    std::uint32_t bestId = 0;
    for (int i = 0; i < state.count; ++i) {
        if (state.entries[i].score > best) {
            best = state.entries[i].score;
            bestId = state.entries[i].playerId;
        }
    }
    if (best >= state.scoreLimit && bestId != 0) {
        evaluate->outEndMatch = 1;
        evaluate->outWinnerKind = 1;
        evaluate->outWinnerId = bestId;
        evaluate->outVictoryType = 0;
    }
}

void MIMITA_GAME_CALL scoreSnapshot(void* host, GameMatchScoreSnapshotV1* out)
{
    if (!out)
        return;
    *out = GameMatchScoreSnapshotV1{};
    auto* ctx = static_cast<GameplayContextV1*>(host);
    FfaMatchState state{};
    if (!readState(ctx, currentMatch(ctx), state))
        return;
    ensureDefaults(state);
    std::uint32_t count = static_cast<std::uint32_t>(
        state.count < 0 ? 0 : (state.count > kMaxFfaActors ? kMaxFfaActors : state.count));
    out->count = count;
    for (std::uint32_t i = 0; i < count; ++i) {
        out->entries[i].ownerId = state.entries[i].playerId;
        out->entries[i].score = state.entries[i].score;
        out->entries[i].deaths = state.entries[i].deaths;
        out->entries[i].kind = 0;
    }
}

void MIMITA_GAME_CALL ffaModeTick(void*, std::uint64_t, float) {}

// FFA owns its match lifecycle policy: countdown/intermission/results durations
// and the respawn rule are decided here, in the hot mode domain. Edit this file
// live to change FFA lifecycle behavior with no cold build.
void MIMITA_GAME_CALL onMatchLifecycle(void* /*host*/, const GameEventV1* event)
{
    auto* p = event ? static_cast<GameMatchLifecycleV1*>(event->payload) : nullptr;
    if (!p)
        return;
    p->handled = 1;
    p->outCountdownSeconds = 3.0f;
    p->outGoSeconds = 1.0f;
    p->outIntermissionSeconds = 10.0f;
    p->outResultsSeconds = 8.0f;
    p->outTimeLimitSeconds = 300.0f;
    p->outRespawnsEnabled = 1u;
    p->outRespawnSeconds = 2.5f;
}

} // namespace

const MimitaHotPackage::ModeRegistrar s_ffaMode{
    {kModeFfa, kModeDomain, kFfaState, gameHash("FfaMatchState.v1"), "FFA"}};
const MimitaHotPackage::EventRegistrar s_ffaKilled{
    {gameHash("actor.killed"), gameHash("actor.killed.v1"), kModeDomain,
     onActorKilled, "ffa.actor-killed"}};
const MimitaHotPackage::EventRegistrar s_ffaEvaluate{
    {gameHash("match.evaluate"), gameHash("match.evaluate.v1"), kModeDomain,
     onMatchEvaluate, "ffa.match-evaluate"}};
const MimitaHotPackage::SchemaRegistrar s_ffaSchema{
    {kFfaState, gameHash("FfaMatchState.v1"), sizeof(FfaMatchState), 4,
     GAME_COPY_AUTHORING, 0, "FfaMatchState", 1, 0}};
const MimitaHotPackage::CapabilityRegistrar s_ffaSnapshot{
    {gameHash("match.score.snapshot"), 0, gameHash("match.score.snapshot.v1"),
     reinterpret_cast<void*>(&scoreSnapshot), "ffa.score-snapshot"}};
const MimitaHotPackage::SystemRegistrar s_ffaSystem{
    {gameHash("ffa.mode-tick"), kModeDomain, 0, 0, ffaModeTick, "ffa.mode-tick"}};
const MimitaHotPackage::EventRegistrar s_ffaLifecycle{
    {GAME_EVENT_MATCH_LIFECYCLE, 0, kModeDomain, onMatchLifecycle,
     "ffa.match-lifecycle"}};

#endif
