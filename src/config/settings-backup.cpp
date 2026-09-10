#include "config/settings-backup.h"

#include <filesystem>
#include <fstream>

#include "debug/debug-log.h"

namespace {

const char* kBackupPaths[] = {
    "config/camconfig.json",
    "config/ragdolldeath.json",
    "config/impact_decals.json"
};

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

    for (const char* path : kBackupPaths) {
        std::string bakPath = std::string(path) + ".bak";
        if (std::filesystem::exists(bakPath)) continue; // don't overwrite existing backup

        if (!std::filesystem::exists(path)) continue;

        if (copyFile(path, bakPath)) {
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

    for (const char* path : kBackupPaths) {
        std::string bakPath = std::string(path) + ".bak";
        if (!std::filesystem::exists(bakPath)) continue;

        if (copyFile(bakPath, path)) {
            Debug::log(Debug::Category::General, "[SETTINGS BACKUP] restored %s from %s\n", path, bakPath.c_str());
            std::error_code ec;
            std::filesystem::remove(bakPath, ec);
        } else {
            Debug::warn(Debug::Category::General, "[SETTINGS BACKUP] failed to restore %s\n", path);
        }
    }

    mActive = false;
}
