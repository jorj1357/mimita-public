#include <cstdio>
#include <string>
#include <vector>
#include "debug/structured-log.h"
#include "devtools/terminal.h"
#include "render/post-fx.h"
#include "render/render-world.h"

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
        "postfx_mode", "Set PostFX mode: full, minimal, or off", "postfx_mode <full|minimal|off>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                const auto mode = PostFX::instance().mode();
                const char* name = mode == PostFX::Mode::Off ? "off"
                    : mode == PostFX::Mode::Minimal ? "minimal" : "full";
                Terminal::instance().addLog(std::string("[POSTFX] mode=") + name);
                return;
            }
            if (args[0] == "off") PostFX::instance().setMode(PostFX::Mode::Off);
            else if (args[0] == "minimal") PostFX::instance().setMode(PostFX::Mode::Minimal);
            else if (args[0] == "full") PostFX::instance().setMode(PostFX::Mode::Full);
            else {
                Terminal::instance().addLog("[POSTFX] usage: postfx_mode <full|minimal|off>");
                return;
            }
            Terminal::instance().addLog("[POSTFX] mode=" + args[0] + " (temporary; JSON reload wins)");
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Rendering, StructuredLevel::Important,
                "postfx.mode", "render",
                "PostFX mode changed at runtime", 0,
                nlohmann::json{{"mode", args[0]}, {"temporary", true}},
                __FILE__, __LINE__, __FUNCTION__);
        }
    });
    Terminal::instance().registerCommand({
        "postfx_reload", "Reload config/postfx.json", "postfx_reload",
        [](const std::vector<std::string>&) {
            PostFX::instance().loadConfig("config/postfx.json");
            Terminal::instance().addLog("[POSTFX] Reloaded config/postfx.json");
        }
    });
    Terminal::instance().registerCommand({
        "postfxrand", "Randomly animate all post-processing values",
        "postfxrand <0|1|exclude <name>>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[POSTFXRAND] Usage:");
                Terminal::instance().addLog("  postfxrand 1              - enable random animation");
                Terminal::instance().addLog("  postfxrand 0              - disable, restore original");
                Terminal::instance().addLog("  postfxrand exclude <name> - exclude property from animation");
                return;
            }
            if (args[0] == "1") {
                PostFX::instance().enableRandomMode();
                Terminal::instance().addLog("[POSTFXRAND] Random animation enabled");
            } else if (args[0] == "0") {
                PostFX::instance().disableRandomMode();
                Terminal::instance().addLog("[POSTFXRAND] Random animation disabled, restoring original state");
            } else if (args[0] == "exclude" && args.size() >= 2) {
                if (args[1] == "curvy" || args[1] == "screen") {
                    PostFX::instance().setAnimExcluded("worldWave", true);
                    PostFX::instance().setAnimExcluded("screenWave", true);
                    PostFX::instance().setAnimExcluded("lensDistortion", true);
                    Terminal::instance().addLog("[POSTFXRAND] Excluded wave/distortion properties");
                } else if (PostFX::instance().setAnimExcluded(args[1], true)) {
                    Terminal::instance().addLog("[POSTFXRAND] Excluded '" + args[1] + "' from animation");
                } else {
                    Terminal::instance().addLog("[POSTFXRAND] Unknown property: " + args[1]);
                }
            } else {
                Terminal::instance().addLog("[POSTFXRAND] Unknown subcommand: " + args[0]);
            }
        }
    });
    Terminal::instance().registerCommand({
        "texbreathe", "Make world textures slowly drift, breathe, and shift",
        "texbreathe <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[TEXBREATHE] Usage: texbreathe <0|1>");
                return;
            }
            if (args[0] == "1") {
                setTexBreatheEnabled(true);
                Terminal::instance().addLog("[TEXBREATHE] World textures now breathing");
            } else {
                setTexBreatheEnabled(false);
                Terminal::instance().addLog("[TEXBREATHE] Restoring original textures");
            }
        }
    });
}
