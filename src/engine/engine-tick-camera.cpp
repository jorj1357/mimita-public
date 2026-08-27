#include "engine/engine-tick-camera.h"
#include "engine/engine.h"
#include "terminal/terminal-state.h"
#include <cmath>
#include <functional>
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
#include "gui/hud/chat-history.h"
#include "gui/hud/chat-window.h"
#include "ui/hitmarker.h"
#include "config/player-settings.h"
#include "config/camera-config.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "debug/debug-visuals.h"
#include "game/duel.h"
#include "devtools/terminal.h"
#include "input/input-commands.h"
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

// Helper: log camera state to structured logger during export
static void logModeState(const char* eventId, const char* stage)
{
    if (!isReplayExportActive()) return;
    if (!StructuredLogger::instance().shouldLog(
            StructuredCategory::Camera, StructuredLevel::Trace)) return;
    StructuredLogger::Entry e;
    e.category = StructuredCategory::Camera;
    e.level = StructuredLevel::Trace;
    e.eventId = eventId;
    e.correlationId = "REPLAY";
    e.reason = stage;
    e.sourceFile = __FILE__;
    e.sourceLine = __LINE__;
    e.functionName = __FUNCTION__;
    e.numericKeys = {"freecam", "cameraMode"};
    e.numericExpected = {
        gReplayEditor.freecam ? 1.0 : 0.0,
        (double)(int)REPLAY_PLAYER.cameraController().mode()
    };
    e.numericActual = e.numericExpected;
    StructuredLogger::instance().write(e);
}

// Coupling diagnostic: measures how much orientation changes per unit of
// position movement. In a correct system, this should be near-zero unless
// an orientation keyframe intentionally changes the rotation.
static void logCouplingDiagnostic(const Camera& cam, uint32_t tick)
{
    if (!isReplayExportActive()) return;
    if (!StructuredLogger::instance().shouldLog(
            StructuredCategory::Camera, StructuredLevel::Trace)) return;
    static glm::vec3 prevPos(0,0,0);
    static glm::vec3 prevFront(0,0,0);
    static bool firstFrame = true;
    if (firstFrame) {
        prevPos = cam.pos;
        prevFront = cam.front;
        firstFrame = false;
        return;
    }
    glm::vec3 dPos = cam.pos - prevPos;
    float distMoved = glm::length(dPos);
    if (distMoved < 0.001f) { prevPos = cam.pos; prevFront = cam.front; return; }
    float dotFwd = glm::clamp(glm::dot(cam.front, prevFront), -1.0f, 1.0f);
    float angleDelta = glm::degrees(std::acos(dotFwd));
    float coupling = angleDelta / distMoved;
    if (coupling > 0.01f) {
        StructuredLogger::Entry e;
        e.category = StructuredCategory::Camera;
        e.level = StructuredLevel::Important;
        e.eventId = "CAM_COUPLING";
        e.correlationId = "REPLAY_FRAME_" + std::to_string(tick);
        e.reason = "Position/rotation coupling detected";
        e.sourceFile = __FILE__;
        e.sourceLine = __LINE__;
        e.functionName = __FUNCTION__;
        e.tick = tick;
        e.numericKeys = {"angleDelta","distMoved","coupling"};
        e.numericExpected = {0.0, 0.0, 0.0}; // Expected: no coupling
        e.numericActual = {angleDelta, distMoved, coupling};
        e.tolerance = 0.01;
        StructuredLogger::instance().write(e);
    }
    prevPos = cam.pos;
    prevFront = cam.front;
}

// Helper: log camera state to structured logger during export
static void logCameraState(const Camera& cam, uint32_t tick,
    const char* eventId, const char* reason)
{
    if (!isReplayExportActive()) return;
    float ey = glm::radians(cam.yaw);
    float ep = glm::radians(cam.pitch);
    glm::vec3 expFront = glm::normalize(glm::vec3(
        std::cos(ey) * std::cos(ep),
        std::sin(ey) * std::cos(ep),
        std::sin(ep)));
    StructuredLogger::Entry e;
    e.category = StructuredCategory::Camera;
    e.level = StructuredLevel::Trace;
    e.eventId = eventId;
    e.correlationId = "REPLAY_FRAME_" + std::to_string(tick);
    e.reason = reason;
    e.sourceFile = __FILE__;
    e.sourceLine = __LINE__;
    e.functionName = __FUNCTION__;
    e.tick = tick;
    e.frame = tick;
    e.numericKeys = {"pos_x","pos_y","pos_z",
        "front_x","front_y","front_z",
        "right_x","right_y","right_z",
        "up_x","up_y","up_z",
        "exp_front_x","exp_front_y","exp_front_z",
        "yaw","pitch","roll","fov"};
    e.numericExpected = {(double)cam.pos.x,(double)cam.pos.y,(double)cam.pos.z,
        (double)expFront.x,(double)expFront.y,(double)expFront.z,
        (double)cam.right.x,(double)cam.right.y,(double)cam.right.z,
        (double)cam.up.x,(double)cam.up.y,(double)cam.up.z,
        (double)expFront.x,(double)expFront.y,(double)expFront.z,
        (double)cam.yaw,(double)cam.pitch,(double)cam.roll,(double)cam.fov};
    e.numericActual = {(double)cam.pos.x,(double)cam.pos.y,(double)cam.pos.z,
        (double)cam.front.x,(double)cam.front.y,(double)cam.front.z,
        (double)cam.right.x,(double)cam.right.y,(double)cam.right.z,
        (double)cam.up.x,(double)cam.up.y,(double)cam.up.z,
        (double)cam.front.x,(double)cam.front.y,(double)cam.front.z,
        (double)cam.yaw,(double)cam.pitch,(double)cam.roll,(double)cam.fov};
    e.tolerance = 0.001;
    StructuredLogger::instance().write(e);
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

    if (InputCommandSystem::instance().isKeyboardEnabled())
        applyDebugMovement(player, engine.window(), camera, dt);

    camera.decayPunch(dt);
    // Zero weapon recoil punch during replay — it would otherwise add a
    // phantom rotation from decaying gameplay residuals into every frame's
    // effective yaw/pitch via updateVectors()'s internal punch inclusion.
    if (REPLAY_PLAYER.isPlaying()) {
        camera.punchPitch = 0.0f;
        camera.punchYaw = 0.0f;
    }
    camera.updateVectors();

    // Log camera mode state at start of engineTickCamera
    logModeState("CAM_MODE_START", "Mode state at engineTickCamera start");

    // ── Replay camera control ──────────────────────────────
    const bool replayPlaybackActive = gReplayPlayer.isPlaying();
    const bool replayFreecam =
        replayPlaybackActive &&
        (gReplayPlayer.cameraController().mode() ==
            ReplayCameraMode::Freecam ||
         gReplayCameraMgr.mode() == "freecam");
    const bool anyFreecam = (freecamEnabled || replayFreecam) &&
                            InputCommandSystem::instance().isKeyboardEnabled();

    // Camera is the source of truth: player root yaw = camera yaw, no smoothing.
    // Skip during freecam to preserve the player's original facing direction.
    if (!anyFreecam) {
        syncPlayerYawFromCamera(player, camera);
        logRotationDebug(player, camera, dt);
    }

    // ── Replay Editor keyboard controls ─────────────────────
    // Space/Arrows handled by engine-tick-combat.cpp; this block handles
    // editor-only keys (F, K, Shift+Up/Down, Ctrl+Z).
    if (gReplayEditor.isLoaded()) {
        GLFWwindow* win = engine.window();

        // Process keyframe prompt
        // Keyboard 1/2/3 handled here; visual popup drawn in engine-tick-ui-replay-hud.cpp.
        // Popup handles mouse clicks; Escape cancels.
        if (InputCommandSystem::instance().isKeyboardEnabled() && gReplayEditor.keyframePromptStage > 0) {
            // Escape cancels prompt
            static bool escWasDown = false;
            bool escDown = glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS;
            if (escDown && !escWasDown) {
                gReplayEditor.keyframePromptStage = 0;
                Terminal::instance().addLog("[RPLE] Keyframe creation canceled");
            }
            escWasDown = escDown;

            static bool promptKeyConsumed = false;
            if (!promptKeyConsumed) {
                if (gReplayEditor.keyframePromptStage == 1) {
                    if (glfwGetKey(win, GLFW_KEY_1) == GLFW_PRESS ||
                        glfwGetKey(win, GLFW_KEY_KP_1) == GLFW_PRESS) {
                        glm::vec3 pos = gReplayEditor.freecam
                            ? gReplayEditor.freecamPos : camera.pos;
                        glm::quat rot = gReplayEditor.freecam
                            ? gReplayEditor.freecamRot
                            : glm::quatLookAt(glm::normalize(camera.front), glm::vec3(0,0,1));
                        float roll = gReplayEditor.freecamRoll;
                        float fov = gReplayEditor.freecamFov;
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
                        gReplayEditor.keyframePromptStage = 2;
                        promptKeyConsumed = true;
                    } else if (glfwGetKey(win, GLFW_KEY_3) == GLFW_PRESS ||
                               glfwGetKey(win, GLFW_KEY_KP_3) == GLFW_PRESS) {
                        gReplayEditor.keyframePromptStage = 3;
                        gReplayEditor.pbspeedInputBuf[0] = '\0';
                        gReplayEditor.pbspeedInputLen = 0;
                        promptKeyConsumed = true;
                    }
                } else if (gReplayEditor.keyframePromptStage == 2) {
                    ReplayEditorCamMode mode = ReplayEditorCamMode::ThirdPerson;
                    if (glfwGetKey(win, GLFW_KEY_1) == GLFW_PRESS ||
                        glfwGetKey(win, GLFW_KEY_KP_1) == GLFW_PRESS) {
                        mode = ReplayEditorCamMode::ThirdPerson;
                    } else if (glfwGetKey(win, GLFW_KEY_2) == GLFW_PRESS ||
                               glfwGetKey(win, GLFW_KEY_KP_2) == GLFW_PRESS) {
                        mode = ReplayEditorCamMode::Freecam;
                    } else if (glfwGetKey(win, GLFW_KEY_3) == GLFW_PRESS ||
                               glfwGetKey(win, GLFW_KEY_KP_3) == GLFW_PRESS) {
                        mode = ReplayEditorCamMode::FirstPerson;
                    }
                    if (mode != ReplayEditorCamMode::ThirdPerson ||
                        glfwGetKey(win, GLFW_KEY_1) == GLFW_PRESS ||
                        glfwGetKey(win, GLFW_KEY_KP_1) == GLFW_PRESS) {
                        gReplayEditor.addCameraModeKeyframe(
                            gReplayEditor.keyframePromptTick, mode);
                        Terminal::instance().addLog(
                            "[RPLE] Camera mode keyframe at tick " +
                            std::to_string(gReplayEditor.keyframePromptTick));
                        gReplayEditor.keyframePromptStage = 0;
                        promptKeyConsumed = true;
                    }
                } else if (gReplayEditor.keyframePromptStage == 3) {
                    auto& ed = gReplayEditor;

                    // Edge-trackers — reset when stage 3 is first entered
                    static bool enterWasDown = false;
                    static bool backspaceWasDown = false;
                    static bool periodWasDown = false;
                    static int lastDigitKey = -1;
                    static int prevStage3 = 0;
                    if (prevStage3 != 3) {
                        enterWasDown = false;
                        backspaceWasDown = false;
                        periodWasDown = false;
                        lastDigitKey = -1;
                        prevStage3 = 3;
                    }

                    // Enter
                    bool enterDown = glfwGetKey(win, GLFW_KEY_ENTER) == GLFW_PRESS ||
                                     glfwGetKey(win, GLFW_KEY_KP_ENTER) == GLFW_PRESS;
                    if (enterDown && !enterWasDown) {
                        if (ed.pbspeedInputLen > 0) {
                            float speed = 1.0f;
                            try {
                                speed = std::clamp(std::stof(ed.pbspeedInputBuf), 0.01f, 100.0f);
                            } catch (...) { speed = 1.0f; }
                            ed.addTimeKeyframe(ed.keyframePromptTick, speed, ed.defaultInterp);
                            char buf[96];
                            std::snprintf(buf, sizeof(buf),
                                "[RPLE] Playback speed keyframe at tick %d: %.2fx",
                                ed.keyframePromptTick, speed);
                            Terminal::instance().addLog(buf);
                        }
                        ed.keyframePromptStage = 0;
                        promptKeyConsumed = true;
                    }
                    enterWasDown = enterDown;

                    // Backspace
                    bool backspaceDown = glfwGetKey(win, GLFW_KEY_BACKSPACE) == GLFW_PRESS;
                    if (backspaceDown && !backspaceWasDown) {
                        if (ed.pbspeedInputLen > 0) {
                            ed.pbspeedInputBuf[--ed.pbspeedInputLen] = '\0';
                        }
                        promptKeyConsumed = true;
                    }
                    backspaceWasDown = backspaceDown;

                    // Period (keyboard and numpad)
                    bool periodDown = glfwGetKey(win, GLFW_KEY_PERIOD) == GLFW_PRESS ||
                                      glfwGetKey(win, GLFW_KEY_KP_DECIMAL) == GLFW_PRESS;
                    if (periodDown && !periodWasDown) {
                        if (ed.pbspeedInputLen < 15) {
                            bool hasDot = false;
                            for (int i = 0; i < ed.pbspeedInputLen; i++)
                                if (ed.pbspeedInputBuf[i] == '.') { hasDot = true; break; }
                            if (!hasDot) {
                                ed.pbspeedInputBuf[ed.pbspeedInputLen++] = '.';
                                ed.pbspeedInputBuf[ed.pbspeedInputLen] = '\0';
                            }
                        }
                        promptKeyConsumed = true;
                    }
                    periodWasDown = periodDown;

                    // Digits 0-9 (keyboard and numpad)
                    {
                        int digitKey = -1;
                        for (int d = 0; d <= 9; d++) {
                            if (glfwGetKey(win, GLFW_KEY_0 + d) == GLFW_PRESS ||
                                glfwGetKey(win, GLFW_KEY_KP_0 + d) == GLFW_PRESS) {
                                digitKey = d;
                                break;
                            }
                        }
                        if (digitKey >= 0 && digitKey != lastDigitKey) {
                            if (ed.pbspeedInputLen < 15) {
                                ed.pbspeedInputBuf[ed.pbspeedInputLen++] = (char)('0' + digitKey);
                                ed.pbspeedInputBuf[ed.pbspeedInputLen] = '\0';
                            }
                            promptKeyConsumed = true;
                        }
                        lastDigitKey = digitKey;
                    }
                }
            }
            if (glfwGetKey(win, GLFW_KEY_1) != GLFW_PRESS &&
                glfwGetKey(win, GLFW_KEY_2) != GLFW_PRESS &&
                glfwGetKey(win, GLFW_KEY_3) != GLFW_PRESS &&
                glfwGetKey(win, GLFW_KEY_KP_1) != GLFW_PRESS &&
                glfwGetKey(win, GLFW_KEY_KP_2) != GLFW_PRESS &&
                glfwGetKey(win, GLFW_KEY_KP_3) != GLFW_PRESS) {
                promptKeyConsumed = false;
            }
        }

        // F key: toggle freecam
        if (gReplayEditor.keyframePromptStage == 0) {
            static bool fWasDown = false;
            bool fDown = glfwGetKey(win, GLFW_KEY_F) == GLFW_PRESS;
            if (fDown && !fWasDown) {
                gReplayEditor.freecam = !gReplayEditor.freecam;
                if (gReplayEditor.freecam) {
                    gReplayEditor.mPrevCameraMode = REPLAY_PLAYER.cameraController().modeName();
                    gReplayEditor.freecamPos = camera.pos;
                    gReplayEditor.freecamRot = glm::quatLookAt(
                        glm::normalize(camera.front), glm::vec3(0,0,1));
                    gReplayEditor.freecamRoll = camera.roll;
                    gReplayEditor.freecamFov = camera.fov;
                    REPLAY_PLAYER.cameraController().setMode("freecam");
                } else {
                    std::string restore = gReplayEditor.mPrevCameraMode.empty()
                        ? "tp" : gReplayEditor.mPrevCameraMode;
                    REPLAY_PLAYER.cameraController().setMode(restore);
                    gReplayEditor.mPrevCameraMode.clear();
                }
                Terminal::instance().addLog(std::string("[RPLE] Freecam: ") +
                    (gReplayEditor.freecam ? "ON" : "OFF"));
            }
            fWasDown = fDown;
        }

        // Shift+Up/Down: keyframe navigation
        // IMPORTANT: must never change playback state.
        if (gReplayEditor.keyframePromptStage == 0 &&
            (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
             glfwGetKey(win, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)) {
            static bool upWasDown = false, downWasDown = false;
            bool upDown = glfwGetKey(win, GLFW_KEY_UP) == GLFW_PRESS;
            bool downDown = glfwGetKey(win, GLFW_KEY_DOWN) == GLFW_PRESS;

            if (upDown && !upWasDown) {
                bool wasPlaying = gReplayEditor.playing;
                int tick = (int)gReplayEditor.movieTick;
                int next = gReplayEditor.nextKeyframeTick(tick);
                if (next >= 0) {
                    gReplayEditor.seekToTick(next);
                    if (REPLAY_PLAYER.totalTicks() > 0)
                        REPLAY_PLAYER.seekToTick(next);
                    gReplayEditor.playing = wasPlaying;
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
                        camera.front = glm::normalize(kf.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
                        camera.yaw = glm::degrees(std::atan2(camera.front.y, camera.front.x));
                        camera.pitch = glm::degrees(std::asin(std::clamp(camera.front.z, -1.0f, 1.0f)));
                        camera.fov = kf.fov;
                        camera.roll = kf.roll;
                        camera.updateVectors();
                    }
                }
            }
            if (downDown && !downWasDown) {
                bool wasPlaying = gReplayEditor.playing;
                int tick = (int)gReplayEditor.movieTick;
                int prev = gReplayEditor.prevKeyframeTick(tick);
                if (prev >= 0) {
                    gReplayEditor.seekToTick(prev);
                    if (REPLAY_PLAYER.totalTicks() > 0)
                        REPLAY_PLAYER.seekToTick(prev);
                    gReplayEditor.playing = wasPlaying;
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
                        camera.front = glm::normalize(kf.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
                        camera.yaw = glm::degrees(std::atan2(camera.front.y, camera.front.x));
                        camera.pitch = glm::degrees(std::asin(std::clamp(camera.front.z, -1.0f, 1.0f)));
                        camera.fov = kf.fov;
                        camera.roll = kf.roll;
                        camera.updateVectors();
                    }
                }
            }
            upWasDown = upDown;
            downWasDown = downDown;
        }

        // Ctrl+Z: undo via autosave
        if (gReplayEditor.keyframePromptStage == 0) {
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

        // K: create keyframe with type selection (visual popup drawn in HUD)
        if (gReplayEditor.keyframePromptStage == 0) {
            static bool kWasDown = false;
            bool kDown = glfwGetKey(win, GLFW_KEY_K) == GLFW_PRESS;
            if (kDown && !kWasDown) {
                int tick = (int)std::round(gReplayEditor.movieTick);
                if (REPLAY_PLAYER.totalTicks() > 0)
                    tick = (int)REPLAY_PLAYER.currentTick();
                gReplayEditor.keyframePromptTick = tick;
                gReplayEditor.keyframePromptStage = 1;
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
            // During export with camera keyframes, freecam was forced by
            // startReplayExport. Do not allow mode keyframes to override it.
            bool skipModeOverride = isReplayExportActive() &&
                gReplayEditor.freecam &&
                gReplayEditor.cameraKeyframeCount() > 0;
            if (!skipModeOverride) {
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
                                camera.front = glm::normalize(posKf.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
                                camera.yaw = glm::degrees(std::atan2(camera.front.y, camera.front.x));
                                camera.pitch = glm::degrees(std::asin(std::clamp(camera.front.z, -1.0f, 1.0f)));
                                camera.fov = posKf.fov;
                                camera.roll = posKf.roll;
                                // 7 11 2026 commenting these all out test
                                camera.updateVectors();
                            } else {
                                gReplayEditor.freecamPos = camera.pos;
                                gReplayEditor.freecamRot = glm::quatLookAt(
                                    glm::normalize(camera.front), glm::vec3(0,0,1));
                                gReplayEditor.freecamFov = camera.fov;
                            }
                        } else {
                            gReplayEditor.freecamPos = camera.pos;
                            gReplayEditor.freecamRot = glm::quatLookAt(
                                glm::normalize(camera.front), glm::vec3(0,0,1));
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
            Debug::log(Debug::Category::Replay,
                "[RPLE MODE] tick=%d cameraMode=%s paused=%d manualInputAllowed=%d\n",
                gReplayPlayer.currentTick(),
                gReplayEditor.freecam ? "FREECAM" : (camera.thirdPerson ? "THIRDPERSON" : "FIRSTPERSON"),
                (int)gReplayPlayer.isPaused(),
                (int)(gReplayEditor.freecam && gReplayPlayer.isPaused()));
            }
        }
        // Log camera mode state after mode keyframe block
        logModeState("CAM_MODE_AFTER_KF", "Mode state after camera mode keyframe block");

        // Step 1: Camera controller sets default camera
        glm::vec3 playerCamPos = camera.pos;
        glm::quat playerCamRot = glm::quat(1,0,0,0);
        {
            // Save player camera orientation before controller overwrites it
            float playerYaw = camera.yaw, playerPitch = camera.pitch;
            // Only use ReplayCameraMgr (separate config/cameratimeline.json system)
            // when the editor is NOT loaded. Editor keyframes are the authority.
            if (gReplayCameraMgr.mode() == "keyframed" && !gReplayEditor.isLoaded()) {
                gReplayCameraMgr.update(gReplayPlayer.currentTick(), camera, dt);
            } else if (!anyFreecam) {
                if (const ReplaySceneFrame* replayFrame =
                        gReplayPlayer.currentSceneFrame()) {
                    gReplayPlayer.cameraController().update(
                        camera, *replayFrame,
                        gReplayPlayer.killerId(),
                        gReplayPlayer.victimId(), dt,
                        gReplayPlayer.currentTick());
                }
            }
            // Structured log: camera state after Step 1 (camera controller)
            logCameraState(camera, gReplayPlayer.currentTick(),
                "CAM_CTRL_STATE", "Camera state after Step 1 (camera controller)");

            playerCamPos = camera.pos;
            playerCamRot = glm::quat(glm::vec3(
                glm::radians(camera.pitch),
                glm::radians(camera.yaw),
                0.0f));
        }

        if (!isReplayExportActive() || gReplayExportVerbose)
            Debug::log(Debug::Category::Replay,
                "[RPLE CAM BEFORE] tick=%d pos=(%.2f %.2f %.2f) look=(%.4f %.4f %.4f)\n",
                gReplayPlayer.currentTick(),
                camera.pos.x, camera.pos.y, camera.pos.z,
                camera.front.x, camera.front.y, camera.front.z);

        // Step 2: Editor keyframe interpolation.
        // Position keyframes only drive the camera when in Freecam mode.
        // In ThirdPerson / FirstPerson modes, the normal replay camera controller
        // (Step 1) runs instead — position keyframes are ignored by design.
        if (gReplayEditor.isLoaded() && gReplayEditor.freecam &&
            gReplayEditor.cameraKeyframeCount() > 0 && gReplayPlayer.isPlaying()
            && !gReplayPlayer.isPaused())
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

            if (prevKf >= 0 && nextKf >= 0) {
                // Between two keyframes: interpolate using per-keyframe easing
                const auto& kfA = gReplayEditor.cameraKeyframe(prevKf);
                const auto& kfB = gReplayEditor.cameraKeyframe(nextKf);
                float t = (currentTick - kfA.tick) / (float)(kfB.tick - kfA.tick);
                t = std::clamp(t, 0.0f, 1.0f);
                float st = ReplayEditor::applyEasing(t, kfA.interp);

                camera.pos = glm::mix(kfA.position, kfB.position, st);
                glm::quat rot = glm::normalize(glm::slerp(kfA.rotation, kfB.rotation, st));
                // Assert quaternion magnitude
                float qmag = glm::length(rot);
                if (std::fabs(qmag - 1.0f) > 0.001f) {
                    Debug::warn(Debug::Category::Replay,
                        "[QUAT] slerp magnitude=%.6f tick=%.0f\n", qmag, currentTick);
                    rot = glm::normalize(rot);
                }
                // Extract -Z axis (GLM quatLookAt convention): quatLookAt maps -Z to target
                camera.front = glm::normalize(rot * glm::vec3(0.0f, 0.0f, -1.0f));
                camera.yaw = glm::degrees(std::atan2(camera.front.y, camera.front.x));
                camera.pitch = glm::degrees(std::asin(std::clamp(camera.front.z, -1.0f, 1.0f)));
                camera.fov = glm::mix(kfA.fov, kfB.fov, st);
                camera.roll = glm::mix(kfA.roll, kfB.roll, st);
                // 7 11 2026 testing stopping this
                camera.updateVectors();

                if (isReplayExportActive() && gReplayExportVerbose) {
                    printf("[RPLX CAM] tick=%.0f mode=FREECAM segment=KF%d->KF%d alpha=%.3f"
                           " pos=(%.1f %.1f %.1f) look=(%.3f %.3f %.3f) fov=%.0f roll=%.1f\n",
                           currentTick, prevKf, nextKf, st,
                           camera.pos.x, camera.pos.y, camera.pos.z,
                           camera.front.x, camera.front.y, camera.front.z,
                           camera.fov, camera.roll);
                }

                if (!isReplayExportActive() || gReplayExportVerbose)
                Debug::log(Debug::Category::Replay,
                    "[RPLE KF APPLY] tick=%.0f interp=%s KF%d->KF%d alpha=%.3f"
                    " pos=(%.1f %.1f %.1f) look=(%.3f %.3f %.3f) fov=%.0f roll=%.1f\n",
                    currentTick, interpName(kfA.interp),
                    prevKf, nextKf, st,
                    camera.pos.x, camera.pos.y, camera.pos.z,
                    camera.front.x, camera.front.y, camera.front.z,
                    camera.fov, camera.roll);

            } else if (prevKf >= 0) {
                // After last keyframe: HOLD the final keyframe
                const auto& kf = gReplayEditor.cameraKeyframe(prevKf);
                camera.pos = kf.position;
                glm::quat rot = kf.rotation;
                camera.front = glm::normalize(rot * glm::vec3(0.0f, 0.0f, -1.0f));
                camera.yaw = glm::degrees(std::atan2(camera.front.y, camera.front.x));
                camera.pitch = glm::degrees(std::asin(std::clamp(camera.front.z, -1.0f, 1.0f)));
                camera.fov = kf.fov;
                camera.roll = kf.roll;
                // 7 11 2026 test comment 
                camera.updateVectors();

                if (isReplayExportActive() && gReplayExportVerbose) {
                    printf("[RPLX CAM] tick=%.0f mode=FREECAM after-last-KF"
                           " pos=(%.1f %.1f %.1f) look=(%.3f %.3f %.3f) fov=%.0f roll=%.1f\n",
                           currentTick,
                           camera.pos.x, camera.pos.y, camera.pos.z,
                           camera.front.x, camera.front.y, camera.front.z,
                           camera.fov, camera.roll);
                }

                if (!isReplayExportActive() || gReplayExportVerbose)
                Debug::log(Debug::Category::Replay,
                    "[RPLE KF APPLY] tick=%.0f HOLD KF%d"
                    " pos=(%.1f %.1f %.1f) look=(%.3f %.3f %.3f) fov=%.0f roll=%.1f\n",
                    currentTick, prevKf,
                    camera.pos.x, camera.pos.y, camera.pos.z,
                    camera.front.x, camera.front.y, camera.front.z,
                    camera.fov, camera.roll);
            } else {
                // Before first keyframe: in freecam mode, hold the first keyframe
                const auto& kf = gReplayEditor.cameraKeyframe(0);
                camera.pos = kf.position;
                glm::quat rot = kf.rotation;
                camera.front = glm::normalize(rot * glm::vec3(0.0f, 0.0f, -1.0f));
                camera.yaw = glm::degrees(std::atan2(camera.front.y, camera.front.x));
                camera.pitch = glm::degrees(std::asin(std::clamp(camera.front.z, -1.0f, 1.0f)));
                camera.fov = kf.fov;
                camera.roll = kf.roll;
                // 7 11 2026 teting this stopping 
                // 7 11 2026 155311 uncommenting all of them
                camera.updateVectors();

                if (isReplayExportActive() && gReplayExportVerbose) {
                    printf("[RPLX CAM] tick=%.0f mode=FREECAM before-first-KF"
                           " pos=(%.1f %.1f %.1f) look=(%.3f %.3f %.3f) fov=%.0f roll=%.1f\n",
                           currentTick,
                           camera.pos.x, camera.pos.y, camera.pos.z,
                           camera.front.x, camera.front.y, camera.front.z,
                           camera.fov, camera.roll);
                }

                if (!isReplayExportActive() || gReplayExportVerbose)
                Debug::log(Debug::Category::Replay,
                    "[RPLE KF APPLY] tick=%.0f BEFORE first KF (tick=%d) — holding first KF\n",
                    currentTick, kf.tick);
            }
        } else if (gReplayEditor.isLoaded()) {
            if (isReplayExportActive() && gReplayExportVerbose) {
                printf("[RPLX CAM] tick=%u usingDefaultThirdPersonCamera=1"
                       " reason=no_editor_keyframes"
                       " pos=(%.1f %.1f %.1f) look=(%.3f %.3f %.3f) fov=%.0f\n",
                       gReplayPlayer.currentTick(),
                       camera.pos.x, camera.pos.y, camera.pos.z,
                       camera.front.x, camera.front.y, camera.front.z,
                       camera.fov);
            }
            if (!isReplayExportActive() || gReplayExportVerbose)
                Debug::log(Debug::Category::Replay,
                    "[RPLE CAM FINAL] tick=%u pos=(%.1f %.1f %.1f) look=(%.3f %.3f %.3f)"
                    " fov=%.0f roll=%.1f source=%s\n",
                    gReplayPlayer.currentTick(),
                    camera.pos.x, camera.pos.y, camera.pos.z,
                    camera.front.x, camera.front.y, camera.front.z,
                    camera.fov, camera.roll,
                    gReplayEditor.freecam ? "freecam(no-kf)" : "cameraController");

            // Structured log: camera state after keyframe interpolation
            if (isReplayExportActive() && gReplayExportVerbose &&
                    StructuredLogger::instance().shouldLog(
                        StructuredCategory::Camera, StructuredLevel::Trace)) {
                logCameraState(camera, gReplayPlayer.currentTick(), "CAM_KF_STATE",
                    "Camera state after keyframe interpolation");
            }
        }
        if (!isReplayExportActive() || gReplayExportVerbose)
            Debug::log(Debug::Category::Replay,
                "[RPLE CAM AFTER KF] tick=%d pos=(%.2f %.2f %.2f) look=(%.4f %.4f %.4f)\n",
                gReplayPlayer.currentTick(),
                camera.pos.x, camera.pos.y, camera.pos.z,
                camera.front.x, camera.front.y, camera.front.z);
        // Structured log: final camera state after ALL processing
        if (!isReplayExportActive() || gReplayExportVerbose)
            logCameraState(camera, gReplayPlayer.currentTick(),
                "CAM_FINAL_STATE", "Final camera state after keyframe evaluation");
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

        Debug::log(Debug::Category::Replay,
            "[RPLE INPUT] tick=%d paused=%d freecamEnabled=%d"
            " w=%d a=%d s=%d d=%d q=%d e=%d moveDelta=(%.2f %.2f %.2f)\n",
            gReplayPlayer.currentTick(),
            (int)gReplayPlayer.isPaused(), (int)(gReplayEditor.isLoaded() && gReplayEditor.freecam),
            (int)(glfwGetKey(engine.window(), GLFW_KEY_W) == GLFW_PRESS),
            (int)(glfwGetKey(engine.window(), GLFW_KEY_A) == GLFW_PRESS),
            (int)(glfwGetKey(engine.window(), GLFW_KEY_S) == GLFW_PRESS),
            (int)(glfwGetKey(engine.window(), GLFW_KEY_D) == GLFW_PRESS),
            (int)(glfwGetKey(engine.window(), GLFW_KEY_Q) == GLFW_PRESS),
            (int)(glfwGetKey(engine.window(), GLFW_KEY_E) == GLFW_PRESS),
            move.x, move.y, move.z);

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
        auto& camCfg = CamConfig::instance().data();
        camera.fov = camCfg.fov;
        camera.follow(gDuelManager.winnerCameraTarget(), camCfg.offset, camCfg.positionStiffness);
        camera.smoothCollision(gDuelManager.winnerCameraTarget(), world, dt, camCfg.positionStiffness, camCfg.stiffnessEnabled, camCfg.collisionEnabled, camCfg.collisionPushEnabled, camCfg.collisionPushback);
    } else if (!camera.thirdPerson) {
        // First-person camera at eye height
        float eyeHeight = PLAYER_HEIGHT * 0.52f;
        camera.pos = player.pos + glm::vec3(0.0f, 0.0f, eyeHeight);
        // Apply punch for weapon recoil
        camera.pos += camera.front * glm::vec3(0.0f, 0.0f, 0.0f); // no offset
    } else {
        auto& camCfg = CamConfig::instance().data();
        camera.fov = camCfg.fov;
        camera.follow(player.pos, camCfg.offset, camCfg.positionStiffness);
        camera.smoothCollision(player.pos, world, dt, camCfg.positionStiffness, camCfg.stiffnessEnabled, camCfg.collisionEnabled, camCfg.collisionPushEnabled, camCfg.collisionPushback);
    }

    // Debug: final camera state after all evaluation
    if ((gReplayEditor.isLoaded() || replayPlaybackActive) &&
        (!isReplayExportActive() || gReplayExportVerbose)) {
        Debug::log(Debug::Category::Replay,
            "[RPLE CAM FINAL] tick=%u pos=(%.2f %.2f %.2f) look=(%.4f %.4f %.4f)"
            " fov=%.0f roll=%.1f\n",
            gReplayPlayer.currentTick(),
            camera.pos.x, camera.pos.y, camera.pos.z,
            camera.front.x, camera.front.y, camera.front.z,
            camera.fov, camera.roll);
    }

    setAudioListener(camera.pos, camera.front);
    EffectPartSystem::instance().setWorld(world);
    if (replayPlaybackActive) {
        const double tFx0 = replayExportNowSec();
        for (const ReplayEffectEvent& effect :
             gReplayPlayer.takeTriggeredEffects()) {
            if (gReplayExportVerbose)
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
                if (gpChatHistory) {
                    ChatHistoryEntry entry;
                    entry.messageId = ((uint64_t)(uint32_t)effect.spawnTick << 32) |
                        (uint64_t)std::hash<std::string>{}(effect.sourceActorId + effect.assetId);
                    entry.serverTick = (uint64_t)std::max(effect.spawnTick, 0);
                    entry.senderName = effect.sourceActorId;
                    entry.text = effect.assetId;
                    gChatHistory.append(entry);
                    gChatWindowVisible = true;
                }
                if (!isReplayExportActive())
                    playChatSound((int)effect.assetId.size());
            } else if (effect.type == "gunshot") {
                Debug::log(Debug::Category::Replay,
                    "[REPLAY EFFECT] spawned type=gunshot tick=%d from=(%.2f %.2f %.2f) to=(%.2f %.2f %.2f) source=%s\n",
                    effect.spawnTick, effect.from.x, effect.from.y, effect.from.z,
                    effect.to.x, effect.to.y, effect.to.z, effect.sourceActorId.c_str());
                const bool muzzleLighting = !isReplayExportActive() || gExportConfig.effects.muzzleLighting;
                EffectPartSystem::instance().spawnMuzzleFlash(
                    effect.from, effect.sourceActorId, 1.0f, effect.assetId,
                    false, muzzleLighting);
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
                if (effect.sourceActorId == gReplayPlayer.killerId() &&
                    !isReplayExportActive())
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
                // Recreate the surface effects here; HitEffects::onHit is not
                // called during playback because it also owns live hit feedback.
                if (!isReplayExportActive() || gExportConfig.effects.worldDebris)
                    EffectPartSystem::instance().spawnWorldDebris(
                        effect.position, effect.normal, 1.0f);
                if (!isReplayExportActive() || gExportConfig.effects.bulletHoles)
                    EffectPartSystem::instance().spawnBulletImpact(
                        effect.position, effect.normal, 1.0f);
                if (!isReplayExportActive() || gExportConfig.effects.worldCracks)
                    EffectPartSystem::instance().spawnWorldCracks(
                        effect.position, effect.normal, effect.direction, 1.0f);
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
                const bool muzzleFlash = !isReplayExportActive() || gExportConfig.effects.muzzleFlash;
                const bool muzzleLighting = !isReplayExportActive() || gExportConfig.effects.muzzleLighting;
                EffectPartSystem::instance().spawnMuzzleFlash(
                    effect.position, effect.sourceActorId, 1.0f, effect.assetId,
                    muzzleFlash, muzzleLighting);
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
                spawnParams.billboardText = effect.billboardText;
                spawnParams.beam = effect.beam;
                EffectPartSystem::instance().spawn(spawnParams);
            }
        }
        gExportFrameTimings.weaponEventsMs +=
            (replayExportNowSec() - tFx0) * 1000.0;
        {
            const double tAud0 = replayExportNowSec();
            const ReplayCameraMode camMode =
                gReplayPlayer.cameraController().mode();
            std::string viewedEntity;
            switch (camMode) {
                case ReplayCameraMode::Freecam:
                    break;
                case ReplayCameraMode::Recorded:
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
                    if (gReplayExportVerbose)
                        Debug::warn(Debug::Category::Replay, "[REPLAY AUDIO] triggered tick=%d name=%s world=%d listenerValid=%d pos=(%.2f %.2f %.2f) camera=(%.2f %.2f %.2f) dist=%.2f vol=%.2f maxDist=%.2f\n",
                                    sound.tick,
                                    sound.soundPath.c_str(), (int)sound.world, (int)sound.listenerValid,
                                    sound.position.x, sound.position.y, sound.position.z,
                                    camera.pos.x, camera.pos.y, camera.pos.z,
                                    dist, sound.volume, sound.maxDistance);
                    // Structured log: replay sound trigger
                    if (StructuredLogger::instance().shouldLog(
                            StructuredCategory::Audio, StructuredLevel::Important)) {
                        StructuredLogger::Entry se;
                        se.category = StructuredCategory::Audio;
                        se.level = StructuredLevel::Important;
                        se.eventId = "REPLAY_SOUND_TRIGGER";
                        se.correlationId = "REPLAY_FRAME_" + std::to_string(gReplayPlayer.currentTick());
                        se.reason = "Replay sound triggered during playback";
                        se.sourceFile = __FILE__;
                        se.sourceLine = __LINE__;
                        se.functionName = __FUNCTION__;
                        se.tick = gReplayPlayer.currentTick();
                        se.numericKeys = {"triggerTick","dist","volume","recordedPitch"};
                        se.numericExpected = {(double)sound.tick,(double)dist,(double)sound.volume,(double)sound.pitch};
                        se.numericActual = se.numericExpected;
                        StructuredLogger::instance().write(se);
                    }
                }
                float pbspeedMul = 1.0f;
                if (gReplayEditor.isLoaded())
                    pbspeedMul = gReplayEditor.playbackSpeedAtTick((int)gReplayPlayer.currentTick());
                // During export the clip audio is mixed into the MP4 separately;
                // do not also play the sounds live.
                if (!isReplayExportActive())
                    playWorldSound(
                        sound.soundPath, sound.position,
                        sound.volume, sound.pitch * pbspeedMul,
                        sound.maxDistance > 0.0f ? sound.maxDistance : 40.0f);
            }
            gExportFrameTimings.audioEventsMs +=
                (replayExportNowSec() - tAud0) * 1000.0;
        }
    }
    // Coupling diagnostic: orientation change per unit of position movement
    if (replayPlaybackActive && (!isReplayExportActive() || gReplayExportVerbose))
        logCouplingDiagnostic(camera, gReplayPlayer.currentTick());

    // Log view matrix for debug verification during export
    if (isReplayExportActive() && gReplayExportVerbose &&
            StructuredLogger::instance().shouldLog(
                StructuredCategory::Camera, StructuredLevel::Trace)) {
        glm::mat4 view = camera.getView();
        StructuredLogger::Entry ve;
        ve.category = StructuredCategory::Camera;
        ve.level = StructuredLevel::Trace;
        ve.eventId = "CAM_VIEW_MATRIX";
        ve.correlationId = "REPLAY_FRAME_" + std::to_string(gReplayPlayer.currentTick());
        ve.reason = "View matrix after all camera processing";
        ve.sourceFile = __FILE__;
        ve.sourceLine = __LINE__;
        ve.functionName = __FUNCTION__;
        ve.tick = gReplayPlayer.currentTick();
        ve.numericKeys = {
            "m00","m01","m02","m03",
            "m10","m11","m12","m13",
            "m20","m21","m22","m23",
            "m30","m31","m32","m33"};
        ve.numericExpected = {
            view[0][0],view[0][1],view[0][2],view[0][3],
            view[1][0],view[1][1],view[1][2],view[1][3],
            view[2][0],view[2][1],view[2][2],view[2][3],
            view[3][0],view[3][1],view[3][2],view[3][3]};
        ve.numericActual = ve.numericExpected;
        StructuredLogger::instance().write(ve);
    }
}


