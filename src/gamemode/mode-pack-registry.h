// Generic JSON-defined gamemode runtime - community mode-pack registry.
//
// Discovers validated mode-pack manifests from the configured local root(s),
// rejects malformed/duplicate/unknown-capability packs, and replaces the whole
// catalog atomically. A failed reload always retains the last valid catalog.
//
// This is one owner for pack discovery + validation. It does NOT execute
// gameplay and does NOT own match state.

#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "gamemode/mode-pack.h"

namespace MimitaGamemode {

class ModePackRegistry
{
public:
    static constexpr int kSupportedSchemaVersion = 1;

    static ModePackRegistry& instance();

    // Scans `dir` for *.json manifests. On success the catalog is replaced
    // atomically and `true` is returned. On any error the previous catalog is
    // kept, `false` is returned, and `diagnostics` (when provided) receives one
    // line per problem naming the exact pack, field, and capability.
    bool loadDirectory(const std::string& dir,
                       std::vector<std::string>* diagnostics = nullptr);

    // Timestamp-based reload. Re-scans only when a file in the last loaded
    // directory is added, removed, or changed. Retains the last valid catalog
    // on failure. Returns true when a reload was attempted and succeeded.
    bool pollReload();

    const std::string& directory() const { return mDirectory; }

    const ModePack* get(const std::string& id) const;
    bool has(const std::string& id) const;
    std::vector<std::string> ids() const;
    size_t size() const { return mPacks.size(); }

    // Diagnostics from the most recent load attempt (empty on success).
    const std::vector<std::string>& diagnostics() const { return mDiagnostics; }

    void clear();

private:
    ModePackRegistry() = default;

    std::vector<ModePack> mPacks;
    std::vector<std::string> mDiagnostics;
    std::string mDirectory;
    std::vector<std::pair<std::string, std::filesystem::file_time_type>> mFileTimes;
};

} // namespace MimitaGamemode
