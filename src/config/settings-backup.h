#pragma once

#include <string>

class SettingsBackup {
public:
    static SettingsBackup& instance();

    // Save current configs as .bak (only if not already backed up)
    void saveBackups();

    // Restore from .bak (if backups exist)
    void restoreBackups();

    // Check if backups are active
    bool hasActiveBackups() const { return mActive; }

private:
    SettingsBackup() = default;

    bool mActive = false;
};
