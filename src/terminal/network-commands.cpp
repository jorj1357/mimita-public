#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cctype>
#include <string>
#include <vector>
#include <algorithm>
#include <random>
#include <filesystem>
#include <fstream>
#include <shellapi.h>
#include <nlohmann/json.hpp>
#include "devtools/terminal.h"
#include "terminal/terminal-state.h"
#include "network/net_mode.h"
#include "network/server.h"
#include "network/coordinator-client.h"
#include "network/multiplayer-context.h"
#include "network/disagreement-visuals.h"
#include "config/networking-config.h"
#include "utils/path_utils.h"
#include "gui/gui-main.h"
#include "auth/auth-system.h"

extern bool gNetPresentationDebug;
extern bool gRoomCodeShow;

namespace {

// Networking preset snapshots live in a folder next to the live config file.
std::string networkPresetsDir()
{
    return resolveAssetPath("config/networking/presets/");
}

std::vector<std::string> listNetworkPresetNames()
{
    std::vector<std::string> names;
    std::error_code ec;
    for (auto& entry : std::filesystem::directory_iterator(networkPresetsDir(), ec))
    {
        if (ec || !entry.is_regular_file(ec))
            continue;
        std::string name = entry.path().filename().string();
        if (name.size() >= 5 && name.compare(name.size() - 5, 5, ".json") == 0)
            names.push_back(name.substr(0, name.size() - 5));
    }
    std::sort(names.begin(), names.end());
    return names;
}

bool copyLiveConfigTo(const std::string& destination)
{
    std::error_code ec;
    std::filesystem::create_directories(networkPresetsDir(), ec);
    if (ec)
        return false;
    std::filesystem::copy_file(
        NetworkingConfig::defaultPath(), destination,
        std::filesystem::copy_options::overwrite_existing, ec);
    return !ec;
}

std::string sanitizePresetName(const std::string& raw)
{
    std::string safe;
    safe.reserve(raw.size());
    for (const char c : raw)
    {
        if (std::isalnum((unsigned char)c) || c == '_' || c == '-')
            safe.push_back(c);
        else if (c == ' ' || c == '.')
            safe.push_back('_');
    }
    return safe;
}

void startInteractiveSave()
{
    const std::vector<std::string> names = listNetworkPresetNames();
    std::string prompt = "[NETWORK CONFIG] What preset do you want to save to?";
    for (size_t i = 0; i < names.size(); ++i)
        prompt += "\n  " + std::to_string(i + 1) + ": " + names[i];
    prompt += "\n  " + std::to_string(names.size() + 1) + ": New preset";
    prompt += "\n  (type 'cancel' to abort)";
    Terminal::instance().addLog(prompt);

    Terminal::instance().requestInput("[save to preset] > ",
        [names](const std::string& answer) {
            if (answer.empty() || answer == "cancel")
            {
                Terminal::instance().addLog("[NETWORK CONFIG] save cancelled");
                return;
            }
            bool numeric = !answer.empty();
            for (const char c : answer)
                if (!std::isdigit((unsigned char)c)) { numeric = false; break; }
            if (!numeric)
            {
                Terminal::instance().addLog("[NETWORK CONFIG] invalid preset number — cancelled");
                return;
            }
            const int number = std::atoi(answer.c_str());
            if (number >= 1 && (size_t)number <= names.size())
            {
                const std::string file = names[(size_t)number - 1];
                if (copyLiveConfigTo(networkPresetsDir() + file + ".json"))
                    Terminal::instance().addLog("[NETWORK CONFIG] saved to preset " +
                        std::to_string(number) + " (" + file + ")");
                else
                    Terminal::instance().addLog("[NETWORK CONFIG] save FAILED");
                return;
            }
            if (number == (int)names.size() + 1)
            {
                Terminal::instance().requestInput("[name the preset] > ",
                    [](const std::string& rawName) {
                        if (rawName.empty() || rawName == "cancel")
                        {
                            Terminal::instance().addLog("[NETWORK CONFIG] save cancelled");
                            return;
                        }
                        const std::string safe = sanitizePresetName(rawName);
                        if (safe.empty())
                        {
                            Terminal::instance().addLog("[NETWORK CONFIG] invalid preset name — cancelled");
                            return;
                        }
                        if (copyLiveConfigTo(networkPresetsDir() + safe + ".json"))
                            Terminal::instance().addLog("[NETWORK CONFIG] saved new preset '" + safe + "'");
                        else
                            Terminal::instance().addLog("[NETWORK CONFIG] save FAILED");
                    });
                return;
            }
            Terminal::instance().addLog("[NETWORK CONFIG] invalid preset number — cancelled");
        });
}

void listNetworkPresets()
{
    const std::vector<std::string> names = listNetworkPresetNames();
    if (names.empty())
    {
        Terminal::instance().addLog("[NETWORK CONFIG] no presets found in " + networkPresetsDir());
        return;
    }
    Terminal::instance().addLog("[NETWORK CONFIG] presets:");
    for (size_t i = 0; i < names.size(); ++i)
        Terminal::instance().addLog("  " + std::to_string(i + 1) + ": " + names[i]);
}

void loadNetworkPreset(int number)
{
    const std::vector<std::string> names = listNetworkPresetNames();
    if (number < 1 || (size_t)number > names.size())
    {
        Terminal::instance().addLog("[NETWORK CONFIG] preset " + std::to_string(number) +
                                    " not found — use 'networkconfig list'");
        return;
    }
    const std::string name = names[(size_t)number - 1];
    std::error_code ec;
    std::filesystem::copy_file(
        networkPresetsDir() + name + ".json",
        NetworkingConfig::defaultPath(),
        std::filesystem::copy_options::overwrite_existing, ec);
    if (ec)
    {
        Terminal::instance().addLog("[NETWORK CONFIG] could not load preset '" + name + "'");
        return;
    }
    if (NetworkingConfig::instance().reloadFromDisk())
        Terminal::instance().addLog("[NETWORK CONFIG] loaded preset " + std::to_string(number) +
                                    " ('" + name + "') — hot-reloaded everywhere");
    else
        Terminal::instance().addLog("[NETWORK CONFIG] preset '" + name + "' loaded but file invalid — keeping previous config");
}

// Live-editable knobs under remote_player_interpolation. `set` writes the new
// value into networkingconfig.json and hot-reloads so every client + hosted/
// dedicated server picks it up within ~250ms.
struct NetConfigKnob
{
    const char* key;
    bool isMs;      // value is milliseconds (stored as seconds internally)
    bool isBool;
    bool isString;  // arbitrary string value (mode names, etc.)
    double minMs = 0.0;
    double maxMs = 60000.0;
    // JSON block to write into ("remote_player_interpolation" or
    // "remote_motion_smoothing"). Empty = legacy behavior = remote_player_interpolation.
    const char* block = "remote_player_interpolation";
};

const std::vector<NetConfigKnob>& netConfigKnobs()
{
    static const std::vector<NetConfigKnob> knobs = {
        {"interpolation_delay_ms", true, false, false, 0.0, 500.0},
        {"server_broadcast_delay_ms", true, false, false, 0.0, 1000.0},
        {"server_broadcast_extrapolation_ms", true, false, false, 0.0, 1000.0},
        {"maximum_extrapolation_ms", true, false, false, 0.0, 1000.0},
        {"maximum_buffered_snapshots", false, false, false, 2.0, 512.0},
        {"minimum_snapshots_before_rendering", false, false, false, 1.0, 64.0},
        {"server_smoothing_duration_ticks", false, false, false, 1.0, 32.0},
        {"teleport_distance", false, false, false, 0.1, 100000.0},
        {"rewind_compensation_ms", true, false, false, -2000.0, 2000.0},
        {"rewind_hit_tolerance", false, false, false, 0.1, 100.0},
        {"server_broadcast_max_speed", false, false, false, 0.0, 100000.0},
        {"server_sim_smooth_ticks", false, false, false, 0.0, 32.0},
        {"direct_render", false, true, false, 0.0, 1.0},
        {"server_smoothing", false, true, false, 0.0, 1.0},
        {"allow_extrapolation", false, true, false, 0.0, 1.0},
        {"enabled", false, true, false, 0.0, 1.0},
        {"position_mode", false, false, true, 0.0, 1.0},
        {"rotation_mode", false, false, true, 0.0, 1.0},
        // linear-mode motion-smoothing knobs (written to remote_motion_smoothing)
        {"render_filter", false, false, true, 0.0, 1.0, "remote_motion_smoothing"},
        {"linear_delay_ticks", false, false, false, 1.0, 128.0, "remote_motion_smoothing"},
        {"linear_min_delay_ticks", false, false, false, 1.0, 128.0, "remote_motion_smoothing"},
        {"linear_max_delay_ticks", false, false, false, 0.0, 128.0, "remote_motion_smoothing"},
        {"linear_hold_on_dry", false, true, false, 0.0, 1.0, "remote_motion_smoothing"},
        {"linear_allow_extrapolation", false, true, false, 0.0, 1.0, "remote_motion_smoothing"},
        {"linear_catchup_rate_ticks_per_second", false, false, false, 0.0, 1000.0, "remote_motion_smoothing"},
        {"linear_delay_jitter_multiplier", false, false, false, 0.0, 20.0, "remote_motion_smoothing"},
        {"linear_delay_loss_weight", false, false, false, 0.0, 10.0, "remote_motion_smoothing"},
        {"linear_snap_after_gap_ticks", false, false, false, 0.0, 1000.0, "remote_motion_smoothing"},
        {"linear_deadzone_units", false, false, false, 0.0, 1.0, "remote_motion_smoothing"},
        {"linear_clock_source", false, false, true, 0.0, 1.0, "remote_motion_smoothing"},
        {"linear_reanchor_enabled", false, true, false, 0.0, 1.0, "remote_motion_smoothing"},
        {"linear_reanchor_only_if_error_ms", true, false, false, 0.0, 5000.0, "remote_motion_smoothing"},
        {"max_render_time_jump_ms", true, false, false, 0.0, 1000.0, "remote_motion_smoothing"},
        {"remote_effect_maximum_hold_ms", true, false, false, 0.0, 5000.0, "event_timeline"},
        {"predict_damage", false, true, false, 0.0, 1.0, "prediction"},
        {"predict_deaths", false, true, false, 0.0, 1.0, "prediction"},
        {"log_interpolation_state", false, true, false, 0.0, 1.0, "debug"},
        {"interpolation_log_rate_hz", false, false, false, 0.1, 60.0, "debug"},
        {"detect_interpolation_jitter", false, true, false, 0.0, 1.0, "debug"},
        {"max_allowed_alpha_jump", false, false, false, 0.0, 1.0, "debug"},
        {"max_allowed_visual_delta_multiplier", false, false, false, 0.0, 100.0, "debug"},
    };
    return knobs;
}

bool networkConfigSetKey(const std::string& key, const std::string& value)
{
    const std::vector<NetConfigKnob>& knobs = netConfigKnobs();
    const NetConfigKnob* knob = nullptr;
    for (const auto& k : knobs)
        if (key == k.key) { knob = &k; break; }
    if (!knob)
        return false;

    if (knob->isString)
    {
        if (value.empty())
            return false;
    }
    else
    {
        double parsed = 0.0;
        if (knob->isBool)
        {
            if (value == "1" || value == "true" || value == "on") parsed = 1.0;
            else if (value == "0" || value == "false" || value == "off") parsed = 0.0;
            else return false;
        }
        else
        {
            parsed = std::atof(value.c_str());
            if (parsed == 0.0 && value != "0" && value != "0.0") return false;
            if (knob->isMs)
            {
                parsed = std::max(knob->minMs, std::min(knob->maxMs, parsed));
            }
            else
            {
                parsed = std::max(knob->minMs, std::min(knob->maxMs, parsed));
            }
        }

        const std::string path = NetworkingConfig::defaultPath();
        std::ifstream in(path);
        if (!in.is_open())
            return false;
        nlohmann::json root;
        try { in >> root; }
        catch (...) { return false; }
        in.close();

        if (!root.is_object())
            root = nlohmann::json::object();
        if (!root.contains(knob->block) ||
            !root[knob->block].is_object())
            root[knob->block] = nlohmann::json::object();

        nlohmann::json& block = root[knob->block];
        if (knob->isBool)
            block[key] = parsed != 0.0;
        else
            block[key] = parsed;

        std::ofstream out(path);
        if (!out.is_open())
            return false;
        out << root.dump(2) << "\n";
        out.close();

        return NetworkingConfig::instance().reloadFromDisk();
    }

    // String-valued knob path.
    {
        const std::string path = NetworkingConfig::defaultPath();
        std::ifstream in(path);
        if (!in.is_open())
            return false;
        nlohmann::json root;
        try { in >> root; }
        catch (...) { return false; }
        in.close();

        if (!root.is_object())
            root = nlohmann::json::object();
        if (!root.contains(knob->block) ||
            !root[knob->block].is_object())
            root[knob->block] = nlohmann::json::object();

        root[knob->block][key] = value;

        std::ofstream out(path);
        if (!out.is_open())
            return false;
        out << root.dump(2) << "\n";
        out.close();

        return NetworkingConfig::instance().reloadFromDisk();
    }
}

void listNetworkConfigKnobs()
{
    for (const auto& k : netConfigKnobs())
        Terminal::instance().addLog("  " + std::string(k.key) +
            (k.isMs ? " (ms)" : k.isBool ? " (0/1)" : k.isString ? " (string)" : ""));
}

} // namespace

void registerNetworkCommands()
{
    Terminal::instance().registerCommand({
        "startserver", "Start the local game server",
        "startserver",
        [](const std::vector<std::string>&) {
            MimitaNet::ListenServerState* ls = getListenServerState();
            if (ls && ls->active) {
                Terminal::instance().addLog("[SERVER] already running");
                return;
            }
            if (MimitaNet::startListenServer(*ls, MimitaNet::DEFAULT_PORT))
                Terminal::instance().addLog("[SERVER] started on port " +
                    std::to_string(MimitaNet::DEFAULT_PORT) + " code=" + ls->serverCode);
            else
                Terminal::instance().addLog("[SERVER] failed to start");
        }
    });
    Terminal::instance().registerCommand({
        "stopserver", "Stop the local game server",
        "stopserver",
        [](const std::vector<std::string>&) {
            MimitaNet::ListenServerState* ls = getListenServerState();
            if (!ls || !ls->active) {
                Terminal::instance().addLog("[SERVER] not running");
                return;
            }
            if (MP_CONTEXT.active)
                MimitaNet::mpShutdown(MP_CONTEXT);
            MimitaNet::stopListenServer(*ls);
            Terminal::instance().addLog("[SERVER] stopped");
        }
    });
    Terminal::instance().registerCommand({
        "server_help", "List server commands",
        "server_help",
        [](const std::vector<std::string>&) {
            Terminal::instance().addLog("=== SERVER COMMANDS ===");
            Terminal::instance().addLog("startserver              - Start local server");
            Terminal::instance().addLog("stopserver               - Stop local server");
            Terminal::instance().addLog("server_info              - Show server status");
            Terminal::instance().addLog("server_leave             - Host client disconnects, server continues");
            Terminal::instance().addLog("server_help              - This help");
            Terminal::instance().addLog("serverconnect <ip>       - Connect to server (legacy)");
            Terminal::instance().addLog("disconnectserver         - Disconnect from server");
            Terminal::instance().addLog("badconn <list|N|0>      - Per-client network simulation");
            Terminal::instance().addLog("net_debug_presentation [0|1] - Debug overlay");
            Terminal::instance().addLog("net_damage_debug [0|1]   - Damage pipeline debug");
            Terminal::instance().addLog("net_hit_debug [0|1]      - Hit raycast debug");
            Terminal::instance().addLog("net_compare              - Client/server state compare");
            Terminal::instance().addLog("net_entity_dump          - Full entity state dump");
            Terminal::instance().addLog("net_disagreement_debug [0|1] - Disagreement visuals debug");
        }
    });
    Terminal::instance().registerCommand({
        "server_info", "Show server status, players, uptime",
        "server_info",
        [](const std::vector<std::string>&) {
            MimitaNet::ListenServerState* ls = getListenServerState();
            ExternalServerProcessStatus external = getExternalServerProcessStatus();

            const bool listenRunning = ls && ls->active;
            const bool externalRunning = external.running;

            if (!listenRunning && !externalRunning)
            {
                Terminal::instance().addLog("[SERVER] not running");
                return;
            }

            if (listenRunning && externalRunning)
                Terminal::instance().addLog("[SERVER WARNING] external and listen servers are both active");

            char buf[256];

            if (externalRunning)
            {
                snprintf(buf, sizeof(buf), "[SERVER] type=external-headless running=1");
                Terminal::instance().addLog(buf);

                uint64_t uptimeSec = external.uptimeMs / 1000;
                snprintf(buf, sizeof(buf), "[SERVER] pid=%u code=%s port=%u uptime=%llus",
                         external.processId, external.roomCode.c_str(),
                         external.port, (unsigned long long)uptimeSec);
                Terminal::instance().addLog(buf);

                snprintf(buf, sizeof(buf), "[SERVER] name=\"%s\" map=%s",
                         external.serverName.c_str(), external.mapName.c_str());
                Terminal::instance().addLog(buf);

                Terminal::instance().addLog("[SERVER] player count is owned by the child process and unavailable locally");
            }

            if (listenRunning)
            {
                snprintf(buf, sizeof(buf), "[SERVER] type=listen running=1");
                Terminal::instance().addLog(buf);

                snprintf(buf, sizeof(buf), "[SERVER] code=%s port=%u players=%zu/%d tick=%u",
                         ls->serverCode.c_str(), ls->port, ls->players.size(),
                         MimitaNet::MAX_PLAYERS, ls->tick);
                Terminal::instance().addLog(buf);

                for (const auto& kv : ls->players) {
                    snprintf(buf, sizeof(buf), "  player id=%u name=\"%s\" hp=%d dead=%d",
                             kv.second.id, kv.second.name.c_str(), kv.second.health, (int)kv.second.dead);
                    Terminal::instance().addLog(buf);
                }
            }
        }
    });
    Terminal::instance().registerCommand({
        "server_leave", "Host client disconnects but server continues running",
        "server_leave",
        [](const std::vector<std::string>&) {
            if (MP_CONTEXT.active && MP_CONTEXT.localPlayerId) {
                MimitaNet::mpShutdown(MP_CONTEXT);
                Terminal::instance().addLog("[SERVER] host client disconnected, server continues");
            } else {
                Terminal::instance().addLog("[SERVER] not connected to any server");
            }
        }
    });
    Terminal::instance().registerCommand({
        "serverconnect", "Connect to a server IP", "serverconnect <ip> [args...]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: serverconnect <ip> [args...]");
                return;
            }
            MimitaNet::MultiplayerContext& mpContext = MP_CONTEXT;
            std::string address = args[0];
            if (MimitaNet::mpInit(mpContext, address, AuthSystem::instance().displayName())) {
                Terminal::instance().addLog("[SERVER] connecting to " + address);
            } else {
                Terminal::instance().addLog("[ERROR] failed to connect to " + address);
            }
        }
    });
    Terminal::instance().registerCommand({
        "disconnectserver", "Disconnect from server",
        "disconnectserver",
        [](const std::vector<std::string>&) {
            MimitaNet::MultiplayerContext& mpContext = MP_CONTEXT;
            if (mpContext.active) {
                MimitaNet::mpShutdown(mpContext);
                Terminal::instance().addLog("[SERVER] disconnected");
            } else {
                Terminal::instance().addLog("[SERVER] not connected");
            }
        }
    });
    Terminal::instance().registerCommand({
        "server_showghost", "Show/hide ghost of authoritative server position",
        "server_showghost [0|1]",
        [](const std::vector<std::string>& args) {
            MimitaNet::MultiplayerContext& mpContext = MP_CONTEXT;
            if (args.empty()) {
                Terminal::instance().addLog(
                    "[SERVER GHOST] " + std::string(mpContext.showServerGhost ? "enabled" : "disabled"));
                Terminal::instance().addLog("Usage: server_showghost [0|1]");
                return;
            }
            mpContext.showServerGhost = args[0] == "1";
            Terminal::instance().addLog(mpContext.showServerGhost
                ? "[SERVER GHOST] Server position ghost enabled"
                : "[SERVER GHOST] Server position ghost disabled");
        }
    });
    Terminal::instance().registerCommand({
        "net_disagreement_debug", "Toggle server disagreement debug logging",
        "net_disagreement_debug [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                MimitaNet::gDisagreementDebug = !MimitaNet::gDisagreementDebug;
            } else {
                MimitaNet::gDisagreementDebug = args[0] == "1";
            }
            Terminal::instance().addLog(MimitaNet::gDisagreementDebug
                ? "[NET] Disagreement debug ON"
                : "[NET] Disagreement debug OFF");
        }
    });

    Terminal::instance().registerCommand({
        "server_disagreement_test", "Spawn a test server disagreement event at look position",
        "server_disagreement_test [hit|damage|movement|teleport|state]",
        [](const std::vector<std::string>& args) {
            Player& player = THE_PLAYER;

            using namespace MimitaNet;

            DisagreementReason reason = DISAGREEMENT_POSITION_CORRECTION;
            const char* desc = "test correction";
            glm::vec3 correction(0.5f, 0.3f, 0.2f);

            if (!args.empty())
            {
                if (args[0] == "hit") {
                    reason = DISAGREEMENT_OCCLUDED_SHOT;
                    desc = "test occluded shot";
                    correction = glm::vec3(0.0f);
                } else if (args[0] == "damage") {
                    reason = DISAGREEMENT_INVALID_DAMAGE;
                    desc = "test invalid damage";
                    correction = glm::vec3(0.0f);
                } else if (args[0] == "movement") {
                    reason = DISAGREEMENT_INVALID_MOVEMENT;
                    desc = "test invalid movement";
                    correction = glm::vec3(1.2f, 0.5f, 0.0f);
                } else if (args[0] == "teleport") {
                    reason = DISAGREEMENT_POSITION_CORRECTION;
                    desc = "test teleport correction";
                    correction = glm::vec3(5.0f, 0.0f, 2.0f);
                } else if (args[0] == "state") {
                    reason = DISAGREEMENT_INVALID_STATE;
                    desc = "test invalid state";
                    correction = glm::vec3(0.0f);
                }
            }

            glm::vec3 front(std::cos(player.yaw), std::sin(player.yaw), 0.0f);

            DisagreementEvent event;
            event.timeMs = nowMs();
            event.reason = reason;
            event.position = player.pos + front * 3.0f;
            event.correction = correction;
            event.description = desc;
            event.lifetime = 3.0f;

            spawnDisagreementEffect(event);
            logDisagreement(event);

            Terminal::instance().addLog(
                "[NET] server_disagreement_test: reason=" + std::string(reason == DISAGREEMENT_OCCLUDED_SHOT ? "hit" :
                    reason == DISAGREEMENT_INVALID_DAMAGE ? "damage" :
                    reason == DISAGREEMENT_INVALID_MOVEMENT ? "movement" :
                    reason == DISAGREEMENT_POSITION_CORRECTION ? "teleport" : "state") +
                " pos=(" + std::to_string((int)event.position.x) + "," +
                           std::to_string((int)event.position.y) + "," +
                           std::to_string((int)event.position.z) + ")" +
                " correction=(" + std::to_string(event.correction.x) + "," +
                                  std::to_string(event.correction.y) + "," +
                                  std::to_string(event.correction.z) + ")");
        }
    });

    Terminal::instance().registerCommand({
        "net_debug_presentation", "Toggle remote player presentation debug overlay", "net_debug_presentation [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                gNetPresentationDebug = !gNetPresentationDebug;
            } else {
                gNetPresentationDebug = args[0] == "1";
            }
            printf("[NET PRESENTATION DEBUG] %s\n", gNetPresentationDebug ? "ON" : "OFF");
            Terminal::instance().addLog(gNetPresentationDebug
                ? "[NET] Presentation debug ON"
                : "[NET] Presentation debug OFF");
        }
    });
    Terminal::instance().registerCommand({
        "net_damage_debug", "Log damage pipeline: shooter/target/health/accept/reject", "net_damage_debug [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                MimitaNet::gNetDamageDebug = !MimitaNet::gNetDamageDebug;
            } else {
                MimitaNet::gNetDamageDebug = args[0] == "1";
            }
            printf("[NET DAMAGE DEBUG] %s\n", MimitaNet::gNetDamageDebug ? "ON" : "OFF");
            Terminal::instance().addLog(MimitaNet::gNetDamageDebug
                ? "[NET] Damage debug ON"
                : "[NET] Damage debug OFF");
        }
    });
    Terminal::instance().registerCommand({
        "net_hit_debug", "Log raycast hit details: origin/direction/entity/distance", "net_hit_debug [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                MimitaNet::gNetHitDebug = !MimitaNet::gNetHitDebug;
            } else {
                MimitaNet::gNetHitDebug = args[0] == "1";
            }
            printf("[NET HIT DEBUG] %s\n", MimitaNet::gNetHitDebug ? "ON" : "OFF");
            Terminal::instance().addLog(MimitaNet::gNetHitDebug
                ? "[NET] Hit debug ON"
                : "[NET] Hit debug OFF");
        }
    });
    Terminal::instance().registerCommand({
        "net_compare", "Show client/server/remote state side-by-side", "net_compare",
        [](const std::vector<std::string>&) {
            Player& player = THE_PLAYER;
            MimitaNet::MultiplayerContext& mpContext = MP_CONTEXT;
            if (!mpContext.active) {
                Terminal::instance().addLog("[NET] Not connected to server");
                return;
            }
            Terminal::instance().addLog("=== NET COMPARE ===");
            Terminal::instance().addLog("LOCAL CLIENT VIEW:");
            Terminal::instance().addLog("  id=" + std::to_string(mpContext.localPlayerId) +
                " hp=" + std::to_string(player.currentHp) +
                " dead=" + std::to_string((int)player.dead) +
                " pos=(" + std::to_string((int)player.pos.x) + "," +
                           std::to_string((int)player.pos.y) + "," +
                           std::to_string((int)player.pos.z) + ")" +
                " weapon=" + player.equippedWeaponId);
            Terminal::instance().addLog("SERVER SNAPSHOT:");
            Terminal::instance().addLog("  id=" + std::to_string(mpContext.localPlayerId) +
                " hp=" + std::to_string(mpContext.localServerHealth) +
                " pos=(" + std::to_string((int)mpContext.localServerPosition.x) + "," +
                           std::to_string((int)mpContext.localServerPosition.y) + "," +
                           std::to_string((int)mpContext.localServerPosition.z) + ")" +
                " awaitingExplode=" + std::to_string((int)mpContext.awaitingExplodeDeath) +
                " reconciled=" + std::to_string((int)mpContext.localPlayerReconciled));
            for (const auto& kv : mpContext.remotePlayers) {
                auto it = mpContext.remotePlayerInterpolation.find(kv.first);
                int snapAge = it != mpContext.remotePlayerInterpolation.end()
                    ? (int)(MimitaNet::nowMs() - it->second.target.receivedMs)
                    : -1;
                Terminal::instance().addLog("REMOTE PLAYER id=" + std::to_string(kv.first) +
                    " hp=" + std::to_string(kv.second.currentHp) +
                    " dead=" + std::to_string((int)kv.second.dead) +
                    " pos=(" + std::to_string((int)kv.second.pos.x) + "," +
                               std::to_string((int)kv.second.pos.y) + "," +
                               std::to_string((int)kv.second.pos.z) + ")" +
                    " weapon=" + kv.second.equippedWeaponId +
                    " snapAge=" + std::to_string(snapAge) + "ms");
            }
            for (const auto& kv : mpContext.remoteNpcs) {
                Terminal::instance().addLog("REMOTE NPC id=" + std::to_string(kv.first) +
                    " hp=" + std::to_string(kv.second.currentHp) +
                    " dead=" + std::to_string((int)kv.second.dead) +
                    " pos=(" + std::to_string((int)kv.second.pos.x) + "," +
                               std::to_string((int)kv.second.pos.y) + "," +
                               std::to_string((int)kv.second.pos.z) + ")");
            }
            Terminal::instance().addLog("=== END ===");
        }
    });
    Terminal::instance().registerCommand({
        "net_entity_dump", "Dump all replicated entities with full state", "net_entity_dump",
        [](const std::vector<std::string>&) {
            Player& player = THE_PLAYER;
            MimitaNet::MultiplayerContext& mpContext = MP_CONTEXT;
            if (!mpContext.active) {
                Terminal::instance().addLog("[NET] Not connected to server");
                return;
            }
            Terminal::instance().addLog("=== NET ENTITY DUMP ===");
            Terminal::instance().addLog("LOCAL id=" + std::to_string(mpContext.localPlayerId) +
                " hp=" + std::to_string(player.currentHp) +
                " maxHp=" + std::to_string(player.maxHp) +
                " dead=" + std::to_string((int)player.dead) +
                " onGround=" + std::to_string((int)player.ground.onGround) +
                " slot=" + std::to_string(player.equippedSlot) +
                " weapon=" + player.equippedWeaponId +
                " pos=(" + std::to_string(player.pos.x) + "," + std::to_string(player.pos.y) + "," + std::to_string(player.pos.z) + ")" +
                " vel=(" + std::to_string(player.vel.x) + "," + std::to_string(player.vel.y) + "," + std::to_string(player.vel.z) + ")" +
                " yaw=" + std::to_string(player.yaw) +
                " respawnTimer=" + std::to_string(player.respawnTimer));
            Terminal::instance().addLog("SERVER health=" + std::to_string(mpContext.localServerHealth) +
                " ping=" + std::to_string(mpContext.localPingMs) +
                " tick=" + std::to_string(mpContext.tick) +
                " snapshotsReceived=" + std::to_string(mpContext.snapshotsReceived) +
                " snapshotsMissed=" + std::to_string(mpContext.snapshotsMissed));
            for (const auto& kv : mpContext.remotePlayers) {
                auto it = mpContext.remotePlayerInterpolation.find(kv.first);
                std::string age = it != mpContext.remotePlayerInterpolation.end()
                    ? std::to_string(MimitaNet::nowMs() - it->second.target.receivedMs) + "ms"
                    : "none";
                Terminal::instance().addLog("PLAYER id=" + std::to_string(kv.first) +
                    " hp=" + std::to_string(kv.second.currentHp) +
                    " dead=" + std::to_string((int)kv.second.dead) +
                    " onGround=" + std::to_string((int)kv.second.ground.onGround) +
                    " slot=" + std::to_string(kv.second.equippedSlot) +
                    " weapon=" + kv.second.equippedWeaponId +
                    " pos=(" + std::to_string((int)kv.second.pos.x) + "," +
                               std::to_string((int)kv.second.pos.y) + "," +
                               std::to_string((int)kv.second.pos.z) + ")" +
                    " yaw=" + std::to_string((int)kv.second.yaw) +
                    " snapAge=" + age);
            }
            for (const auto& kv : mpContext.remoteNpcs) {
                Terminal::instance().addLog("NPC id=" + std::to_string(kv.first) +
                    " hp=" + std::to_string(kv.second.currentHp) +
                    " dead=" + std::to_string((int)kv.second.dead) +
                    " pos=(" + std::to_string((int)kv.second.pos.x) + "," +
                               std::to_string((int)kv.second.pos.y) + "," +
                               std::to_string((int)kv.second.pos.z) + ")" +
                    " yaw=" + std::to_string((int)kv.second.yaw));
            }
            Terminal::instance().addLog("=== END DUMP ===");
        }
    });
    Terminal::instance().registerCommand({
        "roomcodeshow", "Show/hide room code HUD overlay",
        "roomcodeshow 0|1",
        [](const std::vector<std::string>& args) {
            if (args.size() == 1 && args[0] == "0") {
                gRoomCodeShow = false;
                printf("[ROOM CODE] HUD hidden\n");
                Terminal::instance().addLog("[ROOM CODE] HUD hidden");
            } else if (args.size() == 1 && args[0] == "1") {
                gRoomCodeShow = true;
                printf("[ROOM CODE] HUD shown\n");
                Terminal::instance().addLog("[ROOM CODE] HUD shown");
            } else {
                printf("usage: roomcodeshow 0|1\n");
                Terminal::instance().addLog("usage: roomcodeshow 0|1");
            }
        }
    });
    Terminal::instance().registerCommand({
        "server_rejectallhits", "Show rejectAllHits flag. Edit config/serverdisagree.json to toggle (hot-reloads).",
        "server_rejectallhits",
        [](const std::vector<std::string>&) {
            int val = MimitaNet::isRejectAllHitsEnabled() ? 1 : 0;
            printf("[SERVER] rejectAllHits=%d (edit config/serverdisagree.json to change)\n", val);
            Terminal::instance().addLog(std::string("[SERVER] rejectAllHits=") + std::to_string(val) + " (edit config to change)");
        }
    });

    // ── goonline / gooffline ──────────────────────────────────────────
    Terminal::instance().registerCommand({
        "goonline", "Make local server visible to others on the coordinator",
        "goonline",
        [](const std::vector<std::string>&) {
            MimitaNet::ListenServerState* ls = getListenServerState();
            if (!ls || !ls->active) {
                Terminal::instance().addLog("[SERVER] no server running");
                return;
            }
            if (ls->serverCode.find("LOCAL-") != 0) {
                Terminal::instance().addLog("[SERVER] already online — code: " + ls->serverCode);
                return;
            }
            Terminal::instance().addLog("[SERVER] contacting coordinator...");
            if (MimitaNet::initServerIceListener(*ls)) {
                Terminal::instance().addLog("[SERVER] now online — room: " + ls->serverCode);
            } else {
                Terminal::instance().addLog("[SERVER] failed to go online — check internet connectivity");
            }
        }
    });

    Terminal::instance().registerCommand({
        "gooffline", "Hide server from coordinator (local only, invisible)",
        "gooffline",
        [](const std::vector<std::string>&) {
            MimitaNet::ListenServerState* ls = getListenServerState();
            if (!ls || !ls->active) {
                Terminal::instance().addLog("[SERVER] no server running");
                return;
            }
            if (ls->serverCode.find("LOCAL-") == 0) {
                Terminal::instance().addLog("[SERVER] already offline (local only)");
                return;
            }
            // Deregister with coordinator
            if (!ls->serverCode.empty())
                MimitaNet::coordinatorIceDone(ls->serverCode);
            // Generate new local code
            const char* chars = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
            std::string suffix;
            {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> dist(0, 31);
                for (int i = 0; i < 4; ++i)
                    suffix += chars[dist(gen)];
            }
            ls->serverCode = "LOCAL-" + suffix;
            ls->iceListenerAgent.reset();
            MimitaNet::setServerCoordinatorState("LOCAL", "");
            MimitaNet::hostedRoomSession().active = false;
            MimitaNet::hostedRoomSession().roomCode.clear();
            Terminal::instance().addLog("[SERVER] now offline (local only) — code: " + ls->serverCode);
        }
    });

    // ── Live networking config control ─────────────────────────────────
    Terminal::instance().registerCommand({
        "networkconfig", "Networking config control: reload | list | load | save | print | set | knobs | reset",
        "networkconfig <reload|list|load N|save|print|set <key> <value>|knobs|reset>",
        [](const std::vector<std::string>& args) {
            NetworkingConfig& cfg = NetworkingConfig::instance();
            const std::string sub = args.empty() ? "" : args[0];
            if (sub == "reload") {
                bool ok = cfg.reloadFromDisk();
                Terminal::instance().addLog(std::string("[NETWORK CONFIG] ") +
                    (ok ? "reloaded" : "reload FAILED (kept previous valid values)"));
            } else if (sub == "path") {
                Terminal::instance().addLog("[NETWORK CONFIG] path=" + cfg.configPath());
            } else if (sub == "list") {
                listNetworkPresets();
            } else if (sub == "load") {
                if (args.size() < 2 || !std::isdigit((unsigned char)args[1][0])) {
                    Terminal::instance().addLog("[NETWORK CONFIG] usage: networkconfig load <N> (see 'networkconfig list')");
                } else {
                    loadNetworkPreset(std::atoi(args[1].c_str()));
                }
            } else if (sub == "save") {
                if (args.size() >= 3 && args[1] == "new") {
                    const std::string safe = sanitizePresetName(args[2]);
                    if (safe.empty()) {
                        Terminal::instance().addLog("[NETWORK CONFIG] invalid preset name");
                    } else if (copyLiveConfigTo(networkPresetsDir() + safe + ".json")) {
                        Terminal::instance().addLog("[NETWORK CONFIG] saved new preset '" + safe + "'");
                    } else {
                        Terminal::instance().addLog("[NETWORK CONFIG] save FAILED");
                    }
                } else if (args.size() >= 2 && std::isdigit((unsigned char)args[1][0])) {
                    const std::vector<std::string> names = listNetworkPresetNames();
                    const int number = std::atoi(args[1].c_str());
                    if (number < 1 || (size_t)number > names.size()) {
                        Terminal::instance().addLog("[NETWORK CONFIG] preset " + std::to_string(number) + " not found");
                    } else if (copyLiveConfigTo(networkPresetsDir() + names[(size_t)number - 1] + ".json")) {
                        Terminal::instance().addLog("[NETWORK CONFIG] saved to preset " + std::to_string(number) +
                                                    " (" + names[(size_t)number - 1] + ")");
                    } else {
                        Terminal::instance().addLog("[NETWORK CONFIG] save FAILED");
                    }
                } else {
                    startInteractiveSave();
                }
            } else if (sub == "print") {
                const auto& d = cfg.data();
                char buf[256];
                snprintf(buf, sizeof(buf), "[NETWORK CONFIG] directRender=%d serverSmoothing=%d "
                         "delayMs=%.0f (override=%s) "
                         "enabled=%d buffer=%zu extrap=%d maxExtrapMs=%.0f "
                         "serverDelayMs=%.0f serverExtrapMs=%.0f serverTicks=%u",
                         (int)d.remotePlayers.directRender,
                         (int)d.remotePlayers.serverSmoothing,
                         cfg.effectiveRemoteInterpolationDelaySeconds() * 1000.0,
                         cfg.overrideInterpolationDelayMs().has_value() ? "yes" : "no",
                         (int)d.remotePlayers.enabled,
                         d.remotePlayers.maximumBufferedSnapshots,
                         (int)d.remotePlayers.allowExtrapolation,
                         d.remotePlayers.maximumExtrapolationSeconds * 1000.0,
                         d.remotePlayers.serverBroadcastDelaySeconds * 1000.0,
                         d.remotePlayers.serverBroadcastExtrapolationSeconds * 1000.0,
                         (unsigned)d.remotePlayers.serverSmoothingDurationTicks);
                Terminal::instance().addLog(buf);
            } else if (sub == "set") {
                if (args.size() < 3) {
                    Terminal::instance().addLog("[NETWORK CONFIG] usage: networkconfig set <key> <value>");
                    listNetworkConfigKnobs();
                } else {
                    const bool ok = networkConfigSetKey(args[1], args[2]);
                    if (ok)
                        Terminal::instance().addLog("[NETWORK CONFIG] set " + args[1] + " = " +
                            args[2] + " — hot-reloaded everywhere");
                    else
                        Terminal::instance().addLog("[NETWORK CONFIG] unknown key or bad value: " + args[1] +
                            " — use 'networkconfig knobs' to list editable keys");
                }
            } else if (sub == "knobs") {
                listNetworkConfigKnobs();
            } else if (sub == "reset") {
                cfg.resetToDefaults();
                Terminal::instance().addLog("[NETWORK CONFIG] reset to compiled defaults");
            } else {
                Terminal::instance().addLog("[NETWORK CONFIG] usage: networkconfig <reload|path|list|load N|save|print|set <key> <value>|knobs|reset>");
            }
        }
    });

    Terminal::instance().registerCommand({
        "netinterp", "Remote-player interpolation control: debug | delay | buffer",
        "netinterp <debug [0|1] | delay <ms> | buffer>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[NETINTERP] usage: netinterp <debug [0|1] | delay <ms> | buffer>");
                return;
            }
            const std::string sub = args[0];
            if (sub == "debug") {
                bool val = args.size() > 1 ? (args[1] == "1") : !MimitaNet::gNetInterpDebug;
                MimitaNet::gNetInterpDebug = val;
                Terminal::instance().addLog(std::string("[NETINTERP] debug=") + (val ? "1" : "0"));
            } else if (sub == "delay") {
                if (args.size() < 2) {
                    NetworkingConfig& cfg = NetworkingConfig::instance();
                    Terminal::instance().addLog("[NETINTERP] delayMs=" +
                        std::to_string((int)(cfg.effectiveRemoteInterpolationDelaySeconds() * 1000.0)));
                    return;
                }
                double ms = std::atof(args[1].c_str());
                NetworkingConfig::instance().setOverrideInterpolationDelayMs(ms);
                Terminal::instance().addLog("[NETINTERP] delay override = " +
                    std::to_string((int)ms) + "ms (in-memory; file reload clears it)");
            } else if (sub == "buffer") {
                const auto& cfg = NetworkingConfig::instance().data();
                MimitaNet::MultiplayerContext& mp = MP_CONTEXT;
                char buf[256];
                snprintf(buf, sizeof(buf), "[NETINTERP] renderTick=%.1f clockStarted=%d enabled=%d",
                         mp.interpolationRenderTick, (int)mp.interpolationClockStarted,
                         (int)cfg.remotePlayers.enabled);
                Terminal::instance().addLog(buf);
                for (const auto& kv : mp.remotePlayerInterpolation) {
                    snprintf(buf, sizeof(buf), "  id=%u buffer=%zu newestTick=%u",
                             kv.first, kv.second.buffer.size(),
                             kv.second.hasTarget ? kv.second.target.serverTick : 0u);
                    Terminal::instance().addLog(buf);
                }
            } else {
                Terminal::instance().addLog("[NETINTERP] usage: netinterp <debug [0|1] | delay <ms> | buffer>");
            }
        }
    });

    Terminal::instance().registerCommand({
        "netstats", "Per-connection + per-remote-entity network/interpolation stats",
        "netstats",
        [](const std::vector<std::string>&) {
            MimitaNet::MultiplayerContext& mp = MP_CONTEXT;
            const auto& netCfg = NetworkingConfig::instance().data();
            const uint64_t now = MimitaNet::nowMs();
            const uint64_t snapshotTotal =
                mp.snapshotsReceived + mp.snapshotsMissed;
            const float lossPct = snapshotTotal
                ? 100.0f * (float)mp.snapshotsMissed / (float)snapshotTotal
                : 0.0f;
            const uint64_t snapshotAge = mp.lastSnapshotReceivedMs
                ? (now >= mp.lastSnapshotReceivedMs
                    ? now - mp.lastSnapshotReceivedMs : 0)
                : 0;
            char buf[640];
            snprintf(buf, sizeof(buf),
                "[NETSTATS] state=%s connected=%d ping=%dms clientTick=%u "
                "serverTick=%llu snapRx=%llu snapMissed=%llu loss=%.1f%% age=%llums",
                MimitaNet::connectionStateName(mp.connectionState),
                (int)mp.connected, mp.localPingMs, mp.tick,
                (unsigned long long)mp.latestServerTick,
                (unsigned long long)mp.snapshotsReceived,
                (unsigned long long)mp.snapshotsMissed, lossPct,
                (unsigned long long)snapshotAge);
            Terminal::instance().addLog(buf);

            snprintf(buf, sizeof(buf),
                "[NETSTATS] renderClock=%.2f ticks delay=%.1fms mode=%s "
                "frame=%llu clockStepMs=%.1f reanchors=%u lastReanchorMs=%.1f reason=%s",
                mp.interpolationRenderTick,
                NetworkingConfig::instance()
                    .effectiveRemoteInterpolationDelaySeconds() * 1000.0,
                netCfg.remotePlayers.directRender ? "direct" : "interp",
                (unsigned long long)mp.interpolationFrameNumber,
                mp.lastInterpolationClockStepMs,
                (unsigned)mp.interpolationReanchorCount,
                mp.lastInterpolationReanchorMagnitudeMs,
                mp.lastInterpolationReanchorReason.empty()
                    ? "none" : mp.lastInterpolationReanchorReason.c_str());
            Terminal::instance().addLog(buf);

            {
                const auto& m = netCfg.remoteMotionSmoothing;
                snprintf(buf, sizeof(buf),
                    "[NETSTATS] filter=%s freqHz=%.1f zeta=%.2f ff=%.2f "
                    "ffSmooth=%.2f zMult=%.2f maxSpd=%.0f maxStep=%.0f clampZ=%d "
                    "linTicks=%u linMinMax=%u/%u linHold=%d catchup=%.2f "
                    "clock=%s reanchorMs=%.0f maxJumpMs=%.1f sFreq=%.1f sFF=%.2f sDead=%.3f",
                    m.renderFilter.c_str(), m.hybridFrequencyHz,
                    m.hybridDampingRatio, m.hybridFeedForward,
                    m.hybridFeedForwardSmoothing, m.hybridFrequencyZMultiplier,
                    m.hybridMaxSpeedUnitsPerSecond,
                    m.filterMaxStepUnitsPerSecond,
                    (int)m.filterClampZBelowTarget,
                    (unsigned)m.linearDelayTicks,
                    (unsigned)m.linearMinDelayTicks,
                    (unsigned)m.linearMaxDelayTicks,
                    (int)m.linearHoldOnDry,
                    m.linearCatchupRateTicksPerSecond,
                    m.linearClockSource.c_str(),
                    m.linearReanchorOnlyIfErrorSeconds * 1000.0,
                    m.maxRenderTimeJumpSeconds * 1000.0,
                    m.springFrequencyHz, m.springFeedForward,
                    m.springLinearDeadzoneUnits);
                Terminal::instance().addLog(buf);
            }

            {
                float agreePct = mp.predictedHits
                    ? 100.0f * (float)mp.confirmedHits / (float)mp.predictedHits
                    : 0.0f;
                snprintf(buf, sizeof(buf),
                    "[NETSTATS] HITS pred=%llu confirmed=%llu rejected=%llu agree=%.1f%%",
                    (unsigned long long)mp.predictedHits,
                    (unsigned long long)mp.confirmedHits,
                    (unsigned long long)mp.rejectedHits, agreePct);
                Terminal::instance().addLog(buf);
            }

            if (mp.hasLocalServerPosition && gpPlayer)
            {
                snprintf(buf, sizeof(buf),
                    "[NETSTATS] simDivergence=%.2fm (predicted vs server_sim)",
                    glm::length(gpPlayer->pos - mp.localServerPosition));
                Terminal::instance().addLog(buf);
            }

            for (const auto& kv : mp.remotePlayerInterpolation)
            {
                const auto& s = kv.second;
                if (!s.hasTarget)
                    continue;
                float dTarget = 0.0f;
                auto replicaIt = mp.remotePlayers.find(kv.first);
                if (replicaIt != mp.remotePlayers.end())
                    dTarget = glm::length(replicaIt->second.pos - s.target.position);
                snprintf(buf, sizeof(buf),
                    "  id=%u buf=%zu newest=%u render=%.2f dRender=%.2f older=%u newer=%u "
                    "alpha=%.3f dAlpha=%.3f delay=%.1fms jit=%.1fms hold=%u underrun=%u "
                    "jump=%u hardSnap=%u predDmg=%d hpCap=%d dRenderToTarget=%.2fm",
                    kv.first, s.buffer.size(), s.target.serverTick, s.renderTick,
                    s.renderTickDelta, s.sampleOlderTick, s.sampleNewerTick,
                    s.sampleAlpha, s.sampleAlphaDelta,
                    s.adaptiveDelaySeconds * 1000.0, s.estimatedArrivalJitterMs,
                    s.holdCount, s.bufferUnderrunCount,
                    s.renderJumpCount, s.hardSnapCount,
                    s.pendingPredictedDamage, s.predictedHealthCap,
                    dTarget);
                Terminal::instance().addLog(buf);
            }
        }
    });
}
