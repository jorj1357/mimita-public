// 09 14 2026
/* purpose
* Hot TDM gamemode. Owns participant team/role assignment and the complete match
* lifecycle: countdown -> active -> results -> intermission -> next round, plus
* score-limit, time-limit, tie handling, win/end condition, and respawn policy.
* It signals phase ownership with a generic MatchPhaseOwnership component so the
* cold state machine skips its own transitions. No EXE mode enum or mode switch.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"
#include "network/match-lifecycle.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace {

const std::uint64_t kModeTdm = gameHash("tdm");
const std::uint64_t kModeDomain = gameHash("mode.tdm");
const std::uint64_t kTdmState = gameHash("TdmMatchState");
const std::uint64_t kPhaseOwnership = gameHash("MatchPhaseOwnership");
const std::uint64_t kHealthState = gameHash("ActorHealthState");
const std::uint64_t kTeamState = gameHash("ActorTeamState");
const std::uint64_t kRoleState = gameHash("ActorRoleState");

// DuelStatePhase wire values (see network/packets.h).
constexpr std::uint32_t kPhaseCountdown = 1;
constexpr std::uint32_t kPhaseActive = 2;
constexpr std::uint32_t kPhaseIntermission = 4;
constexpr std::uint32_t kPhaseResults = 6;

constexpr float kCountdownSeconds = 3.0f;
constexpr float kResultsSeconds = 8.0f;
constexpr float kIntermissionSeconds = 10.0f;

struct ActorTeamStateV1 {
    std::int32_t team;
    std::uint32_t reserved;
};
struct ActorRoleStateV1 {
    std::uint64_t roleHash;
    std::uint32_t roleIndex;
    std::uint32_t reserved;
};
struct TdmMatchState {
    std::uint32_t phase;
    float timer;
    std::int32_t scoreLimit;
    std::int32_t redScore;
    std::int32_t blueScore;
    std::uint32_t round;
    std::uint32_t timeLimitSeconds;
    std::uint64_t roundStartTick;
};

std::uint64_t currentMatch(GameplayContextV1* ctx)
{
    std::uint64_t match = 0;
    if (ctx && ctx->matchCurrent)
        ctx->matchCurrent(ctx->host, &match);
    return match;
}

bool readState(GameplayContextV1* ctx, std::uint64_t match, TdmMatchState& out)
{
    if (!ctx || !ctx->dynamicReadComponent || match == 0)
        return false;
    return ctx->dynamicReadComponent(ctx->host, match, kTdmState, &out, sizeof(out));
}

void writeState(GameplayContextV1* ctx, std::uint64_t match, const TdmMatchState& s)
{
    if (ctx && ctx->dynamicWriteComponent && match != 0)
        ctx->dynamicWriteComponent(ctx->host, match, kTdmState, &s, sizeof(s));
}

void writeTeam(GameplayContextV1* ctx, std::uint64_t entity, std::int32_t team)
{
    ActorTeamStateV1 s{};
    s.team = team;
    ctx->dynamicWriteComponent(ctx->host, entity, kTeamState, &s, sizeof(s));
}

void writeRole(GameplayContextV1* ctx, std::uint64_t entity, std::uint64_t roleHash)
{
    ActorRoleStateV1 r{};
    r.roleHash = roleHash;
    ctx->dynamicWriteComponent(ctx->host, entity, kRoleState, &r, sizeof(r));
}

// Deterministic participant team/role assignment (sorted by stable EntityId).
void assignParticipants(GameplayContextV1* ctx)
{
    if (!ctx || !ctx->dynamicEnumerateComponent)
        return;
    std::uint64_t actors[64] = {0};
    const std::uint32_t count =
        ctx->dynamicEnumerateComponent(ctx->host, kHealthState, actors, 64);
    std::vector<std::uint64_t> ordered(actors, actors + count);
    std::sort(ordered.begin(), ordered.end());
    const std::uint64_t soldierRole = gameHash("role.soldier");
    for (std::size_t i = 0; i < ordered.size(); ++i) {
        writeTeam(ctx, ordered[i], static_cast<std::int32_t>(i % 2));
        writeRole(ctx, ordered[i], soldierRole);
    }
}

void ensureDefaults(TdmMatchState& s, std::uint64_t tick)
{
    if (s.scoreLimit <= 0)
        s.scoreLimit = 20;
    if (s.timeLimitSeconds == 0)
        s.timeLimitSeconds = 600;
    if (s.phase == 0)
        s.phase = kPhaseCountdown;
    if (s.round == 0)
        s.round = 1;
    if (s.roundStartTick == 0)
        s.roundStartTick = tick;
}

void MIMITA_GAME_CALL tdmModeTick(void* host, std::uint64_t tick, float dt)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx)
        return;
    const std::uint64_t match = currentMatch(ctx);
    if (match == 0)
        return;
    TdmMatchState state{};
    if (!readState(ctx, match, state)) {
        state.phase = kPhaseCountdown;
        state.timer = kCountdownSeconds;
        assignParticipants(ctx);
    }
    ensureDefaults(state, tick);

    switch (state.phase) {
    case kPhaseCountdown:
        state.timer -= dt;
        if (state.timer <= 0.0f) {
            state.phase = kPhaseActive;
            state.timer = 0.0f;
            state.roundStartTick = tick;
            if (ctx->matchSetPhase)
                ctx->matchSetPhase(ctx->host, kPhaseActive);
        }
        break;
    case kPhaseActive:
        if (ctx->matchActorTeamRead) {
            // (no-op; kept for clarity)
        }
        if (state.timeLimitSeconds > 0 && tick > state.roundStartTick &&
            (tick - state.roundStartTick) >=
                (std::uint64_t)state.timeLimitSeconds * 60u) {
            const std::uint32_t winner = state.redScore >= state.blueScore ? 0u : 1u;
            if (ctx->matchFinish)
                ctx->matchFinish(ctx->host, 2 /*team*/, winner, 1 /*time*/);
            state.phase = kPhaseResults;
            state.timer = kResultsSeconds;
        }
        break;
    case kPhaseResults:
        state.timer -= dt;
        if (state.timer <= 0.0f) {
            state.phase = kPhaseIntermission;
            state.timer = kIntermissionSeconds;
            if (ctx->matchSetPhase)
                ctx->matchSetPhase(ctx->host, kPhaseIntermission);
        }
        break;
    case kPhaseIntermission:
        state.timer -= dt;
        if (state.timer <= 0.0f) {
            state.round += 1;
            state.redScore = 0;
            state.blueScore = 0;
            state.phase = kPhaseCountdown;
            state.timer = kCountdownSeconds;
            assignParticipants(ctx);
            if (ctx->matchSetPhase)
                ctx->matchSetPhase(ctx->host, kPhaseCountdown);
        }
        break;
    default:
        state.phase = kPhaseCountdown;
        state.timer = kCountdownSeconds;
        break;
    }

    writeState(ctx, match, state);
    std::uint32_t owned = 1;
    if (ctx->dynamicWriteComponent)
        ctx->dynamicWriteComponent(ctx->host, match, kPhaseOwnership, &owned,
                                   sizeof(owned));
}

void MIMITA_GAME_CALL onActorKilled(void* host, const GameEventV1* event)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    auto* killed = event ? static_cast<GameActorKilledV1*>(event->payload) : nullptr;
    if (!ctx || !killed)
        return;
    const std::uint64_t match = currentMatch(ctx);
    TdmMatchState state{};
    readState(ctx, match, state);
    ensureDefaults(state, killed->tick);
    if (state.phase != kPhaseActive) {
        killed->handled = 1;  // TDM owns scoring even when no points are awarded
        return;
    }
    std::int32_t killerTeam = -1;
    if (ctx->matchActorTeamRead)
        ctx->matchActorTeamRead(ctx->host, killed->killerId, &killerTeam);
    if (killerTeam == 0)
        state.redScore += 1;
    else if (killerTeam == 1)
        state.blueScore += 1;

    killed->handled = 1;
    if (state.redScore >= state.scoreLimit || state.blueScore >= state.scoreLimit) {
        const std::uint32_t winner = state.redScore >= state.blueScore ? 0u : 1u;
        if (ctx->matchFinish)
            ctx->matchFinish(ctx->host, 2 /*team*/, winner, 0 /*score*/);
        state.phase = kPhaseResults;
        state.timer = kResultsSeconds;
    }
    writeState(ctx, match, state);
}

void MIMITA_GAME_CALL onMatchEvaluate(void* host, const GameEventV1* event)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    auto* evaluate = event ? static_cast<GameMatchEvaluateV1*>(event->payload) : nullptr;
    if (!ctx || !evaluate)
        return;
    evaluate->handled = 1;
    TdmMatchState state{};
    if (!readState(ctx, currentMatch(ctx), state))
        return;
    ensureDefaults(state, evaluate->tick);
    if (state.redScore >= state.scoreLimit || state.blueScore >= state.scoreLimit) {
        evaluate->outEndMatch = 1;
        evaluate->outWinnerKind = 2;
        evaluate->outWinnerId = state.redScore >= state.blueScore ? 0u : 1u;
        evaluate->outVictoryType = 0;
    }
}

void MIMITA_GAME_CALL onMatchLifecycle(void* /*host*/, const GameEventV1* event)
{
    auto* p = event ? static_cast<GameMatchLifecycleV1*>(event->payload) : nullptr;
    if (!p)
        return;
    p->handled = 1;
    p->outCountdownSeconds = kCountdownSeconds;
    p->outGoSeconds = 0.5f;
    p->outIntermissionSeconds = kIntermissionSeconds;
    p->outResultsSeconds = kResultsSeconds;
    p->outTimeLimitSeconds = 600.0f;
    p->outRespawnsEnabled = 1u;
    p->outRespawnSeconds = 5.0f;
}

} // namespace

const MimitaHotPackage::ModeRegistrar s_tdmMode{
    {kModeTdm, kModeDomain, kTdmState, gameHash("TdmMatchState.v1"), "TDM"}};
const MimitaHotPackage::SchemaRegistrar s_tdmSchema{
    {kTdmState, gameHash("TdmMatchState.v1"), sizeof(TdmMatchState), 8,
     GAME_COPY_AUTHORING, 0, "TdmMatchState", 1, 0}};
const MimitaHotPackage::SchemaRegistrar s_tdmPhaseOwnership{
    {kPhaseOwnership, gameHash("MatchPhaseOwnership.v1"), 4, 4, GAME_COPY_AUTHORING,
     0, "MatchPhaseOwnership", 1, 0}};
// Generic actor team/role schemas: the hot mode owns assignment, so it must be
// able to write them even when the cold assignment path did not run.
const MimitaHotPackage::SchemaRegistrar s_tdmTeamSchema{
    {kTeamState, gameHash("ActorTeamState.v1"), sizeof(ActorTeamStateV1), 4,
     GAME_COPY_AUTHORING, GAME_NET_ALL, "ActorTeamState", 1, 0}};
const MimitaHotPackage::SchemaRegistrar s_tdmRoleSchema{
    {kRoleState, gameHash("ActorRoleState.v1"), sizeof(ActorRoleStateV1), 8,
     GAME_COPY_AUTHORING, GAME_NET_ALL, "ActorRoleState", 1, 0}};
const MimitaHotPackage::SystemRegistrar s_tdmSystem{
    {gameHash("tdm.mode-tick"), kModeDomain, 0, 0, tdmModeTick, "tdm.mode-tick"}};
const MimitaHotPackage::EventRegistrar s_tdmKilled{
    {gameHash("actor.killed"), gameHash("actor.killed.v1"), kModeDomain,
     onActorKilled, "tdm.actor-killed"}};
const MimitaHotPackage::EventRegistrar s_tdmEvaluate{
    {gameHash("match.evaluate"), gameHash("match.evaluate.v1"), kModeDomain,
     onMatchEvaluate, "tdm.match-evaluate"}};
const MimitaHotPackage::EventRegistrar s_tdmLifecycle{
    {GAME_EVENT_MATCH_LIFECYCLE, 0, kModeDomain, onMatchLifecycle,
     "tdm.match-lifecycle"}};

#endif
