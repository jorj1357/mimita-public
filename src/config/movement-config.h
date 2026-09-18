// 08 02 2026, 00 00
/* purpose
* Declares the movement tuning preset loader (config/movement.json selector + config/movement/*.json presets).
* COMPARISON-ONLY: runtime movement tuning is owned by the hot C++ preset
* registry (hot-reload/hot-movement-presets.h: source, default, heavy,
* retrograd_fast, counterstrike). This JSON loader is retained only for the
* JSON-versus-C++ deterministic comparison harness; it is NOT consulted by any
* movement, collision, NPC, prediction, replay, validation, or terminal path.
* Does NOT run movement simulation, poll input, or own the movement kernel formulas.
* Does NOT serialize packets, render, play audio, or decide network authority.
* Does NOT replace physics/config.h as the source of built-in default constants.
*/

#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "physics/movement/movement-types.h"

// Built-in C++ movement defaults (no JSON). This is the base the single C++
// Source movement authority tunes on top of. Preset JSON is reference/archive
// material only and is never a runtime movement authority.
MovementConfig movementRuntimeDefaults();

class MovementJsonConfig {
public:
    static MovementJsonConfig& instance();

    // Reads the selector file and loads the preset it names. Default: "config/movement.json".
    bool load(const std::string& path = "config/movement.json");
    // Loads a preset by name without touching the selector file.
    bool loadPresetByName(const std::string& preset);
    // Parses a preset by name into `out` without changing the global active
    // config or the selector file. On success, `outPath` (optional) receives the
    // resolved preset file path so callers can hot-reload it. Returns false and
    // leaves `out` untouched when the preset is unknown or unparseable.
    bool loadPresetInto(const std::string& preset, MovementConfig& out,
                        std::string* outPath = nullptr);
    // Writes the selector file to select a preset, then loads it. Returns false on failure.
    bool savePresetSelection(const std::string& preset);
    // Hot-reloads when the selector file or the active preset file changes on disk.
    bool pollReload();

    const MovementConfig& config() const { return mConfig; }
    const std::string& activePresetName() const { return mActivePreset; }
    const std::string& activePresetPath() const { return mActivePresetPath; }
    std::string selectorPath() const { return mSelectorPath; }
    std::vector<std::string> availablePresets() const;

    // Runtime-only toggle for the bhop debug overlay. Not persisted; a config
    // reload resets it to the preset's debug_draw_enabled value.
    void setDebugDrawEnabled(bool enabled) { mConfig.debugDrawEnabled = enabled; }

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
