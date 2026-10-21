#include <cstdio>
#include <cstdlib>
#include <charconv>
#include <string>
#include <vector>
#include "devtools/terminal.h"
#include "terminal/terminal-state.h"
#include "video/frame-pacer.h"
#include "video/video-settings.h"
#include "renderer/renderer.h"
#include "config/player-settings.h"
#include "debug/structured-log.h"

extern FramePacer gFramePacer;
extern Renderer* gRenderer;

void registerVideoCommands()
{
    Terminal::instance().registerCommand({
        "resolution", "Set display resolution", "resolution <1-4> (1=800x600 2=1024x768 3=1280x720 4=1920x1080)",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                int idx = VideoSettings::instance().resolutionIndex();
                Terminal::instance().addLog("[VIDEO] Current resolution: " +
                    std::to_string(VideoSettings::instance().width()) + "x" +
                    std::to_string(VideoSettings::instance().height()) +
                    " (index " + std::to_string(idx) + ")");
                Terminal::instance().addLog("[VIDEO] Usage: resolution <1-4>");
                Terminal::instance().addLog("[VIDEO]   1 = 800x600");
                Terminal::instance().addLog("[VIDEO]   2 = 1024x768");
                Terminal::instance().addLog("[VIDEO]   3 = 1280x720");
                Terminal::instance().addLog("[VIDEO]   4 = 1920x1080");
                return;
            }
            int idx = std::atoi(args[0].c_str());
            VideoSettings::instance().setResolution(idx);
            Terminal::instance().addLog("[VIDEO] Resolution set to " +
                std::to_string(VideoSettings::instance().width()) + "x" +
                std::to_string(VideoSettings::instance().height()));
        }
    });
    Terminal::instance().registerCommand({
        "fullscreen", "Set fullscreen mode", "fullscreen <0-6> (0=windowed 1=exclusive 2=borderless 3=exclusive world-stretched 4=borderless world-stretched 5=exclusive full-frame 1024x768 stretched 6=borderless full-frame 1024x768 stretched)",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                const auto mode = VideoSettings::instance().fullscreenMode();
                Terminal::instance().addLog("[VIDEO] Fullscreen mode: " +
                    std::to_string(static_cast<int>(mode)) + " (" + fullscreenModeName(mode) + ")");
                Terminal::instance().addLog("[VIDEO] Usage: fullscreen <0-6>");
                Terminal::instance().addLog("[VIDEO] 0=windowed 1=exclusive 2=borderless 3=exclusive world-stretched 4=borderless world-stretched 5=exclusive full-frame 1024x768 stretched 6=borderless full-frame 1024x768 stretched");
                return;
            }
            if (args.size() != 1) {
                Terminal::instance().addLog("[VIDEO] Usage: fullscreen <0-6>");
                return;
            }
            int mode = -1;
            const std::string& value = args[0];
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), mode);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
                mode < 0 || mode > 6) {
                Terminal::instance().addLog("[VIDEO] Invalid mode. Use fullscreen <0-6>.");
                return;
            }
            if (VideoSettings::instance().setFullscreenMode(mode)) {
                const auto applied = VideoSettings::instance().fullscreenMode();
                Terminal::instance().addLog("[VIDEO] Fullscreen mode: " +
                    std::to_string(static_cast<int>(applied)) + " (" + fullscreenModeName(applied) + ")");
                const std::string requestId = Terminal::instance().currentRequestId();
                const bool stretched = fullscreenModeIsStretched(applied);
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::General, StructuredLevel::Important,
                    "video.fullscreen.mode_applied",
                    requestId.empty() ? "fullscreen-command" : requestId,
                    applied == static_cast<FullscreenMode>(mode)
                        ? "requested mode applied" : "requested mode fell back",
                    0,
                    {{"requested_mode", mode},
                     {"applied_mode", static_cast<int>(applied)},
                     {"mode_name", fullscreenModeName(applied)},
                     {"stretched", stretched},
                     {"whole_frame_stretched", fullscreenModeStretchesWholeFrame(applied)},
                     {"scene_width", gRenderer ? gRenderer->sceneWidth : 0},
                     {"scene_height", gRenderer ? gRenderer->sceneHeight : 0},
                     {"output_width", gRenderer ? gRenderer->width : 0},
                     {"output_height", gRenderer ? gRenderer->height : 0}},
                    __FILE__, __LINE__, __FUNCTION__);
            }
        }
    });
    Terminal::instance().registerCommand({
        "video_info", "Show current video settings", "video_info",
        [](const std::vector<std::string>&) {
            auto& vs = VideoSettings::instance();
            char buf[256];
            snprintf(buf, sizeof(buf), "Resolution: %dx%d  (index %d)",
                     vs.width(), vs.height(), vs.resolutionIndex());
            Terminal::instance().addLog(std::string("[VIDEO] ") + buf);
            const auto mode = vs.fullscreenMode();
            Terminal::instance().addLog(std::string("[VIDEO] Fullscreen mode: ") +
                std::to_string(static_cast<int>(mode)) + " (" + fullscreenModeName(mode) + ")");
            if (gRenderer) {
                Terminal::instance().addLog("[VIDEO] Scene/output: " +
                    std::to_string(gRenderer->sceneWidth) + "x" + std::to_string(gRenderer->sceneHeight) +
                    " / " + std::to_string(gRenderer->width) + "x" + std::to_string(gRenderer->height));
            }
            Terminal::instance().addLog("[VIDEO] Resizable: OFF");
            Terminal::instance().addLog("[VIDEO] maxFrames=" +
                std::to_string(vs.maxFrames()));
            Terminal::instance().addLog("[VIDEO] vsync=OFF (forced)");
        }
    });
    Terminal::instance().registerCommand({
        "maxframes", "Set maximum FPS cap", "maxframes <10-999>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[VIDEO] maxFrames=" +
                    std::to_string(VideoSettings::instance().maxFrames()));
                return;
            }
            int fps = std::atoi(args[0].c_str());
            VideoSettings::instance().setMaxFrames(fps);
            gFramePacer.setMaxFrames(VideoSettings::instance().maxFrames());
            Terminal::instance().addLog("[VIDEO] maxFrames set to " +
                std::to_string(gFramePacer.maxFrames()));
        }
    });
    Terminal::instance().registerCommand({
        "vsync", "VSync is forced OFF", "vsync",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[VSYNC] VSync is FORCED OFF (cannot be enabled)");
                return;
            }
            Terminal::instance().addLog("[VSYNC] BLOCKED: VSync is forced OFF and cannot be enabled");
        }
    });
    Terminal::instance().registerCommand({
        "showfps", "Toggle FPS display", "showfps [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                gFramePacer.setShowFPS(!gFramePacer.showFPS());
            } else {
                gFramePacer.setShowFPS(args[0] == "1");
            }
            Terminal::instance().addLog(std::string("[VIDEO] showfps=") +
                (gFramePacer.showFPS() ? "ON" : "OFF"));
        }
    });
    Terminal::instance().registerCommand({
        "frame_debug", "Toggle frame pacing diagnostics", "frame_debug [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                gFramePacer.setFrameDebug(!gFramePacer.frameDebug());
            } else {
                gFramePacer.setFrameDebug(args[0] == "1");
            }
            if (gFramePacer.frameDebug())
                gFramePacer.setShowFPS(true);
            Terminal::instance().addLog(std::string("[VIDEO] frame_debug=") +
                (gFramePacer.frameDebug() ? "ON" : "OFF"));
        }
    });
}
