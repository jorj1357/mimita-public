#include "crosshair-commands.h"

#include <algorithm>
#include <cstdlib>

#include "crosshair-config.h"
#include "devtools/terminal.h"

static void saved(const char* name)
{
    CrosshairConfig::instance().save();
    Terminal::instance().addLog(std::string("[CROSSHAIR] updated ") + name);
}

void registerCrosshairCommands()
{
    auto number = [](const char* name, const char* usage, float minValue,
                     float maxValue, float CrosshairSettings::*field) {
        Terminal::instance().registerCommand({
            name, std::string("Set crosshair ") + name, usage,
            [=](const std::vector<std::string>& args) {
                if (args.empty()) {
                    Terminal::instance().addLog(std::string("[ERROR] Usage: ") + usage);
                    return;
                }
                auto& d = CrosshairConfig::instance().edit();
                d.*field = std::clamp(std::strtof(args[0].c_str(), nullptr),
                                      minValue, maxValue);
                saved(name);
            }
        }, "2026-06-14", CommandCategory::UI);
    };
    auto toggle = [](const char* name, const char* usage,
                     bool CrosshairSettings::*field) {
        Terminal::instance().registerCommand({
            name, std::string("Toggle crosshair ") + name, usage,
            [=](const std::vector<std::string>& args) {
                if (args.empty()) {
                    Terminal::instance().addLog(std::string("[ERROR] Usage: ") + usage);
                    return;
                }
                CrosshairConfig::instance().edit().*field = args[0] != "0";
                saved(name);
            }
        }, "2026-06-14", CommandCategory::UI);
    };

    number("crosshair_size", "crosshair_size <0-64>", 0.0f, 64.0f, &CrosshairSettings::size);
    number("crosshair_gap", "crosshair_gap <0-64>", 0.0f, 64.0f, &CrosshairSettings::gap);
    number("crosshair_thickness", "crosshair_thickness <1-16>", 1.0f, 16.0f, &CrosshairSettings::thickness);
    toggle("crosshair_dot", "crosshair_dot <0|1>", &CrosshairSettings::dot);
    toggle("crosshair_laser", "crosshair_laser <0|1>", &CrosshairSettings::laserSight);
    toggle("crosshair_outline", "crosshair_outline <0|1>", &CrosshairSettings::outline);
    toggle("crosshair_dynamic", "crosshair_dynamic <0|1>", &CrosshairSettings::dynamic);

    Terminal::instance().registerCommand({
        "crosshair_color", "Set crosshair RGB color", "crosshair_color <r> <g> <b>",
        [](const std::vector<std::string>& args) {
            if (args.size() != 3) {
                Terminal::instance().addLog("[ERROR] Usage: crosshair_color <r> <g> <b>");
                return;
            }
            auto& d = CrosshairConfig::instance().edit();
            d.red = std::clamp(std::atoi(args[0].c_str()), 0, 255);
            d.green = std::clamp(std::atoi(args[1].c_str()), 0, 255);
            d.blue = std::clamp(std::atoi(args[2].c_str()), 0, 255);
            saved("color");
        }
    }, "2026-06-14", CommandCategory::UI);
    Terminal::instance().registerCommand({
        "crosshair_alpha", "Set crosshair alpha", "crosshair_alpha <0-255>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: crosshair_alpha <0-255>");
                return;
            }
            int newAlpha = std::clamp(std::atoi(args[0].c_str()), 0, 255);
            CrosshairConfig::instance().edit().alpha = newAlpha;
            saved("alpha");
        }
    }, "2026-06-14", CommandCategory::UI);
    Terminal::instance().registerCommand({
        "crosshair_save", "Save crosshair settings", "crosshair_save",
        [](const std::vector<std::string>&) {
            Terminal::instance().addLog(CrosshairConfig::instance().save()
                ? "[CROSSHAIR] saved" : "[ERROR] crosshair save failed");
        }
    }, "2026-06-14", CommandCategory::UI);
    Terminal::instance().registerCommand({
        "crosshair_reload", "Reload crosshair settings", "crosshair_reload",
        [](const std::vector<std::string>&) {
            Terminal::instance().addLog(CrosshairConfig::instance().reload()
                ? "[CROSSHAIR] reloaded" : "[ERROR] crosshair reload failed");
        }
    }, "2026-06-14", CommandCategory::UI);
    Terminal::instance().registerCommand({
        "crosshair_reset", "Reset crosshair settings", "crosshair_reset",
        [](const std::vector<std::string>&) {
            CrosshairConfig::instance().reset();
            Terminal::instance().addLog("[CROSSHAIR] reset");
        }
    }, "2026-06-14", CommandCategory::UI);
}
