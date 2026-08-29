#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "devtools/terminal.h"
#include "terminal/terminal-state.h"
#include "video/frame-pacer.h"
#include "video/video-settings.h"
#include "renderer/renderer.h"
#include "config/player-settings.h"

extern FramePacer gFramePacer;

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
        "fullscreen", "Toggle fullscreen mode", "fullscreen <0|1> (0=windowed 1=fullscreen)",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[VIDEO] Fullscreen: " +
                    std::string(VideoSettings::instance().fullscreen() ? "ON" : "OFF"));
                Terminal::instance().addLog("[VIDEO] Usage: fullscreen <0|1>");
                return;
            }
            bool on = args[0] == "1";
            VideoSettings::instance().setFullscreen(on);
            Terminal::instance().addLog("[VIDEO] Fullscreen: " +
                std::string(on ? "ON" : "OFF"));
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
            Terminal::instance().addLog(std::string("[VIDEO] Fullscreen: ") +
                (vs.fullscreen() ? "ON" : "OFF"));
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
