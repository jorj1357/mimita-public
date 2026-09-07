// 2026-09-07 16:20 EST
/* purpose
* parse and validate config/playervisuals.json
* atomically retain the last valid player-visual presentation state
* report reload failures without interrupting gameplay
* does NOT render outlines or inspect player entities
* does NOT apply competitive restrictions
* does NOT mutate simulation state
*/
#include "config/player-visuals-config.h"
#include "debug/debug-log.h"
#include <fstream>
#include <cmath>
#include <nlohmann/json.hpp>
#include <sstream>

namespace {
using json = nlohmann::json;
std::filesystem::file_time_type writeTime(const std::filesystem::path& p) {
    std::error_code ec;
    return std::filesystem::last_write_time(p, ec);
}
bool finite(float v) { return std::isfinite(v); }
bool readSettings(const json& root, const char* name, PlayerOutlineSettings& s, std::string& err) {
    if (!root.contains(name)) return true;
    if (!root[name].is_object()) { err = std::string(name) + " must be an object"; return false; }
    const json& o = root[name];
    if (!o.contains("enabled") && !o.contains("thickness") && !o.contains("alpha") &&
        !o.contains("color") && !o.contains("visibleThroughWalls") &&
        !o.contains("disappearOnDeath") && !o.contains("renderOrder")) return true;
    if (o.contains("enabled")) s.enabled = o.at("enabled").get<bool>();
    if (o.contains("thickness")) s.thickness = o.at("thickness").get<float>();
    if (o.contains("alpha")) s.alpha = o.at("alpha").get<float>();
    if (o.contains("color")) {
        const auto& c = o.at("color");
        if (!c.is_array() || c.size() < 3) { err = std::string(name) + ".outline.color must have 3 values"; return false; }
        s.color = {c[0].get<float>(), c[1].get<float>(), c[2].get<float>()};
    }
    if (o.contains("visibleThroughWalls")) s.visibleThroughWalls = o.at("visibleThroughWalls").get<bool>();
    if (o.contains("disappearOnDeath")) s.disappearOnDeath = o.at("disappearOnDeath").get<bool>();
    if (o.contains("renderOrder")) s.renderOrder = o.at("renderOrder").get<int>();
    if (!finite(s.thickness) || !finite(s.alpha) || s.thickness < 0.0f) { err = std::string(name) + " outline numeric value is invalid"; return false; }
    for (int i = 0; i < 3; ++i) if (!finite(s.color[i])) { err = std::string(name) + ".outline.color contains a non-finite value"; return false; }
    return true;
}
}

PlayerVisualsConfig& PlayerVisualsConfig::instance() { static PlayerVisualsConfig c; return c; }
PlayerVisualsConfig::PlayerVisualsConfig() = default;

bool PlayerVisualsConfig::parseAndValidate(const std::string& text, PlayerVisualsData& out) {
    try {
        const json root = json::parse(text);
        if (!root.is_object()) { mLastError = "root must be an object"; return false; }
        if (root.contains("renderOrder")) out.renderOrder = root.at("renderOrder").get<std::vector<std::string>>();
        for (const char* layer : {"self", "enemy", "teammate"}) {
            const json& section = root.contains(layer) ? root.at(layer) : json::object();
            if (section.contains("outline")) {
                json wrapper = json::object();
                wrapper[layer] = section.at("outline");
                PlayerOutlineSettings* target = &out.self;
                if (std::string(layer) == "enemy") target = &out.enemy;
                if (std::string(layer) == "teammate") target = &out.teammate;
                if (!readSettings(wrapper, layer, *target, mLastError)) return false;
            }
        }
        return true;
    } catch (const std::exception& e) { mLastError = e.what(); return false; }
}

bool PlayerVisualsConfig::load() {
    std::ifstream f(mPath);
    if (!f) { Debug::warn(Debug::Category::Render, "[PLAYER VISUALS] missing %s; using defaults", mPath.string().c_str()); return false; }
    std::stringstream ss; ss << f.rdbuf();
    PlayerVisualsData candidate = mData;
    if (!parseAndValidate(ss.str(), candidate)) {
        Debug::warn(Debug::Category::Render, "[PLAYER VISUALS] reload failed path=%s reason=%s; keeping previous valid state", mPath.string().c_str(), mLastError.c_str());
        return false;
    }
    mData = std::move(candidate); mLastWrite = writeTime(mPath); mLastError.clear();
    Debug::log(Debug::Category::Render, "[PLAYER VISUALS] loaded path=%s", mPath.string().c_str());
    return true;
}
bool PlayerVisualsConfig::reload() { return load(); }
bool PlayerVisualsConfig::pollReload() {
    const auto wt = writeTime(mPath);
    if (wt == std::filesystem::file_time_type{} || wt == mLastWrite) return false;
    return load();
}
std::string PlayerVisualsConfig::describe() const {
    std::ostringstream s; s << "player visuals enemy=" << (mData.enemy.enabled ? "on" : "off")
      << " teammate=" << (mData.teammate.enabled ? "on" : "off")
      << " self=" << (mData.self.enabled ? "on" : "off"); return s.str();
}
