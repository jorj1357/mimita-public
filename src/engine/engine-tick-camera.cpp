#include "engine/engine-tick-camera.h"
#include "engine/engine.h"
#include "terminal/terminal-state.h"
#include <cmath>
#include <GLFW/glfw3.h>
#include "camera.h"
#include "world/world.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "physics/physics-debug-movement.h"
#include "audio/audio.h"
#include "audio/hitmarker-audio.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "replay/replay.h"
#include "replay/replay-camera.h"
#include "replay/replay-editor.h"
#include "replay/replay-export.h"
#include "gui/hud/chat-bubble.h"
#include "ui/hitmarker.h"
#include "config/player-settings.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "game/duel.h"
#include "devtools/terminal.h"
#include "game/game-state.h"
#include "physics/config.h"

extern DuelManager gDuelManager;

// Export debug effect counters (declared extern in replay-export.h)
int gRplxImpactWorldCount = 0;
int gRplxHitBurstCount = 0;
int gRplxDebrisBlockCount = 0;
int gRplxEffectDuplicateCount = 0;
static std::string gRplxLastEffectKey;

static void syncPlayerYawFromCamera(Player& player, const Camera& camera)
{
    if (player.dead) return;
    glm::vec2 flat(camera.front.x, camera.front.y);
    if (glm::length(flat) > 0.0001f) {
        flat = glm::normalize(flat);
        player.yaw = glm::degrees(std::atan2(flat.y, flat.x));
    }
}

static void logRotationDebug(const Player& player, const Camera& camera, float dt)
{
    if (!DebugConfig::DEBUG_ROTATION) return;
    static float timer = 0.0f;
    timer -= dt;
    if (timer > 0.0f) return;
    timer = 0.25f;

    float cameraYaw = camera.yaw;
    float rootYaw = player.yaw;
    float capsuleYaw = 0.0f;
    glm::vec3 capsuleFwd = player.movementCapsule.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
    if (glm::length(capsuleFwd) > 0.0001f)
        capsuleYaw = glm::degrees(std::atan2(capsuleFwd.y, capsuleFwd.x));

    float aimYaw = 0.0f;
    if (player.hasAimData) {
        glm::vec3 flatAim = glm::normalize(glm::vec3(player.aimDirection.x, player.aimDirection.y, 0.0f));
        aimYaw = glm::degrees(std::atan2(flatAim.y, flatAim.x) - glm::radians(rootYaw));
    }

    Debug::log(Debug::Category::General,
        "[ROTATION] cameraYaw=%.2f rootYaw=%.2f capsuleYaw=%.2f aimYaw=%.2f diff(cam-root)=%.2f diff(root-cap)=%.2f\n",
        cameraYaw, rootYaw, capsuleYaw, aimYaw, cameraYaw - rootYaw, rootYaw - capsuleYaw);
}

void engineTickCamera(Engine& engine, float dt)
{
    Player& player = THE_PLAYER;
    Camera& camera = THE_CAMERA;
    World& world = THE_WORLD;
    NpcSystem& npcSystem = THE_NPC_SYSTEM;
    WeaponSystem& weapons = THE_WEAPONS;
    bool& freecamEnabled = FREECAM_ENABLED;
    GameState& gameState = GAME_STATE;
    auto& gReplayPlayer = REPLAY_PLAYER;
    auto& gReplayCameraMgr = REPLAY_CAMERA_MGR;
    auto& gReplayChatStates = REPLAY_CHAT_STATES;
    auto& mpContext = MP_CONTEXT;

    if (!Terminal::instance().isOpen())
        applyDebugMovement(player, engine.window(), camera, dt);

    camera.decayPunch(dt);
    camera.updateVectors();

    // Camera is the source of truth: player root yaw = camera yaw, no smoothing.
    syncPlayerYawFromCamera(player, camera);
    logRotationDebug(player, camera, dt);

    // ── Replay camera control ──────────────────────────────
    const bool replayPlaybackActive = gReplayPlayer.isPlaying();
    const bool replayFreecam =
        replayPlaybackActive &&
        (gReplayPlayer.cameraController().mode() ==
            ReplayCameraMode::Freecam ||
         gReplayCameraMgr.mode() == "freecam");
    const bool anyFreecam = (freecamEnabled || replayFreecam) &&
                            !Terminal::instance().isOpen();

    // ── Replay Editor keyboard controls ─────────────────────
    // These only fire when the editor is loaded (gReplayEditor.isLoaded()).
    // Freecam must not be blocked by play/pause.
    {
        GLFWwindow* win = engine.window();

        // Editor keyboard handler: process prompt state first
        if (gReplayEditor.isLoaded() && gReplayEditor.keyframePromptStage > 0) {
            static bool promptKeyConsumed = false;
            if (!promptKeyConsumed) {
                if (gReplayEditor.keyframePromptStage == 1) {
                    // Type selection: 1=camera-pos, 2=camera-mode, 3=speed
                    if (glfwGetKey(win, GLFW_KEY_1) == GLFW_PRESS ||
                        glfwGetKey(win, GLFW_KEY_KP_1) == GLFW_PRESS) {
                        // Camera Position keyframe
                        glm::quat rot = gReplayEditor.freecam
                            ? gReplayEditor.freecamRot
                            : glm::quatLookAt(glm::normalize(camera.front), glm::vec3(0,0,1));
                        float roll = gReplayEditor.freecamRoll;
                        float fov = gReplayEditor.freecamFov;
                        glm::vec3 pos = gReplayEditor.freecam
                            ? gReplayEditor.freecamPos : camera.pos;
                        gReplayEditor.addCameraKeyframe(
                            gReplayEditor.keyframePromptTick, pos, rot, roll, fov,
                            gReplayEditor.defaultInterp);
                        Terminal::instance().addLog(
                            "[RPLE] Camera position keyframe at tick " +
                            std::to_string(gReplayEditor.keyframePromptTick));
                        gReplayEditor.keyframePromptStage = 0;
                        promptKeyConsumed = true;
                    } else if (glfwGetKey(win, GLFW_KEY_2) == GLFW_PRESS ||
                               glfwGetKey(win, GLFW_KEY_KP_2) == GLFW_PRESS) {
                        // Camera Mode keyframe — advance to mode selection
                        Terminal::instance().addLog(
                            "Camera mode:\n"
                            "1 = Third Person\n"
                            "2 = Freecam\n"
                            "3 = First Person");
                        gReplayEditor.keyframePromptStage = 2;
                        promptKeyConsumed = true;
                    } else if (glfwGetKey(win, GLFW_KEY_3) == GLFW_PRESS ||
                               glfwGetKey(win, GLFW_KEY_KP_3) == GLFW_PRESS) {
                        // Playback speed keyframe
                        gReplayEditor.addTimeKeyframe(
                            gReplayEditor.keyframePromptTick, 1.0f,
                            gReplayEditor.defaultInterp);
                        Terminal::instance().addLog(
                            "[RPLE] Playback speed keyframe at tick " +
                            std::to_string(gReplayEditor.keyframePromptTick));
                        gReplayEditor.keyframePromptStage = 0;
                        promptKeyConsumed = true;
                    }
                } else if (gReplayEditor.keyframePromptStage == 2) {
                    // Camera mode sub-selection
                    ReplayEditorCamMode mode = ReplayEditorCamMode::ThirdPerson;
                    const char* modeName = "thirdperson";
                    if (glfwGetKey(win, GLFW_KEY_1) == GLFW_PRESS ||
                        glfwGetKey(win, GLFW_KEY_KP_1) == GLFW_PRESS) {
                        mode = ReplayEditorCamMode::ThirdPerson;
                        modeName = "Third Person";
                    } else if (glfwGetKey(win, GLFW_KEY_2) == GLFW_PRESS ||
                               glfwGetKey(win, GLFW_KEY_KP_2) == GLFW_PRESS) {
                        mode = ReplayEditorCamMode::Freecam;
                        modeName = "Freecam";
                    } else if (glfwGetKey(win, GLFW_KEY_3) == GLFW_PRESS ||
                               glfwGetKey(win, GLFW_KEY_KP_3) == GLFW_PRESS) {
                        mode = ReplayEditorCamMode::FirstPerson;
                        modeName = "First Person";
                    }
                    if (mode != ReplayEditorCamMode::ThirdPerson ||
                        glfwGetKey(win, GLFW_KEY_1) == GLFW_PRESS ||
                        glfwGetKey(win, GLFW_KEY_KP_1) == GLFW_PRESS) {
                        gReplayEditor.addCameraModeKeyframe(
                            gReplayEditor.keyframePromptTick, mode);
                        Terminal::instance().addLog(
                            std::string("[RPLE] Camera mode keyframe at tick ") +
                            std::to_string(gReplayEditor.keyframePromptTick) +
                            " mode=" + modeName);
                        gReplayEditor.keyframePromptStage = 0;
                        promptKeyConsumed = true;
                    }
                }
            }
            // Reset consumed flag when all keys are released
            if (glfwGetKey(win, GLFW_KEY_1) != GLFW_PRESS &&
                glfwGetKey(win, GLFW_KEY_2) != GLFW_PRESS &&
                glfwGetKey(win, GLFW_KEY_3) != GLFW_PRESS &&
                glfwGetKey(win, GLFW_KEY_KP_1) != GLFW_PRESS &&
                glfwGetKey(win, GLFW_KEY_KP_2) != GLFW_PRESS &&
                glfwGetKey(win, GLFW_KEY_KP_3) != GLFW_PRESS) {
                promptKeyConsumed = false;
            }
        }

        // F key: toggle freecam (only when editor is loaded)
        if (gReplayEditor.isLoaded() && gReplayEditor.keyframePromptStage == 0) {
            static bool fWasDown = false;
            bool fDown = glfwGetKey(win, GLFW_KEY_F) == GLFW_PRESS;
            if (fDown && !fWasDown) {
                gReplayEditor.freecam = !gReplayEditor.freecam;
                if (gReplayEditor.freecam) {
                    gReplayEditor.mPrevCameraMode = REPLAY_PLAYER.cameraController().modeName();
                    gReplayEditor.freecamPos = camera.pos;
                    gReplayEditor.freecamRot = glm::quat(glm::vec3(
                        glm::radians(camera.pitch), glm::radians(camera.yaw), 0.0f));
                    gReplayEditor.freecamFov = camera.fov;
                    REPLAY_PLAYER.cameraController().setMode("freecam");
                    Debug::log(Debug::Category::Replay,
                        "[ReplayEditor] F: Entering freecam, prev mode=%s\n",
                        gReplayEditor.mPrevCameraMode.c_str());
                } else {
                    std::string restore = gReplayEditor.mPrevCameraMode.empty()
                        ? "tp" : gReplayEditor.mPrevCameraMode;
                    REPLAY_PLAYER.cameraController().setMode(restore);
                    gReplayEditor.mPrevCameraMode.clear();
                    Debug::log(Debug::Category::Replay,
                        "[ReplayEditor] F: Exiting freecam\n");
                }
                Terminal::instance().addLog(std::string("[RPLE] Freecam: ") +
                    (gReplayEditor.freecam ? "ON" : "OFF"));
            }
            fWasDown = fDown;
        }

        // Space: play/pause (works in any camera mode during replay)
        {
            static bool spaceWasDown = false;
            bool spaceDown = glfwGetKey(win, GLFW_KEY_SPACE) == GLFW_PRESS;
            if (spaceDown && !spaceWasDown && replayPlaybackActive &&
                gReplayEditor.keyframePromptStage == 0) {
                if (gReplayPlayer.isPaused())
                    gReplayPlayer.resume();
                else
                    gReplayPlayer.pause();
                gReplayEditor.playing = !gReplayPlayer.isPaused();
                Debug::log(Debug::Category::Replay,
                    "[ReplayEditor] Space: %s\n",
                    gReplayPlayer.isPaused() ? "PAUSED" : "PLAYING");
            }
            spaceWasDown = spaceDown;
        }

        // Arrow keys: seek by 60 ticks (1 second) when editor is loaded
        if (gReplayEditor.isLoaded() && gReplayEditor.keyframePromptStage == 0) {
            static bool leftWasDown = false, rightWasDown = false;
            bool leftDown = glfwGetKey(win, GLFW_KEY_LEFT) == GLFW_PRESS;
            bool rightDown = glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS;

            if (leftDown && !leftWasDown) {
                int tick = (int)gReplayEditor.movieTick;
                tick = std::max(0, tick - 60);
                gReplayEditor.seekToTick(tick);
                if (REPLAY_PLAYER.totalTicks() > 0)
                    REPLAY_PLAYER.seekToTick(tick);
                Debug::log(Debug::Category::Replay,
                    "[ReplayEditor] Left: seeked to tick %d\n", tick);
            }
            if (rightDown && !rightWasDown) {
                int tick = (int)gReplayEditor.movieTick;
                tick = std::min(gReplayEditor.totalTicks(), tick + 60);
                gReplayEditor.seekToTick(tick);
                if (REPLAY_PLAYER.totalTicks() > 0)
                    REPLAY_PLAYER.seekToTick(tick);
                Debug::log(Debug::Category::Replay,
                    "[ReplayEditor] Right: seeked to tick %d\n", tick);
            }
            leftWasDown = leftDown;
            rightWasDown = rightDown;
        }

        // Shift+Up/Down: keyframe navigation
        if (gReplayEditor.isLoaded() && gReplayEditor.keyframePromptStage == 0 &&
            (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
             glfwGetKey(win, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)) {
            static bool upWasDown = false, downWasDown = false;
            bool upDown = glfwGetKey(win, GLFW_KEY_UP) == GLFW_PRESS;
            bool downDown = glfwGetKey(win, GLFW_KEY_DOWN) == GLFW_PRESS;

            if (upDown && !upWasDown) {
                int tick = (int)gReplayEditor.movieTick;
                int next = gReplayEditor.nextKeyframeTick(tick);
                if (next >= 0) {
                    gReplayEditor.seekToTick(next);
                    if (REPLAY_PLAYER.totalTicks() > 0)
                        REPLAY_PLAYER.seekToTick(next);
                    int kfIdx = gReplayEditor.findNearestCameraKeyframe(next);
                    char buf[128];
                    std::snprintf(buf, sizeof(buf),
                        "[RPLE] Next keyframe: tick %d (camera KF %d)", next, kfIdx);
                    Terminal::instance().addLog(buf);
                    // Move freecam to keyframe position if freecam is on
                    if (gReplayEditor.freecam && kfIdx >= 0) {
                        const auto& kf = gReplayEditor.cameraKeyframe(kfIdx);
                        gReplayEditor.freecamPos = kf.position;
                        gReplayEditor.freecamRot = kf.rotation;
                        camera.pos = kf.position;
                        camera.front = glm::normalize(kf.rotation * glm::vec3(1.0f, 0.0f, 0.0f));
                        camera.yaw = glm::degrees(std::atan2(camera.front.y, camera.front.x));
                        camera.pitch = glm::degrees(std::asin(std::clamp(camera.front.z, -1.0f, 1.0f)));
                        camera.fov = kf.fov;
                        camera.updateVectors();
                    }
                }
            }
            if (downDown && !downWasDown) {
                int tick = (int)gReplayEditor.movieTick;
                int prev = gReplayEditor.prevKeyframeTick(tick);
                if (prev >= 0) {
                    gReplayEditor.seekToTick(prev);
                    if (REPLAY_PLAYER.totalTicks() > 0)
                        REPLAY_PLAYER.seekToTick(prev);
                    int kfIdx = gReplayEditor.findNearestCameraKeyframe(prev);
                    char buf[128];
                    std::snprintf(buf, sizeof(buf),
                        "[RPLE] Previous keyframe: tick %d (camera KF %d)", prev, kfIdx);
                    Terminal::instance().addLog(buf);
                    if (gReplayEditor.freecam && kfIdx >= 0) {
                        const auto& kf = gReplayEditor.cameraKeyframe(kfIdx);
                        gReplayEditor.freecamPos = kf.position;
                        gReplayEditor.freecamRot = kf.rotation;
                        camera.pos = kf.position;
                        camera.front = glm::normalize(kf.rotation * glm::vec3(1.0f, 0.0f, 0.0f));
                        camera.yaw = glm::degrees(std::atan2(camera.front.y, camera.front.x));
                        camera.pitch = glm::degrees(std::asin(std::clamp(camera.front.z, -1.0f, 1.0f)));
                        camera.fov = kf.fov;
                        camera.updateVectors();
                    }
                }
            }
            upWasDown = upDown;
            downWasDown = downDown;
        }

        // Ctrl+Z: undo via autosave
        if (gReplayEditor.isLoaded() && gReplayEditor.keyframePromptStage == 0) {
            static bool zWasDown = false;
            bool ctrlDown = glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                            glfwGetKey(win, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
            bool zDown = glfwGetKey(win, GLFW_KEY_Z) == GLFW_PRESS;
            if (ctrlDown && zDown && !zWasDown) {
                if (gReplayEditor.undoLastAutosave())
                    Terminal::instance().addLog("[RPLE] Undo: restored previous state");
                else
                    Terminal::instance().addLog("[RPLE] Undo: no history available");
            }
            zWasDown = zDown;
        }

        // K: create keyframe with type selection
        if (gReplayEditor.isLoaded() && gReplayEditor.keyframePromptStage == 0) {
            static bool kWasDown = false;
            bool kDown = glfwGetKey(win, GLFW_KEY_K) == GLFW_PRESS;
            if (kDown && !kWasDown) {
                int tick = (int)std::round(gReplayEditor.movieTick);
                if (REPLAY_PLAYER.totalTicks() > 0)
                    tick = (int)REPLAY_PLAYER.currentTick();
                gReplayEditor.keyframePromptTick = tick;
                gReplayEditor.keyframePromptStage = 1;
                Terminal::instance().addLog(
                    "Keyframe type\n"
                    "1 / Enter = Camera Position\n"
                    "2         = Camera Mode\n"
                    "3         = Playback Speed");
                Debug::log(Debug::Category::Replay,
                    "[ReplayEditor] K: prompt at tick %d\n", tick);
            }
            kWasDown = kDown;
        }
    }

    if (replayPlaybackActive) {
        // Step 0: Apply camera mode keyframes from editor
        if (gReplayEditor.isLoaded() &&
            gReplayEditor.cameraModeKeyframeCount() > 0) {
            int currentTick = (int)gReplayPlayer.currentTick();
            ReplayEditorCamMode cm = gReplayEditor.cameraModeAtTick(currentTick);
            switch (cm) {
                case ReplayEditorCamMode::Freecam:
                    if (!gReplayEditor.freecam) {
                        gReplayEditor.mPrevCameraMode = REPLAY_PLAYER.cameraController().modeName();
                        // Restore freecam position from camera position keyframe at same tick if one exists
                        int posKfIdx = gReplayEditor.findNearestCameraKeyframe(currentTick);
                        if (posKfIdx >= 0) {
                            const auto& posKf = gReplayEditor.cameraKeyframe(posKfIdx);
                            if (std::abs(posKf.tick - currentTick) < 10) {
                                gReplayEditor.freecamPos = posKf.position;
                                gReplayEditor.freecamRot = posKf.rotation;
                                gReplayEditor.freecamRoll = posKf.roll;
                                gReplayEditor.freecamFov = posKf.fov;
                                camera.pos = posKf.position;
                                camera.front = glm::normalize(posKf.rotation * glm::vec3(1.0f, 0.0f, 0.0f));
                                camera.yaw = glm::degrees(std::atan2(camera.front.y, camera.front.x));
                                camera.pitch = glm::degrees(std::asin(std::clamp(camera.front.z, -1.0f, 1.0f)));
                                camera.fov = posKf.fov;
                                camera.updateVectors();
                            } else {
                                gReplayEditor.freecamPos = camera.pos;
                                gReplayEditor.freecamRot = glm::quat(glm::vec3(
                                    glm::radians(camera.pitch), glm::radians(camera.yaw), 0.0f));
                                gReplayEditor.freecamFov = camera.fov;
                            }
                        } else {
                            gReplayEditor.freecamPos = camera.pos;
                            gReplayEditor.freecamRot = glm::quat(glm::vec3(
                                glm::radians(camera.pitch), glm::radians(camera.yaw), 0.0f));
                            gReplayEditor.freecamFov = camera.fov;
                        }
                        gReplayEditor.freecam = true;
                        REPLAY_PLAYER.cameraController().setMode("freecam");
                        Debug::log(Debug::Category::Replay,
                            "[ReplayEditor] Camera mode keyframe: entering freecam at tick %d\n",
                            currentTick);
                    }
                    break;
                case ReplayEditorCamMode::ThirdPerson:
                    if (gReplayEditor.freecam) {
                        gReplayEditor.freecam = false;
                        gReplayEditor.mPrevCameraMode.clear();
                    }
                    REPLAY_PLAYER.cameraController().setMode("tp");
                    break;
                case ReplayEditorCamMode::FirstPerson:
                    if (gReplayEditor.freecam) {
                        gReplayEditor.freecam = false;
                        gReplayEditor.mPrevCameraMode.clear();
                    }
                    REPLAY_PLAYER.cameraController().setMode("fp");
                    break;
            }
        }

        // Step 1: Camera controller sets default camera
        glm::vec3 playerCamPos = camera.pos;
        glm::quat playerCamRot = glm::quat(1,0,0,0);
        {
            // Save player camera orientation before controller overwrites it
            float playerYaw = camera.yaw, playerPitch = camera.pitch;
            if (gReplayCameraMgr.mode() == "keyframed") {
                gReplayCameraMgr.update(gReplayPlayer.currentTick(), camera, dt);
            } else if (!anyFreecam) {
                if (const ReplaySceneFrame* replayFrame =
                        gReplayPlayer.currentSceneFrame()) {
                    gReplayPlayer.cameraController().update(
                        camera, *replayFrame,
                        gReplayPlayer.killerId(),
                        gReplayPlayer.victimId(), dt);
                }
            }
            playerCamPos = camera.pos;
            playerCamRot = glm::quat(glm::vec3(
                glm::radians(camera.pitch),
                glm::radians(camera.yaw),
                0.0f));
        }

        // Step 2: Editor keyframe interpolation (overrides camera if active)
        if (gReplayEditor.isLoaded() && !anyFreecam &&
            gReplayEditor.cameraKeyframeCount() > 0)
        {
            float currentTick = (float)gReplayPlayer.currentTick();
            int kfCount = gReplayEditor.cameraKeyframeCount();

            // Find bracketing keyframes
            int prevKf = -1, nextKf = -1;
            for (int i = 0; i < kfCount; i++) {
                if (gReplayEditor.cameraKeyframe(i).tick <= currentTick)
                    prevKf = i;
                if (gReplayEditor.cameraKeyframe(i).tick > currentTick && nextKf < 0)
                    nextKf = i;
            }

            constexpr float BLEND_DURATION = 30.0f; // 0.5s at 60fps

            if (prevKf >= 0 && nextKf >= 0) {
                // Between two keyframes: interpolate
                const auto& kfA = gReplayEditor.cameraKeyframe(prevKf);
                const auto& kfB = gReplayEditor.cameraKeyframe(nextKf);
                float t = (currentTick - kfA.tick) / (float)(kfB.tick - kfA.tick);
                t = std::clamp(t, 0.0f, 1.0f);
                float st = t * t * (3.0f - 2.0f * t); // smoothstep

                camera.pos = glm::mix(kfA.position, kfB.position, st);
                glm::quat rot = glm::slerp(kfA.rotation, kfB.rotation, st);
                camera.front = glm::normalize(rot * glm::vec3(1.0f, 0.0f, 0.0f));
                camera.yaw = glm::degrees(std::atan2(camera.front.y, camera.front.x));
                camera.pitch = glm::degrees(std::asin(std::clamp(camera.front.z, -1.0f, 1.0f)));
                camera.fov = glm::mix(kfA.fov, kfB.fov, st);
                camera.updateVectors();

                Debug::logThrottled(Debug::Category::Replay, "kf-interp", 0.5f,
                    "[ReplayCamera] Interpolating: KF%d(tick=%d) -> KF%d(tick=%d) progress=%.0f%%\n"
                    "  pos=(%.1f %.1f %.1f)\n",
                    prevKf, kfA.tick, nextKf, kfB.tick, st * 100.0f,
                    camera.pos.x, camera.pos.y, camera.pos.z);

            } else if (prevKf >= 0) {
                // After last keyframe: blend back to player camera
                const auto& kf = gReplayEditor.cameraKeyframe(prevKf);
                float blendT = (currentTick - kf.tick) / BLEND_DURATION;
                if (blendT < 1.0f) {
                    float st = blendT * blendT * (3.0f - 2.0f * blendT); // smoothstep
                    camera.pos = glm::mix(kf.position, playerCamPos, st);
                    glm::quat targetRot = glm::quat(glm::vec3(
                        glm::radians(camera.pitch),
                        glm::radians(camera.yaw),
                        0.0f));
                    glm::quat rot = glm::slerp(kf.rotation, targetRot, st);
                    camera.front = glm::normalize(rot * glm::vec3(1.0f, 0.0f, 0.0f));
                    camera.yaw = glm::degrees(std::atan2(camera.front.y, camera.front.x));
                    camera.pitch = glm::degrees(std::asin(std::clamp(camera.front.z, -1.0f, 1.0f)));
                    camera.updateVectors();

                    Debug::logThrottled(Debug::Category::Replay, "kf-blend", 0.5f,
                        "[ReplayCamera] Returning to Player Camera: progress=%.0f%%\n",
                        st * 100.0f);
                } else {
                    // Blended fully — no override, camera controller's position stays
                    Debug::logThrottled(Debug::Category::Replay, "kf-done", 2.0f,
                        "[ReplayCamera] Mode: Player Camera (past all keyframes)\n");
                }
            } else {
                // Before first keyframe: player camera (no override)
                Debug::logThrottled(Debug::Category::Replay, "kf-before", 2.0f,
                    "[ReplayCamera] Mode: Player Camera (before first keyframe at tick %d)\n",
                    gReplayEditor.cameraKeyframe(0).tick);
            }
        } else {
            Debug::logThrottled(Debug::Category::Replay, "replay-cam", 2.0f,
                "[ReplayCamera] Mode: %s%s\n",
                gReplayPlayer.cameraController().modeName(),
                anyFreecam ? " + Freecam" : "");
        }
    }

    if (anyFreecam) {
        // Mouse look: rely on existing camera.updateMouse() callback
        if (glfwGetInputMode(engine.window(), GLFW_CURSOR) != GLFW_CURSOR_DISABLED) {
            glfwSetInputMode(engine.window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            camera.firstMouse = true;
        }

        // WASD + QE movement
        glm::vec3 flatForward = camera.front;
        flatForward.z = 0.0f;
        if (glm::length(flatForward) > 0.001f) flatForward = glm::normalize(flatForward);
        glm::vec3 flatRight = glm::normalize(glm::cross(flatForward, glm::vec3(0,0,1)));
        glm::vec3 move(0.0f);
        float speed = GetPlayerSettings().freecamSpeed;
        if (glfwGetKey(engine.window(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
            glfwGetKey(engine.window(), GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) speed *= 3.0f;
        if (glfwGetKey(engine.window(), GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
            glfwGetKey(engine.window(), GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS) speed *= 0.3f;
        if (glfwGetKey(engine.window(), GLFW_KEY_W) == GLFW_PRESS) move += flatForward;
        if (glfwGetKey(engine.window(), GLFW_KEY_S) == GLFW_PRESS) move -= flatForward;
        if (glfwGetKey(engine.window(), GLFW_KEY_D) == GLFW_PRESS) move += flatRight;
        if (glfwGetKey(engine.window(), GLFW_KEY_A) == GLFW_PRESS) move -= flatRight;
        if (glfwGetKey(engine.window(), GLFW_KEY_E) == GLFW_PRESS) move.z += 1.0f;
        if (glfwGetKey(engine.window(), GLFW_KEY_Q) == GLFW_PRESS) move.z -= 1.0f;
        if (glm::length(move) > 0.001f)
            camera.pos += glm::normalize(move) * speed * dt;

        // Update editor freecam state from actual camera
        if (gReplayEditor.isLoaded() && gReplayEditor.freecam) {
            gReplayEditor.freecamPos = camera.pos;
            gReplayEditor.freecamRot = glm::quatLookAt(glm::normalize(camera.front), glm::vec3(0,0,1));
        }
    } else if (replayPlaybackActive) {
        // Non-freecam replay: camera already handled above (controller + keyframes)
    } else if (gDuelManager.phase() == DuelPhase::MatchEnd) {
        camera.follow(gDuelManager.winnerCameraTarget());
        camera.smoothCollision(gDuelManager.winnerCameraTarget(), world.collisionMesh.triangles, dt);
    } else if (!camera.thirdPerson) {
        // First-person camera at eye height
        float eyeHeight = PLAYER_HEIGHT * 0.52f;
        camera.pos = player.pos + glm::vec3(0.0f, 0.0f, eyeHeight);
        // Apply punch for weapon recoil
        camera.pos += camera.front * glm::vec3(0.0f, 0.0f, 0.0f); // no offset
    } else {
        camera.follow(player.pos);
        camera.smoothCollision(player.pos, world.collisionMesh.triangles, dt);
    }
    setAudioListener(camera.pos, camera.front);
    EffectPartSystem::instance().setWorld(world);
    if (replayPlaybackActive) {
        for (const ReplayEffectEvent& effect :
             gReplayPlayer.takeTriggeredEffects()) {
            printf("[REPLAY EFFECT] type=%s pos=(%.1f %.1f %.1f) label=%s\n",
                   effect.type.c_str(),
                   effect.position.x, effect.position.y, effect.position.z,
                   effect.label.c_str());
            // Track effect duplicates for world hit effects
            std::string effectKey = effect.type + ":" + std::to_string((int)effect.position.x) + "," + std::to_string((int)effect.position.y) + "," + std::to_string((int)effect.position.z);
            if (effectKey == gRplxLastEffectKey)
                gRplxEffectDuplicateCount++;
            gRplxLastEffectKey = effectKey;
            if (effect.type == "impact_world" || effect.type == "hit_burst" || effect.type == "debris_block")
            {
                RPLXDEBUG("[EFFECT] frame=? tick=? type=%s source=recorded_replay pos=(%.2f %.2f %.2f) spawned=YES reason=playback duplicate=%d\n",
                          effect.type.c_str(),
                          effect.position.x, effect.position.y, effect.position.z,
                          gRplxEffectDuplicateCount);
            }
            if (effect.type == "impact_world") gRplxImpactWorldCount++;
            else if (effect.type == "hit_burst") gRplxHitBurstCount++;
            else if (effect.type == "debris_block") gRplxDebrisBlockCount++;
            if (effect.type == "chat") {
                ActorChatState& chatState = gReplayChatStates[effect.sourceActorId];
                addChatMessage(chatState, effect.assetId, effect.sourceActorId);
                playChatSound((int)effect.assetId.size());
            } else if (effect.type == "gunshot") {
                Debug::log(Debug::Category::Replay,
                    "[REPLAY EFFECT] spawned type=gunshot tick=%d from=(%.2f %.2f %.2f) to=(%.2f %.2f %.2f) source=%s\n",
                    effect.spawnTick, effect.from.x, effect.from.y, effect.from.z,
                    effect.to.x, effect.to.y, effect.to.z, effect.sourceActorId.c_str());
                EffectPartSystem::instance().spawnMuzzleFlash(
                    effect.from, effect.sourceActorId);
                EffectPartSystem::instance().spawnTracer(
                    effect.from, effect.to, effect.sourceActorId);
            } else if (effect.type == "blood_spurt_emitter") {
                Debug::log(Debug::Category::Replay,
                    "[REPLAY EFFECT] spawned type=blood_spurt_emitter tick=%d pos=(%.2f %.2f %.2f) dir=(%.2f %.2f %.2f) source=%s target=%s\n",
                    effect.spawnTick, effect.position.x, effect.position.y, effect.position.z,
                    effect.direction.x, effect.direction.y, effect.direction.z,
                    effect.sourceActorId.c_str(), effect.targetActorId.c_str());
                EffectPartSystem::instance().spawnBloodEffect(
                    effect.position, effect.direction, 50.0f,
                    effect.sourceActorId, effect.targetActorId);
                if (effect.sourceActorId == gReplayPlayer.killerId())
                    hitmarker();
            } else if (effect.type == "dash") {
                Debug::log(Debug::Category::Replay,
                    "[REPLAY EFFECT] spawned type=dash tick=%d pos=(%.2f %.2f %.2f)\n",
                    effect.spawnTick, effect.position.x, effect.position.y, effect.position.z);
                EffectPartSystem::instance().spawnDash(effect.position);
            } else if (effect.type == "footstep") {
                Debug::log(Debug::Category::Replay,
                    "[REPLAY EFFECT] spawned type=footstep tick=%d pos=(%.2f %.2f %.2f)\n",
                    effect.spawnTick, effect.position.x, effect.position.y, effect.position.z);
                EffectPartSystem::instance().spawnFootstep(effect.position);
            } else if (effect.type == "impact_world") {
                // Visual effects (debris, bullet impact) are separate events.
                // Only spawn world debris here; HitEffects::onHit is NOT called
                // because damage numbers and hit burst are separate events.
                EffectPartSystem::instance().spawnWorldDebris(
                    effect.position, effect.normal, 1.0f);
            } else if (effect.type == "debris_block") {
                // No-op: impact_world already spawns the full debris burst.
                // Per-particle debris_block events recorded during gameplay
                // must not spawn another full burst here.
            } else if (effect.type == "hit_burst") {
                // Already handled as separate effect events; no action needed.
            } else if (effect.type == "impact_entity") {
                // Visual effects are separate events; no action needed.
            } else if (effect.type == "muzzle_flash") {
                Debug::log(Debug::Category::Replay,
                    "[REPLAY EFFECT] spawned type=muzzle_flash tick=%d pos=(%.2f %.2f %.2f) source=%s\n",
                    effect.spawnTick, effect.position.x, effect.position.y, effect.position.z,
                    effect.sourceActorId.c_str());
                EffectPartSystem::instance().spawnMuzzleFlash(
                    effect.position, effect.sourceActorId);
            } else if (effect.type == "tracer") {
                Debug::log(Debug::Category::Replay,
                    "[REPLAY EFFECT] spawned type=tracer tick=%d from=(%.2f %.2f %.2f) to=(%.2f %.2f %.2f) source=%s\n",
                    effect.spawnTick, effect.from.x, effect.from.y, effect.from.z,
                    effect.to.x, effect.to.y, effect.to.z, effect.sourceActorId.c_str());
                EffectPartSystem::instance().spawnTracer(
                    effect.from, effect.to, effect.sourceActorId);
            } else if (effect.type == "death_ellipsoid") {
                glm::vec3 dir = glm::length(effect.to - effect.from) > 0.001f
                    ? glm::normalize(effect.to - effect.from)
                    : glm::vec3(1.0f, 0.0f, 0.0f);
                float len = glm::length(effect.to - effect.from);
                const auto& deCfg = HitEffects::config().deathEllipsoid;
                float rad = effect.scale.x > 0.0f ? effect.scale.x : deCfg.radius;
                Debug::log(Debug::Category::Replay,
                    "[REPLAY EFFECT] spawned type=death_ellipsoid tick=%d from=(%.2f %.2f %.2f) to=(%.2f %.2f %.2f) len=%.2f rad=%.2f alpha=%.2f\n",
                    effect.spawnTick, effect.from.x, effect.from.y, effect.from.z,
                    effect.to.x, effect.to.y, effect.to.z, len, rad, effect.alpha);
                EffectPartSystem::instance().spawnDeathEllipsoid(
                    effect.from, dir, len, rad, std::max(effect.lifetime, 0.1f));
            } else if (effect.type == "damage_number") {
                if (!effect.label.empty()) {
                    int damage = 0;
                    try { damage = std::stoi(effect.label); } catch (...) {}
                    Debug::log(Debug::Category::Replay,
                        "[REPLAY EFFECT] spawned type=damage_number tick=%d pos=(%.2f %.2f %.2f) damage=%d target=%s\n",
                        effect.spawnTick, effect.position.x, effect.position.y, effect.position.z,
                        damage, effect.targetActorId.c_str());
                    EffectPartSystem::instance().spawnDamage(
                        effect.position, effect.targetActorId, damage);
                }
            } else if (effect.type == "damage_impact_sphere") {
                glm::vec3 dir = glm::length(effect.to - effect.from) > 0.001f
                    ? glm::normalize(effect.to - effect.from)
                    : glm::vec3(1.0f, 0.0f, 0.0f);
                EffectPartSystem::instance().spawnDamageImpactSphere(
                    effect.position, dir, effect.targetActorId);
            } else if (effect.type == "godball") {
                Debug::log(Debug::Category::Replay,
                    "[REPLAY EFFECT] spawned type=godball tick=%d pos=(%.2f %.2f %.2f) scale=(%.2f %.2f %.2f) color=(%.2f %.2f %.2f %.2f) alpha=%.2f\n",
                    effect.spawnTick, effect.position.x, effect.position.y, effect.position.z,
                    effect.scale.x, effect.scale.y, effect.scale.z,
                    effect.color.x, effect.color.y, effect.color.z, effect.color.w,
                    effect.alpha);
                DebugVis::drawFilledSphere(camera, effect.position, effect.scale.x, effect.color);
                DebugVis::drawWireSphere(camera, effect.position, effect.scale.x, {0.4f, 0.6f, 1.0f, 1.0f});
            } else if (effect.type == "godball_rope") {
                glm::vec3 dir = effect.to - effect.from;
                float dist = glm::length(dir);
                if (dist > 0.01f) {
                    constexpr int SEGMENTS = 6;
                    constexpr float ROPE_RADIUS = 0.025f;
                    glm::vec3 segDir = dir / (float)SEGMENTS;
                    glm::vec3 current = effect.from;
                    for (int i = 0; i < SEGMENTS; i++) {
                        glm::vec3 next = current + segDir;
                        glm::vec3 mid = (current + next) * 0.5f;
                        glm::vec3 axis = next - current;
                        float segLen = glm::length(axis);
                        if (segLen > 0.001f) {
                            DebugVis::drawFilledCylinder(camera, mid, glm::normalize(axis),
                                                          ROPE_RADIUS, segLen, {0.6f, 0.5f, 0.3f, 0.9f});
                        }
                        current = next;
                    }
                }
            } else if (!effect.type.empty() &&
                       effect.type != "corpse_spawn") {
                Debug::log(Debug::Category::Replay,
                    "[REPLAY EFFECT] spawned type=%s tick=%d pos=(%.2f %.2f %.2f) scale=(%.2f %.2f %.2f) endScale=(%.2f %.2f %.2f) color=(%.2f %.2f %.2f %.2f) alpha=%.2f lifetime=%.2f\n",
                    effect.type.c_str(), effect.spawnTick,
                    effect.position.x, effect.position.y, effect.position.z,
                    effect.scale.x, effect.scale.y, effect.scale.z,
                    effect.endScale.x, effect.endScale.y, effect.endScale.z,
                    effect.color.x, effect.color.y, effect.color.z, effect.color.w,
                    effect.alpha, effect.lifetime);
                EffectPart spawnParams;
                spawnParams.position = effect.position;
                spawnParams.color = effect.color;
                spawnParams.velocity = effect.velocity;
                spawnParams.maxLifetime = std::max(effect.lifetime, 0.1f);
                spawnParams.replayType = effect.type;
                spawnParams.label = effect.label;
                spawnParams.scale = effect.scale.x > 0.0f ? effect.scale.x : 0.2f;
                spawnParams.endScale = effect.endScale.x > 0.0f ? effect.endScale.x : 0.01f;
                spawnParams.alpha = effect.alpha;
                spawnParams.endPosition = effect.to;
                spawnParams.rotation = effect.rotation;
                spawnParams.normal = effect.normal;
                spawnParams.thickness = effect.thickness;
                spawnParams.endThickness = effect.endThickness;
                spawnParams.billboardText = false;
                EffectPartSystem::instance().spawn(spawnParams);
            }
        }
        {
            const ReplayCameraMode camMode =
                gReplayPlayer.cameraController().mode();
            std::string viewedEntity;
            switch (camMode) {
                case ReplayCameraMode::Freecam:
                    break;
                case ReplayCameraMode::FirstPerson:
                case ReplayCameraMode::Orbit:
                    viewedEntity = gReplayPlayer.killerId();
                    break;
                case ReplayCameraMode::Victim:
                    viewedEntity = gReplayPlayer.victimId();
                    break;
            }
            for (const ReplaySoundEvent& sound :
                 gReplayPlayer.takeTriggeredSounds()) {
                bool play = true;
                if (sound.soundPath == "hitmarker1") {
                    play = ReplayShouldPlayHitmarkerAudio(
                        gReplayPlayer.killerId(), camMode, viewedEntity);
                }
                if (!play)
                    continue;
                {
                    glm::vec3 toSound = sound.position - camera.pos;
                    float dist = glm::length(toSound);
                    Debug::warn(Debug::Category::Replay, "[REPLAY AUDIO] triggered tick=%d name=%s world=%d listenerValid=%d pos=(%.2f %.2f %.2f) camera=(%.2f %.2f %.2f) dist=%.2f vol=%.2f maxDist=%.2f\n",
                                sound.tick,
                                sound.soundPath.c_str(), (int)sound.world, (int)sound.listenerValid,
                                sound.position.x, sound.position.y, sound.position.z,
                                camera.pos.x, camera.pos.y, camera.pos.z,
                                dist, sound.volume, sound.maxDistance);
                }
                playWorldSound(
                    sound.soundPath, sound.position,
                    sound.volume, sound.pitch,
                    sound.maxDistance > 0.0f ? sound.maxDistance : 40.0f);
            }
        }
    }
}
