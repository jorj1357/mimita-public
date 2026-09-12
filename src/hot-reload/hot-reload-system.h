#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "hot-reload/game-api.h"
#include "project/project-watcher.h"

// Owns the live replaceable-code pipeline: hash-based change detection, a
// non-blocking background build worker, generation-stamped candidates, API/ABI
// and deterministic self-test validation, safe-tick activation, and rollback.
// The game thread never blocks on compilation; it only loads and swaps a ready
// candidate at the top of the fixed tick.
class HotReloadSystem {
public:
    static HotReloadSystem& instance();

    // Load any existing DLL and start the background build worker.
    void startup();
    // Called each frame at the top of the fixed tick (the safe activation
    // boundary). Returns true when a new generation became active.
    bool pollAndAdvance();

    void unloadGameDLL();

    // Reactivate the previous generation at the next safe boundary.
    bool rollback();

    const GameAPI* gameAPI() const;
    GameMemory& gameMemory();
    bool loaded() const;

    struct Status {
        bool loaded = false;
        bool workerRunning = false;
        bool buildRunning = false;
        bool candidateReady = false;
        std::uint32_t activeGeneration = 0;
        std::uint32_t previousGeneration = 0;
        std::string activeHash;
        std::string observedSourceHash;
        std::string lastError;
        std::uint32_t reloadCount = 0;
    };
    Status status() const;

private:
    HotReloadSystem();
    ~HotReloadSystem();
    HotReloadSystem(const HotReloadSystem&) = delete;
    HotReloadSystem& operator=(const HotReloadSystem&) = delete;

    struct GenerationRecord {
        std::uint32_t generation = 0;
        std::string codeHash;
        std::filesystem::path dllPath;
        std::filesystem::path loadedTempPath;
        void* module = nullptr;
        GameAPI api{};
        bool valid = false;
    };

    struct BuildRequest {
        std::uint32_t generation = 0;
        std::string sourceHash;
        std::filesystem::path outputPath;
        std::filesystem::path resultPath;
        std::filesystem::path logPath;
        std::string reason;
    };

    struct BuildResult {
        bool success = false;
        std::uint32_t generation = 0;
        std::string sourceHash;
        std::string codeHash;
        std::string error;
        std::filesystem::path outputPath;
    };

    void loadManifest();
    std::string computeSourceHash() const;
    std::string manifestSummary() const;
    std::filesystem::path manifestPath() const;
    void pollManifestReload();
    void pollColdBoundary();
    bool beginBuild(const std::string& reason);
    void workerMain();
    BuildResult runBuild(const BuildRequest& request);
    bool tryActivateCandidate();
    bool loadCandidateFromFile(const std::filesystem::path& sourceDLL,
                               GenerationRecord& out, std::string& error);
    void retireRecord(GenerationRecord& record);
    std::filesystem::path makeUniqueTempDLLPath();

    std::vector<std::string> hotSources_;
    std::vector<std::string> coldSources_;
    std::filesystem::path root_;
    std::filesystem::path sourceDLL_;
    std::string manifestHash_;
    std::unordered_map<std::string, std::uint64_t> coldMtimes_;
    std::string coldPendingFile_;
    std::uint64_t lastColdNoticeMs_ = 0;
    Project::ProjectWatcher watcher_;

    GameMemory memory_{};
    GenerationRecord active_;
    GenerationRecord previous_;

    std::atomic<bool> workerStop_{false};
    std::atomic<bool> buildRequested_{false};
    std::atomic<bool> buildRunning_{false};
    std::atomic<bool> candidateReady_{false};
    std::thread worker_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    BuildRequest request_;
    BuildResult result_;

    std::string observedSourceHash_;
    std::string attemptedHash_;
    std::string pendingHash_;
    int attemptFailures_ = 0;
    std::uint64_t nextRetryMonoMs_ = 0;
    std::uint32_t nextGeneration_ = 1;
    std::uint32_t tempGeneration_ = 0;
    std::uint32_t pollCounter_ = 0;
    std::string lastError_;
};
