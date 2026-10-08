#include "config/settings-backup.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "debug/debug-log.h"
#include "debug/structured-log.h"

namespace {

const char* kBackupPaths[] = {
    "config/camconfig.json",
    "config/ragdolldeath.json"
};

std::string fileFingerprint(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) return "missing";

    uint64_t hash = 1469598103934665603ull;
    uint64_t bytes = 0;
    char buffer[4096];
    while (input.read(buffer, sizeof(buffer)) || input.gcount() > 0) {
        const std::streamsize count = input.gcount();
        bytes += static_cast<uint64_t>(count);
        for (std::streamsize i = 0; i < count; ++i) {
            hash ^= static_cast<unsigned char>(buffer[i]);
            hash *= 1099511628211ull;
        }
    }

    std::ostringstream result;
    result << bytes << ":" << std::hex << hash;
    return result.str();
}

void logBackupScope(const char* operation)
{
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "settings.backup.scope", "settings-backup", operation, 0,
        nlohmann::json{
            {"operation", operation},
            {"managed_paths", {"config/camconfig.json", "config/ragdolldeath.json"}},
            {"excluded_paths", {"config/impact_decals.json"}},
            {"reason", "gamemode_runtime_visual_overrides"}
        }, __FILE__, __LINE__, __FUNCTION__);
}

bool copyFile(const std::string& from, const std::string& to)
{
    std::error_code ec;
    std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing, ec);
    return !ec;
}

} // namespace

SettingsBackup& SettingsBackup::instance()
{
    static SettingsBackup backup;
    return backup;
}

void SettingsBackup::saveBackups()
{
    if (mActive) return; // already backed up

    logBackupScope("save_begin");

    for (const char* path : kBackupPaths) {
        std::string bakPath = std::string(path) + ".bak";
        if (std::filesystem::exists(bakPath)) continue; // don't overwrite existing backup

        if (!std::filesystem::exists(path)) continue;

        const std::string sourceBefore = fileFingerprint(path);
        const std::string backupBefore = fileFingerprint(bakPath);
        const bool copied = copyFile(path, bakPath);
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "settings.backup.copy", "settings-backup", "save_backup", 0,
            nlohmann::json{
                {"operation", "backup_create"}, {"source", path},
                {"destination", bakPath}, {"source_before", sourceBefore},
                {"destination_before", backupBefore}, {"copy_succeeded", copied},
                {"source_after", fileFingerprint(path)},
                {"destination_after", fileFingerprint(bakPath)}
            }, __FILE__, __LINE__, __FUNCTION__);

        if (copied) {
            Debug::log(Debug::Category::General, "[SETTINGS BACKUP] saved %s\n", bakPath.c_str());
        } else {
            Debug::warn(Debug::Category::General, "[SETTINGS BACKUP] failed to save %s\n", bakPath.c_str());
        }
    }

    mActive = true;
}

void SettingsBackup::restoreBackups()
{
    if (!mActive) return;

    logBackupScope("restore_begin");

    for (const char* path : kBackupPaths) {
        std::string bakPath = std::string(path) + ".bak";
        if (!std::filesystem::exists(bakPath)) continue;

        const std::string sourceBefore = fileFingerprint(bakPath);
        const std::string destinationBefore = fileFingerprint(path);
        const bool copied = copyFile(bakPath, path);
        const std::string destinationAfter = fileFingerprint(path);
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "settings.backup.copy", "settings-backup", "restore_backup", 0,
            nlohmann::json{
                {"operation", "backup_restore"}, {"source", bakPath},
                {"destination", path}, {"source_before", sourceBefore},
                {"destination_before", destinationBefore}, {"copy_succeeded", copied},
                {"source_after", fileFingerprint(bakPath)},
                {"destination_after", destinationAfter}
            }, __FILE__, __LINE__, __FUNCTION__);

        if (copied) {
            Debug::log(Debug::Category::General, "[SETTINGS BACKUP] restored %s from %s\n", path, bakPath.c_str());
            std::error_code ec;
            std::filesystem::remove(bakPath, ec);
        } else {
            Debug::warn(Debug::Category::General, "[SETTINGS BACKUP] failed to restore %s\n", path);
        }
    }

    mActive = false;
}
