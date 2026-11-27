// 08 15 2026, 12 00
/* purpose
* Registers terminal commands to control the body-aim animation live: toggle it,
* set a limb's pitch/yaw/roll gain per axis, reload config/aimbody.json, and
* dump the active config plus whether the animation applied it last frame.
* Writes straight into AimBodyConfig so changes apply immediately and hot-reload
* still works from the JSON file.
* Does NOT own the animation math, the pose pipeline, or firing logic.
*/

#include "entities/aim-commands.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "devtools/terminal.h"
#include "entities/aimbody-config.h"
#include "entities/player-avatar-animations.h"
#include "debug/structured-log.h"
#include "terminal/terminal-state.h"

namespace {

void dumpAimbody()
{
    const AimBodyConfig& ab = AimBodyConfig::instance();
    std::string s = "[AIMBODY] enabled=" + std::string(ab.enabled() ? "1" : "0") +
        " applied_last_frame=" + std::string(ab.appliedLastFrame() ? "1" : "0") +
        " look_pitch=" + std::to_string(ab.lastLookPitch());
    Terminal::instance().addLog(s);
    for (const auto& [name, limb] : ab.limbs())
        Terminal::instance().addLog(
            "  " + name + "  pitch=" + std::to_string(limb.pitch) +
            "  yaw=" + std::to_string(limb.yaw) +
            "  roll=" + std::to_string(limb.roll));
}

} // namespace

void registerAimCommands()
{
    Terminal::instance().registerCommand({
        "body_aim", "Toggle limbs following the camera look pitch",
        "body_aim <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                dumpAimbody();
                return;
            }
            AimBodyConfig::instance().setEnabled(args[0] != "0");
            dumpAimbody();
        }
    }, "2026-08-15", CommandCategory::Player);

    Terminal::instance().registerCommand({
        "aim_pitch", "Set a limb's pitch gain (degrees of tilt per degree of look)",
        "aim_pitch <head|torso|leftArm|rightArm|leftLeg|rightLeg> <0-2>",
        [](const std::vector<std::string>& args) {
            if (args.size() != 2) {
                Terminal::instance().addLog("[ERROR] Usage: aim_pitch <limb> <0-2>");
                return;
            }
            const float v = std::clamp(std::strtof(args[1].c_str(), nullptr), 0.0f, 2.0f);
            AimBodyConfig::instance().setLimbPitch(args[0], v);
            dumpAimbody();
        }
    }, "2026-08-15", CommandCategory::Player);

    Terminal::instance().registerCommand({
        "aim_roll", "Set a limb's roll gain (degrees of sideways tilt per degree of look)",
        "aim_roll <head|torso|leftArm|rightArm|leftLeg|rightLeg> <value>",
        [](const std::vector<std::string>& args) {
            if (args.size() != 2) {
                Terminal::instance().addLog("[ERROR] Usage: aim_roll <limb> <value>");
                return;
            }
            const float v = std::clamp(std::strtof(args[1].c_str(), nullptr), -2.0f, 2.0f);
            AimBodyConfig::instance().setLimbRoll(args[0], v);
            dumpAimbody();
        }
    }, "2026-08-15", CommandCategory::Player);

    Terminal::instance().registerCommand({
        "aim_yaw", "Set a limb's yaw gain (degrees of turn per degree of look)",
        "aim_yaw <head|torso|leftArm|rightArm|leftLeg|rightLeg> <value>",
        [](const std::vector<std::string>& args) {
            if (args.size() != 2) {
                Terminal::instance().addLog("[ERROR] Usage: aim_yaw <limb> <value>");
                return;
            }
            const float v = std::clamp(std::strtof(args[1].c_str(), nullptr), -2.0f, 2.0f);
            AimBodyConfig::instance().setLimbYaw(args[0], v);
            dumpAimbody();
        }
    }, "2026-08-15", CommandCategory::Player);

    Terminal::instance().registerCommand({
        "aimbody_dump", "Show active aimbody config and whether it applied last frame",
        "aimbody_dump",
        [](const std::vector<std::string>&) { dumpAimbody(); }
    }, "2026-08-15", CommandCategory::Player);

    Terminal::instance().registerCommand({
        "aimbody_reload", "Reload config/aimbody.json",
        "aimbody_reload",
        [](const std::vector<std::string>&) {
            Terminal::instance().addLog(AimBodyConfig::instance().reload()
                ? "[AIMBODY] reloaded" : "[ERROR] aimbody reload failed");
            dumpAimbody();
        }
    }, "2026-08-15", CommandCategory::Player);

    Terminal::instance().registerCommand({
        "anim_root_mode",
        "Set the procedural limb translation mode: 1=as authored, 2=soft tether",
        "anim_root_mode <1|2|config>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog(
                    "[ANIM] root_translation_mode=" +
                    std::to_string(animationRootTranslationMode()));
                return;
            }
            int mode = 0;
            const bool useConfig = args[0] == "config";
            if (useConfig) {
                setAnimationRootTranslationModeOverride(0);
                mode = animationRootTranslationMode();
            } else {
                try { mode = std::stoi(args[0]); } catch (...) { mode = 0; }
                if (mode != 1 && mode != 2) {
                    Terminal::instance().addLog(
                        "[ANIM] usage: anim_root_mode <1|2|config>");
                    return;
                }
                setAnimationRootTranslationModeOverride(mode);
            }
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Avatar, StructuredLevel::Important,
                "anim.root_mode.changed", "animation-tether",
                "procedural root translation mode changed", 0,
                nlohmann::json{
                    {"mode", animationRootTranslationMode()},
                    {"override", !useConfig}});
            Terminal::instance().addLog(
                "[ANIM] root_translation_mode=" +
                std::to_string(animationRootTranslationMode()));
        }
    }, "2026-11-26", CommandCategory::Player);

    Terminal::instance().registerCommand({
        "anim_tether",
        "Set one limb's tether budget in metres, or clear it",
        "anim_tether <torso|head|leftArm|rightArm|leftLeg|rightLeg> <metres|clear>",
        [](const std::vector<std::string>& args) {
            if (args.size() != 2) {
                Terminal::instance().addLog(
                    "[ANIM] usage: anim_tether <part> <metres|clear>");
                return;
            }
            const bool clear = args[1] == "clear";
            float meters = -1.0f;
            if (!clear) {
                meters = std::strtof(args[1].c_str(), nullptr);
                if (!std::isfinite(meters) || meters < 0.0f) {
                    Terminal::instance().addLog(
                        "[ANIM] budget must be a finite value >= 0");
                    return;
                }
            }
            setAnimationTetherBudgetOverride(args[0], meters);
            const float applied = animationTetherBudget(args[0]);
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Avatar, StructuredLevel::Important,
                "anim.tether.budget", "animation-tether",
                "limb tether budget changed", 0,
                nlohmann::json{
                    {"part", args[0]}, {"budget", applied},
                    {"cleared", clear}, {"mode",
                        animationRootTranslationMode()}});
            Terminal::instance().addLog(
                "[ANIM] tether " + args[0] + "=" + std::to_string(applied));
        }
    }, "2026-11-26", CommandCategory::Player);

    Terminal::instance().registerCommand({
        "anim_tether_probe",
        "Emit tether settings and start per-second tether evidence for the local player",
        "anim_tether_probe",
        [](const std::vector<std::string>&) {
            const std::string actor = THE_PLAYER.username;
            setAnimationTetherDiagnosticActor(actor);
            const auto& s = animationRuntimeSettings();
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Avatar, StructuredLevel::Important,
                "anim.tether.settings", "animation-tether",
                "tether proof started for local actor", 0,
                nlohmann::json{
                    {"actor", actor},
                    {"mode", animationRootTranslationMode()},
                    {"torso", animationTetherBudget("torso")},
                    {"head", animationTetherBudget("head")},
                    {"leftArm", animationTetherBudget("leftArm")},
                    {"rightArm", animationTetherBudget("rightArm")},
                    {"leftLeg", animationTetherBudget("leftLeg")},
                    {"rightLeg", animationTetherBudget("rightLeg")},
                    {"soft_saturation", "tanh"},
                    {"json_mode", s.rootTranslationMode}});
            Terminal::instance().addLog(
                "[ANIM] tether probe actor=" + actor +
                " mode=" + std::to_string(animationRootTranslationMode()));
        }
    }, "2026-11-26", CommandCategory::Player);

    // ── Body rig editor (2026-11-27) ───────────────────────────────
    // Socket = point, torso→part line = parent link, transform axes + quad =
    // orientation/face. Live preview applies to the local player immediately;
    // rig_editor_save persists per avatar in config/rig-sockets.json.
    Terminal::instance().registerCommand({
        "rig_editor",
        "Toggle the live body-socket rig editor (point/line/face overlay)",
        "rig_editor [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                playerRigEditorToggle(THE_PLAYER);
            } else {
                const bool want = args[0] != "0";
                if (want != playerRigEditorActive())
                    playerRigEditorToggle(THE_PLAYER);
            }
            playerRigEditorPrint(THE_PLAYER);
        }
    }, "2026-11-27", CommandCategory::Player);

    Terminal::instance().registerCommand({
        "rig_editor_select",
        "Select a body socket for editing",
        "rig_editor_select <head|leftArm|rightArm|leftLeg|rightLeg>",
        [](const std::vector<std::string>& args) {
            if (args.size() != 1) {
                Terminal::instance().addLog(
                    "[RIG] usage: rig_editor_select <head|leftArm|rightArm|leftLeg|rightLeg>");
                return;
            }
            playerRigEditorSelect(THE_PLAYER, args[0]);
        }
    }, "2026-11-27", CommandCategory::Player);

    Terminal::instance().registerCommand({
        "rig_editor_move",
        "Move the selected socket in torso space (metres)",
        "rig_editor_move <dx> <dy> <dz>",
        [](const std::vector<std::string>& args) {
            if (args.size() != 3) {
                Terminal::instance().addLog("[RIG] usage: rig_editor_move <dx> <dy> <dz>");
                return;
            }
            playerRigEditorNudge(THE_PLAYER,
                glm::vec3(std::strtof(args[0].c_str(), nullptr),
                          std::strtof(args[1].c_str(), nullptr),
                          std::strtof(args[2].c_str(), nullptr)));
            playerRigEditorPrint(THE_PLAYER);
        }
    }, "2026-11-27", CommandCategory::Player);

    Terminal::instance().registerCommand({
        "rig_editor_rot",
        "Rotate the selected socket about its point (degrees, XYZ)",
        "rig_editor_rot <rx> <ry> <rz>",
        [](const std::vector<std::string>& args) {
            if (args.size() != 3) {
                Terminal::instance().addLog("[RIG] usage: rig_editor_rot <rx> <ry> <rz>");
                return;
            }
            playerRigEditorRotate(THE_PLAYER,
                glm::vec3(std::strtof(args[0].c_str(), nullptr),
                          std::strtof(args[1].c_str(), nullptr),
                          std::strtof(args[2].c_str(), nullptr)));
            playerRigEditorPrint(THE_PLAYER);
        }
    }, "2026-11-27", CommandCategory::Player);

    Terminal::instance().registerCommand({
        "rig_editor_save",
        "Save the current body sockets for this avatar/model",
        "rig_editor_save",
        [](const std::vector<std::string>&) {
            playerRigEditorSave(THE_PLAYER);
        }
    }, "2026-11-27", CommandCategory::Player);

    Terminal::instance().registerCommand({
        "rig_editor_reset",
        "Restore the selected-model sockets (discard live edits)",
        "rig_editor_reset",
        [](const std::vector<std::string>&) {
            playerRigEditorReset(THE_PLAYER);
            playerRigEditorPrint(THE_PLAYER);
        }
    }, "2026-11-27", CommandCategory::Player);

    Terminal::instance().registerCommand({
        "rig_editor_print",
        "Print the active body socket table",
        "rig_editor_print",
        [](const std::vector<std::string>&) {
            playerRigEditorPrint(THE_PLAYER);
        }
    }, "2026-11-27", CommandCategory::Player);
}
