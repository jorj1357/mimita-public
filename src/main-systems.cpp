// 08 16 2026, 01 35
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <cstdint>
/* purpose
* Initializes MiMITA runtime subsystems, command registration, and default binds.
* Connects engine-owned services to the shared gameplay and UI orchestration loop.
* Establishes replay clip export shortcuts while preserving editor commands.
* Does NOT implement subsystem feature logic or encoder backends.
* Does NOT own production deployment, launcher updates, or website behavior.
* Does NOT replace feature-owned configuration or terminal command handlers.
*/
#include "main-systems.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <random>
#include <memory>
#include <new>
#include <utility>
#include <filesystem>
#include <fstream>
#include <limits>
#include <windows.h>
#include "engine/engine.h"
#include "host/host-runtime.h"
#include "host/abi/state-capability.h"
#include "game/state-bridge-module.h"
#include "game/player-core-migration.h"
#include "host/host-transport-adapter.h"
#include "world/world.h"
#include "world/world-loader.h"
#include "world/world-gltf-loader.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "npc/npc-combat.h"
#include "camera.h"
#include "config/camera-config.h"
#include "input/input-state.h"
#include "input/input-poll.h"
#include "input/input-frame.h"
#include "input/input-commands.h"
#include "input/input-router.h"
#include "game/window-bridge-module.h"
#include "gui/menus/pause-menu.h"
#include "render/render-command.h"
#include "render/render-world.h"
#include "render/post-fx.h"
#include "render/render-player.h"
#include "physics/physics-mini.h"
#include "physics/physics-debug-movement.h"
#include "physics/ray-utils.h"
#include "physics/movement/physics-collision.h"
#include "audio/audio.h"
#include "audio/hitmarker-audio.h"
#include "audio/music-manager.h"
#include "analytics/analytics-manager.h"
#include "gui/gui-main.h"
#include "gui/ui-system.h"
#include "gui/gui-coord.h"
#include "gui/gui-layout.h"
#include "gui/gui-editor.h"
#include "gui/gui-element-render.h"
#include "gui/hud/player-nameplates.h"
#include "gui/hud/chat-history.h"
#include "gui/hud/chat-window.h"
#include "gui/font-stuff/font-loader.h"
#include "game/game-state.h"
#include "game/game-cli.h"
#include "debug/debug-visuals.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "debug/transform-debug.h"
#include "debug/transform-debug-commands.h"
#include "debug/debug-diag.h"
#include "debug/log-manager.h"
#include "network/net_mode.h"
#include "network/multiplayer-context.h"
#include "devtools/dev-config.h"
#include "devtools/dev-overlay.h"
#include "devtools/dev-npc-selection.h"
#include "devtools/dev-teleport.h"
#include "devtools/dev-commands.h"
#include "devtools/terminal.h"
#include "devtools/account-config.h"
#include "devtools/npc-spawn-commands.h"
#include "devtools/scenario-commands.h"
#include "devtools/dev-log-commands.h"
#include "devtools/dev-overlay-commands.h"
#include "ui/hitmarker.h"
#include "gui/hud/chat-bubble.h"
#include "effects/effect-part.h"
#include "replay/replay.h"
#include "replay/replay-export-ui.h"
#include "replay/replay-factory.h"
#include "replay/replay-factory-worker.h"
#include "shadow/shadow-config.h"
#include "shadow/shadow-render.h"
#include "shadow/shadow-commands.h"
#include "video/video-settings.h"
#include "video/outro.h"
#include "video/frame-pacer.h"
#include "video/video-commands.h"
#include "sim/sim-context.h"
#include "engine/engine-tick.h"
#include "combat/weapon-hit.h"
#include "combat/weapon-system.h"
#include "combat/weapon-registry.h"
#include "combat/melee-client.h"
#include "combat/death-system.h"
#include "ragdoll/ragdoll-commands.h"
#include "config/spawn-velocity-config.h"
#include "void-death/void-death.h"
#include "crosshair/crosshair-commands.h"
#include "crosshair/crosshair-config.h"
#include "crosshair/crosshair-render.h"
#include "gui/hud/healthbar-config.h"
#include "replay/replay-camera.h"
#include "config/gameplay-config.h"
#include "config/player-settings.h"
#include "config/size-scaling-config.h"
#include "config/movement-config.h"
#include "npc/npc-difficulty-config.h"
#include "gamemode/gamemode.h"
#include "gamemode/match-roles.h"
#include "gamemode/monster-pools.h"
#include "npc/npc-behavior.h"
#include "gamemode/gamemode-map-pool.h"
#include "avatar/avatar.h"
#include "avatar/avatar-commands.h"
#include "terminal/map-entity-commands.h"
#include "entities/aim-commands.h"
#include "entities/aimbody-config.h"
void registerCompetitiveCommands();
void registerLeaderboardCommands();
void registerCursorCommands();

// The terminal-state convenience macros are included later with the command
// declarations; these globals are needed by the early deterministic-command
// registration helpers below.
extern Player* gpPlayer;
extern Camera* gpCamera;
extern MimitaNet::MultiplayerContext* gpMpContext;

namespace {

bool parseAgentFloat(const std::vector<std::string>& args, size_t index, float& out)
{
    if (index >= args.size()) return false;
    try {
        size_t consumed = 0;
        out = std::stof(args[index], &consumed);
        return consumed == args[index].size() && std::isfinite(out);
    } catch (...) {
        return false;
    }
}

void writeAgentCommandEvent(const std::string& eventName,
                            const std::string& reason,
                            nlohmann::json fields)
{
    const std::string requestId = Terminal::instance().currentRequestId();
    fields["request_id"] = requestId;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Avatar, StructuredLevel::Important,
        eventName, requestId.empty() ? "agent-command" : requestId,
        reason, 0, fields,
        __FILE__, __LINE__, __FUNCTION__);
}

namespace {

void postAgentMouseButton(int button, bool down)
{
    const DWORD flag = button == GLFW_MOUSE_BUTTON_RIGHT
        ? (down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP)
        : (down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP);
    mouse_event(flag, 0, 0, 0, 0);
}

bool postAgentMousePosition(double x, double y)
{
    if (!mimita::game::hostFocusWindow() ||
        !mimita::game::hostSetCursorPos(x, y)) return false;
    int wx = 0, wy = 0;
    if (!mimita::game::hostWindowPosition(wx, wy)) return false;
    return SetCursorPos(wx + static_cast<int>(x), wy + static_cast<int>(y)) != 0;
}

bool captureAgentWindowBmp(const std::string& relativePath,
                           int& width, int& height, std::string& error)
{
    const std::filesystem::path relative(relativePath);
    if (relative.is_absolute() || relative.has_root_name() ||
        relative.extension() != ".bmp") {
        error = "use a relative .bmp path under logs/";
        return false;
    }
    for (const auto& part : relative) {
        if (part == "..") {
            error = "parent traversal is not allowed";
            return false;
        }
    }
    if (relative.generic_string().rfind("logs/", 0) != 0) {
        error = "screenshot must be stored under logs/";
        return false;
    }

    int windowX = 0, windowY = 0;
    if (!mimita::game::hostWindowPosition(windowX, windowY) ||
        !mimita::game::hostWindowSize(width, height) ||
        width <= 0 || height <= 0 || width > 8192 || height > 8192) {
        error = "could not read a valid client window rectangle";
        return false;
    }
    const auto root = std::filesystem::weakly_canonical(
        std::filesystem::current_path() / "logs");
    const auto output = std::filesystem::weakly_canonical(
        std::filesystem::current_path() / relative);
    const auto rootText = root.native();
    const auto outputText = output.native();
    if (outputText.size() <= rootText.size() ||
        outputText.compare(0, rootText.size(), rootText) != 0 ||
        outputText[rootText.size()] != L'\\') {
        error = "resolved screenshot path escaped logs/";
        return false;
    }
    std::error_code ec;
    std::filesystem::create_directories(output.parent_path(), ec);
    if (ec) {
        error = "could not create screenshot directory";
        return false;
    }

    const std::size_t pixelCount = static_cast<std::size_t>(width) *
                                   static_cast<std::size_t>(height);
    if (pixelCount > std::numeric_limits<std::size_t>::max() / 4) {
        error = "screenshot dimensions overflow";
        return false;
    }
    std::vector<std::uint8_t> pixels(pixelCount * 4);
    HDC desktop = GetDC(nullptr);
    HDC memory = desktop ? CreateCompatibleDC(desktop) : nullptr;
    HBITMAP bitmap = desktop ? CreateCompatibleBitmap(desktop, width, height) : nullptr;
    HGDIOBJ previous = memory && bitmap ? SelectObject(memory, bitmap) : nullptr;
    const bool copied = previous &&
        BitBlt(memory, 0, 0, width, height, desktop, windowX, windowY,
               SRCCOPY | CAPTUREBLT) != 0;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    const bool read = copied && GetDIBits(memory, bitmap, 0,
        static_cast<UINT>(height), pixels.data(), &info, DIB_RGB_COLORS) != 0;
    if (previous) SelectObject(memory, previous);
    if (bitmap) DeleteObject(bitmap);
    if (memory) DeleteDC(memory);
    if (desktop) ReleaseDC(nullptr, desktop);
    if (!read) {
        error = "window pixels could not be captured; ensure the client is visible";
        return false;
    }

    BITMAPFILEHEADER header{};
    header.bfType = 0x4D42;
    header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    header.bfSize = static_cast<DWORD>(header.bfOffBits + pixels.size());
    std::ofstream file(output, std::ios::binary | std::ios::trunc);
    if (!file) {
        error = "could not open screenshot output";
        return false;
    }
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(reinterpret_cast<const char*>(&info.bmiHeader), sizeof(info.bmiHeader));
    file.write(reinterpret_cast<const char*>(pixels.data()),
               static_cast<std::streamsize>(pixels.size()));
    if (!file) {
        error = "could not write screenshot output";
        return false;
    }
    error.clear();
    return true;
}

}  // namespace

struct MigrationProbeV1 {
    std::uint32_t structSize = sizeof(MigrationProbeV1);
    std::uint32_t value = 0;
};

struct MigrationProbeV2 {
    std::uint32_t structSize = sizeof(MigrationProbeV2);
    std::uint32_t value = 0;
    std::uint32_t added = 0;
};

struct MigrationProbeV3 {
    std::uint32_t structSize = sizeof(MigrationProbeV3);
    std::uint32_t value = 0;
    std::uint32_t added = 0;
    std::uint32_t extra = 0;
};

bool migrateProbeV1ToV2(void* oldBlock, std::uint32_t oldSize,
                        void* newBlock, std::uint32_t newSize) {
    if (!oldBlock || !newBlock ||
        oldSize < sizeof(MigrationProbeV1) ||
        newSize < sizeof(MigrationProbeV2)) return false;
    auto* oldValue = static_cast<MigrationProbeV1*>(oldBlock);
    auto* newValue = static_cast<MigrationProbeV2*>(newBlock);
    newValue->value = oldValue->value;
    newValue->added = 7;
    return true;
}

bool rejectProbeV2ToV3(void*, std::uint32_t, void*, std::uint32_t) {
    return false;
}

void* migrateProbeObject(void* oldBlock, std::uint32_t oldSize,
                         void* newBlock, std::uint32_t newSize,
                         mimita::host::StateMigrateFn migrate, bool& created) {
    created = false;
#ifdef MIMITA_GAME_DLL
    return mimita::game::ensureResidentObjectMigrating(
        mimita::host::StateObjectId::MigrationProbe, newSize,
        alignof(std::uint64_t), migrate, &created);
#else
    std::int32_t first = 0;
    void* result = mimita::host::HostRuntime::instance().ensureStateObjectMigrating(
        static_cast<std::uint32_t>(mimita::host::StateObjectId::MigrationProbe),
        static_cast<std::uint32_t>(newSize),
        static_cast<std::uint32_t>(alignof(std::uint64_t)), migrate, &first);
    created = first != 0;
    (void)oldBlock; (void)oldSize;
    return result;
#endif
}

void runStateMigrationSelfTest() {
    static bool alreadyRan = false;
    if (alreadyRan) {
        Terminal::instance().addLog("[STATE] migration selftest already ran");
        return;
    }
    alreadyRan = true;
    bool created = false;
    auto* v1 = static_cast<MigrationProbeV1*>(
        migrateProbeObject(nullptr, 0, nullptr, sizeof(MigrationProbeV1),
                           migrateProbeV1ToV2, created));
    if (!v1) {
        Terminal::instance().addLog("[STATE] migration selftest failed: v1 create");
        return;
    }
    v1->value = 42;
    auto* v2 = static_cast<MigrationProbeV2*>(
        migrateProbeObject(v1, sizeof(MigrationProbeV1), nullptr,
                           sizeof(MigrationProbeV2), migrateProbeV1ToV2, created));
    const bool migrated = v2 && v2->value == 42 && v2->added == 7;
    auto* rejected = static_cast<MigrationProbeV3*>(
        migrateProbeObject(v2, sizeof(MigrationProbeV2), nullptr,
                           sizeof(MigrationProbeV3), rejectProbeV2ToV3, created));
    const bool preserved = v2 && v2->value == 42 && v2->added == 7;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "state.migration.selftest", "state",
        migrated && preserved ? "state migration success/rollback selftest passed"
                              : "state migration selftest failed",
        0,
        nlohmann::json{{"migrated", migrated},
                       {"rejected", rejected == nullptr},
                       {"old_state_preserved", preserved},
                       {"new_field", v2 ? v2->added : 0}},
        __FILE__, __LINE__, __FUNCTION__);
    Terminal::instance().addLog(
        migrated && preserved ? "[STATE] migration selftest PASS"
                              : "[STATE] migration selftest FAIL");
}

void registerAgentCommands()
{
    Terminal::instance().registerCommand({
        "agent.move", "Hold camera-relative movement for fixed ticks",
        "agent.move <forward> <side> <duration_ticks>",
        [](const std::vector<std::string>& args) {
            float forward = 0.0f, side = 0.0f, ticksFloat = 0.0f;
            if (args.size() != 3 || !parseAgentFloat(args, 0, forward) ||
                !parseAgentFloat(args, 1, side) || !parseAgentFloat(args, 2, ticksFloat)) {
                Terminal::instance().addLog(
                    "[AGENT] Usage: agent.move <forward> <side> <duration_ticks>");
                return;
            }
            const int ticks = std::clamp((int)std::lround(ticksFloat), 1, 3600);
            glm::vec3 forwardVec = gpCamera->front;
            forwardVec.z = 0.0f;
            if (glm::length(forwardVec) < 0.001f)
                forwardVec = glm::vec3(0.0f, 1.0f, 0.0f);
            else
                forwardVec = glm::normalize(forwardVec);
            const glm::vec3 rightVec = glm::normalize(
                glm::cross(forwardVec, glm::vec3(0.0f, 0.0f, 1.0f)));
            const glm::vec3 worldMove = forward * forwardVec + side * rightVec;
            // The modern multiplayer owner consumes raw camera-relative axes;
            // it applies camera orientation exactly like keyboard input.
            scheduleTerminalMove(side, forward, ticks,
                                 Terminal::instance().currentRequestId());
            writeAgentCommandEvent(
                "avatar.command.dispatch", "agent.move accepted",
                {{"command", "agent.move"}, {"forward", forward}, {"side", side},
                 {"duration_ticks", ticks}, {"move_axes", {side, forward}},
                 {"world_move", {worldMove.x, worldMove.y}},
                 {"player", gpPlayer->username}});
        }, "2026-10-13", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "agent.aim", "Aim the normal camera and player input owner at a world point",
        "agent.aim <x> <y> <z>",
        [](const std::vector<std::string>& args) {
            float x = 0.0f, y = 0.0f, z = 0.0f;
            if (args.size() != 3 || !parseAgentFloat(args, 0, x) ||
                !parseAgentFloat(args, 1, y) || !parseAgentFloat(args, 2, z)) {
                Terminal::instance().addLog(
                    "[AGENT] Usage: agent.aim <x> <y> <z>");
                return;
            }
            glm::vec3 direction = glm::vec3(x, y, z) - gpCamera->pos;
            if (glm::length(direction) < 0.001f) {
                Terminal::instance().addLog("[AGENT] aim target is at camera position");
                return;
            }
            direction = glm::normalize(direction);
            gpCamera->yaw = glm::degrees(std::atan2(direction.y, direction.x));
            gpCamera->pitch = glm::degrees(std::asin(std::clamp(direction.z, -1.0f, 1.0f)));
            gpCamera->updateVectors();
            writeAgentCommandEvent(
                "avatar.command.dispatch", "agent.aim accepted",
                {{"command", "agent.aim"}, {"target", {x, y, z}},
                 {"camera_position", {gpCamera->pos.x, gpCamera->pos.y, gpCamera->pos.z}},
                 {"camera_front", {gpCamera->front.x, gpCamera->front.y, gpCamera->front.z}},
                 {"yaw", gpCamera->yaw}, {"pitch", gpCamera->pitch},
                 {"player", gpPlayer->username}});
            // agent.aim writes the shared camera owner synchronously, so the
            // accepted state is the applied state; record it as applied with
            // the resulting world direction for correlation and assertion.
            writeAgentCommandEvent(
                "agent.aim.applied", "camera/aim owner updated",
                {{"command", "agent.aim"}, {"target", {x, y, z}},
                 {"camera_front", {gpCamera->front.x, gpCamera->front.y, gpCamera->front.z}},
                 {"yaw", gpCamera->yaw}, {"pitch", gpCamera->pitch},
                 {"player", gpPlayer->username}});
        }, "2026-10-13", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "agent.shoot", "Fire once through the normal weapon command owner",
        "agent.shoot",
        [](const std::vector<std::string>& args) {
            if (!args.empty()) {
                Terminal::instance().addLog("[AGENT] agent.shoot currently accepts no duration; use repeated commands");
                return;
            }
            writeAgentCommandEvent(
                "avatar.command.dispatch", "agent.shoot accepted",
                {{"command", "agent.shoot"}, {"player", gpPlayer->username},
                 {"camera_front", {gpCamera->front.x, gpCamera->front.y, gpCamera->front.z}}});
            Terminal::instance().execute("shoot");
        }, "2026-10-13", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "agent.action", "Issue one normal action edge",
        "agent.action <jump|dash>",
        [](const std::vector<std::string>& args) {
            if (args.size() != 1 || (args[0] != "jump" && args[0] != "dash")) {
                Terminal::instance().addLog("[AGENT] Usage: agent.action <jump|dash>");
                return;
            }
            noteTerminalInputOverrideRequestId(Terminal::instance().currentRequestId());
            if (args[0] == "jump") gTerminalInputOverride.jumpPressed = true;
            if (args[0] == "dash") gTerminalInputOverride.dashPressed = true;
            writeAgentCommandEvent(
                "avatar.command.dispatch", "agent.action accepted",
                {{"command", "agent.action"}, {"action", args[0]},
                 {"player", gpPlayer->username}});
        }, "2026-10-13", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "agent.interact", "Drive the objective interaction input (F)",
        "agent.interact <hold|tap> <ticks|presses>",
        [](const std::vector<std::string>& args) {
            if (args.size() != 2 || (args[0] != "hold" && args[0] != "tap")) {
                Terminal::instance().addLog(
                    "[AGENT] Usage: agent.interact <hold|tap> <ticks|presses>");
                return;
            }
            int amount = 0;
            try { amount = std::stoi(args[1]); } catch (...) { amount = 0; }
            amount = std::clamp(amount, 0, 3600);
            if (args[0] == "hold")
                scheduleTerminalInteractHold(amount);
            else
                scheduleTerminalInteractTaps(amount);
            writeAgentCommandEvent(
                "avatar.command.dispatch", "agent.interact accepted",
                {{"command", "agent.interact"}, {"kind", args[0]},
                 {"amount", amount}, {"player", gpPlayer ? gpPlayer->username
                                                          : std::string()}});
        }, "2026-11-01", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "agent.hold", "Hold primary or secondary gameplay input for fixed ticks",
        "agent.hold <primary|secondary> <duration_ticks>",
        [](const std::vector<std::string>& args) {
            if (args.size() != 2 || (args[0] != "primary" && args[0] != "secondary")) {
                Terminal::instance().addLog(
                    "[AGENT] Usage: agent.hold <primary|secondary> <duration_ticks>");
                return;
            }
            int ticks = 0;
            try {
                size_t consumed = 0;
                ticks = std::stoi(args[1], &consumed);
                if (consumed != args[1].size()) throw std::invalid_argument("trailing text");
            } catch (...) {
                Terminal::instance().addLog("[AGENT] duration_ticks must be an integer from 1 to 3600");
                return;
            }
            if (ticks < 1 || ticks > 3600) {
                Terminal::instance().addLog("[AGENT] duration_ticks must be an integer from 1 to 3600");
                return;
            }
            const std::string requestId = Terminal::instance().currentRequestId();
            scheduleTerminalGameplayHold(args[0], ticks, requestId);
            writeAgentCommandEvent(
                "avatar.command.dispatch", "agent.hold accepted",
                {{"command", "agent.hold"}, {"action", args[0]},
                 {"duration_ticks", ticks}, {"player", gpPlayer ? gpPlayer->username : std::string()}});
        }, "2026-11-05", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "agent.holdprimary", "Compatibility alias for holding primary gameplay input",
        "agent.holdprimary <frames>",
        [](const std::vector<std::string>& args) {
            int frames = 60;
            if (!args.empty()) {
                try { frames = std::stoi(args[0]); } catch (...) { frames = 60; }
            }
            frames = std::clamp(frames, 0, 3600);
            const std::string requestId = Terminal::instance().currentRequestId();
            scheduleTerminalGameplayHold("primary", frames, requestId);
            writeAgentCommandEvent(
                "avatar.command.dispatch", "agent.holdprimary compatibility alias accepted",
                {{"command", "agent.holdprimary"}, {"frames", frames},
                 {"player", gpPlayer ? gpPlayer->username : std::string()}});

        }, "2026-10-29", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "agent.snapshot", "Record player, camera, avatar, and posed body transforms",
        "agent.snapshot <label>",
        [](const std::vector<std::string>& args) {
            const std::string label = args.empty() ? "unnamed" : args[0];
            nlohmann::json parts = nlohmann::json::array();
            for (const auto& part : gpPlayer->physicalBody.parts) {
                nlohmann::json item = {
                    {"name", part.name}, {"node_index", part.nodeIndex},
                    {"world_matrix", nlohmann::json::array()}};
                for (int col = 0; col < 4; ++col)
                    for (int row = 0; row < 4; ++row)
                        item["world_matrix"].push_back(part.worldTransform[col][row]);
                parts.push_back(std::move(item));
            }
            nlohmann::json fields = {
                {"command", "agent.snapshot"}, {"label", label},
                {"player", gpPlayer->username},
                {"fixed_tick", gpMpContext ? gpMpContext->tick : 0},
                {"movement_simulation_tick", gpPlayer->movementSimulationTick},
                {"server_tick", gpMpContext && gpMpContext->active
                    ? gpMpContext->latestServerTick : 0},
                {"player_position", {gpPlayer->pos.x, gpPlayer->pos.y, gpPlayer->pos.z}},
                {"player_velocity", {gpPlayer->vel.x, gpPlayer->vel.y, gpPlayer->vel.z}},
                {"camera_position", {gpCamera->pos.x, gpCamera->pos.y, gpCamera->pos.z}},
                {"camera_front", {gpCamera->front.x, gpCamera->front.y, gpCamera->front.z}},
                {"camera_forward", {gpCamera->front.x, gpCamera->front.y, gpCamera->front.z}},
                {"camera_yaw", gpCamera->yaw}, {"camera_pitch", gpCamera->pitch},
                {"current_animation", gpPlayer->currentAnimName},
                {"on_ground", gpPlayer->ground.onGround},
                {"stable_on_ground", gpPlayer->ground.stableOnGround},
                {"has_world_contact", gpPlayer->ground.hasWorldContact},
                {"size_scale", gpPlayer->sizeScale},
                {"model_loaded", gpPlayer->modelLoaded},
                {"freeze_active", gpPlayer->freeze.freezeActive},
                {"procedural_frozen", gpPlayer->proceduralFrozen},
                {"ragdoll_active", gpPlayer->ragdollModeActive},
                {"movement_mode", gpPlayer->rig.movement.mode},
                {"anim_clip_count",
                    gpPlayer->animationSet ? gpPlayer->animationSet->clips.size() : 0},
                {"server_pos_error",
                    (gpMpContext && gpMpContext->hasLocalServerPosition)
                        ? glm::length(gpPlayer->pos - gpMpContext->localServerPosition)
                        : -1.0f},
                {"snapshot_schema_version", 3}, {"parts", std::move(parts)}};
            fields["active_animation"] = gpPlayer->currentAnimName;
            fields["physicalBody"] = {{"parts", fields["parts"]}};
            if (gpPlayer->avatarInstance) {
                fields["avatar_name"] = gpPlayer->avatarInstance->definition.name;
                fields["model_path"] = gpPlayer->avatarInstance->definition.playerModel;
                fields["body_transform_source"] = gpPlayer->avatarInstance->definition.bodyTransformSource;
                fields["body_frame_policy"] = gpPlayer->avatarInstance->definition.bodyFramePolicy;
            }
            writeAgentCommandEvent("avatar.pose.sample", "agent.snapshot captured pose", fields);
            writeAgentCommandEvent("avatar.command.assert", "agent.snapshot captured", fields);
            Terminal::instance().addLog("[AGENT] snapshot captured: " + label);
        }, "2026-10-13", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "agent.melee", "Start an equipped weapon action by ID through its shared action driver.",
        "agent.melee <actionId>",
        [](const std::vector<std::string>& args) {
            if (args.size() != 1) {
                Terminal::instance().addLog("[AGENT] Usage: agent.melee <actionId>");
                return;
            }
            MimitaMelee::debugRequestAction(args[0]);
            writeAgentCommandEvent(
                "avatar.command.dispatch", "agent.melee accepted",
                {{"command", "agent.melee"}, {"action", args[0]}, {"player", gpPlayer->username}});
        }, "2026-10-17", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "agent.weaponbutton", "Press/release a shared weapon-action input.",
        "agent.weaponbutton <primary|secondary> <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.size() != 2 || (args[0] != "primary" && args[0] != "secondary") ||
                (args[1] != "0" && args[1] != "1")) {
                Terminal::instance().addLog("[AGENT] Usage: agent.weaponbutton <primary|secondary> <0|1>");
                return;
            }
            MimitaMelee::requestButton(args[0] == "secondary", args[1] == "1");
            writeAgentCommandEvent("weapon.input", "shared weapon input accepted",
                {{"button", args[0]}, {"held", args[1] == "1"}});
        }, "2026-10-21", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "agent.crouch", "Hold/release the crouch input for the Melee pose test.",
        "agent.crouch <0|1>",
        [](const std::vector<std::string>& args) {
            const bool on = !args.empty() && args[0] == "1";
            MimitaMelee::debugSetCrouch(on);
            writeAgentCommandEvent(
                "avatar.command.dispatch", "agent.crouch accepted",
                {{"command", "agent.crouch"}, {"crouch", on}, {"player", gpPlayer->username}});
        }, "2026-10-18", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "agent.shield", "Hold/release the Melee shield input for a runtime test.",
        "agent.shield <0|1>",
        [](const std::vector<std::string>& args) {
            const bool on = !args.empty() && args[0] == "1";
            MimitaMelee::debugSetShield(on);
            writeAgentCommandEvent(
                "avatar.command.dispatch", "agent.shield accepted",
                {{"command", "agent.shield"}, {"shield", on}, {"player", gpPlayer->username}});
        }, "2026-10-19", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "agent.hurt", "Add Melee percent to the local player for the hurt-pose test.",
        "agent.hurt <percent>",
        [](const std::vector<std::string>& args) {
            float amount = 8.0f;
            if (!args.empty()) {
                try { amount = std::stof(args[0]); } catch (...) {}
            }
            if (gpPlayer)
                gpPlayer->percent += amount;
            writeAgentCommandEvent(
                "avatar.command.dispatch", "agent.hurt accepted",
                {{"command", "agent.hurt"}, {"amount", amount},
                 {"percent", gpPlayer ? gpPlayer->percent : 0.0f}, {"player", gpPlayer->username}});
        }, "2026-10-18", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "agent.join", "Join a server through the normal connection path.",
        "agent.join <address>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[AGENT] Usage: agent.join <address>");
                return;
            }
            writeAgentCommandEvent("agent.command.dispatched", "agent.join accepted",
                                   {{"command", "agent.join"}, {"address", args[0]}});
            Terminal::instance().execute("serverconnect " + args[0]);
        }, "2026-11-14", CommandCategory::Network
    });

    Terminal::instance().registerCommand({
        "agent.leave", "Leave the current server through the normal disconnect path.",
        "agent.leave",
        [](const std::vector<std::string>&) {
            Terminal::instance().execute("disconnectserver");
        }, "2026-11-14", CommandCategory::Network
    });

    Terminal::instance().registerCommand({
        "agent.terminal", "Run one normal terminal command as an agent action.",
        "agent.terminal <command>",
        [](const std::vector<std::string>& args) {
            if (args.empty() || args[0] == "agent.terminal") {
                Terminal::instance().addLog("[AGENT] Usage: agent.terminal <command>");
                return;
            }
            std::string command;
            for (const std::string& arg : args) {
                if (!command.empty()) command += ' ';
                command += arg;
            }
            Terminal::instance().execute(command);
        }, "2026-11-14", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "agent.mouse_move", "Move the real cursor to window coordinates using GLFW.",
        "agent.mouse_move <x> <y>",
        [](const std::vector<std::string>& args) {
            if (args.size() < 2) {
                Terminal::instance().addLog("[AGENT] Usage: agent.mouse_move <x> <y>");
                return;
            }
            double x = 0.0, y = 0.0;
            try {
                x = std::stod(args[0]);
                y = std::stod(args[1]);
            } catch (...) {
                Terminal::instance().addLog("[AGENT] invalid mouse coordinates");
                return;
            }
            if (mimita::game::hostSetCursorPos(x, y)) {
                writeAgentCommandEvent("input.normalized", "agent.mouse_move applied",
                                       {{"source", "agent"}, {"context", "menu_or_gameplay"},
                                        {"action", "mouse_move"}, {"x", x}, {"y", y}});
            }
        }, "2026-11-14", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "agent.mouse_down", "Hold a real mouse button at window coordinates.",
        "agent.mouse_down <x> <y> [left|right]",
        [](const std::vector<std::string>& args) {
            if (args.size() < 2) {
                Terminal::instance().addLog("[AGENT] Usage: agent.mouse_down <x> <y> [left|right]");
                return;
            }
            const int button = args.size() > 2 && args[2] == "right"
                ? GLFW_MOUSE_BUTTON_RIGHT : GLFW_MOUSE_BUTTON_LEFT;
            try {
                if (!postAgentMousePosition(std::stod(args[0]), std::stod(args[1]))) return;
            } catch (...) {
                Terminal::instance().addLog("[AGENT] invalid mouse coordinates");
                return;
            }
            postAgentMouseButton(button, true);
            writeAgentCommandEvent("input.normalized", "agent.mouse_down applied",
                {{"source", "agent"}, {"context", "menu_or_gameplay"},
                 {"action", "mouse_down"}, {"button", button == GLFW_MOUSE_BUTTON_RIGHT ? "right" : "left"}});
        }, "2026-11-14", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "agent.mouse_up", "Release a real mouse button.",
        "agent.mouse_up [left|right]",
        [](const std::vector<std::string>& args) {
            const int button = !args.empty() && args[0] == "right"
                ? GLFW_MOUSE_BUTTON_RIGHT : GLFW_MOUSE_BUTTON_LEFT;
            postAgentMouseButton(button, false);
            writeAgentCommandEvent("input.normalized", "agent.mouse_up applied",
                {{"source", "agent"}, {"context", "menu_or_gameplay"},
                 {"action", "mouse_up"}, {"button", button == GLFW_MOUSE_BUTTON_RIGHT ? "right" : "left"}});
        }, "2026-11-14", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "agent.mouse_click", "Click a real mouse button at window coordinates.",
        "agent.mouse_click <x> <y> [left|right]",
        [](const std::vector<std::string>& args) {
            if (args.size() < 2) {
                Terminal::instance().addLog("[AGENT] Usage: agent.mouse_click <x> <y> [left|right]");
                return;
            }
            const int button = args.size() > 2 && args[2] == "right"
                ? GLFW_MOUSE_BUTTON_RIGHT : GLFW_MOUSE_BUTTON_LEFT;
            try {
                if (!postAgentMousePosition(std::stod(args[0]), std::stod(args[1]))) return;
            } catch (...) {
                Terminal::instance().addLog("[AGENT] invalid mouse coordinates");
                return;
            }
            postAgentMouseButton(button, true);
            postAgentMouseButton(button, false);
            writeAgentCommandEvent("input.normalized", "agent.mouse_click queued",
                {{"source", "agent"}, {"context", "menu_or_gameplay"},
                 {"action", "mouse_click"}, {"button", button == GLFW_MOUSE_BUTTON_RIGHT ? "right" : "left"}});
        }, "2026-11-14", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "agent.mouse_scroll", "Scroll at a window-relative cursor position.",
        "agent.mouse_scroll <x> <y> <notches>",
        [](const std::vector<std::string>& args) {
            if (args.size() < 3) {
                Terminal::instance().addLog(
                    "[AGENT] Usage: agent.mouse_scroll <x> <y> <notches>");
                return;
            }
            try {
                const double x = std::stod(args[0]);
                const double y = std::stod(args[1]);
                const int notches = std::clamp(std::stoi(args[2]), -20, 20);
                if (notches == 0 || !postAgentMousePosition(x, y)) return;
                const LONG wheelDelta = static_cast<LONG>(notches * WHEEL_DELTA);
                mouse_event(MOUSEEVENTF_WHEEL, 0, 0,
                            static_cast<DWORD>(wheelDelta), 0);
                writeAgentCommandEvent("input.normalized", "agent mouse scroll injected",
                    {{"source", "agent"}, {"context", "menu_or_gameplay"},
                     {"action", "mouse_scroll"}, {"x", x}, {"y", y},
                     {"notches", notches}, {"wheel_delta", wheelDelta}});
            } catch (...) {
                Terminal::instance().addLog("[AGENT] invalid scroll coordinates or notches");
            }
        }, "2026-11-19", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "agent.gui_scan", "Report the visible UI widgets and clickable rectangles.",
        "agent.gui_scan",
        [](const std::vector<std::string>&) {
            int fbW = 0, fbH = 0, winW = 0, winH = 0;
            mimita::game::hostFramebufferSize(fbW, fbH);
            mimita::game::hostWindowSize(winW, winH);
            if (fbW <= 0 || fbH <= 0 || winW <= 0 || winH <= 0) {
                Terminal::instance().addLog("[AGENT] UI scan unavailable: window dimensions missing");
                return;
            }
            nlohmann::json widgets = nlohmann::json::array();
            const auto& tracked = uiGetTrackedWidgets();
            const std::size_t count = std::min<std::size_t>(tracked.size(), 256);
            for (std::size_t i = 0; i < count; ++i) {
                const auto& widget = tracked[i];
                const UIRect screen =
                    GuiCoordinateSystem::instance().designToScreen(widget.rect);
                widgets.push_back({
                    {"index", i}, {"id", widget.id},
                    {"design", {{"x", widget.rect.x}, {"y", widget.rect.y},
                                 {"w", widget.rect.w}, {"h", widget.rect.h}}},
                    {"screen", {{"x", screen.x}, {"y", screen.y},
                                {"w", screen.w}, {"h", screen.h}}},
                    {"window", {{"x", screen.x * winW / fbW},
                                 {"y", screen.y * winH / fbH},
                                 {"w", screen.w * winW / fbW},
                                 {"h", screen.h * winH / fbH}}},
                    {"hovered", widget.hovered}, {"pressed", widget.pressed}});
            }
            const auto& requestId = Terminal::instance().currentRequestId();
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Gui, StructuredLevel::Important,
                "agent.gui.scan", requestId,
                "captured the current frame's interactive UI rectangles", 0,
                {{"framebuffer_width", fbW}, {"framebuffer_height", fbH},
                 {"window_width", winW}, {"window_height", winH},
                 {"widget_count", count}, {"truncated", tracked.size() > count},
                 {"widgets", std::move(widgets)}},
                __FILE__, __LINE__, __FUNCTION__);
        }, "2026-11-19", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "agent.gui_button", "Press or release a UI button by its current scanned ID.",
        "agent.gui_button <element_id> <down|up>",
        [](const std::vector<std::string>& args) {
            if (args.size() < 2 || (args[1] != "down" && args[1] != "up")) {
                Terminal::instance().addLog(
                    "[AGENT] Usage: agent.gui_button <element_id> <down|up>");
                return;
            }
            const auto& widgets = uiGetTrackedWidgets();
            const auto found = std::find_if(widgets.rbegin(), widgets.rend(),
                [&](const UITrackedWidget& widget) { return widget.id == args[0]; });
            if (found == widgets.rend()) {
                writeAgentCommandEvent("agent.gui.button.failed",
                    "element ID is not present in the latest UI scan",
                    {{"element_id", args[0]}, {"operation", args[1]},
                     {"visible_widget_count", widgets.size()}});
                return;
            }
            int fbW = 0, fbH = 0, winW = 0, winH = 0;
            if (!mimita::game::hostFramebufferSize(fbW, fbH) ||
                !mimita::game::hostWindowSize(winW, winH) ||
                fbW <= 0 || fbH <= 0 || winW <= 0 || winH <= 0) return;
            const UIRect rect = GuiCoordinateSystem::instance().designToScreen(found->rect);
            const double x = (rect.x + rect.w * 0.5) * winW / fbW;
            const double y = (rect.y + rect.h * 0.5) * winH / fbH;
            if (!postAgentMousePosition(x, y)) {
                writeAgentCommandEvent("agent.gui.button.failed",
                    "target window could not receive pointer input",
                    {{"element_id", args[0]}, {"operation", args[1]},
                     {"x", x}, {"y", y}});
                return;
            }
            const bool down = args[1] == "down";
            postAgentMouseButton(GLFW_MOUSE_BUTTON_LEFT, down);
            writeAgentCommandEvent("agent.gui.button", "agent UI button input injected",
                {{"element_id", found->id}, {"operation", args[1]},
                 {"x", x}, {"y", y}, {"design_rect", {
                     {"x", found->rect.x}, {"y", found->rect.y},
                     {"w", found->rect.w}, {"h", found->rect.h}}}});
        }, "2026-11-19", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "agent.screenshot", "Save the current client window to a run-local BMP.",
        "agent.screenshot <logs/date/run/client-N/screenshot.bmp>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog(
                    "[AGENT] Usage: agent.screenshot <logs/date/run/client-N/screenshot.bmp>");
                return;
            }
            int width = 0, height = 0;
            std::string error;
            const bool captured = captureAgentWindowBmp(
                args[0], width, height, error);
            writeAgentCommandEvent(
                captured ? "agent.screenshot.saved" : "agent.screenshot.failed",
                captured ? "saved visible client window screenshot" : error,
                {{"path", args[0]}, {"width", width}, {"height", height},
                 {"captured", captured}});
        }, "2026-11-19", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "agent.input", "Drive normalized input without window focus.",
        "agent.input <look dx dy|key <name> <tap|down|up>|action <name> <tap>|mouse_button <left|right> <click|down|up>|terminal_toggle|menu_toggle|equip <slot>>",
        [](const std::vector<std::string>& args) {
            auto keyCode = [](const std::string& name) -> int {
                if (name == "W" || name == "w") return GLFW_KEY_W;
                if (name == "A" || name == "a") return GLFW_KEY_A;
                if (name == "S" || name == "s") return GLFW_KEY_S;
                if (name == "D" || name == "d") return GLFW_KEY_D;
                if (name == "SPACE" || name == "Space") return GLFW_KEY_SPACE;
                if (name == "ESCAPE" || name == "Escape") return GLFW_KEY_ESCAPE;
                if (name == "GRAVE" || name == "Grave") return GLFW_KEY_GRAVE_ACCENT;
                if (name.size() == 1 && name[0] >= '0' && name[0] <= '9')
                    return GLFW_KEY_0 + (name[0] - '0');
                return -1;
            };
            if (args.empty()) {
                Terminal::instance().addLog("[AGENT] Usage: agent.input <look|key|action|mouse_button|terminal_toggle|menu_toggle|equip>");
                return;
            }
            if (args[0] == "look" && args.size() >= 3) {
                try {
                    InputRouter::ingestLookDelta(
                        static_cast<float>(std::stod(args[1])),
                        static_cast<float>(std::stod(args[2])),
                        InputRouter::Source::Agent);
                } catch (...) { return; }
                writeAgentCommandEvent("input.normalized", "agent.input look applied",
                    {{"source", "agent"}, {"action", "look"},
                     {"dx", args[1]}, {"dy", args[2]}});
            } else if (args[0] == "key" && args.size() >= 3) {
                const int key = keyCode(args[1]);
                if (key < 0) return;
                const bool down = args[2] != "up";
                InputRouter::ingestKey(key, 0, down ? GLFW_PRESS : GLFW_RELEASE,
                                       0, InputRouter::Source::Agent);
                writeAgentCommandEvent("input.normalized", "agent.input key applied",
                    {{"source", "agent"}, {"action", "key"}, {"key", args[1]},
                     {"down", down}});
            } else if (args[0] == "action" && args.size() >= 2) {
                InputCommandSystem::instance().pulseAction(args[1]);
                writeAgentCommandEvent("input.normalized", "agent.input action applied",
                    {{"source", "agent"}, {"action", args[1]}});
            } else if (args[0] == "mouse_button" && args.size() >= 3) {
                const int button = args[1] == "right" ? GLFW_MOUSE_BUTTON_RIGHT
                                                      : GLFW_MOUSE_BUTTON_LEFT;
                const bool down = args[2] != "up";
                InputRouter::ingestMouseButton(button, down ? GLFW_PRESS : GLFW_RELEASE,
                                               InputRouter::Source::Agent);
                writeAgentCommandEvent("input.normalized", "agent.input mouse_button applied",
                    {{"source", "agent"}, {"action", "mouse_button"},
                     {"button", args[1]}, {"down", down}});
            } else if (args[0] == "shoot") {
                InputRouter::ingestMouseButton(GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS,
                                               InputRouter::Source::Agent);
                writeAgentCommandEvent("input.normalized", "agent.input shoot applied",
                    {{"source", "agent"}, {"action", "shoot"}});
            } else if (args[0] == "terminal_toggle") {
                Terminal::instance().toggle();
            } else if (args[0] == "menu_toggle") {
                PauseMenu::toggle(InputRouter::window());
            } else if (args[0] == "equip" && args.size() >= 2) {
                Terminal::instance().execute("equipslot" + args[1]);
            }
        }, "2026-11-15", CommandCategory::Debug
    });
    Terminal::instance().registerCommand({
        "state.migration-selftest", "Verify resident state migration and rejection semantics.",
        "state.migration-selftest",
        [](const std::vector<std::string>&) { runStateMigrationSelfTest(); },
        "2026-11-16", CommandCategory::Debug
    });
    Terminal::instance().registerCommand({
        "state.player-core-seed", "Seed player_core v1/v2 state for a live migration proof.",
        "state.player-core-seed <1|2> [reject]",
        [](const std::vector<std::string>& args) {
            const int version = args.empty() ? 0 : std::atoi(args[0].c_str());
            const bool reject = args.size() > 1 && args[1] == "reject";
            if (version != 1 && version != 2) {
                Terminal::instance().addLog(
                    "[STATE] usage: state.player-core-seed <1|2> [reject]");
                return;
            }
            const bool ok = mimita::game::seedPlayerCoreState(version, reject);
            Terminal::instance().addLog(
                ok ? "[STATE] player_core seed PASS"
                   : "[STATE] player_core seed FAIL");
        },
        "2026-11-16", CommandCategory::Debug
    });
    Terminal::instance().registerCommand({
        "state.player-core-migrate", "Migrate the resident player_core domain to v3.",
        "state.player-core-migrate",
        [](const std::vector<std::string>&) {
            const bool ok = mimita::game::migratePlayerCoreState();
            Terminal::instance().addLog(
                ok ? "[STATE] player_core migration PASS"
                   : "[STATE] player_core migration FAIL");
        },
        "2026-11-16", CommandCategory::Debug
    });
    Terminal::instance().registerCommand({
        "state.player-core-migrate-v2", "Migrate the resident player_core domain from v1 to v2.",
        "state.player-core-migrate-v2",
        [](const std::vector<std::string>&) {
            const bool ok = mimita::game::migratePlayerCoreStateToV2();
            Terminal::instance().addLog(
                ok ? "[STATE] player_core v1->v2 PASS"
                   : "[STATE] player_core v1->v2 FAIL");
        },
        "2026-11-16", CommandCategory::Debug
    });

}

} // namespace

#include "avatar/character-registry.h"
#include "render/lighting-config.h"
#include "render/lighting-commands.h"
#include "render/dynamic-light-commands.h"
#include "render/dynamic-light-config.h"
#include "render/postfx-commands.h"
#include "hot-reload/hot-reload-system.h"
#include "auth/auth-system.h"
#include "profile/local-profile-system.h"
#include "gui/menus/sign-in-menu.h"
#include "gui/menus/server-info-menu.h"
#include "gui/menus/online-menu.h"
#include "game/duel.h"
#include "gui/menus/duel-config-menu.h"
#include "terminal/terminal-state.h"
#include "terminal/replay-commands.h"
#include "terminal/debug-commands.h"
#include "terminal/player-commands.h"
#include "terminal/movement-commands.h"
#include "terminal/weapon-commands.h"
#include "terminal/weapon-bench-commands.h"
#include "terminal/npc-commands.h"
#include "terminal/admin-commands.h"
#include "terminal/crate-commands.h"
#include "terminal/object-commands.h"
#include "physics/constraints/constraint-commands.h"
#include "vehicles/vehicle-commands.h"
#include "vehicles/vehicle-system.h"
#include "interaction/interaction-commands.h"
#include "physics/runtime-objects-commands.h"
#include "terminal/procedural-world-commands.h"
#include "procedural/infinite-world-runtime.h"
#include "interaction/world-interaction-runtime.h"
#include "worldgen/worldgen-commands.h"
#include "terminal/duel-commands.h"
#include "terminal/actor-commands.h"
#include "terminal/editor-commands.h"
#include "mapeditor/map-editor-commands.h"
#include "mapeditor/map-editor-projects.h"
#include "mapeditor/world-edit-commands.h"
#include "terminal/network-commands.h"
#include "terminal/badconn-commands.h"
#include "terminal/vip-commands.h"
#include "network/badconn/badconn.h"
#include "terminal/auth-commands.h"
#include "perf/perf.h"
#include "replay/replay-export.h"
#include "effects/hitfx-commands.h"
#include "effects/hit-effects.h"
#include "config/weapon-hitfx-config.h"
#include "config/impact-decals-config.h"
#include "config/weapon-tracers-config.h"
#include "game/gamemode-manager.h"
#include "gui/gui-editor-commands.h"
#include "camera/camera-commands.h"
#include "audio/audio-commands.h"
#include "audio/music-commands.h"
#include "notifications/notification-commands.h"
#include <windows.h>
#include <GLFW/glfw3.h>

extern DuelManager gDuelManager;
extern GamemodeManager gGamemodeManager;
extern FramePacer gFramePacer;
extern Player* gpPlayer;
extern Camera* gpCamera;
extern World* gpWorld;
extern NpcSystem* gpNpcSystem;
extern WeaponSystem* gpWeapons;
extern bool* gpFreecamEnabled;
extern glm::vec3* gpDeathPosition;
extern int* gpSelectedEditorObject;
extern bool* gpEditorMode;
extern std::string* gpActiveGameMode;
extern std::string* gpActiveMapPath;
extern bool* gpWorldLoaded;
extern ReplayRingBuffer* gpReplayRecorder;
extern ReplayPlayer* gpReplayPlayer;
extern ReplayFactory* gpReplayFactory;
extern ReplayBrowser* gpReplayBrowser;
extern ReplayTimeline* gpReplayTimeline;
extern ReplayCameraMgr* gpReplayCameraMgr;
extern std::unordered_map<std::string, ActorChatState>* gpReplayChatStates;
extern std::vector<std::string>* gpReplayClipsCache;
extern std::unordered_map<int, std::string>* gpCommandBinds;
extern std::unordered_map<int, bool>* gpBindPrev;
extern DuelConfig* gpDuelConfig;
extern MimitaNet::MultiplayerContext* gpMpContext;
extern GameState* gpGameState;
extern ChatHistory* gpChatHistory;
extern bool gReplayExportRenderMode;
extern bool gReplayCinematicMode;
extern bool gNetPresentationDebug;
extern bool gNetDebugEntities;
extern bool gMainmenuDebug;

extern std::unordered_map<std::string, std::unique_ptr<Player>>* gpReplayActorModels;
extern std::unordered_map<std::string, WeaponViewModel>* gpReplayWeaponModels;

extern Renderer* gRenderer;

template <typename T, typename... Args>
T* residentObject(mimita::host::StateObjectId id, Args&&... args) {
    bool created = false;
#ifdef MIMITA_GAME_DLL
    void* storage = mimita::game::ensureResidentObject(
        id, sizeof(T), alignof(T), &created);
#else
    std::int32_t first = 0;
    void* storage = mimita::host::HostRuntime::instance().ensureStateObject(
        static_cast<std::uint32_t>(id), static_cast<std::uint32_t>(sizeof(T)),
        static_cast<std::uint32_t>(alignof(T)), &first);
    created = first != 0;
#endif
    if (!storage) return nullptr;
    auto* value = static_cast<T*>(storage);
    if (created) new (value) T(std::forward<Args>(args)...);
    return value;
}

template <typename T, typename... Args>
T& requireResident(mimita::host::StateObjectId id, Args&&... args) {
    if (T* value = residentObject<T>(id, std::forward<Args>(args)...)) {
        return *value;
    }
    std::fprintf(stderr, "[STATE] incompatible resident object id=%u\n",
                 static_cast<unsigned>(id));
    std::abort();
}

void gameInitSubsystems(Engine& engine)
{
#ifdef MIMITA_GAME_DLL
    auto* menuState = mimita::game::menuState();
#else
    auto* menuState = mimita::host::HostRuntime::instance().menuState();
#endif
    GameState& gameState = reinterpret_cast<GameState&>(menuState->gameState);
    bool& worldLoaded = menuState->worldLoaded;
    bool& editorMode = menuState->editorMode;
    int& selectedEditorObject = menuState->selectedEditorObject;
    bool& freecamEnabled = menuState->freecamEnabled;

    // Create or migrate the resident player_core domain before gameplay reads
    // it. A rejected migration leaves the previous resident block committed.
    mimita::game::ensurePlayerCoreState();

    DevConfig::instance().load("config/dev_controls.txt");
    DevOverlay::instance().init(engine.window());
    DevOverlay::instance().showNotification("Dev mode enabled. Press ` to open console.", 5.0f);
    CreateDefaultAccountConfig();
    LoadAccountConfig("default");
    LoadDuelStats("default");

    // MusicManager is lazy-initialized on first update() — not blocking startup

    VideoSettings::instance().load();
    VideoSettings::instance().apply();
    gFramePacer.setMaxFrames(VideoSettings::instance().maxFrames());
    gFramePacer.setVSync(false);

    printf("[VSYNC] FORCED OFF: software frame pacing only (maxFrames=%d)\n",
           gFramePacer.maxFrames());
    Debug::log(Debug::Category::Render,
        "[VSYNC] FORCED OFF: swap interval=0, software pacer active, "
        "VSync cannot be re-enabled at runtime\n");

    PostFX::instance().loadConfig("config/postfx.json");
    // PostFX FBO is lazily initialized on first bindFBO() call

    InputCommandSystem::instance().init(engine.window());
    InputCommandSystem::instance().loadBinds("config/input/binds.json");
    initChatWindowState(gChatWindowState);
    RegisterTeleportCommands();
    Terminal::instance().init(engine.window());
    AnalyticsManager::instance().registerCommands();

    LightingConfig::instance().load("config/lighting.json");
    DynamicLightConfig::instance().load("config/lighting.json");
    ShadowConfig::instance().load("config/shadows.json");
    GameplayConfig::instance().load("config/gameplay.json");
    NpcDifficultyConfig::instance().load("config/npc-difficulty.json");
    GamemodeRegistry::instance().loadDirectory("config/gamemodes");
    MatchRoleRegistry::instance().load("config/roles.json");
    MatchRoleRegistry::instance().loadActorPresets("config/actor-presets");
    MonsterPoolRegistry::instance().load("config/monster-pools.json");
    BehaviorProfileRegistry::instance().load("config/behavior-profiles.json");
    GamemodeMapPool::instance().load("config/gamemode-good-maps.json");
    MovementJsonConfig::instance().load("config/movement.json");
    CrosshairConfig::instance().load();
    AimBodyConfig::instance().load("config/aimbody.json");
    registerCrosshairCommands();
    HealthbarConfig::instance().load();

    EffectPartSystem::instance().init();
    // HotReload DLL is lazy-loaded on first reloadGameDLLIfChanged() call
    printf("[MAIN] dev tools initialized\n");

    namespace r = mimita::game::render;
    r::enable(r::Capability::Blend);
    r::blendFunc(r::BlendFactor::SrcAlpha, r::BlendFactor::OneMinusSrcAlpha);
    r::enable(r::Capability::DepthTest);

    printf("[MAIN] after blend/depth setup\n");

    World& world = requireResident<World>(mimita::host::StateObjectId::World);
    printf("[MAIN] world object made; world JSON loads when PLAY is pressed so the menu appears first\n");

    Player& player = requireResident<Player>(mimita::host::StateObjectId::Player);
    player.username = AuthSystem::instance().displayName();
    Debug::warn(Debug::Category::Auth, "[BOOT] player.username=%s\n", player.username.c_str());
    // Every new game starts with empty hands. The saved slot is a convenience
    // for later equips, not an initial loadout.
    player.equippedSlot = 0;
    player.mCharacterName = GetPlayerSettings().characterName;
    if (player.mCharacterName.empty())
        player.mCharacterName = "DefaultGuy";

    // Avatar metadata is loaded at boot (fast ~1ms JSON parse).
    // Model + atlas are loaded lazily on first render via worker threads.
    if (!GetPlayerSettings().avatarName.empty())
        AvatarSystem::instance().loadAvatar(GetPlayerSettings().avatarName);

    printf("[MAIN] player made\n");

    NpcSystem& npcSystem = requireResident<NpcSystem>(
        mimita::host::StateObjectId::NpcSystem);
    printf("[MAIN] npc system made\n");

    Camera& camera = requireResident<Camera>(mimita::host::StateObjectId::Camera);
    printf("[MAIN] camera made\n");
    printf("[CAMERA] Config: stiffness=%.2f  fov=%.0f\n", CamConfig::instance().data().positionStiffness, CamConfig::instance().data().fov);
    printf("[CAMERA] Raw 1:1 camera enabled\n");

    engine.bindCamera(&camera);
    glfwSetWindowUserPointer(engine.window(), &camera);
    printf("[MAIN] camera bound\n");

    ReplayRingBuffer& gReplayRecorder = requireResident<ReplayRingBuffer>(
        mimita::host::StateObjectId::ReplayRecorder);
    ReplayPlayer& gReplayPlayer = requireResident<ReplayPlayer>(
        mimita::host::StateObjectId::ReplayPlayer);
    ReplaySaveWorker* gReplayWorker = &requireResident<ReplaySaveWorker>(
        mimita::host::StateObjectId::ReplaySaveWorker);
    AvatarSystem::instance().setBackgroundWorker(gReplayWorker);
    ReplayFactory& gReplayFactory = requireResident<ReplayFactory>(
        mimita::host::StateObjectId::ReplayFactory, gReplayRecorder);
    gReplayFactory.setWorker(gReplayWorker);
    ReplayBrowser& gReplayBrowser = requireResident<ReplayBrowser>(
        mimita::host::StateObjectId::ReplayBrowser);
    ReplayTimeline& gReplayTimeline = requireResident<ReplayTimeline>(
        mimita::host::StateObjectId::ReplayTimeline);
    ReplayCameraMgr& gReplayCameraMgr = requireResident<ReplayCameraMgr>(
        mimita::host::StateObjectId::ReplayCameraMgr);


    setReplayFactoryNotifyFn([](const std::string& killerId,
                                 const std::string& victimId,
                                 bool killerAirborne,
                                 bool victimAirborne,
                                 bool roundWinning) {
        REPLAY_FACTORY.notifyKill(killerId, victimId, killerAirborne, victimAirborne, roundWinning);
    });

    gReplayBrowser.setPlayCallback([](const std::string& path) {
        if (!REPLAY_PLAYER.loadFromJSON(path)) {
            Terminal::instance().addLog("[ERROR] failed to load: " + path);
            return;
        }
        REPLAY_PLAYER.preloadAssets();
        REPLAY_PLAYER.beginPlayback();
        GAME_STATE = GAME_PLAYING;
    });
    auto& gReplayChatStates = requireResident<
        std::unordered_map<std::string, ActorChatState>>(
        mimita::host::StateObjectId::ReplayChatStates);
    auto& G_REPLAY_CLIPS_CACHE = requireResident<std::vector<std::string>>(
        mimita::host::StateObjectId::ReplayClipsCache);
    auto& G_COMMAND_BINDS = requireResident<std::unordered_map<int, std::string>>(
        mimita::host::StateObjectId::CommandBinds);
    auto& G_BIND_PREV = requireResident<std::unordered_map<int, bool>>(
        mimita::host::StateObjectId::BindPrev);
    static std::mt19937 rng(std::random_device{}());

    // Keep the owning object distinct from the gChatHistory convenience macro
    // in terminal-state.h so initialization targets the real storage object.
    ChatHistory& sChatHistory = requireResident<ChatHistory>(
        mimita::host::StateObjectId::ChatHistory);

    DuelConfig& gDuelConfig = requireResident<DuelConfig>(
        mimita::host::StateObjectId::DuelConfig);

    MimitaNet::MultiplayerContext& mpContext = requireResident<
        MimitaNet::MultiplayerContext>(mimita::host::StateObjectId::MpContext);

    std::string& activeGameMode = requireResident<std::string>(
        mimita::host::StateObjectId::ActiveGameMode, "sandbox");
    // const std::string defaultMapPath =
    //     "assets/maps/mimita-aabb-only-interior-small-v4.glb";
    // 9 5 2026 not good  needs to work without wifi
    // cuz i cant strart a server without wfii so just make it start a local one todo
    const std::string defaultMapPath =
       "assets/maps/dust2cyberiav4.glb";
        
    std::string& activeMapPath = requireResident<std::string>(
        mimita::host::StateObjectId::ActiveMapPath);
    WeaponSystem& weapons = requireResident<WeaponSystem>(
        mimita::host::StateObjectId::Weapons);
    auto& replayActorModels = requireResident<
        std::unordered_map<std::string, std::unique_ptr<Player>>>(
        mimita::host::StateObjectId::ReplayActorModels);
    auto& replayWeaponModels = requireResident<
        std::unordered_map<std::string, WeaponViewModel>>(
        mimita::host::StateObjectId::ReplayWeaponModels);
    gpReplayActorModels = &replayActorModels;
    gpReplayWeaponModels = &replayWeaponModels;
        static glm::vec3 deathPosition{0.0f};
    struct ReplayTestState {
        bool active = false;
        uint32_t tick = 0;
        uint32_t npcId = 0;
    };
    static ReplayTestState replayTest;

    gpPlayer = &player;
    gpCamera = &camera;
    gpWeapons = &weapons;
    gpReplayRecorder = &gReplayRecorder;
    gpReplayPlayer = &gReplayPlayer;
    gpReplayFactory = &gReplayFactory;
    gpReplayBrowser = &gReplayBrowser;
    gpReplayTimeline = &gReplayTimeline;
    gpReplayCameraMgr = &gReplayCameraMgr;
    gpReplayChatStates = &gReplayChatStates;
    gpReplayClipsCache = &G_REPLAY_CLIPS_CACHE;
    gpCommandBinds = &G_COMMAND_BINDS;
    gpBindPrev = &G_BIND_PREV;
    gpGameState = reinterpret_cast<GameState*>(&menuState->gameState);
    Debug::log(Debug::Category::Chat,
               "[CHAT TRACE 1 BEFORE INIT] gpChatHistory=%p\n",
               (void*)gpChatHistory);
    gpChatHistory = &sChatHistory;
    Debug::log(Debug::Category::Chat,
               "[CHAT TRACE 1 AFTER INIT] gpChatHistory=%p historyAddress=%p\n",
               (void*)gpChatHistory, (void*)&sChatHistory);
    gpWorld = &world;
    setAudioSpatialWorld(&world);
    gpActiveMapPath = &activeMapPath;
    gpNpcSystem = &npcSystem;
    gpFreecamEnabled = &menuState->freecamEnabled;
    gpDeathPosition = &deathPosition;
    gpEditorMode = &menuState->editorMode;
    gpActiveGameMode = &activeGameMode;
    gpWorldLoaded = &menuState->worldLoaded;
    gpDuelConfig = &gDuelConfig;
    gpMpContext = &mpContext;
#ifndef MIMITA_GAME_DLL
    mimita::host::HostTransportAdapter::instance().bind(&mpContext);
    mimita::host::HostRuntime::instance().setTransportAdapter(
        mimita::host::HostTransportAdapter::instance().adapter(),
        mimita::host::HostTransportAdapter::instance().owner());
#endif

    registerPlayerCommands();
    registerMovementCommands();
    registerAimCommands();
    registerAuthCommands();
    registerVipCommands();
    registerWeaponCommands();
    registerWeaponBenchCommands();
    registerWeaponDebugCommand();
    loadWorldCrosshairConfig();
    loadCoolShotLineConfig();
    WeaponTracersConfig::instance().load();
    applyStartupDefaults();
    registerWorldXhReloadCommand();
    registerCoolShotLineCommands();
    registerCameraCommands();
    registerGuiEditorCommands();
    registerPostFxCommands();
    registerLightingCommands();
    registerDynamicLightCommands();
    registerVideoCommands();
    registerMusicCommands();
    registerAudioCommands();
    registerNotificationCommands();
    registerDevLogCommands();
    registerDevOverlayCommands();

    registerNpcCommands();
    registerAdminCommands();
    registerMapEntityCommands();
    registerCrateCommands();
    registerObjectCommands();
    registerConstraintCommands();
    registerVehicleCommands();
    registerInteractionCommands();
    registerRuntimeObjectCommands();
    VehicleSystem::instance().loadConfig("config/vehicles");
    VehicleSystem::instance().registerFixedTick();
    registerProceduralWorldCommands();
    registerInfiniteWorldCommands();
    registerWorldInteractionCommands();
    registerWorldGenCommands();
    registerTransformDebugCommands();

    registerEditorCommands();
    registerMapEditorCommands();
    registerWorldProjectCommands();
    registerWorldEditCommands();
    registerNetworkCommands();
    badconn::loadConfig(badconn::configPath());
    registerBadConnCommands();

    registerReplayCaptureCommands();
    registerReplayCommands();
    registerReplayCameraCommands();

    // Bind F3 to save instant replay
    {
        auto& binds = G_COMMAND_BINDS;
        binds[GLFW_KEY_F3] = "replay.save";
        binds[GLFW_KEY_P] = "rplfx";
        binds[GLFW_KEY_J] = "replay_open_last_export";
    }
    registerAvatarCommands(player);
    registerAgentCommands();
    RegisterScenarioCommands();
    registerVoidDeathCommands();
    registerHitmarkerAudioCommands();
    registerOutroCommands();
    registerPerfCommands();
    registerHitFxCommands();
    registerDiagnosticCommands();
    registerShadowCommands();
    registerDebugCommands();
    registerWorldTextureCommands();
    SizeScalingConfig::instance().load("config/size_scaling.json");
    registerRagdollCommands();
    HitEffects::loadConfig("config/hitfx.json");
    WeaponHitFxConfig::instance().load("config/weapon_hitfx.json");
    ImpactDecalsConfig::instance().load("config/impact_decals.json");
    SpawnVelocityConfig::instance().load("config/spawnvelocity.json");

    Terminal::instance().registerCommand({
        "spawnvelocity_reload",
        "Reload spawnvelocity.json without restart",
        "spawnvelocity_reload",
        [](const std::vector<std::string>&) {
            if (SpawnVelocityConfig::instance().load()) {
                const auto& d = SpawnVelocityConfig::instance().data();
                char buf[256];
                snprintf(buf, sizeof(buf),
                    "[SPAWN VELOCITY] reloaded: enabled=%d mode=%s speed=%.1f",
                    (int)d.enabled, d.mode.c_str(), d.speed);
                Terminal::instance().addLog(buf);
            } else {
                Terminal::instance().addLog(
                    "[SPAWN VELOCITY] config reload failed (see log)");
            }
        },
        "2026-09-08",
        CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "spawnvelocity_status",
        "Print spawn velocity config status",
        "spawnvelocity_status",
        [](const std::vector<std::string>&) {
            const auto& d = SpawnVelocityConfig::instance().data();
            char buf[512];
            snprintf(buf, sizeof(buf),
                "[SPAWN VELOCITY] enabled=%d mode=%s speed=%.1f "
                "dir=(%.1f,%.1f,%.1f) vertical=%d range=(%.0f,%.0f)",
                (int)d.enabled, d.mode.c_str(), d.speed,
                d.direction.x, d.direction.y, d.direction.z,
                (int)d.applyVertical, d.randomRange.x, d.randomRange.y);
            Terminal::instance().addLog(buf);
        },
        "2026-09-08",
        CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "net_debug_entities", "Toggle entity replication debug overlay", "net_debug_entities [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                gNetDebugEntities = !gNetDebugEntities;
            } else {
                gNetDebugEntities = args[0] == "1";
            }
            printf("[NET ENTITIES DEBUG] %s\n", gNetDebugEntities ? "ON" : "OFF");
            Terminal::instance().addLog(gNetDebugEntities
                ? "[NET] Entity debug ON"
                : "[NET] Entity debug OFF");
        }
    });
    Terminal::instance().registerCommand({
        "replay_test",
        "Record a deterministic gameplay replay and validate it in Blender",
        "replay_test",
        [](const std::vector<std::string>&) {
            if (GAME_STATE != GAME_PLAYING || ACTIVE_MAP_PATH.empty()) {
                Terminal::instance().addLog(
                    "[REPLAY TEST] Enter a loaded game map first");
                return;
            }
            if (MP_CONTEXT.active) {
                Terminal::instance().addLog(
                    "[REPLAY TEST] Disabled during multiplayer");
                return;
            }
            if (replayTest.active) {
                Terminal::instance().addLog(
                    "[REPLAY TEST] A recording is already active");
                return;
            }

            THE_NPC_SYSTEM.destroyAll();
            if (REPLAY_RECORDER.isRecording())
                REPLAY_RECORDER.stopRecording();
            Terminal::instance().execute("replay.record");
            if (!REPLAY_RECORDER.isRecording()) {
                Terminal::instance().addLog(
                    "[REPLAY TEST] Could not start recording");
                return;
            }

            glm::vec3 forward = THE_CAMERA.front;
            if (glm::length(forward) < 0.001f)
                forward = glm::vec3(0.0f, 1.0f, 0.0f);
            forward = glm::normalize(forward);
            replayTest.npcId = THE_NPC_SYSTEM.nextNpcId();
            THE_NPC_SYSTEM.spawnNpc(replayTest.npcId, 1.0f, npcSpawnPoint);
            if (!THE_NPC_SYSTEM.all().empty()) {
                Npc& npc = THE_NPC_SYSTEM.all().back();
                npc.trainingMode = 0;
                npc.body.maxHp = 500;
                npc.body.currentHp = 500;
            }

            THE_PLAYER.dead = false;
            THE_PLAYER.currentHp = THE_PLAYER.maxHp;
            replayTest.tick = 0;
            replayTest.active = true;
            Terminal::instance().addLog(
                "[REPLAY TEST] Running 300-tick automated scenario");
        }
    });

    registerDuelCommands();
    registerActorCommands();
    registerCompetitiveCommands();
    registerLeaderboardCommands();
    registerCursorCommands();
}

