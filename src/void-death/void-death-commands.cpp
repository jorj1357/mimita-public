#include "void-death/void-death.h"

#include <fstream>
#include "devtools/terminal.h"
#include "nlohmann/json.hpp"

static void saveVoidDeathConfig()
{
    const auto& cfg = getVoidDeathConfig();
    nlohmann::json j;
    j["enabled"] = cfg.enabled;
    j["killZ"] = cfg.killZ;
    j["command"] = cfg.command;

    std::ofstream file("config/gameplay/void-death.json");
    if (file.is_open())
        file << j.dump(4) << std::endl;
}

void registerVoidDeathCommands()
{
    Terminal::instance().registerCommand({
        "voiddeath", "Enable or disable void death (1=on, 0=off)", "voiddeath <0|1>",
        [](const std::vector<std::string>& args) {
            auto& cfg = getVoidDeathConfig();
            if (args.empty()) {
                Terminal::instance().addLog(
                    std::string("[VOID DEATH] enabled=") + (cfg.enabled ? "1" : "0"));
                return;
            }
            cfg.enabled = args[0] != "0";
            saveVoidDeathConfig();
            Terminal::instance().addLog(
                std::string("[VOID DEATH] enabled=") + (cfg.enabled ? "1" : "0"));
        }
    });

    Terminal::instance().registerCommand({
        "voiddeath_z", "Set the void death Z threshold", "voiddeath_z <value>",
        [](const std::vector<std::string>& args) {
            auto& cfg = getVoidDeathConfig();
            if (args.empty()) {
                Terminal::instance().addLog(
                    "[VOID DEATH] killZ=" + std::to_string((int)cfg.killZ));
                return;
            }
            cfg.killZ = std::stof(args[0]);
            saveVoidDeathConfig();
            Terminal::instance().addLog(
                "[VOID DEATH] killZ=" + std::to_string((int)cfg.killZ));
        }
    });

    Terminal::instance().registerCommand({
        "voiddeath_status", "Print current void death configuration", "voiddeath_status",
        [](const std::vector<std::string>&) {
            const auto& cfg = getVoidDeathConfig();
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "[VOID DEATH] enabled=%d killZ=%.1f command=%s",
                     (int)cfg.enabled, cfg.killZ, cfg.command.c_str());
            Terminal::instance().addLog(buf);
        }
    });
}
