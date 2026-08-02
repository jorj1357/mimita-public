// 08 02 2026, 00 00
/* purpose
* Declares the movement tuning preset loader (config/movement.json selector + config/movement/*.json presets).
* Exposes the active MovementConfig so the shared movement kernel reads every tuning constant from one place.
* Resolves a preset by its "name" field, falls back to the matching filename, then to built-in defaults.
* Does NOT run movement simulation, poll input, or own the movement kernel formulas.
* Does NOT serialize packets, render, play audio, or decide network authority.
* Does NOT replace physics/config.h as the source of built-in default constants.
*/

#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "physics/movement/movement-types.h"

class MovementJsonConfig {
public:
    static MovementJsonConfig& instance();

    // Reads the selector file and loads the preset it names. Default: "config/movement.json".
    bool load(const std::string& path = "config/movement.json");
    // Loads a preset by name without touching the selector file.
    bool loadPresetByName(const std::string& preset);
    // Writes the selector file to select a preset, then loads it. Returns false on failure.
    bool savePresetSelection(const std::string& preset);
    // Hot-reloads when the selector file or the active preset file changes on disk.
    bool pollReload();

    const MovementConfig& config() const { return mConfig; }
    const std::string& activePresetName() const { return mActivePreset; }
    const std::string& activePresetPath() const { return mActivePresetPath; }
    std::string selectorPath() const { return mSelectorPath; }
    std::vector<std::string> availablePresets() const;

private:
    MovementJsonConfig();

    std::string resolvePresetPath(const std::string& preset) const;
    bool loadPresetFile(const std::string& path, const std::string& preset);

    MovementConfig mConfig;
    std::string mActivePreset = "default";
    std::string mSelectorPath = "config/movement.json";
    std::string mPresetDir = "config/movement";
    std::string mActivePresetPath;
    std::filesystem::file_time_type mSelectorWrite{};
    std::filesystem::file_time_type mPresetWrite{};
    bool mWatchLogged = false;
};
