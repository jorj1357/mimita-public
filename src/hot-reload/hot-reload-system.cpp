#include "hot-reload/hot-reload-system.h"

#include "hot-reload/game-api.h"
#include "live-code/code-hash.h"
#include "live-code/live-code-events.h"
#include "live-code/live-journal.h"
#include "utils/path_utils.h"
#include "utils/time-format.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <system_error>

#include <windows.h>
#include <nlohmann/json.hpp>

namespace {

constexpr int HOT_RELOAD_POLL_THROTTLE = 15;

void MIMITA_GAME_CALL platformLog(const char* message)
{
    if (message)
        std::printf("%s\n", message);
}

bool wildcardMatch(const std::string& pattern, const std::string& text)
{
    std::size_t p = 0;
    std::size_t t = 0;
    std::size_t star = std::string::npos;
    std::size_t match = 0;
    while (t < text.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == text[t])) {
            ++p;
            ++t;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star = p++;
            match = t;
        } else if (star != std::string::npos) {
            p = star + 1;
            t = ++match;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*')
        ++p;
    return p == pattern.size();
}

// Expands a manifest glob like "src/hot-reload/modules/*.cpp" into relative
// source paths. Supports '*' and '?' in the final segment (recursive).
void expandGlob(const std::filesystem::path& root, const std::string& pattern,
                std::vector<std::string>& out)
{
    const std::filesystem::path patternPath(pattern);
    const std::filesystem::path base = patternPath.parent_path();
    const std::string filePattern = patternPath.filename().string();
    const std::filesystem::path searchRoot = root / base;

    std::error_code error;
    if (!std::filesystem::exists(searchRoot, error))
        return;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(searchRoot, error)) {
        if (error)
            break;
        if (!entry.is_regular_file())
            continue;
        const std::string name = entry.path().filename().string();
        if (!wildcardMatch(filePattern, name))
            continue;
        std::error_code relError;
        const std::filesystem::path relative =
            std::filesystem::relative(entry.path(), root, relError);
        if (!relError)
            out.push_back(relative.generic_string());
    }
}

} // namespace

HotReloadSystem& HotReloadSystem::instance()
{
    static HotReloadSystem system;
    return system;
}

HotReloadSystem::HotReloadSystem()
{
    // Prefer the executable directory so a server process spawned with a
    // different working directory still finds the manifest and build output.
    // Fall back to the current directory for normal local development.
    root_ = std::filesystem::current_path();
    const std::string executableDir = getExecutableDirectory();
    if (!executableDir.empty()) {
        std::error_code existsError;
        const std::filesystem::path candidate(executableDir);
        if (std::filesystem::exists(
                candidate / "src" / "hot-reload" / "hot-modules.json", existsError)) {
            root_ = candidate;
        }
    }
    sourceDLL_ = root_ / "build" / "mimita-game.dll";
    memory_.apiVersion = MIMITA_GAME_API_VERSION;
    memory_.platform.version = MIMITA_GAME_API_VERSION;
    memory_.platform.log = platformLog;
}

HotReloadSystem::~HotReloadSystem()
{
    unloadGameDLL();
}

void HotReloadSystem::startup()
{
    if (worker_.joinable())
        return;

    memory_.apiVersion = MIMITA_GAME_API_VERSION;
    memory_.platform.version = MIMITA_GAME_API_VERSION;
    memory_.platform.log = platformLog;

    loadManifest();
    observedSourceHash_ = computeSourceHash();
    manifestHash_ = LiveCodeHash::sha256File(manifestPath().string());
    for (const auto& relative : coldSources_) {
        const std::string path = (root_ / relative).string();
        WIN32_FILE_ATTRIBUTE_DATA data{};
        if (GetFileAttributesExW((root_ / relative).wstring().c_str(),
                                 GetFileExInfoStandard, &data)) {
            ULARGE_INTEGER value{};
            value.LowPart = data.ftLastWriteTime.dwLowDateTime;
            value.HighPart = data.ftLastWriteTime.dwHighDateTime;
            coldMtimes_[path] = value.QuadPart;
        }
    }

    if (std::filesystem::exists(sourceDLL_)) {
        GenerationRecord initial;
        std::string error;
        if (loadCandidateFromFile(sourceDLL_, initial, error)) {
            initial.generation = nextGeneration_++;
            initial.codeHash = LiveCodeHash::sha256File(sourceDLL_.string());
            active_ = initial;
            ++memory_.reloadCount;
            std::printf("[HOT RELOAD] initial generation=%u\n", active_.generation);
        } else {
            lastError_ = error;
            std::printf("[HOT RELOAD] initial load failed: %s\n", error.c_str());
        }
    }

    workerStop_ = false;
    worker_ = std::thread(&HotReloadSystem::workerMain, this);

    // Low-latency change signal; the hash scan remains the source of truth.
    watcher_.start(root_ / "src" / "hot-reload");
}

bool HotReloadSystem::pollAndAdvance()
{
    if (!worker_.joinable())
        return false;

    if (candidateReady_.exchange(false)) {
        if (tryActivateCandidate())
            return true;
    }

    const bool watcherDirty = watcher_.poll();
    if (!buildRunning_.load() && !buildRequested_.load()) {
        const bool throttled = (++pollCounter_ % HOT_RELOAD_POLL_THROTTLE == 0);
        if (throttled || watcherDirty) {
            pollManifestReload();
            // Re-resolve globs so live-added/removed/renamed hot files change
            // the package source set (and therefore the source hash).
            loadManifest();
            pollColdBoundary();
            const std::string hash = computeSourceHash();
            if (!hash.empty() && hash != observedSourceHash_) {
                // Retry the same failed hash with bounded backoff instead of
                // suppressing it forever. A transient or racing build must be
                // recoverable without another source edit.
                const std::uint64_t nowMs = MiMitaTime::monotonicMillis();
                const bool sameAsFailed = (!attemptedHash_.empty() && hash == attemptedHash_);
                if (!sameAsFailed || nowMs >= nextRetryMonoMs_)
                    beginBuild(sameAsFailed ? "retry" : "source_change");
            }
        }
    }
    return false;
}

bool HotReloadSystem::beginBuild(const std::string& reason)
{
    if (buildRunning_.load() || buildRequested_.load())
        return false;

    const std::string hash = computeSourceHash();
    if (hash.empty())
        return false;
    // Do NOT mark this hash active before the build succeeds. `attemptedHash_`
    // records the try so a failure can be retried with backoff.
    attemptedHash_ = hash;
    pendingHash_ = hash;

    const std::uint32_t generation = nextGeneration_++;
    const std::filesystem::path processDir =
        root_ / "build" / "hotreload" / ("p" + std::to_string(GetCurrentProcessId()));
    const std::filesystem::path dir = processDir / ("gen" + std::to_string(generation));
    std::error_code error;
    std::filesystem::create_directories(dir, error);

    BuildRequest request;
    request.generation = generation;
    request.sourceHash = hash;
    // Immutable generation filename inside a per-process directory. The result
    // and log are per-generation as well, so two processes can never read each
    // other's build outcome.
    char generationName[64]{};
    std::snprintf(generationName, sizeof(generationName),
                  "mimita-live-g%06u.dll", generation);
    request.outputPath = dir / generationName;
    request.resultPath = dir / "build-result.json";
    request.logPath = dir / "build.log";
    request.reason = reason;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        request_ = request;
    }

    const std::string summary = manifestSummary();
    if (reason == "source_change")
        LiveCodeEvents::notifyEditDetected(summary);
    LiveCodeEvents::notifyCompiling(summary, generation, attemptFailures_);

    LiveEventJournal::Fields fields;
    fields.generation = generation;
    fields.hasGeneration = true;
    fields.codeHash = hash;
    fields.module = summary;
    fields.result = reason;
    LiveEventJournal::instance().record("source_hash_changed", fields);

    buildRunning_ = true;
    buildRequested_ = true;
    cv_.notify_one();
    return true;
}

void HotReloadSystem::workerMain()
{
    for (;;) {
        BuildRequest request;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] {
                return workerStop_.load() || buildRequested_.load();
            });
            if (workerStop_.load())
                break;
            request = request_;
            buildRequested_ = false;
        }

        BuildResult result = runBuild(request);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            result_ = result;
        }
        candidateReady_ = true;
        buildRunning_ = false;
    }
}

HotReloadSystem::BuildResult HotReloadSystem::runBuild(const BuildRequest& request)
{
    BuildResult result;
    result.generation = request.generation;
    result.sourceHash = request.sourceHash;
    result.outputPath = request.outputPath;

    const std::string manifest =
        (root_ / "src" / "hot-reload" / "hot-modules.json").string();
    std::string command = "cd /d \"" + root_.string() + "\" && python build_game_dll.py";
    command += " --generation " + std::to_string(request.generation);
    command += " --output \"" + request.outputPath.string() + "\"";
    command += " --result \"" + request.resultPath.string() + "\"";
    command += " --hot-modules \"" + manifest + "\"";
    command += " > \"" + request.logPath.string() + "\" 2>&1";

    const int exitCode = std::system(command.c_str());

    std::ifstream file(request.resultPath);
    if (file.is_open()) {
        try {
            nlohmann::json json;
            file >> json;
            result.codeHash = json.value("code_hash", "");
            result.error = json.value("error", "");
            result.success = json.value("status", "") == "ok";
        } catch (...) {
            result.success = false;
            result.error = "build result parse error";
        }
    } else {
        result.success = false;
        result.error = "build result missing";
    }

    if (result.success && exitCode != 0) {
        result.success = false;
        result.error = "compiler exit " + std::to_string(exitCode);
    }
    if (result.success && !std::filesystem::exists(request.outputPath)) {
        result.success = false;
        result.error = "staged DLL missing";
    }
    return result;
}

bool HotReloadSystem::tryActivateCandidate()
{
    BuildResult result;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        result = result_;
    }
    const std::string summary = manifestSummary();

    if (!result.success) {
        lastError_ = result.error;
        ++attemptFailures_;
        const std::uint64_t backoffMs =
            (std::uint64_t)std::min(10000, 2000 * attemptFailures_);
        nextRetryMonoMs_ = MiMitaTime::monotonicMillis() + backoffMs;
        LiveEventJournal::Fields failure;
        failure.generation = result.generation;
        failure.hasGeneration = true;
        failure.file = summary;
        failure.result = "failed";
        failure.error = result.error;
        failure.extra = std::string("\"active_generation\":") +
            std::to_string(active_.generation) +
            ",\"attempt\": " + std::to_string(attemptFailures_) +
            ",\"retry_in_ms\": " + std::to_string(backoffMs);
        LiveEventJournal::instance().record("compile_finished", failure);
        LiveCodeEvents::notifyCompileFailed(summary, result.generation,
                                            active_.generation, result.error,
                                            attemptFailures_);
        return false;
    }

    LiveEventJournal::Fields built;
    built.generation = result.generation;
    built.hasGeneration = true;
    built.codeHash = result.codeHash;
    built.result = "ok";
    LiveEventJournal::instance().record("compile_finished", built);

    GenerationRecord candidate;
    std::string error;
    if (!loadCandidateFromFile(result.outputPath, candidate, error)) {
        lastError_ = error;
        ++attemptFailures_;
        const std::uint64_t backoffMs =
            (std::uint64_t)std::min(10000, 2000 * attemptFailures_);
        nextRetryMonoMs_ = MiMitaTime::monotonicMillis() + backoffMs;
        LiveCodeEvents::notifyValidationFailed(result.generation, error);
        return false;
    }
    candidate.generation = result.generation;
    candidate.codeHash = result.codeHash;

    LiveCodeEvents::notifyCandidateReady(result.generation, result.codeHash);
    {
        LiveEventJournal::Fields loaded;
        loaded.generation = result.generation;
        loaded.hasGeneration = true;
        loaded.codeHash = result.codeHash;
        loaded.result = "loaded";
        LiveEventJournal::instance().record("candidate_loaded", loaded);
    }

    retireRecord(previous_);
    previous_ = active_;
    active_ = candidate;
    ++memory_.reloadCount;
    lastError_.clear();

    // Success: this hash is now the active one. Clear failure/retry state.
    observedSourceHash_ = result.sourceHash.empty() ? active_.codeHash : result.sourceHash;
    attemptedHash_ = observedSourceHash_;
    pendingHash_.clear();
    attemptFailures_ = 0;
    nextRetryMonoMs_ = 0;

    LiveCodeEvents::notifyActivated(active_.generation, active_.codeHash);
    return true;
}

bool HotReloadSystem::loadCandidateFromFile(const std::filesystem::path& sourceDLL,
                                            GenerationRecord& out, std::string& error)
{
    if (!std::filesystem::exists(sourceDLL)) {
        error = "candidate DLL missing";
        return false;
    }

    const std::filesystem::path tempDLL = makeUniqueTempDLLPath();
    std::error_code copyError;
    std::filesystem::copy_file(sourceDLL, tempDLL,
                               std::filesystem::copy_options::overwrite_existing, copyError);
    if (copyError) {
        error = "copy: " + copyError.message();
        return false;
    }

    HMODULE module = LoadLibraryW(tempDLL.wstring().c_str());
    if (!module) {
        error = "LoadLibrary error " + std::to_string(GetLastError());
        std::filesystem::remove(tempDLL, copyError);
        return false;
    }

    auto getGameAPI = reinterpret_cast<GetGameAPIFn>(
        GetProcAddress(module, "GetGameAPI"));
    GameAPI api{};
    const bool compatible = getGameAPI &&
        getGameAPI(MIMITA_GAME_API_VERSION, &api) &&
        api.version == MIMITA_GAME_API_VERSION &&
        api.structSize == sizeof(GameAPI) &&
        api.updateEffects != nullptr &&
        api.selfTest != nullptr;
    if (!compatible) {
        error = "incompatible or incomplete GameAPI";
        FreeLibrary(module);
        std::filesystem::remove(tempDLL, copyError);
        return false;
    }

    if (api.onReload && !api.onReload(&memory_)) {
        error = "onReload rejected persistent memory";
        FreeLibrary(module);
        std::filesystem::remove(tempDLL, copyError);
        return false;
    }

    GameSelfTestResult selfTest{};
    selfTest.structSize = sizeof(GameSelfTestResult);
    if (!api.selfTest(&selfTest) || !selfTest.passed) {
        error = selfTest.message[0]
            ? "self-test failed: " + std::string(selfTest.message)
            : "self-test failed";
        FreeLibrary(module);
        std::filesystem::remove(tempDLL, copyError);
        return false;
    }

    out.module = module;
    out.api = api;
    out.dllPath = sourceDLL;
    out.loadedTempPath = tempDLL;
    out.valid = true;
    return true;
}

void HotReloadSystem::retireRecord(GenerationRecord& record)
{
    if (!record.valid)
        return;

    if (record.module) {
        if (record.api.beforeUnload)
            record.api.beforeUnload(&memory_);
        FreeLibrary(static_cast<HMODULE>(record.module));
    }
    std::error_code error;
    if (!record.loadedTempPath.empty())
        std::filesystem::remove(record.loadedTempPath, error);
    record = GenerationRecord{};
}

bool HotReloadSystem::rollback()
{
    if (!previous_.valid)
        return false;

    GenerationRecord target = previous_;
    previous_ = active_;
    active_ = target;
    ++memory_.reloadCount;
    LiveCodeEvents::notifyRollbackActivated(active_.generation, active_.codeHash);
    return true;
}

void HotReloadSystem::unloadGameDLL()
{
    watcher_.stop();
    workerStop_ = true;
    cv_.notify_all();
    if (worker_.joinable())
        worker_.join();

    retireRecord(previous_);
    retireRecord(active_);
}

void HotReloadSystem::loadManifest()
{
    hotSources_.clear();
    coldSources_.clear();
    std::ifstream file(manifestPath());
    if (!file.is_open()) {
        hotSources_ = {"src/effects/effect-part.cpp", "src/hot-reload/game-api.h"};
        return;
    }
    try {
        nlohmann::json json;
        file >> json;
        if (json.contains("modules") && json["modules"].is_array()) {
            for (const auto& module : json["modules"]) {
                if (module.contains("sources") && module["sources"].is_array()) {
                    for (const auto& source : module["sources"])
                        hotSources_.push_back(source.get<std::string>());
                }
            }
        }
        if (json.contains("headers") && json["headers"].is_array()) {
            for (const auto& header : json["headers"])
                hotSources_.push_back(header.get<std::string>());
        }
        if (json.contains("globs") && json["globs"].is_array()) {
            for (const auto& pattern : json["globs"])
                expandGlob(root_, pattern.get<std::string>(), hotSources_);
        }
        if (json.contains("cold") && json["cold"].is_array()) {
            for (const auto& cold : json["cold"])
                coldSources_.push_back(cold.get<std::string>());
        }
    } catch (...) {
        hotSources_ = {"src/effects/effect-part.cpp", "src/hot-reload/game-api.h"};
    }

    if (hotSources_.empty())
        hotSources_ = {"src/effects/effect-part.cpp", "src/hot-reload/game-api.h"};

    std::sort(hotSources_.begin(), hotSources_.end());
    hotSources_.erase(std::unique(hotSources_.begin(), hotSources_.end()), hotSources_.end());
    std::sort(coldSources_.begin(), coldSources_.end());
    coldSources_.erase(std::unique(coldSources_.begin(), coldSources_.end()), coldSources_.end());
}

std::filesystem::path HotReloadSystem::manifestPath() const
{
    return root_ / "src" / "hot-reload" / "hot-modules.json";
}

void HotReloadSystem::pollManifestReload()
{
    const std::string hash = LiveCodeHash::sha256File(manifestPath().string());
    if (hash.empty() || hash == manifestHash_)
        return;
    manifestHash_ = hash;
    loadManifest();
    observedSourceHash_ = computeSourceHash();

    LiveEventJournal::Fields fields;
    fields.file = "src/hot-reload/hot-modules.json";
    fields.codeHash = hash;
    fields.result = "reloaded";
    LiveEventJournal::instance().record("manifest_reloaded", fields);
    std::printf("[LIVE CODE] hot-modules manifest reloaded\n");
}

void HotReloadSystem::pollColdBoundary()
{
    for (const auto& relative : coldSources_) {
        const std::filesystem::path path = root_ / relative;
        WIN32_FILE_ATTRIBUTE_DATA data{};
        if (!GetFileAttributesExW(path.wstring().c_str(), GetFileExInfoStandard, &data))
            continue;
        ULARGE_INTEGER value{};
        value.LowPart = data.ftLastWriteTime.dwLowDateTime;
        value.HighPart = data.ftLastWriteTime.dwHighDateTime;
        const std::uint64_t stamp = value.QuadPart;
        auto it = coldMtimes_.find(relative);
        if (it == coldMtimes_.end()) {
            coldMtimes_[relative] = stamp;
            continue;
        }
        if (it->second == stamp)
            continue;
        it->second = stamp;

        LiveEventJournal::Fields fields;
        fields.file = relative;
        fields.result = "HOT_RELOAD_BOUNDARY_VIOLATION";
        fields.error = "cold kernel change cannot be activated without relinking mimita.exe";
        LiveEventJournal::instance().record("hot_reload_boundary_violation", fields);
        LiveCodeEvents::notifyBoundaryViolation(relative);
        coldPendingFile_ = relative;
    }

    // Periodic in-game reminder while a cold change is waiting for a restart.
    if (!coldPendingFile_.empty()) {
        const std::uint64_t nowMs = MiMitaTime::monotonicMillis();
        if (nowMs - lastColdNoticeMs_ >= 10000) {
            lastColdNoticeMs_ = nowMs;
            LiveCodeEvents::notifyColdRestartPending(coldPendingFile_);
        }
    }
}

std::string HotReloadSystem::computeSourceHash() const
{
    std::string combined;
    for (const auto& relative : hotSources_) {
        const std::string hash = LiveCodeHash::sha256File((root_ / relative).string());
        if (hash.empty())
            return {};
        combined += relative;
        combined += ':';
        combined += hash;
        combined += '\n';
    }
    if (combined.empty())
        return {};
    return LiveCodeHash::sha256Bytes(combined.data(), combined.size());
}

std::string HotReloadSystem::manifestSummary() const
{
    std::string summary;
    for (std::size_t i = 0; i < hotSources_.size(); ++i) {
        if (i)
            summary += "+";
        summary += hotSources_[i];
    }
    return summary;
}

std::filesystem::path HotReloadSystem::makeUniqueTempDLLPath()
{
    const DWORD processId = GetCurrentProcessId();
    ++tempGeneration_;
    return sourceDLL_.parent_path() /
        ("mimita-game-live-" + std::to_string(processId) + "-" +
         std::to_string(tempGeneration_) + ".dll");
}

const GameAPI* HotReloadSystem::gameAPI() const
{
    return active_.valid ? &active_.api : nullptr;
}

GameMemory& HotReloadSystem::gameMemory()
{
    return memory_;
}

bool HotReloadSystem::loaded() const
{
    return active_.valid;
}

HotReloadSystem::Status HotReloadSystem::status() const
{
    Status status;
    status.loaded = active_.valid;
    status.workerRunning = worker_.joinable();
    status.buildRunning = buildRunning_.load();
    status.candidateReady = candidateReady_.load();
    status.activeGeneration = active_.generation;
    status.previousGeneration = previous_.generation;
    status.activeHash = active_.codeHash;
    status.observedSourceHash = observedSourceHash_;
    status.lastError = lastError_;
    status.reloadCount = memory_.reloadCount;
    return status;
}
