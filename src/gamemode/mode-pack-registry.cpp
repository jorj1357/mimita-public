// Generic JSON-defined gamemode runtime - community mode-pack registry.
//
// One validated catalog owner. Every failure path is atomic: the catalog is
// only replaced after the entire directory has parsed and validated. The
// previous catalog survives any failed reload, and diagnostics name the exact
// pack, field, and capability that failed.

#include "gamemode/mode-pack-registry.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>

#include <nlohmann/json.hpp>

#include "gamemode/capability-registry.h"
#include "utils/json-comments.h"

namespace MimitaGamemode {

namespace {

using json = nlohmann::json;

std::string fileNameOf(const std::string& path)
{
    return std::filesystem::path(path).filename().string();
}

std::string optString(const json& root, const char* key, const std::string& fallback = "")
{
    if (root.contains(key) && root[key].is_string())
        return root[key].get<std::string>();
    return fallback;
}

int optInt(const json& root, const char* key, int fallback)
{
    if (root.contains(key) && root[key].is_number())
        return root[key].get<int>();
    return fallback;
}

float optFloat(const json& root, const char* key, float fallback)
{
    if (root.contains(key) && root[key].is_number())
        return root[key].get<float>();
    return fallback;
}

bool optBool(const json& root, const char* key, bool fallback)
{
    if (root.contains(key) && root[key].is_boolean())
        return root[key].get<bool>();
    return fallback;
}

std::vector<std::string> optStringArray(const json& root, const char* key)
{
    std::vector<std::string> out;
    if (!root.contains(key) || !root[key].is_array())
        return out;
    for (const auto& item : root[key]) {
        if (item.is_string())
            out.push_back(item.get<std::string>());
    }
    return out;
}

// Parse one manifest. Returns false and appends a diagnostic on any problem.
// `diagnostics` lines always start with the pack id (or filename when the id is
// missing) so a failure points at the exact owner.
bool parsePack(const std::string& path, ModePack& out, std::vector<std::string>& diagnostics)
{
    const std::string file = fileNameOf(path);

    std::ifstream stream(path);
    if (!stream.is_open()) {
        diagnostics.push_back(file + ": cannot open file");
        return false;
    }

    json root;
    try {
        root = parseJsonConfig(stream);
    } catch (const json::parse_error& e) {
        diagnostics.push_back(file + ": malformed JSON: " + e.what());
        return false;
    } catch (const std::exception& e) {
        diagnostics.push_back(file + ": JSON error: " + e.what());
        return false;
    }

    if (!root.is_object()) {
        diagnostics.push_back(file + ": root must be a JSON object");
        return false;
    }

    const int schema = optInt(root, "schema_version", optInt(root, "schema", 0));
    if (schema != ModePackRegistry::kSupportedSchemaVersion) {
        diagnostics.push_back(file + ": unsupported schema_version=" + std::to_string(schema) +
                              " (expected " + std::to_string(ModePackRegistry::kSupportedSchemaVersion) + ")");
        return false;
    }

    const std::string id = optString(root, "id");
    if (id.empty()) {
        diagnostics.push_back(file + ": missing required string field \"id\"");
        return false;
    }

    ModePack pack;
    pack.schemaVersion = schema;
    pack.id = id;
    pack.name = optString(root, "name", id);
    pack.description = optString(root, "description");
    pack.gamemodeId = optString(root, "gamemode_id", id);
    pack.enabled = optBool(root, "enabled", true);
    pack.intermissionSeconds = std::max(0, optInt(root, "intermission_seconds", 0));
    pack.countdownSeconds = std::max(0, optInt(root, "countdown_seconds", 0));
    pack.resultsSeconds = std::max(0, optInt(root, "results_seconds", 0));
    pack.hudLayoutId = optString(root, "hud_layout", id);
    pack.bannerAudio = optString(root, "banner_audio");
    pack.referencedMaps = optStringArray(root, "maps");
    pack.referencedWeapons = optStringArray(root, "weapons");
    pack.referencedActors = optStringArray(root, "actors");
    pack.referencedAssets = optStringArray(root, "assets");

    // ── Capabilities: every declared id must be known. ──────────────
    const CapabilityRegistry& capabilities = CapabilityRegistry::instance();
    pack.capabilities = optStringArray(root, "capabilities");
    for (const std::string& capabilityId : pack.capabilities) {
        if (capabilityId.empty() || !capabilities.isKnown(capabilityId)) {
            diagnostics.push_back(id + ": unknown capability \"" + capabilityId +
                                  "\" (field: capabilities)");
            return false;
        }
    }

    // ── Disasters: each must be well-formed and self-consistent. ────
    if (root.contains("disasters")) {
        if (!root["disasters"].is_array()) {
            diagnostics.push_back(id + ": field \"disasters\" must be an array");
            return false;
        }
        for (const auto& d : root["disasters"]) {
            if (!d.is_object()) {
                diagnostics.push_back(id + ": disaster entry must be an object");
                return false;
            }
            DisasterDefinition disaster;
            disaster.id = optString(d, "id");
            if (disaster.id.empty()) {
                diagnostics.push_back(id + ": disaster missing required field \"id\"");
                return false;
            }
            disaster.name = optString(d, "name", disaster.id);
            disaster.description = optString(d, "description");
            disaster.durationSeconds = std::max(0.0f, optFloat(d, "duration_seconds", 0.0f));
            if (disaster.durationSeconds <= 0.0f) {
                diagnostics.push_back(id + ": disaster \"" + disaster.id +
                                      "\" requires duration_seconds > 0");
                return false;
            }
            disaster.weaponPool = optStringArray(d, "weapon_pool");
            disaster.winPolicy = optString(d, "win_policy", "last_actor_alive");
            if (disaster.weaponPool.empty()) {
                diagnostics.push_back(id + ": disaster \"" + disaster.id +
                                      "\" requires a non-empty weapon_pool");
                return false;
            }
            pack.disasters.push_back(std::move(disaster));
        }
    }

    out = std::move(pack);
    return true;
}

} // namespace

ModePackRegistry& ModePackRegistry::instance()
{
    static ModePackRegistry registry;
    return registry;
}

void ModePackRegistry::clear()
{
    mPacks.clear();
    mDiagnostics.clear();
    mDirectory.clear();
    mFileTimes.clear();
}

bool ModePackRegistry::pollReload()
{
    if (mDirectory.empty())
        return false;

    std::error_code ec;
    if (!std::filesystem::is_directory(mDirectory, ec))
        return false;

    std::vector<std::pair<std::string, std::filesystem::file_time_type>> current;
    for (const auto& entry : std::filesystem::directory_iterator(mDirectory, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;
        if (entry.path().extension() != ".json") continue;
        std::error_code timeEc;
        current.push_back({entry.path().string(),
                           std::filesystem::last_write_time(entry.path(), timeEc)});
    }
    std::sort(current.begin(), current.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });

    if (current == mFileTimes)
        return false;  // nothing changed since the last successful load

    return loadDirectory(mDirectory);
}

bool ModePackRegistry::loadDirectory(const std::string& dir,
                                     std::vector<std::string>* diagnostics)
{
    std::vector<std::string> problems;

    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) {
        problems.push_back(dir + ": mode-pack directory not found");
        mDiagnostics = std::move(problems);
        if (diagnostics) *diagnostics = mDiagnostics;
        return false;
    }

    // Collect candidate files in sorted order so filesystem iteration order can
    // never change diagnostics or catalog ordering.
    std::vector<std::filesystem::path> files;
    std::vector<std::pair<std::string, std::filesystem::file_time_type>> times;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;
        if (entry.path().extension() != ".json") continue;
        files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    times.reserve(files.size());
    for (const auto& path : files) {
        std::error_code timeEc;
        times.push_back({path.string(), std::filesystem::last_write_time(path, timeEc)});
    }

    std::vector<ModePack> next;
    std::set<std::string> seenIds;
    bool ok = true;

    for (const auto& path : files) {
        ModePack pack;
        if (!parsePack(path.string(), pack, problems)) {
            ok = false;
            continue;  // collect every problem in one pass
        }
        if (!seenIds.insert(pack.id).second) {
            problems.push_back(pack.id + ": duplicate mode-pack id in " +
                               fileNameOf(path.string()));
            ok = false;
            continue;
        }
        next.push_back(std::move(pack));
    }

    if (!ok) {
        // Atomic failure: keep the previous catalog untouched.
        mDiagnostics = std::move(problems);
        if (diagnostics) *diagnostics = mDiagnostics;
        return false;
    }

    // Deterministic catalog order, independent of filename or iteration order.
    std::sort(next.begin(), next.end(),
        [](const ModePack& a, const ModePack& b) { return a.id < b.id; });

    mPacks = std::move(next);
    mDiagnostics.clear();
    mDirectory = dir;
    mFileTimes = std::move(times);
    if (diagnostics) diagnostics->clear();
    return true;
}

const ModePack* ModePackRegistry::get(const std::string& id) const
{
    for (const ModePack& pack : mPacks) {
        if (pack.id == id)
            return &pack;
    }
    return nullptr;
}

bool ModePackRegistry::has(const std::string& id) const
{
    return get(id) != nullptr;
}

std::vector<std::string> ModePackRegistry::ids() const
{
    std::vector<std::string> out;
    out.reserve(mPacks.size());
    for (const ModePack& pack : mPacks)
        out.push_back(pack.id);
    return out;
}

} // namespace MimitaGamemode
