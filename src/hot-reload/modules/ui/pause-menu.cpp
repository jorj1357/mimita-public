// 09 16 2026
/* purpose
* Hot pause-menu composition (Main view). Cold keeps the modal/focus mechanism
* and the Settings/Help/ConfirmLeave views (per-view ownership); hot composes the
* Main view when the cold modal is open, and routes logical pause actions to the
* cold mechanism through the generic pending-action bridge. No pause-specific
* rendering ABI.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-ui.h"

#include <cstdint>
#include <cstdio>

namespace {

const std::uint64_t kScreenPause = gameHash("screen.pause");
const std::uint64_t kPauseMain = gameHash("pause.main");
const std::uint64_t kPauseConfirm = gameHash("pause.confirm-leave");
const std::uint64_t kPauseLeaveConfirm = gameHash("pause.leave.confirm");
const std::uint64_t kPauseLeaveCancel = gameHash("pause.leave.cancel");
const std::uint64_t kPauseResume = gameHash("pause.resume");
const std::uint64_t kPauseSettings = gameHash("pause.settings");
const std::uint64_t kPauseHelp = gameHash("pause.help");
const std::uint64_t kPauseLeave = gameHash("pause.leave");
const std::uint64_t kPauseDiscord = gameHash("pause.discord");
const std::uint64_t kPauseInvite = gameHash("pause.invite");

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

void emitButton(RenderUiFn ui, void* host, std::uint64_t id, const char* label,
                float y, float r, float g, float b)
{
    GameUiCommandV1 c{};
    c.kind = GAME_UI_BUTTON;
    c.elementId = id;
    c.x = 520.0f; c.y = y; c.w = 240.0f; c.h = 44.0f;
    c.scale = 0.42f;
    c.color[0] = r; c.color[1] = g; c.color[2] = b; c.color[3] = 1.0f;
    std::snprintf(c.text, sizeof(c.text), "%s", label);
    ui(host, &c);
}

void MIMITA_GAME_CALL pauseTick(void* host, std::uint64_t /*tick*/,
                                float /*dt*/)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->resolveCapability || !ctx->dynamicReadComponent ||
        !ctx->dynamicWriteComponent)
        return;
    GameSharedStateV1* shared = sharedState(ctx);
    const std::uint64_t entity = shared ? shared->localPlayerEntity : 0;
    if (entity == 0)
        return;
    HotPauseStateV1 st{};
    if (!ctx->dynamicReadComponent(ctx->host, entity, HOT_PAUSE_STATE_COMPONENT,
                                   &st, sizeof(st)) ||
        st.visible == 0 || (st.viewHash != kPauseMain &&
                            st.viewHash != kPauseConfirm))
        return;
    auto ui = reinterpret_cast<RenderUiFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_RENDER_UI));
    if (!ui)
        return;

    GameUiCommandV1 bg{};
    bg.kind = GAME_UI_PANEL;
    bg.x = 0.0f; bg.y = 0.0f; bg.w = 1280.0f; bg.h = 720.0f;
    bg.color[0] = 0.0f; bg.color[1] = 0.0f; bg.color[2] = 0.0f; bg.color[3] = 0.55f;
    ui(ctx->host, &bg);

    if (st.viewHash == kPauseConfirm) {
        GameUiCommandV1 t{};
        t.kind = GAME_UI_TEXT;
        t.x = 500.0f; t.y = 300.0f; t.scale = 0.5f;
        t.color[0] = t.color[1] = t.color[2] = t.color[3] = 1.0f;
        std::snprintf(t.text, sizeof(t.text), "Leave this game?");
        ui(ctx->host, &t);
        emitButton(ui, ctx->host, kPauseLeaveConfirm, "CONFIRM", 280.0f, 0.7f, 0.3f, 0.3f);
        emitButton(ui, ctx->host, kPauseLeaveCancel, "CANCEL", 340.0f, 0.4f, 0.4f, 0.5f);
    } else {
        emitButton(ui, ctx->host, kPauseResume, "RESUME", 200.0f, 0.2f, 0.6f, 0.3f);
        emitButton(ui, ctx->host, kPauseSettings, "SETTINGS", 256.0f, 0.3f, 0.4f, 0.7f);
        emitButton(ui, ctx->host, kPauseHelp, "HELP", 312.0f, 0.4f, 0.4f, 0.5f);
        emitButton(ui, ctx->host, kPauseDiscord, "DISCORD", 368.0f, 0.4f, 0.3f, 0.6f);
        emitButton(ui, ctx->host, kPauseInvite, "INVITE", 424.0f, 0.4f, 0.3f, 0.6f);
        emitButton(ui, ctx->host, kPauseLeave, "LEAVE", 480.0f, 0.7f, 0.3f, 0.3f);
    }

    HotUiClaimV1 claim{};
    claim.screenId = kScreenPause;
    claim.owned = 1;
    ctx->dynamicWriteComponent(ctx->host, entity, HOT_UI_CLAIM_COMPONENT, &claim,
                               sizeof(claim));
}

const MimitaHotPackage::SchemaRegistrar s_pauseStateSchema{
    {HOT_PAUSE_STATE_COMPONENT, gameHash("PauseMenuState.v1"),
     sizeof(HotPauseStateV1), 8, GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE,
     "PauseMenuState", 1, 0}};
const MimitaHotPackage::SystemRegistrar s_pauseSystem{
    {gameHash("hot.pause-menu"), GAME_DOMAIN_UI, 16, 0, pauseTick,
     "hot.pause-menu"}};

} // namespace

#endif
