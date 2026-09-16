// 09 16 2026
/* purpose
* Hot server-browser composition. Enumerates generic ServerListingState entities
* (projected cold from discovery), sorts by hot policy, and emits rows + Refresh/
* Connect/Back through render.ui. Discovery/sockets/ping/connect stay cold via the
* generic pending-action bridge. No browser or socket object is exposed to hot
* code.
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

const std::uint64_t kScreenBrowser = gameHash("screen.server-browser");
const std::uint64_t kScreenMain = gameHash("screen.main-menu");
const std::uint64_t kBack = gameHash("menu.back");
const std::uint64_t kRefresh = gameHash("serverbrowser.refresh");
const std::uint64_t kJoinCode = gameHash("serverbrowser.join-code");

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

std::uint64_t readScreen(GameplayContextV1* ctx)
{
    GameSharedStateV1* shared = sharedState(ctx);
    const std::uint64_t e = shared ? shared->localPlayerEntity : 0;
    if (e == 0 || !ctx->dynamicReadComponent)
        return 0;
    HotUiNavigationStateV1 nav{};
    if (!ctx->dynamicReadComponent(ctx->host, e, HOT_UI_NAV_COMPONENT, &nav,
                                   sizeof(nav)))
        return 0;
    return nav.screenId;
}

void emitButton(RenderUiFn ui, void* host, std::uint64_t id, const char* label,
                float x, float y, float w, float h, float r, float g, float b)
{
    GameUiCommandV1 c{};
    c.kind = GAME_UI_BUTTON;
    c.elementId = id;
    c.x = x; c.y = y; c.w = w; c.h = h;
    c.scale = 0.34f;
    c.color[0] = r; c.color[1] = g; c.color[2] = b; c.color[3] = 1.0f;
    std::snprintf(c.text, sizeof(c.text), "%s", label);
    ui(host, &c);
}

void emitText(RenderUiFn ui, void* host, const char* text, float x, float y,
              float scale, float r, float g, float b)
{
    GameUiCommandV1 c{};
    c.kind = GAME_UI_TEXT;
    c.x = x; c.y = y; c.scale = scale;
    c.color[0] = r; c.color[1] = g; c.color[2] = b; c.color[3] = 1.0f;
    std::snprintf(c.text, sizeof(c.text), "%s", text);
    ui(host, &c);
}

void MIMITA_GAME_CALL browserTick(void* host, std::uint64_t /*tick*/,
                                  float /*dt*/)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->resolveCapability || !ctx->dynamicEnumerateComponent ||
        !ctx->dynamicReadComponent || !ctx->dynamicWriteComponent)
        return;
    if (readScreen(ctx) != kScreenBrowser)
        return;
    auto ui = reinterpret_cast<RenderUiFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_RENDER_UI));
    if (!ui)
        return;

    GameUiCommandV1 bg{};
    bg.kind = GAME_UI_PANEL;
    bg.x = 0.0f; bg.y = 0.0f; bg.w = 1280.0f; bg.h = 720.0f;
    bg.color[0] = 0.04f; bg.color[1] = 0.05f; bg.color[2] = 0.08f; bg.color[3] = 1.0f;
    ui(ctx->host, &bg);
    emitText(ui, ctx->host, "SERVERS", 40.0f, 30.0f, 0.7f, 1.0f, 1.0f, 1.0f);

    std::uint64_t entities[128];
    const std::uint32_t count = ctx->dynamicEnumerateComponent(
        ctx->host, HOT_SERVER_LISTING_COMPONENT, entities, 128);

    struct Row {
        int ping;
        std::uint64_t listingId;
        char text[128];
        char code[16];
    };
    Row rows[128];
    std::uint32_t n = 0;
    for (std::uint32_t i = 0; i < count && n < 128; ++i) {
        HotServerListingV1 l{};
        if (!ctx->dynamicReadComponent(ctx->host, entities[i],
                                       HOT_SERVER_LISTING_COMPONENT, &l,
                                       sizeof(l)))
            continue;
        rows[n].ping = (l.flags & HOT_SERVER_LISTING_REACHABLE) ? l.pingMs : 99999;
        rows[n].listingId = l.listingId;
        std::snprintf(rows[n].text, sizeof(rows[n].text),
                      "%-28s %2d/%-2d  %s", l.name, l.players, l.maxPlayers,
                      l.mode);
        std::snprintf(rows[n].code, sizeof(rows[n].code), "%s", l.code);
        ++n;
    }
    // Hot sorting: ping ascending (unreachable last). Shipping also offers more
    // modes; this is the default hot policy.
    std::sort(rows, rows + n, [](const Row& a, const Row& b) {
        return a.ping < b.ping;
    });
    if (n == 0)
        emitText(ui, ctx->host, "Searching...", 40.0f, 120.0f, 0.4f, 0.7f, 0.7f,
                 0.7f);
    float y = 90.0f;
    for (std::uint32_t i = 0; i < n; ++i) {
        char rowText[160];
        const int ping = rows[i].ping >= 99999 ? -1 : rows[i].ping;
        std::snprintf(rowText, sizeof(rowText), "%s  ping=%d", rows[i].text, ping);
        emitText(ui, ctx->host, rowText, 40.0f, y, 0.32f, 0.9f, 0.9f, 0.9f);
        // Connect button element id = the stable listing id; the action handler
        // resolves it back to the cold room code (no pointer, no string in ABI).
        emitButton(ui, ctx->host, rows[i].listingId, "JOIN", 900.0f, y - 4.0f,
                   120.0f, 24.0f, 0.2f, 0.6f, 0.3f);
        y += 30.0f;
    }
    // Join-by-code field (generic text input; hot owns the text value).
    {
        HotUiTextStateV1 t{};
        if (ctx->dynamicReadComponent &&
            sharedState(ctx) && sharedState(ctx)->localPlayerEntity != 0)
            ctx->dynamicReadComponent(ctx->host,
                                      sharedState(ctx)->localPlayerEntity,
                                      HOT_UI_TEXT_COMPONENT, &t, sizeof(t));
        GameUiCommandV1 ti{};
        ti.kind = GAME_UI_TEXT_INPUT;
        ti.elementId = kJoinCode;
        ti.x = 40.0f; ti.y = 600.0f; ti.w = 200.0f; ti.h = 34.0f;
        ti.scale = 0.34f;
        ti.maxValue = (float)HOT_UI_TEXT_MAX;
        ti.color[0] = ti.color[1] = ti.color[2] = ti.color[3] = 1.0f;
        std::snprintf(ti.text, sizeof(ti.text), "%s",
                      (t.elementId == kJoinCode) ? t.text : "");
        ui(ctx->host, &ti);
        emitButton(ui, ctx->host, kJoinCode, "JOIN CODE", 250.0f, 600.0f, 140.0f,
                   34.0f, 0.2f, 0.6f, 0.3f);
    }
    emitButton(ui, ctx->host, kRefresh, "REFRESH", 40.0f, 660.0f, 140.0f, 40.0f,
               0.3f, 0.4f, 0.7f);
    emitButton(ui, ctx->host, kBack, "BACK", 200.0f, 660.0f, 140.0f, 40.0f, 0.5f,
               0.3f, 0.3f);

    GameSharedStateV1* shared = sharedState(ctx);
    if (shared && shared->localPlayerEntity != 0) {
        HotUiClaimV1 claim{};
        claim.screenId = kScreenBrowser;
        claim.owned = 1;
        ctx->dynamicWriteComponent(ctx->host, shared->localPlayerEntity,
                                   HOT_UI_CLAIM_COMPONENT, &claim, sizeof(claim));
    }
}

const MimitaHotPackage::SystemRegistrar s_browserSystem{
    {gameHash("hot.server-browser"), GAME_DOMAIN_UI, 17, 0, browserTick,
     "hot.server-browser"}};

} // namespace

#endif
