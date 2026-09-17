// 09 16 2026
/* purpose
* Hot-owned aim mode. Publishes the aim mode (crosshair / camforward / physical /
* farpoint / world_hit) into the kernel shared state; the cold aim solver reads
* it and uses it instead of config/gameplay.json. Edit the string below (or use
* the `aimmode` command) live with no EXE rebuild.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

// Default aim mode. Change this and save: the running client switches live.
// "physical" = fire along the gun barrel; "crosshair" = fire at the camera-ray
// target. The list matches GameplayAimMode.
char g_aimMode[32] = "crosshair";

GameSharedStateV1* sharedState(GameplayContextV1* ctx)
{
    if (!ctx || !ctx->permanentStorage ||
        ctx->permanentStorageSize < sizeof(GameSharedStateV1))
        return nullptr;
    GameSharedStateV1* s =
        reinterpret_cast<GameSharedStateV1*>(ctx->permanentStorage);
    return s->magic == GAME_SHARED_MAGIC ? s : nullptr;
}

void MIMITA_GAME_CALL aimModeTick(void* host, std::uint64_t /*tick*/,
                                  float /*dt*/)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (GameSharedStateV1* s = sharedState(ctx))
        s->aimModeHash = gameHash(g_aimMode);
}

void MIMITA_GAME_CALL hotAimModeCommand(void* /*host*/, const char* args)
{
    if (!args || !*args)
        return;
    if (std::strcmp(args, "crosshair") != 0 &&
        std::strcmp(args, "camforward") != 0 &&
        std::strcmp(args, "physical") != 0 &&
        std::strcmp(args, "farpoint") != 0 &&
        std::strcmp(args, "world_hit") != 0) {
        std::printf("[AIM] unknown mode '%s'\n", args);
        return;
    }
    std::snprintf(g_aimMode, sizeof(g_aimMode), "%s", args);
    std::printf("[AIM] mode '%s'\n", g_aimMode);
}

const MimitaHotPackage::SystemRegistrar s_aimModeSystem{
    {gameHash("hot.aim-mode"), GAME_DOMAIN_CLIENT_TICK, 1, 0, aimModeTick,
     "hot.aim-mode"}};
const MimitaHotPackage::CommandRegistrar s_hotAimModeCommand{
    {"aimmode",
     "aimmode <crosshair|camforward|physical|farpoint|world_hit> - set hot aim",
     0, hotAimModeCommand}};

} // namespace

#endif
