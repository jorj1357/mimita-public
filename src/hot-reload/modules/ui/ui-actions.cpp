// 09 15 2026
/* purpose
* Hot UI interaction policy. Cold input/backend emits a generic `ui.action`
* event carrying a logical element id; this module owns what that id MEANS
* (navigation, actions). It also composes the main menu with interactive
* GAME_UI_BUTTON widgets through render.ui. No per-button callback ABI; the
* backend stores only element ids, so hot reload is generation-safe.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-ui.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

// (cstring used for strlen in text handling)

namespace {

// Logical screens (hashes only; no kernel enum).
const std::uint64_t kScreenMainMenu = gameHash("screen.main-menu");
const std::uint64_t kScreenPlay = gameHash("screen.play");
const std::uint64_t kScreenSettings = gameHash("screen.settings");

const std::uint64_t kMenuPlay = gameHash("menu.play");
const std::uint64_t kMenuSettings = gameHash("menu.settings");
const std::uint64_t kMenuQuit = gameHash("menu.quit");
const std::uint64_t kAccountSignIn = gameHash("account.signin");
const std::uint64_t kAccountSignUp = gameHash("account.signup");
const std::uint64_t kAccountSwitch = gameHash("account.switch");
const std::uint64_t kAccountLogout = gameHash("account.logout");

const std::uint64_t kMenuBackground = gameHash("ui.menu.background");
const std::uint64_t kMenuLogo = gameHash("ui.menu.logo");
const char* const kMenuBackgroundPath = "assets/ui/backgrounds/main-menu-bg.png";
const char* const kMenuLogoPath = "assets/uitextures/mimita-cover-v1.png";
bool g_menuResourcesRegistered = false;

using ResourceRegisterFn = bool (MIMITA_GAME_CALL *)(void*, GameResourceRegisterV1*);

void registerMenuResources(GameplayContextV1* ctx)
{
    if (g_menuResourcesRegistered || !ctx || !ctx->resolveCapability)
        return;
    auto reg = reinterpret_cast<ResourceRegisterFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_RESOURCE_REGISTER));
    if (!reg)
        return;
    GameResourceRegisterV1 bg{};
    bg.logicalId = kMenuBackground;
    bg.kind = GAME_RESOURCE_TEXTURE;
    bg.applyNow = 1;
    std::snprintf(bg.path, sizeof(bg.path), "%s", kMenuBackgroundPath);
    reg(ctx->host, &bg);
    GameResourceRegisterV1 logo{};
    logo.logicalId = kMenuLogo;
    logo.kind = GAME_RESOURCE_TEXTURE;
    logo.applyNow = 1;
    std::snprintf(logo.path, sizeof(logo.path), "%s", kMenuLogoPath);
    reg(ctx->host, &logo);
    g_menuResourcesRegistered = true;
}

// ON by default: the hot shell now covers background/logo/title, account
// name/stats/VIP colour, primary nav and account/auth entry actions, so it is the
// shipping owner. `uiscreen off` disables it (dev/fallback). Cold auth modals and
// the actual auth/screen transitions remain cold via the pending-action bridge.
bool g_menuEnabled = true;

GameSharedStateV1* sharedState(GameplayContextV1* ctx);
void requestColdAction(GameplayContextV1* ctx, std::uint64_t actionId,
                       const char* value = nullptr);

using SettingSetFn2 = bool (MIMITA_GAME_CALL *)(void*, GameSettingV1*);
void applySetting(GameplayContextV1* ctx, std::uint64_t settingId,
                  std::uint32_t type, float f, std::int32_t i)
{
    if (!ctx || !ctx->resolveCapability)
        return;
    auto set = reinterpret_cast<SettingSetFn2>(
        ctx->resolveCapability(ctx->host, GAME_CAP_SETTING_SET));
    if (!set)
        return;
    GameSettingV1 s{};
    s.settingId = settingId;
    s.type = type;
    s.floatValue = f;
    s.intValue = i;
    set(ctx->host, &s);
}

// Navigation state is migratable dynamic state (NOT a module static), so a hot
// generation swap keeps the current screen instead of resetting to main.
std::uint64_t readScreen(GameplayContextV1* ctx)
{
    GameSharedStateV1* shared = sharedState(ctx);
    const std::uint64_t entity = shared ? shared->localPlayerEntity : 0;
    if (entity == 0 || !ctx->dynamicReadComponent)
        return kScreenMainMenu;
    HotUiNavigationStateV1 nav{};
    if (!ctx->dynamicReadComponent(ctx->host, entity, HOT_UI_NAV_COMPONENT, &nav,
                                   sizeof(nav)) ||
        nav.screenId == 0)
        return kScreenMainMenu;
    return nav.screenId;
}

void writeScreen(GameplayContextV1* ctx, std::uint64_t screenId)
{
    GameSharedStateV1* shared = sharedState(ctx);
    const std::uint64_t entity = shared ? shared->localPlayerEntity : 0;
    if (entity == 0 || !ctx->dynamicWriteComponent)
        return;
    HotUiNavigationStateV1 nav{};
    if (ctx->dynamicReadComponent)
        ctx->dynamicReadComponent(ctx->host, entity, HOT_UI_NAV_COMPONENT, &nav,
                                  sizeof(nav));
    nav.previousScreenId = nav.screenId;
    nav.screenId = screenId;
    ctx->dynamicWriteComponent(ctx->host, entity, HOT_UI_NAV_COMPONENT, &nav,
                               sizeof(nav));
}

const std::uint64_t kMenuShell = gameHash("MenuShellState");
struct MenuShellStateV1 {
    char username[32];
    char version[24];
    char avatar[24];
    std::uint32_t flags;
    std::uint32_t connection;
    std::int32_t mmr;
    std::int32_t wins;
    std::int32_t losses;
    std::int32_t kills;
    std::int32_t deaths;
    std::uint32_t tier;
    std::uint32_t reserved;
};

using RenderUiFn = void (MIMITA_GAME_CALL *)(void*, const GameUiCommandV1*);
using RenderMeshFn = void (MIMITA_GAME_CALL *)(void*, const GameRenderMeshCommandV1*);

GameSharedStateV1* sharedState(GameplayContextV1* ctx)
{
    if (!ctx || !ctx->permanentStorage ||
        ctx->permanentStorageSize < sizeof(GameSharedStateV1))
        return nullptr;
    GameSharedStateV1* shared =
        reinterpret_cast<GameSharedStateV1*>(ctx->permanentStorage);
    return shared->magic == GAME_SHARED_MAGIC ? shared : nullptr;
}

// Generic UI action handler: element id -> behavior. No cold callback table.
void MIMITA_GAME_CALL onUiAction(void* host, const GameEventV1* event)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    auto* action = event ? static_cast<GameUiActionV1*>(event->payload) : nullptr;
    if (!ctx || !action)
        return;

    // Text keystroke / submit for the focused field: hot owns the text value.
    if (action->actionType == GAME_UI_ACTION_TEXT_INPUT ||
        action->actionType == GAME_UI_ACTION_TEXT_SUBMIT) {
        GameSharedStateV1* shared = sharedState(ctx);
        const std::uint64_t e = shared ? shared->localPlayerEntity : 0;
        if (e == 0 || !ctx->dynamicWriteComponent)
            return;
        HotUiTextStateV1 t{};
        if (!ctx->dynamicReadComponent ||
            !ctx->dynamicReadComponent(ctx->host, e, HOT_UI_TEXT_COMPONENT, &t,
                                       sizeof(t)))
            t = HotUiTextStateV1{};
        if (t.elementId != action->elementId) {
            t.elementId = action->elementId;
            t.text[0] = '\0';
        }
        if (action->actionType == GAME_UI_ACTION_TEXT_INPUT) {
            const std::size_t len = std::strlen(t.text);
            const int cp = (int)action->value;
            if (cp == 0) {
                if (len > 0)
                    t.text[len - 1] = '\0';
            } else if (cp >= 32 && cp < 127 && len < HOT_UI_TEXT_MAX - 1) {
                t.text[len] = (char)cp;
                t.text[len + 1] = '\0';
            }
        }
        ctx->dynamicWriteComponent(ctx->host, e, HOT_UI_TEXT_COMPONENT, &t,
                                   sizeof(t));
        if (action->actionType == GAME_UI_ACTION_TEXT_SUBMIT)
            requestColdAction(ctx, gameHash("serverbrowser.join-code"), t.text);
        action->handled = 1;
        return;
    }

    // Setting VALUE_CHANGED: write through the generic setting seam (the kernel
    // validates/clamps; the value stays engine-owned).
    if (action->actionType == GAME_UI_ACTION_VALUE_CHANGED) {
        static const std::uint64_t kFloats[] = {
            gameHash("video.fov"), gameHash("audio.master"),
            gameHash("audio.music"), gameHash("audio.sfx"),
            gameHash("input.sensitivity")};
        bool known = false;
        for (std::uint64_t sid : kFloats)
            if (sid == action->elementId) known = true;
        if (action->elementId == gameHash("audio.muted")) {
            applySetting(ctx, action->elementId, GAME_SETTING_BOOL, 0.0f,
                         action->value > 0.5f ? 1 : 0);
            action->handled = 1;
            return;
        }
        if (action->elementId == gameHash("video.resolution") ||
            action->elementId == gameHash("video.graphicsPreset")) {
            applySetting(ctx, action->elementId, GAME_SETTING_OPTION, 0.0f,
                         (std::int32_t)(action->value + 0.5f));
            action->handled = 1;
            return;
        }
        if (known) {
            applySetting(ctx, action->elementId, GAME_SETTING_FLOAT,
                         action->value, 0);
            action->handled = 1;
        }
        return;
    }

    if (action->actionType != GAME_UI_ACTION_CLICK)
        return;
    const std::uint64_t id = action->elementId;

    // Hot UI-sound policy: pick the logical sound id here (no cold widget knows
    // product sounds). Cold owns decode/mix/device only.
    if (ctx->resolveCapability) {
        using AudioPlayFn = void (MIMITA_GAME_CALL *)(void*,
                                                      const GameAudioCommandV1*);
        auto play = reinterpret_cast<AudioPlayFn>(
            ctx->resolveCapability(ctx->host, GAME_CAP_AUDIO_PLAY));
        if (play) {
            const char* sound = "audio.ui.click";
            if (id == gameHash("menu.back") ||
                id == gameHash("pause.leave.cancel"))
                sound = "audio.ui.back";
            else if (id == gameHash("menu.play") ||
                     id == gameHash("serverbrowser.connect") ||
                     id == gameHash("serverbrowser.join-code") ||
                     id == gameHash("pause.resume"))
                sound = "audio.ui.confirm";
            GameAudioCommandV1 cmd{};
            std::snprintf(cmd.sound, sizeof(cmd.sound), "%s", sound);
            cmd.volume = 0.8f;
            cmd.pitch = 1.0f;
            cmd.spatial = 0;
            play(ctx->host, &cmd);
        }
    }
    if (id == gameHash("menu.back")) {
        // Generic return navigation: settings opened from pause returns to pause.
        GameSharedStateV1* shared = sharedState(ctx);
        const std::uint64_t e = shared ? shared->localPlayerEntity : 0;
        HotUiNavigationStateV1 nav{};
        if (e != 0 && ctx->dynamicReadComponent)
            ctx->dynamicReadComponent(ctx->host, e, HOT_UI_NAV_COMPONENT, &nav,
                                      sizeof(nav));
        if (nav.previousScreenId == gameHash("screen.pause"))
            writeScreen(ctx, gameHash("screen.pause"));
        else
            writeScreen(ctx, kScreenMainMenu);
        action->handled = 1;
        return;
    }
    if (id == gameHash("pause.settings")) {
        // Route to the existing hot settings screen, remembering the return.
        GameSharedStateV1* shared = sharedState(ctx);
        const std::uint64_t e = shared ? shared->localPlayerEntity : 0;
        HotUiNavigationStateV1 nav{};
        if (e != 0 && ctx->dynamicReadComponent)
            ctx->dynamicReadComponent(ctx->host, e, HOT_UI_NAV_COMPONENT, &nav,
                                      sizeof(nav));
        nav.previousScreenId = gameHash("screen.pause");
        nav.screenId = kScreenSettings;
        if (e != 0 && ctx->dynamicWriteComponent)
            ctx->dynamicWriteComponent(ctx->host, e, HOT_UI_NAV_COMPONENT, &nav,
                                       sizeof(nav));
        action->handled = 1;
        return;
    }
    if (id == gameHash("pause.resume") || id == gameHash("pause.help") ||
        id == gameHash("pause.leave") || id == gameHash("pause.discord") ||
        id == gameHash("pause.invite") || id == gameHash("pause.leave.confirm") ||
        id == gameHash("pause.leave.cancel")) {
        requestColdAction(ctx, id);   // cold modal mechanism performs it
        action->handled = 1;
        return;
    }
    if (id == gameHash("serverbrowser.refresh")) {
        requestColdAction(ctx, id);   // cold discovery refresh
        action->handled = 1;
        return;
    }
    if (id == gameHash("serverbrowser.join-code")) {
        GameSharedStateV1* shared = sharedState(ctx);
        const std::uint64_t e = shared ? shared->localPlayerEntity : 0;
        HotUiTextStateV1 t{};
        if (e != 0 && ctx->dynamicReadComponent &&
            ctx->dynamicReadComponent(ctx->host, e, HOT_UI_TEXT_COMPONENT, &t,
                                      sizeof(t)) &&
            t.text[0] != '\0')
            requestColdAction(ctx, gameHash("serverbrowser.join-code"), t.text);
        action->handled = 1;
        return;
    }
    // A listing id -> resolve the cold room code and request connect.
    if (ctx->dynamicEnumerateComponent && ctx->dynamicReadComponent) {
        std::uint64_t lists[128];
        const std::uint32_t ln = ctx->dynamicEnumerateComponent(
            ctx->host, HOT_SERVER_LISTING_COMPONENT, lists, 128);
        for (std::uint32_t i = 0; i < ln; ++i) {
            HotServerListingV1 l{};
            if (ctx->dynamicReadComponent(ctx->host, lists[i],
                                          HOT_SERVER_LISTING_COMPONENT, &l,
                                          sizeof(l)) &&
                l.listingId == id && l.code[0] != '\0') {
                requestColdAction(ctx, gameHash("serverbrowser.connect"), l.code);
                action->handled = 1;
                return;
            }
        }
    }
    if (id == kMenuSettings) {
        writeScreen(ctx, kScreenSettings);   // hot owns settings composition
    } else if (id == kMenuPlay) {
        // Hot owns the server browser by default (list + join + join-by-code +
        // refresh). Cold online-menu yields when hot owns the screen.
        writeScreen(ctx, gameHash("screen.server-browser"));
    } else if (id == kMenuQuit) {
        writeScreen(ctx, 0);   // leaving to a cold mechanism
        requestColdAction(ctx, id);
    } else if (id == kAccountSignIn || id == kAccountSignUp ||
               id == kAccountSwitch || id == kAccountLogout) {
        requestColdAction(ctx, id);   // cold secure auth mechanism
    } else {
        return;
    }
    action->handled = 1;   // one owner: the cold legacy widget must not act too
}

void emitPanel(RenderUiFn ui, void* host, float x, float y, float w, float h)
{
    GameUiCommandV1 c{};
    c.kind = GAME_UI_PANEL;
    c.x = x; c.y = y; c.w = w; c.h = h;
    c.color[0] = 0.0f; c.color[1] = 0.0f; c.color[2] = 0.0f; c.color[3] = 0.6f;
    ui(host, &c);
}

void emitText(RenderUiFn ui, void* host, const char* text, float x, float y,
              float scale, float r, float g, float b, float a)
{
    if (!text || text[0] == '\0')
        return;
    GameUiCommandV1 c{};
    c.kind = GAME_UI_TEXT;
    c.x = x; c.y = y; c.scale = scale;
    c.color[0] = r; c.color[1] = g; c.color[2] = b; c.color[3] = a;
    std::snprintf(c.text, sizeof(c.text), "%s", text);
    ui(host, &c);
}

void emitImage(RenderUiFn ui, void* host, std::uint64_t resourceId, float x,
               float y, float w, float h, float a)
{
    if (resourceId == 0)
        return;
    GameUiCommandV1 c{};
    c.kind = GAME_UI_IMAGE;
    c.resourceId = resourceId;
    c.x = x; c.y = y; c.w = w; c.h = h;
    c.color[0] = c.color[1] = c.color[2] = 1.0f;
    c.color[3] = a;
    ui(host, &c);
}

// Route a logical UI action to the cold secure/screen mechanism (tokens and
// screen transitions stay cold). Writes an id, never a callback pointer.
void requestColdAction(GameplayContextV1* ctx, std::uint64_t actionId,
                       const char* value)
{
    GameSharedStateV1* shared = sharedState(ctx);
    const std::uint64_t entity = shared ? shared->localPlayerEntity : 0;
    if (entity == 0 || !ctx->dynamicWriteComponent)
        return;
    HotUiPendingActionV1 pending{};
    pending.actionId = actionId;
    if (value && value[0])
        std::snprintf(pending.value, sizeof(pending.value), "%s", value);
    if (ctx->dynamicReadComponent) {
        HotUiPendingActionV1 prev{};
        if (ctx->dynamicReadComponent(ctx->host, entity,
                                      HOT_UI_PENDING_ACTION_COMPONENT, &prev,
                                      sizeof(prev)))
            pending.seq = prev.seq + 1;
    }
    ctx->dynamicWriteComponent(ctx->host, entity,
                               HOT_UI_PENDING_ACTION_COMPONENT, &pending,
                               sizeof(pending));
}

void emitButton(RenderUiFn ui, void* host, std::uint64_t elementId,
                const char* label, float x, float y, float w, float h,
                float r, float g, float b)
{
    GameUiCommandV1 c{};
    c.kind = GAME_UI_BUTTON;
    c.elementId = elementId;
    c.x = x; c.y = y; c.w = w; c.h = h;
    c.scale = 0.5f;
    c.color[0] = r; c.color[1] = g; c.color[2] = b; c.color[3] = 1.0f;
    std::snprintf(c.text, sizeof(c.text), "%s", label);
    ui(host, &c);
}

// Main-menu composition, owned entirely by hot policy (labels/layout/actions).
void MIMITA_GAME_CALL mainMenuTick(void* host, std::uint64_t /*tick*/,
                                   float /*dt*/)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!g_menuEnabled || !ctx || readScreen(ctx) != kScreenMainMenu)
        return;
    if (!ctx->resolveCapability)
        return;
    auto ui = reinterpret_cast<RenderUiFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_RENDER_UI));
    if (!ui)
        return;

    // Shell chrome from generic state (no cold GUI/account objects).
    registerMenuResources(ctx);
    emitPanel(ui, ctx->host, 0.0f, 0.0f, 1280.0f, 720.0f);
    emitImage(ui, ctx->host, kMenuBackground, 0.0f, 0.0f, 1280.0f, 720.0f, 1.0f);
    emitImage(ui, ctx->host, kMenuLogo, 480.0f, 40.0f, 320.0f, 120.0f, 1.0f);
    emitText(ui, ctx->host, "MIMITA", 520.0f, 60.0f, 1.2f, 1.0f, 1.0f, 1.0f, 1.0f);

    GameSharedStateV1* shared = sharedState(ctx);
    const std::uint64_t uiEntity = shared ? shared->localPlayerEntity : 0;
    MenuShellStateV1 shell{};
    const bool haveShell = uiEntity != 0 && ctx->dynamicReadComponent &&
        ctx->dynamicReadComponent(ctx->host, uiEntity, kMenuShell, &shell,
                                  sizeof(shell));
    if (haveShell) {
        // VIP/status presentation policy lives here (tier -> colour).
        const float tr = (shell.tier >= 2) ? 1.0f : 0.85f;
        const float tg = (shell.tier >= 2) ? 0.85f : 0.85f;
        const float tb = (shell.tier >= 2) ? 0.3f : 0.85f;
        emitText(ui, ctx->host,
                 (shell.flags & 1u) ? shell.username : "Not signed in", 40.0f,
                 40.0f, 0.5f, tr, tg, tb, 1.0f);
        char stats[96];
        std::snprintf(stats, sizeof(stats), "MMR %d   W %d  L %d  K %d  D %d",
                      shell.mmr, shell.wins, shell.losses, shell.kills,
                      shell.deaths);
        emitText(ui, ctx->host, stats, 40.0f, 70.0f, 0.32f, 0.8f, 0.8f, 0.8f,
                 1.0f);
        emitText(ui, ctx->host, shell.version, 40.0f, 680.0f, 0.32f, 0.6f, 0.6f,
                 0.6f, 1.0f);
    }

    // Avatar preview via the generic 3D-in-UI primitive (uiClip + view space).
    // Framing is approximate (cosmetic debt); the mechanism is the point.
    if (ctx->resolveCapability) {
        auto mesh = reinterpret_cast<RenderMeshFn>(
            ctx->resolveCapability(ctx->host, GAME_CAP_RENDER_MESH));
        if (mesh) {
            GameRenderMeshCommandV1 mc{};
            mc.entity = 0;
            mc.meshResourceId = gameHash("mesh.actor");
            mc.textureResourceId = gameHash("texture.default");
            mc.flags = GAME_RENDER_MESH_SPACE_VIEW;
            mc.position[0] = 0.0f; mc.position[1] = -0.1f; mc.position[2] = -3.0f;
            mc.rotation[3] = 1.0f;
            mc.scale[0] = mc.scale[1] = mc.scale[2] = 1.0f;
            mc.color[0] = mc.color[1] = mc.color[2] = mc.color[3] = 1.0f;
            mc.uiClip[0] = 880.0f; mc.uiClip[1] = 140.0f;
            mc.uiClip[2] = 340.0f; mc.uiClip[3] = 460.0f;
            mesh(ctx->host, &mc);
        }
    }

    emitPanel(ui, ctx->host, 440.0f, 160.0f, 400.0f, 340.0f);
    emitButton(ui, ctx->host, kMenuPlay, "PLAY", 520.0f, 190.0f, 240.0f, 44.0f,
               0.2f, 0.6f, 0.3f);
    emitButton(ui, ctx->host, kMenuSettings, "SETTINGS", 520.0f, 244.0f, 240.0f,
               44.0f, 0.3f, 0.4f, 0.7f);
    if (haveShell && (shell.flags & 1u)) {
        emitButton(ui, ctx->host, kAccountSwitch, "SWITCH ACCOUNT", 520.0f,
                   298.0f, 240.0f, 40.0f, 0.4f, 0.4f, 0.5f);
        emitButton(ui, ctx->host, kAccountLogout, "LOG OUT", 520.0f, 342.0f,
                   240.0f, 40.0f, 0.5f, 0.35f, 0.35f);
    } else if (haveShell) {
        emitButton(ui, ctx->host, kAccountSignIn, "SIGN IN", 520.0f, 298.0f,
                   240.0f, 40.0f, 0.3f, 0.5f, 0.7f);
        emitButton(ui, ctx->host, kAccountSignUp, "SIGN UP", 520.0f, 342.0f,
                   240.0f, 40.0f, 0.4f, 0.5f, 0.6f);
    }
    emitButton(ui, ctx->host, kMenuQuit, "QUIT", 520.0f, 386.0f, 240.0f, 44.0f,
               0.7f, 0.3f, 0.3f);

    // Ownership claim so the cold legacy main menu yields for this screen.
    if (ctx->dynamicWriteComponent && uiEntity != 0) {
        {
            HotUiClaimV1 claim{};
            claim.screenId = readScreen(ctx);
            claim.owned = 1;
            ctx->dynamicWriteComponent(ctx->host, uiEntity,
                                       HOT_UI_CLAIM_COMPONENT, &claim,
                                       sizeof(claim));
        }
    }
}

const MimitaHotPackage::EventRegistrar s_uiActionEvent{
    {gameHash("ui.action"), gameHash("ui.action.v1"), 0, onUiAction,
     "hot.ui-actions"}};
const MimitaHotPackage::SchemaRegistrar s_uiClaimSchema{
    {HOT_UI_CLAIM_COMPONENT, gameHash("HotUiClaim.v1"), sizeof(HotUiClaimV1), 8,
     GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE, "HotUiClaim", 1, 0}};
const MimitaHotPackage::SchemaRegistrar s_uiNavSchema{
    {HOT_UI_NAV_COMPONENT, gameHash("HotUiNavigationState.v1"),
     sizeof(HotUiNavigationStateV1), 8, GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE,
     "HotUiNavigationState", 1, 0}};
const MimitaHotPackage::SchemaRegistrar s_menuShellSchema{
    {HOT_MENU_SHELL_COMPONENT, gameHash("MenuShellState.v1"),
     sizeof(MenuShellStateV1), 4, GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE,
     "MenuShellState", 1, 0}};
const MimitaHotPackage::SchemaRegistrar s_uiTextSchema{
    {HOT_UI_TEXT_COMPONENT, gameHash("HotUiTextState.v1"),
     sizeof(HotUiTextStateV1), 8, GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE,
     "HotUiTextState", 1, 0}};
const MimitaHotPackage::SchemaRegistrar s_pendingActionSchema{
    {HOT_UI_PENDING_ACTION_COMPONENT, gameHash("HotUiPendingAction.v1"),
     sizeof(HotUiPendingActionV1), 8, GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE,
     "HotUiPendingAction", 1, 0}};
const MimitaHotPackage::SystemRegistrar s_mainMenuSystem{
    {gameHash("hot.main-menu"), GAME_DOMAIN_UI, 12, 0, mainMenuTick,
     "hot.main-menu"}};
void MIMITA_GAME_CALL uiScreenCommand(void* host, const char* args)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    if (args && *args) {
        if (std::strcmp(args, "off") == 0) {
            g_menuEnabled = false;
            return;
        }
        if (std::strcmp(args, "none") == 0) {
            if (ctx)
                writeScreen(ctx, 0);   // no hot screen; cold owns
            return;
        }
        if (ctx)
            writeScreen(ctx, gameHash(args));
        g_menuEnabled = true;   // explicit enable (selftest / dev)
    }
}
const MimitaHotPackage::CommandRegistrar s_uiScreenCommand{
    {"uiscreen", "uiscreen <hash> - set the hot UI screen id", 0,
     uiScreenCommand}};

} // namespace

#endif
