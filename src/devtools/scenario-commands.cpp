#include "devtools/scenario-commands.h"

#include <cmath>
#include <iomanip>
#include <sstream>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "camera.h"
#include "combat/weapon-melee-config.h"
#include "debug/structured-log.h"
#include "entities/player.h"
#include "devtools/scenario-player.h"
#include "devtools/scenario-recorder.h"
#include "devtools/terminal.h"
#include "input/scenario-input.h"
#include "terminal/terminal-state.h"

namespace {

std::string fixed3(float value)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(3) << value;
    return out.str();
}

std::string vec3CommandArgs(const glm::vec3& value)
{
    return fixed3(value.x) + " " + fixed3(value.y) + " " + fixed3(value.z);
}

void emitRejected(const std::string& path, const std::string& reason)
{
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "scenario.playback.rejected", "scenario-commands",
        "input playback rejected", 0,
        nlohmann::json{{"path", path}, {"reason", reason}},
        __FILE__, __LINE__, __FUNCTION__);
    Terminal::instance().addLog("[SCENARIO] playback rejected: " + reason);
}

} // namespace

void RegisterScenarioCommands()
{
    Terminal::instance().registerCommand({
        "scenario_startrecord",
        "Start recording per-tick input into devscripts/scenarios/user",
        "scenario_startrecord [name]",
        [](const std::vector<std::string>& args) {
            const std::string name = args.empty() ? "" : args[0];
            std::string error;
            if (!ScenarioRecorder::instance().start(
                    name, ACTIVE_MAP_PATH, ACTIVE_GAME_MODE,
                    MP_CONTEXT.communityWeaponSetId, THE_PLAYER, THE_CAMERA,
                    &error))
            {
                Terminal::instance().addLog(
                    "[SCENARIO] record start failed: " + error);
                return;
            }
            Terminal::instance().addLog(
                "[SCENARIO] recording -> " +
                ScenarioRecorder::instance().path());
            if (Terminal::instance().isOpen())
                Terminal::instance().toggle();
        },
        "2026-10-25", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "scenario_endrecord",
        "Stop the active per-tick input recording",
        "scenario_endrecord",
        [](const std::vector<std::string>&) {
            if (!ScenarioRecorder::instance().active())
            {
                Terminal::instance().addLog(
                    "[SCENARIO] no active recording");
                return;
            }
            ScenarioRecorder::instance().stop("command");
            Terminal::instance().addLog(
                "[SCENARIO] recording stopped: " +
                ScenarioRecorder::instance().path());
        },
        "2026-10-25", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "scenario_status",
        "Show scenario recording/playback status",
        "scenario_status",
        [](const std::vector<std::string>&) {
            std::ostringstream out;
            out << "[SCENARIO] recording="
                << (ScenarioRecorder::instance().active() ? 1 : 0)
                << " frames=" << ScenarioRecorder::instance().frames()
                << " networked=" << (ScenarioRecorder::instance().networked() ? 1 : 0)
                << " merged=" << (ScenarioRecorder::instance().merged() ? 1 : 0)
                << " merge_error=" << ScenarioRecorder::instance().mergeError()
                << " path=" << ScenarioRecorder::instance().path()
                << " playback="
                << (ScenarioPlayer::instance().active() ? 1 : 0)
                << " playback_frame=" << ScenarioPlayer::instance().frame()
                << "/" << ScenarioPlayer::instance().frameCount()
                << " playback_schema="
                << (ScenarioPlayer::instance().schema2() ? 2 : 1)
                << " playback_path=" << ScenarioPlayer::instance().path();
            Terminal::instance().addLog(out.str());
        },
        "2026-10-25", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "scenario_play",
        "Play a recorded per-tick input scenario",
        "scenario_play <path.jsonl>",
        [](const std::vector<std::string>& args) {
            if (args.empty())
            {
                Terminal::instance().addLog(
                    "[SCENARIO] usage: scenario_play <path.jsonl>");
                return;
            }
            if (ScenarioRecorder::instance().active())
            {
                emitRejected(args[0], "recording_active");
                return;
            }
            if (ScenarioPlayer::instance().active())
            {
                emitRejected(args[0], "playback_already_active");
                return;
            }

            std::string error;
            if (!ScenarioPlayer::instance().load(args[0], &error))
            {
                emitRejected(args[0], error);
                return;
            }

            const std::string& recordedMap = ScenarioPlayer::instance().map();
            const std::string& recordedMode = ScenarioPlayer::instance().mode();
            if (!recordedMap.empty() && !ACTIVE_MAP_PATH.empty() &&
                recordedMap != ACTIVE_MAP_PATH)
            {
                emitRejected(args[0], "map_mismatch");
                return;
            }
            if (!recordedMode.empty() && !ACTIVE_GAME_MODE.empty() &&
                recordedMode != ACTIVE_GAME_MODE)
            {
                emitRejected(args[0], "mode_mismatch");
                return;
            }
            const int recordedSet = ScenarioPlayer::instance().weaponSet();
            if (recordedSet > 0 && MP_CONTEXT.communityWeaponSetId > 0 &&
                recordedSet != MP_CONTEXT.communityWeaponSetId)
            {
                emitRejected(args[0], "weapon_set_mismatch");
                return;
            }
            if (ScenarioPlayer::instance().schema2())
            {
                if (!MP_CONTEXT.active)
                {
                    emitRejected(args[0], "server_required_for_schema_2");
                    return;
                }
                for (const auto& revision : ScenarioPlayer::instance().revisions())
                {
                    const WeaponData::MeleeMoveSet* current =
                        WeaponData::MeleeMoveSetRegistry::instance().get(revision.weaponId);
                    if (!current)
                    {
                        emitRejected(args[0],
                            "melee_moveset_missing:" + revision.weaponId);
                        return;
                    }
                    if (revision.hash != 0 && current->hash != revision.hash)
                    {
                        emitRejected(args[0],
                            "melee_moveset_hash_mismatch:" + revision.weaponId);
                        return;
                    }
                    if (revision.revision != 0 &&
                        current->revision != revision.revision)
                    {
                        emitRejected(args[0],
                            "melee_moveset_revision_mismatch:" + revision.weaponId);
                        return;
                    }
                }
            }

            const auto& start = ScenarioPlayer::instance().startState();
            Terminal::instance().execute("teleport " + vec3CommandArgs(start.position));
            glm::vec3 forward = start.camForward;
            if (glm::length(forward) < 0.001f)
                forward = glm::vec3(0.0f, 1.0f, 0.0f);
            const glm::vec3 target = start.position + forward * 100.0f;
            Terminal::instance().execute("agent.aim " + vec3CommandArgs(target));
            Terminal::instance().execute(
                start.weapon.empty() ? "equipweapon nothing"
                                     : "equipweapon " + start.weapon);

            if (!ScenarioPlayer::instance().begin())
            {
                emitRejected(args[0], "playback_begin_failed");
                return;
            }
            Terminal::instance().addLog(
                "[SCENARIO] playback -> " + ScenarioPlayer::instance().path() +
                " frames=" +
                std::to_string(ScenarioPlayer::instance().frameCount()));
            if (Terminal::instance().isOpen())
                Terminal::instance().toggle();
        },
        "2026-10-25", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "scenario_stop",
        "Stop scenario playback",
        "scenario_stop",
        [](const std::vector<std::string>&) {
            if (!ScenarioPlayer::instance().active())
            {
                Terminal::instance().addLog(
                    "[SCENARIO] no active playback");
                return;
            }
            ScenarioPlayer::instance().stop("command");
            Terminal::instance().addLog("[SCENARIO] playback stopped");
        },
        "2026-10-25", CommandCategory::Debug
    });
}
