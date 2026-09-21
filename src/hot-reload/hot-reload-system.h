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
#include "hot-reload/generation-verify.h"
#include "hot-reload/switch-transaction.h"
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
    // boundary). Returns true when a new generation became active. `tick` is the
    // caller's simulation tick, used to honor a requested coordinated switch.
    bool pollAndAdvance(std::uint32_t tick = 0);

    // Coordinated switch (multiplayer): a validated candidate is held until
    // `tick` so every peer activates the same generation at the same tick.
    bool candidateReady() const { return candidateReady_.load(); }
    bool switchPending() const { return switchPending_; }
    std::uint32_t switchAtTick() const { return switchAtTick_; }
    void requestSwitchAtTick(std::uint32_t tick);
    // Identity of the ready-but-not-yet-active candidate (for the switch announce).
    std::uint32_t candidateGeneration() const;
    std::string candidateCodeHash() const;

    // Distributed artifact stream: read the ready candidate's platform artifact
    // bytes + canonical identity. Returns false when no successful candidate
    // exists. Read-only; never activates anything.
    bool readCandidateArtifact(std::vector<unsigned char>& out,
                               std::uint32_t& logicalGeneration,
                               std::uint64_t& platformArtifactHash) const;

    // Build the bounded requirement manifest the server associates with the
    // ready candidate: identity + ABI + the package's real declared capability
    // requirements and registered schemas. Returns false when no candidate.
    bool buildCandidateManifest(MimitaRuntime::GenerationManifestV1& out) const;
    // Manifest for the generation the authoritative world is ALREADY running,
    // for late-join bootstrap. Returns false when nothing is active.
    bool buildActiveManifest(MimitaRuntime::GenerationManifestV1& out) const;

    // Bind the peer-prepared migration plan to the pending candidate. The switch
    // transaction validates it against the REAL active generation before G may
    // activate; a stale/missing/invalid plan rejects the switch and keeps F live.
    void setCandidateMigrationPlan(const MimitaRuntime::MigrationPlanV1& plan);

    // Install a remotely downloaded platform artifact as a REAL inactive
    // candidate through the SAME load/validate path used by a local build. Does
    // not activate; the candidate activates at the coordinated switch tick (or an
    // explicit requestSwitchAtTick for late-join bootstrap). Returns false and
    // leaves the active generation untouched on any mismatch/load failure.
    bool installCandidateArtifact(const std::vector<unsigned char>& bytes,
                                  std::uint32_t logicalGeneration,
                                  std::uint64_t platformArtifactHash,
                                  std::string& error);
    bool hasInstalledCandidate() const { return haveInstalledCandidate_; }
    std::uint32_t installedCandidateGeneration() const
    {
        return installedCandidate_.generation;
    }
    MimitaRuntime::SwitchRejection lastSwitchRejection() const
    {
        return lastSwitchRejection_;
    }

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
        std::vector<std::string> changedSources;
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
    // Content hash of one hot source, re-read only when its size/mtime changes.
    // Avoids full-file SHA-256 of every hot source every poll on the game thread.
    std::string hashSourceCached(const std::string& relative) const;
    // Per-file hash diff vs the previous build; returns changed relative paths.
    std::vector<std::string> diffSourceHashes();
    std::string manifestSummary() const;
    std::filesystem::path manifestPath() const;
    void pollManifestReload();
    void pollColdBoundary();
    bool beginBuild(const std::string& reason);
    void workerMain();
    BuildResult runBuild(const BuildRequest& request);
    bool tryActivateCandidate();
    // Build the authoritative F -> G migration plan from the loaded candidate's
    // declared schemas against LIVE stored state. Never mutates anything.
    bool buildMigrationPlan(std::uint64_t from, std::uint64_t to,
                            const GamePackageDescriptorV1* descriptor,
                            MimitaRuntime::MigrationPlanV1& out) const;
    // Register the candidate's dynamic-component migrations (additive; harmless
    // if the switch is later rejected) so the plan can resolve real paths.
    void registerDynamicMigrations(const GamePackageDescriptorV1* descriptor) const;
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
    std::unordered_map<std::string, std::string> sourceHashes_;
    struct SourceHashEntry {
        std::uint64_t mtime = 0;
        std::uint64_t size = 0;
        std::string hash;
    };
    mutable std::unordered_map<std::string, SourceHashEntry> sourceHashCache_;
    std::string coldPendingFile_;
    std::uint64_t lastColdNoticeMs_ = 0;
    Project::ProjectWatcher watcher_;

    GameMemory memory_{};
    // Stable buffer that survives DLL unload/reload; exposed as
    // GameMemory::permanentStorage so hot modules keep state across generations.
    std::vector<unsigned char> permanentStorage_{};
    GenerationRecord active_;
    GenerationRecord previous_;
    // A downloaded remote artifact loaded as a real inactive candidate. Consumed
    // by the next activation; never activated before its switch boundary.
    GenerationRecord installedCandidate_{};
    bool haveInstalledCandidate_ = false;
    std::atomic<bool> remoteCandidateInstalled_{false};

    std::atomic<bool> workerStop_{false};
    std::atomic<bool> buildRequested_{false};
    std::atomic<bool> sourceScanRequested_{false};
    std::atomic<bool> buildRunning_{false};
    std::atomic<bool> candidateReady_{false};
    bool switchPending_ = false;
    std::uint32_t switchAtTick_ = 0;
    std::thread worker_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    BuildRequest request_;
    BuildResult result_;

    MimitaRuntime::MigrationPlanV1 candidatePlan_{};
    bool candidatePlanPresent_ = false;
    MimitaRuntime::SwitchRejection lastSwitchRejection_ =
        MimitaRuntime::SwitchRejection::None;

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
