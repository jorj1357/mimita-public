#include "void-death/void-death.h"

#include <fstream>
#include <filesystem>
#include "nlohmann/json.hpp"
#include "debug/debug-log.h"
#include "combat/death-system.h"

static VoidDeathConfig gVoidDeathConfig;
static double gLastPollTime = 0.0;
static uint64_t gLastWriteTime = 0;

static uint64_t fileWriteTime(const char* path)
{
    std::error_code ec;
    auto ft = std::filesystem::last_write_time(path, ec);
    if (ec) return 0;
    return ft.time_since_epoch().count();
}

VoidDeathConfig& getVoidDeathConfig()
{
    return gVoidDeathConfig;
}

static void reloadVoidDeathConfig()
{
    std::ifstream file("config/gameplay/void-death.json");
    if (!file.is_open())
        return;

    try
    {
        nlohmann::json j;
        file >> j;

        VoidDeathConfig loaded;
        if (j.contains("enabled"))
            loaded.enabled = j["enabled"].get<bool>();
        if (j.contains("killZ"))
            loaded.killZ = j["killZ"].get<float>();
        if (j.contains("command"))
            loaded.command = j["command"].get<std::string>();

        gVoidDeathConfig = loaded;
        Debug::log(Debug::Category::General,
                   "[VOID DEATH] config reloaded: enabled=%d killZ=%.1f command=%s\n",
                   (int)gVoidDeathConfig.enabled, gVoidDeathConfig.killZ,
                   gVoidDeathConfig.command.c_str());
    }
    catch (const std::exception& e)
    {
        Debug::log(Debug::Category::General,
                   "[VOID DEATH] config reload failed: %s\n", e.what());
    }
}

void pollVoidDeathConfig()
{
    static double elapsed = 0.0;
    elapsed += 1.0 / 60.0;
    if (elapsed < 0.25)
        return;
    elapsed = 0.0;

    uint64_t wt = fileWriteTime("config/gameplay/void-death.json");
    if (wt == 0)
        return;

    if (wt != gLastWriteTime)
    {
        gLastWriteTime = wt;
        reloadVoidDeathConfig();
    }
}

void checkVoidDeath(Player& entity, const std::string& entityId, const std::string& entityType)
{
    if (!gVoidDeathConfig.enabled)
        return;
    if (entity.dead || entity.voidDeathTriggered)
        return;
    if (entity.pos.z >= gVoidDeathConfig.killZ)
        return;

    entity.voidDeathTriggered = true;

    Debug::log(Debug::Category::General,
               "[VOID DEATH]\n"
               "  entity=%s\n"
               "  name=%s\n"
               "  z=%.1f\n"
               "  killZ=%.1f\n",
               entityId.c_str(), entity.username.c_str(),
               entity.pos.z, gVoidDeathConfig.killZ);

    if (gVoidDeathConfig.command == "explode")
    {
        DeathSystem::instance().kill(
            entity,
            entityId,
            entityType,
            "void_death",
            glm::vec3(0.0f, 0.0f, 1.0f),
            24.0f);
    }
}
