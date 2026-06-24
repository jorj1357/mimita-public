#include <cstdio>
#include <string>
#include <vector>
#include "devtools/terminal.h"
#include "render/post-fx.h"

void registerPostFxCommands()
{
    Terminal::instance().registerCommand({
        "postfx_debug", "Toggle PostFX debug overlay", "postfx_debug [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                PostFX::instance().debugEnabled = !PostFX::instance().debugEnabled;
            } else {
                PostFX::instance().debugEnabled = args[0] == "1";
            }
            const bool on = PostFX::instance().debugEnabled;
            printf("[POSTFX] debug=%s\n", on ? "ON" : "OFF");
            Terminal::instance().addLog(std::string("[POSTFX] debug=") + (on ? "ON" : "OFF"));
        }
    });
    Terminal::instance().registerCommand({
        "postfx_preset", "Apply a PostFX preset", "postfx_preset <name>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[POSTFX] Usage: postfx_preset <normal|dream|void|psychedelic|retro|glitch|competitive>");
                return;
            }
            PostFX::instance().applyPreset(args[0]);
            Terminal::instance().addLog("[POSTFX] Preset applied: " + args[0]);
        }
    });
    Terminal::instance().registerCommand({
        "postfx_reload", "Reload config/postfx.json", "postfx_reload",
        [](const std::vector<std::string>&) {
            PostFX::instance().loadConfig("config/postfx.json");
            Terminal::instance().addLog("[POSTFX] Reloaded config/postfx.json");
        }
    });
}
