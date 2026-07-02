#include "replay-camera.h"

#include <cstdio>

#include <glm/glm.hpp>

#include "camera.h"
#include "devtools/terminal.h"
#include "terminal/terminal-state.h"

void registerReplayCameraCommands()
{
    Terminal::instance().registerCommand({
        "rplcamfree", "Enter freecam during replay playback", "rplcamfree",
        [](const std::vector<std::string>&) {
            REPLAY_CAMERA_MGR.setMode("freecam");
            REPLAY_PLAYER.cameraController().setMode("freecam");
            Terminal::instance().addLog("[CAMERA] freecam mode");
        }
    }, "2026-07-02", CommandCategory::Replay);

    Terminal::instance().registerCommand({
        "rplcamfollow", "Return to player follow camera", "rplcamfollow",
        [](const std::vector<std::string>&) {
            REPLAY_CAMERA_MGR.setMode("follow");
            REPLAY_PLAYER.cameraController().setMode("fp");
            Terminal::instance().addLog("[CAMERA] follow mode");
        }
    }, "2026-07-02", CommandCategory::Replay);

    Terminal::instance().registerCommand({
        "rplcamkey", "Enter keyframed camera mode (interpolate between keyframes)",
        "rplcamkey",
        [](const std::vector<std::string>&) {
            REPLAY_CAMERA_MGR.setMode("keyframed");
            Terminal::instance().addLog("[CAMERA] keyframed mode");
        }
    }, "2026-07-02", CommandCategory::Replay);

    Terminal::instance().registerCommand({
        "rplcks", "Create a camera keyframe at the current tick and position",
        "rplcks",
        [](const std::vector<std::string>&) {
            int tick = (int)REPLAY_PLAYER.currentTick();
            REPLAY_CAMERA_MGR.addKeyframe(tick, THE_CAMERA);
            char buf[128];
            snprintf(buf, sizeof(buf),
                "[CAMERA TIMELINE] keyframe at tick %d  pos=(%.1f %.1f %.1f) yaw=%.1f pitch=%.1f fov=%.1f mode=%s",
                tick, THE_CAMERA.pos.x, THE_CAMERA.pos.y, THE_CAMERA.pos.z,
                THE_CAMERA.yaw, THE_CAMERA.pitch, THE_CAMERA.fov,
                REPLAY_CAMERA_MGR.mode().c_str());
            Terminal::instance().addLog(buf);
            printf("%s\n", buf);
        }
    }, "2026-07-02", CommandCategory::Replay);

    Terminal::instance().registerCommand({
        "rplckdel", "Delete a camera keyframe by index", "rplckdel <index>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: rplckdel <index>");
                return;
            }
            int idx = std::stoi(args[0]);
            if (REPLAY_CAMERA_MGR.deleteKeyframe(idx)) {
                char buf[64];
                snprintf(buf, sizeof(buf), "[CAMERA TIMELINE] deleted keyframe %d", idx);
                Terminal::instance().addLog(buf);
            } else {
                Terminal::instance().addLog("[ERROR] invalid keyframe index");
            }
        }
    }, "2026-07-02", CommandCategory::Replay);

    Terminal::instance().registerCommand({
        "rplcamlist", "List all camera keyframes", "rplcamlist",
        [](const std::vector<std::string>&) {
            int count = REPLAY_CAMERA_MGR.keyframeCount();
            char buf[256];
            snprintf(buf, sizeof(buf), "[CAMERA TIMELINE] %d keyframe(s)  mode=%s",
                count, REPLAY_CAMERA_MGR.mode().c_str());
            Terminal::instance().addLog(buf);
            printf("%s\n", buf);
            for (int i = 0; i < count; ++i) {
                const auto& kf = REPLAY_CAMERA_MGR.keyframe(i);
                snprintf(buf, sizeof(buf),
                    "  [%d] tick=%d  pos=(%.1f %.1f %.1f) yaw=%.1f pitch=%.1f fov=%.1f mode=%s",
                    i, kf.tick,
                    kf.position.x, kf.position.y, kf.position.z,
                    kf.yaw, kf.pitch, kf.fov,
                    kf.mode.empty() ? "(none)" : kf.mode.c_str());
                Terminal::instance().addLog(buf);
                printf("%s\n", buf);
            }
        }
    }, "2026-07-02", CommandCategory::Replay);

    Terminal::instance().registerCommand({
        "rplcamclear", "Remove all camera keyframes", "rplcamclear",
        [](const std::vector<std::string>&) {
            REPLAY_CAMERA_MGR.clearKeyframes();
            Terminal::instance().addLog("[CAMERA TIMELINE] all keyframes cleared");
        }
    }, "2026-07-02", CommandCategory::Replay);

    Terminal::instance().registerCommand({
        "rplcamgoto", "Jump camera to a keyframe position by index", "rplcamgoto <index>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: rplcamgoto <index>");
                return;
            }
            int idx = std::stoi(args[0]);
            if (idx < 0 || idx >= REPLAY_CAMERA_MGR.keyframeCount()) {
                Terminal::instance().addLog("[ERROR] invalid keyframe index");
                return;
            }
            const auto& kf = REPLAY_CAMERA_MGR.keyframe(idx);
            THE_CAMERA.pos = kf.position;
            THE_CAMERA.yaw = kf.yaw;
            THE_CAMERA.pitch = kf.pitch;
            THE_CAMERA.fov = kf.fov;
            THE_CAMERA.updateVectors();
            REPLAY_PLAYER.seekToTick((uint32_t)kf.tick);
            char buf[128];
            snprintf(buf, sizeof(buf),
                "[CAMERA TIMELINE] jumped to keyframe %d at tick %d", idx, kf.tick);
            Terminal::instance().addLog(buf);
        }
    }, "2026-07-02", CommandCategory::Replay);

    Terminal::instance().registerCommand({
        "rplcamsave", "Save camera timeline to config/cameratimeline.json",
        "rplcamsave",
        [](const std::vector<std::string>&) {
            if (REPLAY_CAMERA_MGR.save())
                Terminal::instance().addLog("[CAMERA TIMELINE] saved");
            else
                Terminal::instance().addLog("[ERROR] failed to save camera timeline");
        }
    }, "2026-07-02", CommandCategory::Replay);

    Terminal::instance().registerCommand({
        "rplcamload", "Load camera timeline from config/cameratimeline.json",
        "rplcamload",
        [](const std::vector<std::string>&) {
            if (REPLAY_CAMERA_MGR.load())
                Terminal::instance().addLog("[CAMERA TIMELINE] loaded");
            else
                Terminal::instance().addLog("[ERROR] failed to load camera timeline");
        }
    }, "2026-07-02", CommandCategory::Replay);
}
