// 08 21 2026, 12 00
/* purpose
* Registers terminal commands that control camera behavior.
* Exposes gameplay freecam and avatar-creator preview free-look controls.
* Reports command outcomes through the shared terminal.
* DOES NOT update camera transforms each frame or render preview cameras.
* DOES NOT persist avatar free-look state to GUI JSON.
* DOES NOT change gameplay camera state when freecamav is used.
*/

#include <cstdio>
#include <string>
#include <vector>
#include "devtools/terminal.h"
#include "terminal/terminal-state.h"
#include "config/player-settings.h"
#include "config/camera-config.h"
#include "replay/replay.h"
#include "gui/menus/menu-avatar-preview.h"

#include "physics/config.h"

void registerCameraCommands()
{
    Terminal::instance().registerCommand({
        "freecamav", "Enable or disable avatar-creator preview free look", "freecamav [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.size() > 1 || (!args.empty() && args[0] != "0" && args[0] != "1")) {
                Terminal::instance().addLog("[AVATAR FREECAM] Usage: freecamav [0|1]");
                return;
            }
            MenuAvatarPreview& preview = MenuAvatarPreview::instance();
            bool enabled = args.empty() ? !preview.avatarFreecamEnabled() : args[0] == "1";
            preview.setAvatarFreecam(enabled);
            Terminal::instance().addLog(std::string("[AVATAR FREECAM] ") +
                (enabled ? "enabled: WASD move, Q down, E up, mouse look"
                         : "disabled: JSON orbit camera restored"));
        },
        "2026-08-21", CommandCategory::UI
    });

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
        "settings_camera_smoothness", "Camera follow stiffness 0.0-1.0 (1=rigid, 0=floaty)",
        "settings_camera_smoothness <0.0-1.0>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("camera_stiffness = " + std::to_string(CamConfig::instance().data().positionStiffness));
                return;
            }
            float val = std::clamp(std::stof(args[0]), 0.0f, 1.0f);
            Terminal::instance().addLog("Camera stiffness set to " + std::to_string(val) + " (edit config/camconfig.json to persist)");
        }
    });
    Terminal::instance().registerCommand({
        "scm", "Shorter version of settings_camera_smoothness, camera stiffness 0-1 (1=rigid, 0=floaty)",
        "scm <0.0-1.0>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("camera_stiffness = " + std::to_string(CamConfig::instance().data().positionStiffness));
                return;
            }
            float val = std::clamp(std::stof(args[0]), 0.0f, 1.0f);
            Terminal::instance().addLog("Camera stiffness set to " + std::to_string(val) + " (edit config/camconfig.json to persist)");
        }
    });
    Terminal::instance().registerCommand({
        "thirdp", "Toggle third-person / first-person camera", "thirdp <0|1>",
        [](const std::vector<std::string>& args) {
            Camera& cam = THE_CAMERA;
            bool was = cam.thirdPerson;
            cam.thirdPerson = args.empty() ? !was : args[0] != "0";
            Terminal::instance().addLog(std::string("Camera Mode: ") +
                (cam.thirdPerson ? "Third Person" : "First Person"));
        }
    });

    Terminal::instance().registerCommand({
        "fov", "Set camera field of view (1-359). Default is 100.",
        "fov <1-359>",
        [](const std::vector<std::string>& args) {
            Camera& cam = THE_CAMERA;
            if (args.empty()) {
                Terminal::instance().addLog(
                    "[FOV] current=" + std::to_string(cam.fov));
                return;
            }
            float val = std::stof(args[0]);
            val = std::clamp(val, 1.0f, 359.0f);
            cam.fov = val;
            Terminal::instance().addLog(
                "[FOV] set to " + std::to_string(cam.fov));
        },
        "2026-07-02",
        CommandCategory::Weapon
    });
}
