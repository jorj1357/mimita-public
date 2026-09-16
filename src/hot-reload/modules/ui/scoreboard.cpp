// 09 15 2026
/* purpose
* Hot scoreboard. Enumerates generic actors carrying ActorMatchStatsState and
* joins ActorIdentityState (name) + ActorTeamState (team) by EntityId, sorts by
* score, and emits rows through render.ui. No table ABI, no Player* identity.
* Claims the scoreboard only when it actually emitted rows, so the cold
* MatchLeaderboard remains the live owner until generic stats are projected.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-ui.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>

namespace {

const std::uint64_t kScreenScoreboard = gameHash("screen.scoreboard");
const std::uint64_t kIdentityState = gameHash("ActorIdentityState");
const std::uint64_t kTeamState = gameHash("ActorTeamState");

struct IdentityV1 {
    char name[32];
    std::uint32_t reserved;
};
struct TeamV1 {
    std::int32_t team;
    std::uint32_t reserved;
};

using RenderUiFn = void (MIMITA_GAME_CALL *)(void*, const GameUiCommandV1*);

GameSharedStateV1* sharedState(GameplayContextV1* ctx)
{
    if (!ctx || !ctx->permanentStorage ||
        ctx->permanentStorageSize < sizeof(GameSharedStateV1))
        return nullptr;
    GameSharedStateV1* shared =
        reinterpret_cast<GameSharedStateV1*>(ctx->permanentStorage);
    return shared->magic == GAME_SHARED_MAGIC ? shared : nullptr;
}

struct Row {
    std::uint64_t entity;
    char name[32];
    std::int32_t team;
    std::int32_t score;
    std::uint32_t flags;
};

void emitPanel(RenderUiFn ui, void* host, float x, float y, float w, float h,
               float r, float g, float b, float a)
{
    GameUiCommandV1 c{};
    c.kind = GAME_UI_PANEL;
    c.x = x; c.y = y; c.w = w; c.h = h;
    c.color[0] = r; c.color[1] = g; c.color[2] = b; c.color[3] = a;
    ui(host, &c);
}

void emitText(RenderUiFn ui, void* host, const char* text, float x, float y,
              float scale, float r, float g, float b, float a)
{
    GameUiCommandV1 c{};
    c.kind = GAME_UI_TEXT;
    c.x = x; c.y = y; c.scale = scale;
    c.color[0] = r; c.color[1] = g; c.color[2] = b; c.color[3] = a;
    std::snprintf(c.text, sizeof(c.text), "%s", text);
    ui(host, &c);
}

void MIMITA_GAME_CALL scoreboardTick(void* host, std::uint64_t /*tick*/,
                                     float /*dt*/)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->resolveCapability || !ctx->dynamicEnumerateComponent ||
        !ctx->dynamicReadComponent)
        return;
    auto ui = reinterpret_cast<RenderUiFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_RENDER_UI));
    if (!ui)
        return;

    // Visibility policy: only compose while the generic show state is active
    // (cold input bridged Tab hold into this). Otherwise the cold owner stays.
    if (!ctx->dynamicReadComponent)
        return;
    GameSharedStateV1* visShared = sharedState(ctx);
    const std::uint64_t visEntity =
        visShared ? visShared->localPlayerEntity : 0;
    HotScoreboardVisibleV1 vis{};
    if (visEntity == 0 ||
        !ctx->dynamicReadComponent(ctx->host, visEntity,
                                   HOT_SCOREBOARD_VISIBLE_COMPONENT, &vis,
                                   sizeof(vis)) ||
        vis.visible == 0)
        return;

    std::uint64_t entities[64];
    const std::uint32_t count = ctx->dynamicEnumerateComponent(
        ctx->host, HOT_ACTOR_STATS_COMPONENT, entities, 64);
    if (count == 0)
        return;   // no generic stats: cold MatchLeaderboard stays the owner

    Row rows[64];
    std::uint32_t n = 0;
    for (std::uint32_t i = 0; i < count && n < 64; ++i) {
        HotActorMatchStatsV1 st{};
        if (!ctx->dynamicReadComponent(ctx->host, entities[i],
                                       HOT_ACTOR_STATS_COMPONENT, &st, sizeof(st)))
            continue;
        IdentityV1 id{};
        if (!ctx->dynamicReadComponent(ctx->host, entities[i], kIdentityState,
                                       &id, sizeof(id)) ||
            id.name[0] == '\0')
            continue;   // skip safely; no stale row
        TeamV1 team{};
        ctx->dynamicReadComponent(ctx->host, entities[i], kTeamState, &team,
                                  sizeof(team));
        rows[n].entity = entities[i];
        std::snprintf(rows[n].name, sizeof(rows[n].name), "%s", id.name);
        rows[n].team = team.team;
        rows[n].score = st.score;
        rows[n].flags = st.flags;
        ++n;
    }
    if (n == 0)
        return;

    // Hot sorting: score descending (shipping semantics).
    std::sort(rows, rows + n, [](const Row& a, const Row& b) {
        return a.score > b.score;
    });

    emitPanel(ui, ctx->host, 420.0f, 100.0f, 440.0f, 40.0f + n * 26.0f, 0.0f,
              0.0f, 0.0f, 0.6f);
    emitText(ui, ctx->host, "SCOREBOARD", 440.0f, 110.0f, 0.4f, 1.0f, 1.0f,
             1.0f, 1.0f);
    float y = 150.0f;
    for (std::uint32_t i = 0; i < n; ++i) {
        const Row& r = rows[i];
        const bool local = (r.flags & 1u) != 0;
        const float tr = r.team == 1 ? 0.35f : 0.95f;
        const float tg = r.team == 1 ? 0.55f : 0.45f;
        const float tb = r.team == 1 ? 1.0f : 0.45f;
        emitText(ui, ctx->host, r.name, 440.0f, y, 0.34f, tr, tg, tb,
                 local ? 1.0f : 0.85f);
        char score[32];
        std::snprintf(score, sizeof(score), "%d", r.score);
        emitText(ui, ctx->host, score, 780.0f, y, 0.34f, 1.0f, 1.0f, 1.0f,
                 local ? 1.0f : 0.85f);
        y += 26.0f;
    }

    // Claim only when we actually composed rows (no live regression while the
    // cold projection of stats is not yet wired).
    GameSharedStateV1* shared = sharedState(ctx);
    const std::uint64_t e = shared ? shared->localPlayerEntity : 0;
    if (e != 0 && ctx->dynamicWriteComponent) {
        HotUiClaimV1 claim{};
        claim.screenId = kScreenScoreboard;
        claim.owned = 1;
        ctx->dynamicWriteComponent(ctx->host, e, HOT_UI_CLAIM_COMPONENT, &claim,
                                   sizeof(claim));
    }
}

const MimitaHotPackage::SchemaRegistrar s_scoreboardVisibleSchema{
    {HOT_SCOREBOARD_VISIBLE_COMPONENT, gameHash("ScoreboardVisible.v1"),
     sizeof(HotScoreboardVisibleV1), 4, GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE,
     "ScoreboardVisible", 1, 0}};
const MimitaHotPackage::SchemaRegistrar s_actorStatsSchema{
    {HOT_ACTOR_STATS_COMPONENT, gameHash("ActorMatchStatsState.v1"),
     sizeof(HotActorMatchStatsV1), 4, GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE,
     "ActorMatchStatsState", 1, 0}};
const MimitaHotPackage::SystemRegistrar s_scoreboardSystem{
    {gameHash("hot.scoreboard"), GAME_DOMAIN_UI, 14, 0, scoreboardTick,
     "hot.scoreboard"}};

} // namespace

#endif
