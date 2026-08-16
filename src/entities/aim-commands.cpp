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
#include <cstdlib>

#include "devtools/terminal.h"
#include "entities/aimbody-config.h"

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
}
