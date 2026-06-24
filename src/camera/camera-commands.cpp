#include <cstdio>
#include <string>
#include <vector>
#include "devtools/terminal.h"
#include "terminal/terminal-state.h"
#include "config/player-settings.h"
#include "replay/replay.h"

void registerCameraCommands()
{
    Terminal::instance().registerCommand({
        "freecam", "Detach or attach the gameplay camera", "freecam <0|1>",
        [](const std::vector<std::string>& args) {
            FREECAM_ENABLED = args.empty() ? !FREECAM_ENABLED : args[0] != "0";
            if (FREECAM_ENABLED)
                THE_CAMERA.pos = THE_PLAYER.pos + glm::vec3(0, 0, 2.0f);
            if (REPLAY_PLAYER.isPlaying()) {
                if (FREECAM_ENABLED)
                    REPLAY_PLAYER.cameraController().setMode("freecam");
                else
                    REPLAY_PLAYER.cameraController().setMode("fp");
                Terminal::instance().addLog(std::string("[REPLAY] ") +
                    (FREECAM_ENABLED ? "Replay Freecam Enabled" : "Replay Freecam Disabled"));
            } else {
                Terminal::instance().addLog(std::string("[FREECAM] ") + (FREECAM_ENABLED ? "enabled" : "disabled"));
            }
        }
    });
    Terminal::instance().registerCommand({
        "freecam_speed", "Set freecam speed in meters per second", "freecam_speed <number>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: freecam_speed <number>");
                return;
            }
            GetPlayerSettings().freecamSpeed = std::clamp(std::stof(args[0]), 0.1f, 500.0f);
            SavePlayerSettings();
            Terminal::instance().addLog("[FREECAM] speed=" + std::to_string(GetPlayerSettings().freecamSpeed));
        }
    });
    Terminal::instance().registerCommand({
        "settings_camera_smoothness", "Camera follow smoothness 0-10 (0=locked 5=default 10=floaty)",
        "settings_camera_smoothness <0-10>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("camera_smoothness = " + std::to_string(THE_CAMERA.smoothness));
                return;
            }
            float val = std::clamp(std::stof(args[0]), 0.0f, 10.0f);
            THE_CAMERA.smoothness = val;
            Terminal::instance().addLog("camera_smoothness set to " + std::to_string(val));
        }
    });
    Terminal::instance().registerCommand({
        "scm", "Shorter version of settings_camera_smoothness, camera follow smoothness 0-10 (0=locked 5=default 10=floaty)",
        "settings_camera_smoothness <0-10>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("camera_smoothness = " + std::to_string(THE_CAMERA.smoothness));
                return;
            }
            float val = std::clamp(std::stof(args[0]), 0.0f, 10.0f);
            THE_CAMERA.smoothness = val;
            Terminal::instance().addLog("camera_smoothness set to " + std::to_string(val));
        }
    });
}
