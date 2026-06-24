#include <cstdio>
#include <string>
#include <vector>
#include "devtools/terminal.h"
#include "terminal/terminal-state.h"
#include "devtools/dev-overlay.h"
#include "effects/effect-part.h"
#include "audio/audio.h"
#include "debug/debug-log.h"

extern bool gMainmenuDebug;
extern void forceMainMenu();

void registerDevOverlayCommands()
{
    Terminal::instance().registerCommand({
        "skybox_debug", "Show skybox source info", "skybox_debug",
        [](const std::vector<std::string>&) {
            auto& world = THE_WORLD;
            bool hasSky = !world.skyMesh.verts.empty();
            char buf[512];
            if (hasSky)
            {
                snprintf(buf, sizeof(buf),
                    "sky source=glb\n"
                    "sky mesh found=1\n"
                    "sky verts=%zu\n"
                    "sky batches=%zu",
                    world.skyMesh.verts.size(), world.skyMesh.batches.size());
            }
            else
            {
                snprintf(buf, sizeof(buf),
                    "sky source=fallback\n"
                    "reason=no sky data found");
            }
            Terminal::instance().addLog(buf);
        }
    });

    Terminal::instance().registerCommand({
        "mainmenu_debug", "Log mainmenu cleanup timing breakdown", "mainmenu_debug [0|1]",
        [](const std::vector<std::string>& args) {
            gMainmenuDebug = args.empty() ? !gMainmenuDebug : args[0] != "0";
            Terminal::instance().addLog(std::string("[MAINMENU] debug=") +
                (gMainmenuDebug ? "1" : "0"));
        }
    });

    Terminal::instance().registerCommand({
        "mainmenu", "Return to Main Menu immediately (emergency escape hatch)", "mainmenu",
        [](const std::vector<std::string>&) {
            forceMainMenu();
            Terminal::instance().addLog("[MAINMENU] returned to main menu");
        }
    });

    Terminal::instance().registerCommand({
        "spawnfx_test", "Trigger spawn flash effect immediately", "spawnfx_test",
        [](const std::vector<std::string>&) {
            THE_PLAYER.spawnFlashTimer = 10.0f;
            playSound("entity/player/spawning", 1.0f);
            Debug::log(Debug::Category::Audio, "[SPAWN FX] spawnfx_test triggered\n");
            Terminal::instance().addLog("[SPAWN FX] test triggered");
        },
        "2026-06-14", CommandCategory::Debug
    });
    Terminal::instance().registerCommand({
        "spawnfx_debug", "Show spawn flash debug info", "spawnfx_debug",
        [](const std::vector<std::string>&) {
            char buf[256];
            snprintf(buf, sizeof(buf), "[SPAWN FX] active=%d timer=%.0f ticks",
                     (int)(THE_PLAYER.spawnFlashTimer > 0.0f), THE_PLAYER.spawnFlashTimer);
            Terminal::instance().addLog(buf);
        },
        "2026-06-14", CommandCategory::Debug
    });
}
