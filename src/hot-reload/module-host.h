// 2026-11-07
/* purpose
* own the generic module catalog, background build requests, candidate
* validation, activation, event dispatch, and rollback for the running process
* this file DOES NOT own map documents, editor state, renderer, or input
*/
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "modules/api/module-abi.h"

namespace mimita_map_editor { struct DragMathV1; struct GizmoPickV1; struct EditorInputV1; struct EditorClickV1; }
namespace mimita_world_rendering {
struct FaceAuditV1;
struct BoxMeshV1;
}
namespace mimita_worldgen { struct ResolveV1; }
namespace mimita_chunk_collision { struct BuildChunkV1; }

class ModuleHost
{
public:
    static ModuleHost& instance();

    void startup();
    void shutdown();
    void poll();

    bool moduleLoaded(const char* moduleName) const;
    bool invokeModule(const char* moduleName, std::uint32_t eventType,
                      void* payload, std::uint32_t payloadSize,
                      std::uint64_t tick = 0);
    // Dispatch an event to the catalog-declared owner. Returns false when no
    // module declared the event or no active module handled it.
    bool invokeEvent(std::uint32_t eventType, void* payload,
                     std::uint32_t payloadSize, std::uint64_t tick = 0);

    const void* capability(std::uint32_t capabilityId, std::uint32_t version);

    bool mapEditorLoaded() const;
    bool mapEditorSetSelectedPosition(const float position[3]);
    bool mapEditorComputeDragDelta(
        const mimita_map_editor::DragMathV1* input, float outDelta[3]);
    bool mapEditorPickGizmoAxis(
        const mimita_map_editor::GizmoPickV1* input, int* outAxis);
    bool mapEditorExecuteCommand(const char* command, const char* arguments);
    bool mapEditorMapCursor(mimita_map_editor::EditorInputV1* input);
    bool mapEditorResolveClick(mimita_map_editor::EditorClickV1* click);
    bool mapEditorAddChunk(const char* requestedId, char* outId,
                           std::uint32_t outCapacity);
    bool mapEditorAddChunkPart(const char* kind, char* outId,
                               std::uint32_t outCapacity);
    bool mapEditorAddConnector(const char* side);
    bool mapEditorPreviewConnections(char* report, std::uint32_t capacity);
    bool mapEditorEvaluateConnectorPair(
        const char* parentChunk, const char* parentConnector,
        const char* childChunk, const char* childConnector,
        float outTransform[16], char* reason, std::uint32_t reasonCapacity);

    bool worldRenderingLoaded() const;
    std::uint32_t activeGeneration(const char* moduleName) const;
    bool worldRenderingAnalyzeFaces(
        const float transform[16], const float cameraPosition[3],
        mimita_world_rendering::FaceAuditV1* outFaces,
        std::uint32_t faceCapacity);
    bool worldRenderingBuildBox(mimita_world_rendering::BoxMeshV1* request);

    bool worldgenResolve(mimita_worldgen::ResolveV1* request);
    bool chunkCollisionBuild(
        mimita_chunk_collision::BuildChunkV1* request);

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
        std::vector<std::uint32_t> events;
        std::vector<std::uint32_t> capabilities;
        std::string activation;
        std::filesystem::path outputDir;
        std::filesystem::path currentPath;
        std::filesystem::path resultPath;
        std::filesystem::path depsPath;
        std::uint64_t newestInputTime = 0;
        std::uint64_t lastObservedInputTime = 0;
        std::uint64_t lastResultTime = 0;
        std::uint64_t lastResultJsonTime = 0;
        std::vector<std::string> lastChangedPaths;
        std::uint32_t activeGeneration = 0;
        std::string activeCodeHash;
        std::string lastFailureHash;
        void* moduleHandle = nullptr;
        const mimita_module::ModuleV1* api = nullptr;
        std::filesystem::path loadedTempDll;
        bool buildInFlight = false;
        void* workerProcess = nullptr;
    };

    bool loadCatalog();
    bool loadCurrent(ModuleEntry& entry);
    bool loadCandidate(ModuleEntry& entry, const std::filesystem::path& dll,
                       std::uint32_t generation, const std::string& expectedHash);
    bool activateEntry(ModuleEntry& entry, void* candidateModule,
                       const mimita_module::ModuleV1* candidateApi,
                       const std::filesystem::path& tempDll,
                       std::uint32_t generation, const std::string& codeHash);
    void unloadEntry(ModuleEntry& entry);
    void retirePath(const std::filesystem::path& path);
    ModuleEntry* findModule(const char* name);
    const ModuleEntry* findModule(const char* name) const;
    const ModuleEntry* findEventOwner(std::uint32_t eventType) const;

    bool requestBuild(ModuleEntry& entry);
    bool pollBuild(ModuleEntry& entry);
    std::uint64_t newestInputTime(const ModuleEntry& entry) const;
    std::filesystem::path uniqueLoadPath(const ModuleEntry& entry,
                                         std::uint32_t generation);
    void refreshDependencies(ModuleEntry& entry);

    void writeEvent(const char* event,
                    const char* reason,
                    const std::string& correlation,
                    const std::string& fieldsJson);
    void rememberError(const std::string& error);
    std::string joinedPaths(const ModuleEntry& entry) const;
    void notifyActivation(const ModuleEntry& entry, std::uint32_t generation,
                          std::uint32_t previousGeneration);
    void notifyBuildFailure(const ModuleEntry& entry,
                            const std::string& details);

    bool mStarted = false;
    bool mInStartup = true;
    std::string mLastError;
    std::uint64_t mLastPollTime = 0;
    std::uint32_t mTempGeneration = 0;
    std::vector<ModuleEntry> mModules;
    std::vector<std::filesystem::path> mRetiredPaths;
};
