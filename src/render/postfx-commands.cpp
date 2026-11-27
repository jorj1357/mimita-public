#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <glm/glm.hpp>
#include <vector>
#include "debug/structured-log.h"
#include "devtools/terminal.h"
#include "render/post-fx.h"
#include "render/render-command.h"
#include "render/render-world.h"
#include "renderer/renderer.h"
#include "gui/ui-system.h"
#include "stb_image_write.h"

extern Renderer* gRenderer;
namespace r = mimita::game::render;

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

    Terminal::instance().registerCommand({
        "lens_mode",
        "Switch the live CA presentation mode: 1=mixed 2=world-only 3=normal 4=world+UI 5=bodycam-special; config=clear override",
        "lens_mode <1|2|3|4|5|config>",
        [](const std::vector<std::string>& args) {
            PostFX& fx = PostFX::instance();
            if (args.empty()) {
                const int mode = fx.effectiveLensMode();
                Terminal::instance().addLog(
                    std::string("[LENS] mode=") + std::to_string(mode) + " (" +
                    PostFX::lensModeName(mode) + ") override=" +
                    (fx.hasLensModeOverride() ? "1" : "0"));
                return;
            }
            if (args[0] == "config") {
                fx.clearLensModeOverride();
                const int mode = fx.effectiveLensMode();
                Terminal::instance().addLog("[LENS] override cleared; mode=" +
                    std::to_string(mode) + " (" + PostFX::lensModeName(mode) + ")");
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Rendering, StructuredLevel::Important,
                    "presentation.lens.mode.changed", "lens-mode",
                    "lens mode override cleared; config owns mode", 0,
                    nlohmann::json{{"mode", mode},
                                   {"mode_name", PostFX::lensModeName(mode)},
                                   {"override", false}});
                return;
            }
            int mode = 0;
            try { mode = std::stoi(args[0]); } catch (...) { mode = 0; }
            if (mode < 1 || mode > 5) {
                Terminal::instance().addLog("[LENS] usage: lens_mode <1|2|3|4|5|config>");
                return;
            }
            fx.setLensModeOverride(mode);
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Rendering, StructuredLevel::Important,
                "presentation.lens.mode.changed", "lens-mode",
                "live lens mode override changed", 0,
                nlohmann::json{{"mode", mode},
                               {"mode_name", PostFX::lensModeName(mode)},
                               {"override", true}});
            Terminal::instance().addLog(
                std::string("[LENS] mode=") + std::to_string(mode) + " (" +
                PostFX::lensModeName(mode) + ")");
        }
    });

    Terminal::instance().registerCommand({
        "lens_ca",
        "Enable, disable, or set the live chromatic-aberration strength",
        "lens_ca <on|off|value>",
        [](const std::vector<std::string>& args) {
            PostFX& fx = PostFX::instance();
            if (args.empty()) {
                Terminal::instance().addLog(
                    "[LENS] ca=" + std::string(fx.chromaticAberrationEnabled() ? "on" : "off") +
                    " strength=" + std::to_string(fx.effectiveChromaticAberration()) +
                    " override=" + (fx.hasChromaticAberrationOverride() ? "1" : "0"));
                return;
            }
            if (args[0] == "on") {
                fx.clearChromaticAberrationOverride();
            } else if (args[0] == "off") {
                fx.setChromaticAberrationOverride(0.0f);
            } else {
                float value = 0.0f;
                try { value = std::stof(args[0]); } catch (...) { value = 0.0f; }
                if (!std::isfinite(value)) value = 0.0f;
                fx.setChromaticAberrationOverride(std::max(0.0f, value));
            }
            const float strength = fx.effectiveChromaticAberration();
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Rendering, StructuredLevel::Important,
                "presentation.ca.toggle", "lens-mode",
                "live chromatic-aberration state changed", 0,
                nlohmann::json{
                    {"enabled", fx.chromaticAberrationEnabled()},
                    {"strength", strength},
                    {"override", fx.hasChromaticAberrationOverride()},
                    {"mode", fx.effectiveLensMode()},
                    {"mode_name", PostFX::lensModeName(fx.effectiveLensMode())}});
            Terminal::instance().addLog(
                "[LENS] ca=" + std::string(fx.chromaticAberrationEnabled() ? "on" : "off") +
                " strength=" + std::to_string(strength));
        }
    });

    Terminal::instance().registerCommand({
        "config_limits_probe",
        "Emit the config min/max inventory and the lens no-limit policy",
        "config_limits_probe",
        [](const std::vector<std::string>&) {
            const std::string path =
                "config/gamemodes/masterlist/gamemode-masterlist.json";
            std::ifstream in(path);
            std::string text, line;
            while (std::getline(in, line)) {
                text += line;
                text += '\n';
            }
            auto countKey = [&](const char* key) {
                const std::string needle = std::string("\"") + key + "\"";
                std::size_t n = 0, pos = 0;
                while ((pos = text.find(needle, pos)) != std::string::npos) {
                    ++n;
                    pos += needle.size();
                }
                return n;
            };
            const std::size_t minCount = countKey("min");
            const std::size_t maxCount = countKey("max");
            StructuredLogger::instance().writeEvent(
                StructuredCategory::General, StructuredLevel::Important,
                "config.limits.inventory", "config-limits",
                "numeric config bounds inventory", 0,
                nlohmann::json{
                    {"file", path},
                    {"baseline", "2026-11-26"},
                    {"min_token_count", minCount},
                    {"max_token_count", maxCount},
                    {"field_count_method",
                     "raw token count; run devscripts/config-limit-inventory.py for field counts"}});
            StructuredLogger::instance().writeEvent(
                StructuredCategory::General, StructuredLevel::Important,
                "config.limits.policy", "config-limits",
                "lens range fields are no longer validation gates", 0,
                nlohmann::json{
                    {"scope", "settings.presentation.bodycam.lens"},
                    {"removed", {"chromatic_aberration", "vignette",
                                 "distortion", "curve", "zoom", "edge_radius",
                                 "edge_softness", "edge_darkness",
                                 "peripheral_blur"}},
                    {"remaining", "scheduled migration"},
                    {"baseline", "2026-11-26"}});
            Terminal::instance().addLog(
                "[CONFIG] limits inventory min=" + std::to_string(minCount) +
                " max=" + std::to_string(maxCount));
        }
    });

    Terminal::instance().registerCommand({
        "presentation_stretch_probe",
        "Emit the fullscreen stretch policy and a pixel verdict",
        "presentation_stretch_probe",
        [](const std::vector<std::string>&) {
            if (!gRenderer) {
                Terminal::instance().addLog("[STRETCH] no renderer");
                return;
            }
            const int sceneW = gRenderer->sceneWidth;
            const int sceneH = gRenderer->sceneHeight;
            const int outW = gRenderer->width;
            const int outH = gRenderer->height;
            const bool wholeFrame = gRenderer->stretchesWholeFrame();
            const int mode = gRenderer->appliedVideoMode();
            const double stretchX = sceneW > 0
                ? static_cast<double>(outW) / sceneW : 0.0;
            const double stretchY = sceneH > 0
                ? static_cast<double>(outH) / sceneH : 0.0;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Rendering, StructuredLevel::Important,
                "video.stretch.policy", "presentation-probe",
                "fullscreen stretch policy", 0,
                nlohmann::json{
                    {"fullscreen_mode", mode},
                    {"fullscreen_name", fullscreenModeName(
                        static_cast<FullscreenMode>(mode))},
                    {"whole_frame", wholeFrame},
                    {"scene_width", sceneW}, {"scene_height", sceneH},
                    {"output_width", outW}, {"output_height", outH},
                    {"stretch_x", stretchX}, {"stretch_y", stretchY},
                    {"crisp_upscale", false}});

            std::int32_t viewport[4] = {0, 0, 0, 0};
            r::getViewport(viewport);
            const int w = viewport[2] > 0 ? viewport[2] : outW;
            const int h = viewport[3] > 0 ? viewport[3] : outH;
            double cornerNonBlack = 0.0, centerNonBlack = 0.0;
            if (w > 0 && h > 0) {
                std::vector<unsigned char> pixels(
                    static_cast<size_t>(w) * static_cast<size_t>(h) * 3u);
                r::bindFramebuffer(r::FramebufferTarget::Read, 0);
                r::readPixels(0, 0, w, h, r::PixelFormat::RGB,
                              r::PixelType::UnsignedByte, pixels.data());
                auto fraction = [&](int x0, int y0, int bw, int bh) {
                    std::uint64_t nonBlack = 0, total = 0;
                    for (int y = y0; y < std::min(h, y0 + bh); y += 2) {
                        for (int x = x0; x < std::min(w, x0 + bw); x += 2) {
                            const size_t i =
                                (static_cast<size_t>(y) * w + x) * 3u;
                            ++total;
                            if (pixels[i] || pixels[i + 1] || pixels[i + 2])
                                ++nonBlack;
                        }
                    }
                    return total ? static_cast<double>(nonBlack) / total : 0.0;
                };
                const int cw = std::max(1, w / 10);
                const int ch = std::max(1, h / 10);
                cornerNonBlack = 0.25 * (
                    fraction(0, 0, cw, ch) +
                    fraction(w - cw, 0, cw, ch) +
                    fraction(0, h - ch, cw, ch) +
                    fraction(w - cw, h - ch, cw, ch));
                centerNonBlack = fraction(w / 2 - cw, h / 2 - ch, cw * 2, ch * 2);
            }
            const bool matches = wholeFrame && sceneW == 1024 &&
                sceneH == 768 && (outW > sceneW || outH > sceneH);
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Rendering, StructuredLevel::Important,
                "video.stretch.probe", "presentation-probe",
                "stretched framebuffer pixel probe", 0,
                nlohmann::json{
                    {"width", w}, {"height", h},
                    {"corner_non_black_fraction", cornerNonBlack},
                    {"center_non_black_fraction", centerNonBlack},
                    {"expected_scene", "1024x768"}});
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Rendering, StructuredLevel::Important,
                "video.stretch.verdict", "presentation-probe",
                "fullscreen stretch verdict", 0,
                nlohmann::json{
                    {"fullscreen_mode", mode},
                    {"whole_frame", wholeFrame},
                    {"scene_width", sceneW}, {"scene_height", sceneH},
                    {"output_width", outW}, {"output_height", outH},
                    {"stretch_x", stretchX}, {"stretch_y", stretchY},
                    {"matches", matches}});
            Terminal::instance().addLog(
                std::string("[STRETCH] ") + (matches ? "matches" : "mismatch") +
                " scene=" + std::to_string(sceneW) + "x" +
                std::to_string(sceneH) + " output=" + std::to_string(outW) +
                "x" + std::to_string(outH));
        }
    });

    Terminal::instance().registerCommand({
        "presentation_pattern",
        "Draw a deterministic world+UI edge pattern for the next N frames",
        "presentation_pattern <frames>",
        [](const std::vector<std::string>& args) {
            int frames = 180;
            if (!args.empty()) {
                try { frames = std::stoi(args[0]); } catch (...) { frames = 180; }
            }
            frames = std::clamp(frames, 1, 1200);
            PostFX::instance().setPresentationPatternFrames(frames);
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Rendering, StructuredLevel::Important,
                "presentation.pattern.enabled", "presentation-probe",
                "deterministic world and UI edge pattern enabled", 0,
                nlohmann::json{{"frames", frames}});
            Terminal::instance().addLog(
                "[PROBE] presentation pattern frames=" + std::to_string(frames));
        }
    });

    Terminal::instance().registerCommand({
        "presentation_probe",
        "Capture a PNG and ten shader/pixel evidence events after applying [mode]",
        "presentation_probe <tag> [mode]",
        [](const std::vector<std::string>& args) {
            const std::string tag = args.empty() ? "probe" : args[0];
            int requested = 0;
            if (args.size() > 1) {
                try { requested = std::stoi(args[1]); } catch (...) { requested = 0; }
            }
            requestPresentationProbe(tag, requested);
        }
    });
}

namespace {

struct PresentationProbeRequest {
    bool pending = false;
    int framesRemaining = 0;
    std::string tag;
    int requestedMode = 0;
};

PresentationProbeRequest gPresentationProbeRequest;

void capturePresentationProbe(const std::string& tag, int requestedMode)
{
            PostFX& fx = PostFX::instance();
            const int mode = fx.effectiveLensMode();
            const bool configuredScene = uiHudSceneLayer();
            const bool wholeFrame = gRenderer && gRenderer->stretchesWholeFrame();
            // Modes 5/6 compose the whole frame (world + UI) in the scene
            // target; other modes draw the HUD after PostFX.
            const bool uiScene = wholeFrame;
            std::int32_t viewport[4] = {0, 0, 0, 0};
            r::getViewport(viewport);
            const int w = viewport[2];
            const int h = viewport[3];
            if (w <= 0 || h <= 0 || !gRenderer) {
                Terminal::instance().addLog("[PROBE] no valid framebuffer");
                return;
            }
            std::vector<unsigned char> pixels(
                static_cast<size_t>(w) * static_cast<size_t>(h) * 3u);
            r::bindFramebuffer(r::FramebufferTarget::Read, 0);
            r::readPixels(0, 0, w, h, r::PixelFormat::RGB,
                          r::PixelType::UnsignedByte, pixels.data());

            double darkSum = 0.0, brightSum = 0.0, uiSum = 0.0;
            double uiPatternSum = 0.0;
            double worldPatternDarkSum = 0.0, worldPatternBrightSum = 0.0;
            double allSum = 0.0;
            std::uint64_t darkCount = 0, brightCount = 0, uiCount = 0;
            std::uint64_t uiPatternCount = 0;
            std::uint64_t worldPatternDarkCount = 0, worldPatternBrightCount = 0;
            std::uint64_t nonBlack = 0;
            double centerLumaSum = 0.0, cornerLumaSum = 0.0, edgeLumaSum = 0.0;
            std::uint64_t centerLumaCount = 0, cornerLumaCount = 0;
            std::uint64_t edgeLumaCount = 0;
            std::uint64_t borderCount = 0, borderBlackCount = 0;
            const int uiStartY = std::max(0, h - h / 4);
            const int uiBottomEndY = std::min(h - 1, h / 4);
            const int uiPatternMinX = std::max(0, w / 2 - 220);
            const int uiPatternMaxX = std::min(w - 1, w / 2 + 220);
            const int worldMinX = std::max(0, w / 2 - 120);
            const int worldMaxX = std::min(w - 1, w / 2 + 120);
            const int worldMinY = std::max(0, h / 2 - 90);
            const int worldMaxY = std::min(h - 1, h / 2 + 90);
            auto sample = [&](int x, int y) {
                const size_t index = (static_cast<size_t>(y) * w + x) * 3u;
                return glm::vec3(pixels[index] / 255.0f,
                                 pixels[index + 1] / 255.0f,
                                 pixels[index + 2] / 255.0f);
            };
            auto channelMisalignment = [&](int x, int y) {
                const int x2 = std::min(w - 1, x + 2);
                const int y2 = std::min(h - 1, y + 2);
                const glm::vec3 c = sample(x, y);
                const glm::vec3 hx = sample(x2, y);
                const glm::vec3 vy = sample(x, y2);
                const float hdr = std::fabs(hx.r - c.r);
                const float hdg = std::fabs(hx.g - c.g);
                const float hdb = std::fabs(hx.b - c.b);
                const float vdr = std::fabs(vy.r - c.r);
                const float vdg = std::fabs(vy.g - c.g);
                const float vdb = std::fabs(vy.b - c.b);
                // CA shifts R/B relative to the green edge; natural flat color
                // does not create this misalignment.
                return (std::fabs(hdr - hdg) + std::fabs(hdb - hdg) +
                        std::fabs(vdr - vdg) + std::fabs(vdb - vdg)) * 0.5f;
            };
            for (int y = 0; y < h; y += 2) {
                for (int x = 0; x < w; x += 2) {
                    const size_t index =
                        (static_cast<size_t>(y) * w + x) * 3u;
                    const glm::vec3 c = sample(x, y);
                    const double luma = 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b;
                    const double delta = channelMisalignment(x, y);
                    allSum += delta;
                    if (pixels[index] || pixels[index + 1] || pixels[index + 2])
                        ++nonBlack;
                    if (luma < 0.24) {
                        darkSum += delta;
                        ++darkCount;
                    } else if (luma > 0.45) {
                        brightSum += delta;
                        ++brightCount;
                    }
                    if (y >= uiStartY || y <= uiBottomEndY) {
                        uiSum += delta;
                        ++uiCount;
                        if (x >= uiPatternMinX && x <= uiPatternMaxX) {
                            uiPatternSum += delta;
                            ++uiPatternCount;
                        }
                    }
                    if (x >= worldMinX && x <= worldMaxX &&
                        y >= worldMinY && y <= worldMaxY) {
                        if (luma < 0.24) {
                            worldPatternDarkSum += delta;
                            ++worldPatternDarkCount;
                        } else if (luma > 0.45) {
                            worldPatternBrightSum += delta;
                            ++worldPatternBrightCount;
                        }
                    }
                    const bool centerRegion =
                        x >= w * 2 / 5 && x <= w * 3 / 5 &&
                        y >= h * 2 / 5 && y <= h * 3 / 5;
                    if (centerRegion) {
                        centerLumaSum += luma;
                        ++centerLumaCount;
                    }
                    const int cornerW = std::max(1, w / 12);
                    const int cornerH = std::max(1, h / 12);
                    const bool cornerRegion =
                        (x < cornerW || x >= w - cornerW) &&
                        (y < cornerH || y >= h - cornerH);
                    if (cornerRegion) {
                        cornerLumaSum += luma;
                        ++cornerLumaCount;
                    }
                    const int edgeW = std::max(1, w / 10);
                    const int edgeH = std::max(1, h / 10);
                    if (x < edgeW || x >= w - edgeW ||
                        y < edgeH || y >= h - edgeH) {
                        edgeLumaSum += luma;
                        ++edgeLumaCount;
                    }
                    const int borderW = std::max(1, w * 3 / 100);
                    const int borderH = std::max(1, h * 3 / 100);
                    if (x < borderW || x >= w - borderW ||
                        y < borderH || y >= h - borderH) {
                        ++borderCount;
                        if (luma < 0.02) ++borderBlackCount;
                    }
                }
            }
            const double darkAvg = darkCount ? darkSum / darkCount : 0.0;
            const double brightAvg = brightCount ? brightSum / brightCount : 0.0;
            const double uiAvg = uiCount ? uiSum / uiCount : 0.0;
            const double uiPatternAvg = uiPatternCount
                ? uiPatternSum / uiPatternCount : 0.0;
            const double worldPatternDarkAvg = worldPatternDarkCount
                ? worldPatternDarkSum / worldPatternDarkCount : 0.0;
            const double worldPatternBrightAvg = worldPatternBrightCount
                ? worldPatternBrightSum / worldPatternBrightCount : 0.0;
            const double allAvg = allSum / std::max<std::uint64_t>(1,
                (std::uint64_t)((w + 1) / 2) * (std::uint64_t)((h + 1) / 2));
            const double centerLuma = centerLumaCount
                ? centerLumaSum / centerLumaCount : 0.0;
            const double cornerLuma = cornerLumaCount
                ? cornerLumaSum / cornerLumaCount : 0.0;
            const double edgeLuma = edgeLumaCount
                ? edgeLumaSum / edgeLumaCount : 0.0;
            const double cornerDarkening = centerLuma > 1e-6
                ? std::clamp(1.0 - cornerLuma / centerLuma, 0.0, 1.0) : 0.0;
            const double edgeFalloff = centerLuma > 1e-6
                ? std::clamp(1.0 - edgeLuma / centerLuma, 0.0, 1.0) : 0.0;
            const double borderBlackFraction = borderCount
                ? static_cast<double>(borderBlackCount) / borderCount : 0.0;

            const auto* lens = fx.bodycamLensOverride();
            const bool lensOn = mode == 1 || mode == 5;
            const float lensVignette = lensOn && lens ? lens->vignette : 0.0f;
            const float lensDistortion = lensOn && lens ? lens->distortion : 0.0f;
            const float lensBlur = lensOn && lens ? lens->peripheralBlur : 0.0f;
            const bool caEnabled = fx.chromaticAberrationEnabled();
            const char* caMode = mode == 1 ? "background_only"
                : mode == 3 ? "off" : "world";

            const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            std::filesystem::create_directories("build/presentation-probes");
            const std::string path = "build/presentation-probes/" + tag +
                "-mode" + std::to_string(mode) + "-" +
                std::to_string(now) + ".png";
            // readPixels returns bottom-up rows; PNG expects top-down.
            std::vector<unsigned char> pngPixels(pixels.size());
            const size_t rowBytes = static_cast<size_t>(w) * 3u;
            for (int row = 0; row < h; ++row) {
                std::copy_n(pixels.data() + static_cast<size_t>(row) * rowBytes,
                            rowBytes,
                            pngPixels.data() +
                                static_cast<size_t>(h - 1 - row) * rowBytes);
            }
            stbi_write_png(path.c_str(), w, h, 3, pngPixels.data(), w * 3);

            auto emit = [&](const char* event, nlohmann::json fields) {
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Rendering, StructuredLevel::Important,
                    event, "presentation-probe",
                    "presentation shader evidence", 0, std::move(fields));
            };
            emit("presentation.lens.mode", {
                {"mode", mode}, {"mode_name", PostFX::lensModeName(mode)},
                {"override", fx.hasLensModeOverride()},
                {"whole_frame", wholeFrame}});
            const float effectiveCa = fx.effectiveChromaticAberration();
            emit("presentation.lens.policy", {
                {"configured_scene_layer", configuredScene},
                {"ui_in_scene", uiScene},
                {"ca_strength", effectiveCa},
                {"ca_mode", caMode},
                {"ca_enabled", caEnabled},
                {"background_only", mode == 1},
                {"lens_on", lensOn},
                {"lens_vignette", lensVignette},
                {"lens_distortion", lensDistortion},
                {"lens_peripheral_blur", lensBlur},
                {"lens_edge_mode", lens ? lens->edgeMode : 0}});
            emit("presentation.postfx.shader", {
                {"shader_program", fx.hasFbo() ? "postfx-fbo" : "direct"},
                {"fbo_width", fx.fboWidth()}, {"fbo_height", fx.fboHeight()},
                {"viewport_width", w}, {"viewport_height", h}});
            emit("presentation.postfx.pass", {
                {"mode", mode}, {"bypass", fx.bypass()},
                {"postfx_enabled", fx.enabled()}});
            emit("presentation.ui.layer", {
                {"ui_in_scene", uiScene}, {"configured_scene", configuredScene},
                {"whole_frame", wholeFrame}, {"mode", mode}});
            emit("presentation.framebuffer.readback", {
                {"width", w}, {"height", h}, {"non_black_pixels", nonBlack},
                {"sampled_pixels", ((w + 1) / 2) * ((h + 1) / 2)}});
            emit("presentation.pixel.dark_ca", {
                {"samples", darkCount}, {"mean_abs_r_minus_b", darkAvg},
                {"world_pattern_dark_mean", worldPatternDarkAvg},
                {"world_pattern_dark_samples", worldPatternDarkCount}});
            emit("presentation.pixel.bright_ca", {
                {"samples", brightCount}, {"mean_abs_r_minus_b", brightAvg},
                {"world_pattern_bright_mean", worldPatternBrightAvg},
                {"world_pattern_bright_samples", worldPatternBrightCount}});
            emit("presentation.pixel.ui_ca", {
                {"samples", uiCount}, {"mean_abs_r_minus_b", uiAvg},
                {"region", "top_140px"},
                {"ui_pattern_samples", uiPatternCount},
                {"ui_pattern_mean_channel_misalignment", uiPatternAvg}});
            emit("presentation.pixel.vignette", {
                {"center_luma", centerLuma}, {"corner_luma", cornerLuma},
                {"edge_luma", edgeLuma},
                {"corner_darkening", cornerDarkening},
                {"edge_falloff", edgeFalloff},
                {"region", "corners_8pct_vs_center_20pct"}});
            emit("presentation.pixel.fisheye", {
                {"border_black_fraction", borderBlackFraction},
                {"edge_falloff", edgeFalloff},
                {"border_luma_ratio", centerLuma > 1e-6
                    ? edgeLuma / centerLuma : 0.0},
                {"radial_edge_present",
                    borderBlackFraction > 0.02 || edgeFalloff > 0.02},
                {"region", "outer_3pct_border"}});
            const bool modeApplied = requestedMode == 0 || requestedMode == mode;
            const bool worldCa = worldPatternBrightAvg > (mode == 5 ? 0.01 : 0.02);
            const bool uiCa = uiPatternAvg > 0.02;
            bool matches = false;
            if (mode == 1)
                matches = worldPatternDarkAvg > 0.02 &&
                    worldPatternBrightAvg < 0.012 &&
                    worldPatternDarkAvg > worldPatternBrightAvg * 3.0;
            else if (mode == 2)
                matches = worldCa && !uiCa &&
                    lensVignette == 0.0f && lensDistortion == 0.0f;
            else if (mode == 3)
                matches = worldPatternBrightAvg < 0.002 &&
                    worldPatternDarkAvg < 0.002 && uiPatternAvg < 0.012 &&
                    lensVignette == 0.0f && lensDistortion == 0.0f;
            else if (mode == 4)
                matches = worldCa && uiScene && wholeFrame && uiCa &&
                    lensVignette == 0.0f && lensDistortion == 0.0f;
            else if (mode == 5) {
                const bool caPixels = caEnabled
                    ? (worldPatternBrightAvg > 0.01 && uiPatternAvg > 0.02)
                    : (worldPatternBrightAvg < 0.004 &&
                       uiPatternAvg < 0.012);
                const bool vignetteProof = lensVignette > 0.0f &&
                    cornerDarkening > 0.02;
                const bool fisheyeProof = lensDistortion != 0.0f &&
                    (borderBlackFraction > 0.02 || edgeFalloff > 0.02);
                matches = wholeFrame && uiScene && caPixels &&
                    vignetteProof && fisheyeProof;
            }
            matches = matches && modeApplied;
            emit("presentation.mode.verdict", {
                {"mode", mode}, {"mode_name", PostFX::lensModeName(mode)},
                {"requested_mode", requestedMode},
                {"mode_applied", modeApplied},
                {"matches", matches}, {"dark_mean_abs_r_minus_b", darkAvg},
                {"bright_mean_abs_r_minus_b", brightAvg},
                {"world_pattern_dark_mean", worldPatternDarkAvg},
                {"world_pattern_bright_mean", worldPatternBrightAvg},
                {"ui_mean_abs_r_minus_b", uiAvg},
                {"ui_pattern_mean_channel_misalignment", uiPatternAvg},
                {"all_mean_abs_r_minus_b", allAvg},
                {"screenshot", path}});
            bool lensMatches = false;
            if (mode == 5) {
                const bool caPixels = caEnabled
                    ? (worldPatternBrightAvg > 0.01 && uiPatternAvg > 0.02)
                    : (worldPatternBrightAvg < 0.004 &&
                       uiPatternAvg < 0.012);
                lensMatches = wholeFrame && uiScene && caPixels &&
                    lensVignette > 0.0f && cornerDarkening > 0.02 &&
                    lensDistortion != 0.0f &&
                    (borderBlackFraction > 0.02 || edgeFalloff > 0.02);
            } else if (mode == 2) {
                lensMatches = worldCa && !uiCa &&
                    lensVignette == 0.0f && lensDistortion == 0.0f;
            } else {
                lensMatches = modeApplied;
            }
            emit("presentation.lens.verdict", {
                {"mode", mode}, {"mode_name", PostFX::lensModeName(mode)},
                {"ca_enabled", caEnabled},
                {"world_ca", worldCa}, {"ui_ca", uiCa},
                {"lens_vignette", lensVignette},
                {"lens_distortion", lensDistortion},
                {"corner_darkening", cornerDarkening},
                {"border_black_fraction", borderBlackFraction},
                {"edge_falloff", edgeFalloff},
                {"whole_frame", wholeFrame},
                {"ui_in_scene", uiScene},
                {"matches", lensMatches && modeApplied}});
            Terminal::instance().addLog(
                "[PROBE] mode=" + std::to_string(mode) +
                " matches=" + (matches ? "1" : "0") +
                " png=" + path);
}

} // namespace

void requestPresentationProbe(const std::string& tag, int mode)
{
    PostFX& fx = PostFX::instance();
    if (mode >= 1 && mode <= 5) {
        fx.setLensModeOverride(mode);
        fx.setPresentationPatternFrames(std::max(fx.presentationPatternFrames(), 3));
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Rendering, StructuredLevel::Important,
            "presentation.lens.mode.changed", "lens-mode",
            "lens mode applied by atomic presentation probe", 0,
            nlohmann::json{{"mode", mode},
                           {"mode_name", PostFX::lensModeName(mode)},
                           {"override", true}});
    }
    gPresentationProbeRequest.pending = true;
    gPresentationProbeRequest.framesRemaining = 2;
    gPresentationProbeRequest.tag = tag;
    gPresentationProbeRequest.requestedMode = mode;
    Terminal::instance().addLog(
        "[PROBE] scheduled tag=" + tag + " mode=" + std::to_string(mode));
}

void tickPresentationProbeFrame()
{
    if (!gPresentationProbeRequest.pending) return;
    if (gPresentationProbeRequest.framesRemaining > 0) {
        --gPresentationProbeRequest.framesRemaining;
        return;
    }
    gPresentationProbeRequest.pending = false;
    if (gRenderer) {
        capturePresentationProbe(gPresentationProbeRequest.tag,
                                 gPresentationProbeRequest.requestedMode);
    }
}
