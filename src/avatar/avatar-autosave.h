#pragma once
#include <string>
#include <deque>
#include <chrono>
#include <filesystem>
#include <functional>

struct AvatarDefinition;

// Manages periodic autosave, atomic save, recovery backups, and rolling autosave files.
class AvatarAutosave {
public:
    AvatarAutosave();

    void setBasePath(const std::string& path);
    const std::string& basePath() const { return mBasePath; }

    // Periodic autosave (call every frame with dt). Calls saveFn when timer expires.
    void update(float dt, std::function<bool()> saveFn);

    // Immediately save the project file. Returns true on success.
    bool saveNow(const AvatarDefinition& avatar);

    // Try to recover from backup/autosaves. Returns true if successful.
    // Sets outRecoveryMsg to a human-readable message.
    bool recover(AvatarDefinition& out, std::string& outRecoveryMsg);

    // Copy the primary file to a timestamped autosave (for undo history).
    bool snapshot();

    // Save status string for display.
    enum class Status { Idle, Saving, Ok, Failed, Recovered };
    Status status() const { return mStatus; }
    const std::string& statusMessage() const { return mStatusMsg; }
    double secondsSinceLastSave() const;

    // Path display
    std::string projectFilePath() const;

    // Autosave file management
    void pruneAutosaves();
    void clearAutosaves();

    static constexpr double AUTOSAVE_INTERVAL = 10.0;
    static constexpr int MAX_AUTOSAVES = 5;

private:
    std::string mBasePath;
    std::string mAvatarJsonPath;
    std::string mBackupJsonPath;
    std::string mAutosavesDir;

    double mTimer = 0.0;
    Status mStatus = Status::Idle;
    std::string mStatusMsg;
    double mLastSaveTime = 0.0;

    // Rolling autosave filenames
    std::deque<std::string> mAutosaveFiles;

    bool writeFileAtomic(const std::string& path, const std::string& content);
    bool readFile(const std::string& path, std::string& out);
    std::string generateTimestamp();
    std::string generateAvatarId();
    int getFormatVersion(const std::string& jsonContent);
};
