// 2026-11-07
/* purpose
* own the generic module catalog, background build requests, candidate
* validation, activation, and rollback for the running MiMITA process
* this file DOES NOT own map documents, editor state, renderer, or input
*/
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct MapEditorModuleV1;
namespace mimita_world_rendering { struct FaceAuditV1; }

class ModuleHost
{
public:
    static ModuleHost& instance();

    void startup();
    void shutdown();
    void poll();

    bool mapEditorLoaded() const;
    bool mapEditorSetSelectedPosition(const float position[3]);

    bool worldRenderingLoaded() const;
    std::uint32_t activeGeneration(const char* moduleName) const;
    bool worldRenderingAnalyzeFaces(
        const float transform[16], const float cameraPosition[3],
        mimita_world_rendering::FaceAuditV1* outFaces,
        std::uint32_t faceCapacity);

    const std::string& lastError() const { return mLastError; }

private:
    ModuleHost() = default;
    ~ModuleHost();
    ModuleHost(const ModuleHost&) = delete;
    ModuleHost& operator=(const ModuleHost&) = delete;

    struct ModuleEntry
    {
        std::string name;
        std::uint32_t abi = 0;
        std::uint32_t stateSchema = 0;
        std::vector<std::filesystem::path> sources;
        std::vector<std::filesystem::path> headers;
        std::filesystem::path outputDir;
        std::filesystem::path currentPath;
        std::filesystem::path resultPath;
        std::uint64_t newestInputTime = 0;
        std::uint64_t lastObservedInputTime = 0;
        std::uint64_t lastResultTime = 0;
        std::uint64_t lastResultJsonTime = 0;
        std::uint32_t activeGeneration = 0;
        std::string activeCodeHash;
        void* moduleHandle = nullptr;
        void* api = nullptr;
        std::filesystem::path loadedTempDll;
        bool buildInFlight = false;
        void* workerProcess = nullptr;
    };

    bool loadCatalog();
    bool loadCurrent(ModuleEntry& entry);
    bool loadCandidate(ModuleEntry& entry, const std::filesystem::path& dll,
                       std::uint32_t generation, const std::string& expectedHash);
    bool activateMapEditor(ModuleEntry& entry, void* candidateModule,
                           void* candidateApi, const std::filesystem::path& tempDll,
                           std::uint32_t generation, const std::string& codeHash);
    void unloadEntry(ModuleEntry& entry);
    void retirePath(const std::filesystem::path& path);

    bool requestBuild(ModuleEntry& entry);
    bool pollBuild(ModuleEntry& entry);
    std::uint64_t newestInputTime(const ModuleEntry& entry) const;
    std::filesystem::path uniqueLoadPath(const ModuleEntry& entry,
                                         std::uint32_t generation);

    void writeEvent(const char* event,
                    const char* reason,
                    const std::string& correlation,
                    const std::string& fieldsJson);
    void rememberError(const std::string& error);

    bool mStarted = false;
    std::string mLastError;
    std::uint64_t mLastPollTime = 0;
    std::uint32_t mTempGeneration = 0;
    std::vector<ModuleEntry> mModules;
    std::vector<std::filesystem::path> mRetiredPaths;
};
