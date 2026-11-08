// 2026-11-07
/* purpose
* implement the generic host side of the versioned module ABI: catalog loading,
* background build requests, event dispatch, candidate validation, activation,
* and rollback
* this file DOES NOT own editor behavior, map data, or rendering
*/
#include "hot-reload/module-host.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <set>
#include <system_error>
#include <vector>

#include <windows.h>

#include <nlohmann/json.hpp>

#include "debug/structured-log.h"
#include "devtools/terminal.h"
#include "gamemode/map-config.h"
#include "mapeditor/map-editor.h"
#include "modules/api/actor-contact-policy-api.h"
#include "modules/api/chunk-collision-api.h"
#include "modules/api/map-editor-api.h"
#include "modules/api/world-rendering-api.h"
#include "modules/api/worldgen-api.h"
#include "notifications/notifications.h"
#include "worldgen/worldgen-library.h"

namespace {

using mimita_map_editor::DragMathV1;
using mimita_map_editor::EditorServicesV1;
using mimita_map_editor::GizmoPickV1;
using mimita_module::BaseV1;
using mimita_module::GetModuleAPIFn;
using mimita_module::ModuleEventV1;
using mimita_module::ModuleV1;

constexpr std::uint64_t kPollIntervalMs = 250;

std::uint64_t fileWriteTime(const std::filesystem::path& path)
{
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.wstring().c_str(), GetFileExInfoStandard, &data))
        return 0;
    ULARGE_INTEGER value{};
    value.LowPart = data.ftLastWriteTime.dwLowDateTime;
    value.HighPart = data.ftLastWriteTime.dwHighDateTime;
    return value.QuadPart;
}

bool wildcardMatch(const std::string& text, const std::string& pattern)
{
    size_t t = 0;
    size_t p = 0;
    size_t star = std::string::npos;
    size_t resume = 0;
    while (t < text.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == text[t])) {
            ++t;
            ++p;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star = p++;
            resume = t;
        } else if (star != std::string::npos) {
            p = star + 1;
            t = ++resume;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*')
        ++p;
    return p == pattern.size();
}

void expandPatterns(const std::filesystem::path& root, const std::string& pattern,
                    std::vector<std::filesystem::path>& out)
{
    if (pattern.find('*') == std::string::npos &&
        pattern.find('?') == std::string::npos) {
        out.push_back(root / pattern);
        return;
    }
    const std::filesystem::path relative(pattern);
    const std::filesystem::path directory = root / relative.parent_path();
    const std::string filePattern = relative.filename().string();
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error))
        return;
    for (const auto& item : std::filesystem::directory_iterator(directory, error)) {
        if (error)
            return;
        if (item.is_regular_file(error) &&
            wildcardMatch(item.path().filename().string(), filePattern))
            out.push_back(item.path());
    }
}

std::string readTextFile(const std::filesystem::path& path)
{
    FILE* file = nullptr;
    if (_wfopen_s(&file, path.wstring().c_str(), L"rb") != 0 || !file)
        return {};
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    std::string text;
    if (size > 0) {
        text.resize(static_cast<size_t>(size));
        std::fread(text.data(), 1, text.size(), file);
    }
    std::fclose(file);
    return text;
}

int MIMITA_MODULE_CALL hostGetPrimarySelection(
    char* id, std::uint32_t idCapacity, int* kind, float* position)
{
    glm::vec3 value(0.0f);
    if (!MapEditor::instance().modulePrimarySelection(id, idCapacity, kind, value))
        return 0;
    position[0] = value.x;
    position[1] = value.y;
    position[2] = value.z;
    return 1;
}

int MIMITA_MODULE_CALL hostApplyPrimaryPosition(const float* position)
{
    if (!position)
        return 0;
    return MapEditor::instance().moduleApplyPrimaryPosition(
        glm::vec3(position[0], position[1], position[2])) ? 1 : 0;
}

void MIMITA_MODULE_CALL hostNotify(const char* message)
{
    if (message)
        MapEditor::instance().notify(message);
}

void MIMITA_MODULE_CALL hostLogEvent(
    std::uint32_t category, std::uint32_t level, const char* eventName,
    const char* correlationId, const char* reason, std::uint32_t tick,
    const char* jsonFields, const char* sourceFile, int sourceLine,
    const char* functionName)
{
    nlohmann::json fields = jsonFields
        ? nlohmann::json::parse(jsonFields, nullptr, false)
        : nlohmann::json::object();
    if (fields.is_discarded())
        fields = nlohmann::json::object();
    StructuredLogger::instance().writeEvent(
        static_cast<StructuredCategory>(category),
        static_cast<StructuredLevel>(level),
        eventName ? eventName : "module.event",
        correlationId ? correlationId : "",
        reason ? reason : "",
        tick,
        fields,
        sourceFile, sourceLine, functionName);
}

std::uint64_t MIMITA_MODULE_CALL hostRevision()
{
    return MapConfigRegistry::instance().revision();
}

int MIMITA_MODULE_CALL hostSelectObject(const char* id)
{
    return id && MapEditor::instance().selectObject(id) ? 1 : 0;
}

int MIMITA_MODULE_CALL hostSelectKind(const char* kind)
{
    return kind && MapEditor::instance().selectKind(kind) ? 1 : 0;
}

int MIMITA_MODULE_CALL hostSelectAll()
{
    MapEditor::instance().selectAllEdit();
    return 1;
}

int MIMITA_MODULE_CALL hostClearSelection()
{
    MapEditor::instance().moduleClearSelection();
    return 1;
}

void MIMITA_MODULE_CALL hostLogLine(const char* message)
{
    if (message)
        Terminal::instance().addLog(message);
}

int MIMITA_MODULE_CALL hostCopySelection()
{
    MapEditor::instance().copyEdit();
    return 1;
}

int MIMITA_MODULE_CALL hostPasteClipboard()
{
    MapEditor::instance().pasteEdit();
    return 1;
}

int MIMITA_MODULE_CALL hostCutSelection()
{
    MapEditor::instance().cutEdit();
    return 1;
}

int MIMITA_MODULE_CALL hostDuplicateSelection()
{
    MapEditor::instance().duplicateEdit();
    return 1;
}

int MIMITA_MODULE_CALL hostDeleteSelection()
{
    MapEditor::instance().deleteEdit();
    return 1;
}

int MIMITA_MODULE_CALL hostUndo()
{
    MapEditor::instance().undoEdit();
    return 1;
}

int MIMITA_MODULE_CALL hostRedo()
{
    MapEditor::instance().redoEdit();
    return 1;
}

int MIMITA_MODULE_CALL hostSave()
{
    return MapEditor::instance().save() ? 1 : 0;
}

int MIMITA_MODULE_CALL hostReload()
{
    return MapEditor::instance().reload() ? 1 : 0;
}

int MIMITA_MODULE_CALL hostAutosave()
{
    return MapEditor::instance().autosave() ? 1 : 0;
}

int MIMITA_MODULE_CALL hostRebuildCollision()
{
    MapEditor::instance().flushCollisionNow();
    return 1;
}

int MIMITA_MODULE_CALL hostSetFreezeWorld(int freeze)
{
    MapEditor::instance().setFreezeWorld(freeze != 0);
    return 1;
}

int MIMITA_MODULE_CALL hostFocusSelection()
{
    MapEditor::instance().focusSelection();
    return 1;
}

int MIMITA_MODULE_CALL hostNudge(float dx, float dy, float dz)
{
    MapEditor::instance().nudgeSelection(glm::vec3(dx, dy, dz));
    return 1;
}

int MIMITA_MODULE_CALL hostSetPosition(float x, float y, float z)
{
    return MapEditor::instance().setSelectedPosition(glm::vec3(x, y, z)) ? 1 : 0;
}

int MIMITA_MODULE_CALL hostSetRotation(float x, float y, float z)
{
    return MapEditor::instance().setSelectedRotation(glm::vec3(x, y, z)) ? 1 : 0;
}

int MIMITA_MODULE_CALL hostSetScale(float x, float y, float z)
{
    return MapEditor::instance().setSelectedScale(glm::vec3(x, y, z)) ? 1 : 0;
}

int MIMITA_MODULE_CALL hostSetSize(float x, float y, float z)
{
    return MapEditor::instance().setSelectedSize(glm::vec3(x, y, z)) ? 1 : 0;
}

int MIMITA_MODULE_CALL hostSetBombSiteRadius(float radius)
{
    MapEditor::instance().setSelectedBombSiteRadius(radius);
    return 1;
}

int MIMITA_MODULE_CALL hostRenameSelection(const char* name)
{
    return name && MapEditor::instance().setSelectedName(name) ? 1 : 0;
}

std::uint32_t MIMITA_MODULE_CALL hostGetSelectionCount()
{
    return MapEditor::instance().moduleSelectionCount();
}

int MIMITA_MODULE_CALL hostGetSelectionItem(
    std::uint32_t index, int* kind, char* id, std::uint32_t idCapacity)
{
    return MapEditor::instance().moduleSelectionAt(
        index, kind, id, idCapacity) ? 1 : 0;
}

int MIMITA_MODULE_CALL hostSetSelection(
    std::uint32_t count, const mimita_map_editor::EditorSelectionRefV1* refs)
{
    std::vector<EditorObjectRef> selection;
    selection.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        EditorObjectRef ref;
        ref.kind = static_cast<EditorObjectKind>(refs[i].kind);
        ref.id = refs[i].id;
        selection.push_back(std::move(ref));
    }
    MapEditor::instance().moduleSetSelection(selection);
    return 1;
}

int MIMITA_MODULE_CALL hostGetProfileFootprint(float outFootprint[3])
{
    outFootprint[0] = 10.0f;
    outFootprint[1] = 10.0f;
    outFootprint[2] = 4.0f;
    const std::string mapId = MapConfigRegistry::instance().mapId();
    if (mapId.empty())
        return 1;
    MimitaWorldGen::WorldGenMode profile;
    std::string error;
    if (!MimitaWorldGen::WorldGenLibrary::instance().loadMode(mapId, profile, &error))
        return 1;
    outFootprint[0] = std::max(0.25f, profile.grid.chunkSize.x);
    outFootprint[1] = std::max(0.25f, profile.grid.chunkSize.y);
    outFootprint[2] = std::max(0.25f, profile.grid.chunkSize.z);
    return 1;
}

int MIMITA_MODULE_CALL hostCreateChunk(
    const char* requestedId, const float footprint[3], char* outId,
    std::uint32_t outCapacity)
{
    if (!outId || outCapacity == 0)
        return 0;
    const glm::vec3 size = footprint
        ? glm::vec3(footprint[0], footprint[1], footprint[2])
        : glm::vec3(10.0f, 10.0f, 4.0f);
    const std::string created = MapEditor::instance().addChunkWithFootprint(
        requestedId ? requestedId : "", size);
    if (created.empty())
        return 0;
    std::snprintf(outId, outCapacity, "%s", created.c_str());
    return 1;
}

int MIMITA_MODULE_CALL hostCreateChunkPart(
    const char* chunkId, const char* kind, char* outId, std::uint32_t outCapacity)
{
    (void)chunkId;
    if (!outId || outCapacity == 0)
        return 0;
    const std::string created = MapEditor::instance().addChunkPart(
        kind ? kind : "box");
    if (created.empty())
        return 0;
    std::snprintf(outId, outCapacity, "%s", created.c_str());
    return 1;
}

int MIMITA_MODULE_CALL hostCreateConnector(
    const char* chunkId, const char* side, char* outId, std::uint32_t outCapacity)
{
    (void)chunkId;
    if (!outId || outCapacity == 0)
        return 0;
    const std::string sideValue = side ? side : "east";
    if (!MapEditor::instance().addConnector(sideValue))
        return 0;
    std::snprintf(outId, outCapacity, "%s", sideValue.c_str());
    return 1;
}

int MIMITA_MODULE_CALL hostPreviewConnections(
    char* report, std::uint32_t reportCapacity)
{
    if (!report || reportCapacity == 0)
        return 0;
    const std::string text = MapEditor::instance().previewConnectionsReport();
    std::snprintf(report, reportCapacity, "%s", text.c_str());
    return 1;
}

int MIMITA_MODULE_CALL hostEvaluateConnectorPair(
    const char* parentChunk, const char* parentConnector,
    const char* childChunk, const char* childConnector,
    float outTransform[16], char* reason, std::uint32_t reasonCapacity)
{
    if (reason && reasonCapacity)
        reason[0] = '\0';
    const MapObjectiveConfig& config = MapConfigRegistry::instance().current();
    std::vector<mimita_worldgen::ChunkV1> chunkViews;
    std::vector<mimita_worldgen::PartV1> partViews;
    std::vector<mimita_worldgen::ConnectorV1> connectorViews;
    chunkViews.reserve(config.chunks.size());
    int parentChunkIndex = -1;
    int parentConnectorIndex = -1;
    int childChunkIndex = -1;
    int childConnectorIndex = -1;
    for (const AuthoredChunk& chunk : config.chunks) {
        mimita_worldgen::ChunkV1 view{};
        std::snprintf(view.id, sizeof(view.id), "%s", chunk.id.c_str());
        view.boundsMin[0] = chunk.boundsMin.x;
        view.boundsMin[1] = chunk.boundsMin.y;
        view.boundsMin[2] = chunk.boundsMin.z;
        view.boundsMax[0] = chunk.boundsMax.x;
        view.boundsMax[1] = chunk.boundsMax.y;
        view.boundsMax[2] = chunk.boundsMax.z;
        view.enabled = chunk.enabled ? 1 : 0;
        view.partOffset = static_cast<std::uint32_t>(partViews.size());
        view.connectorOffset = static_cast<std::uint32_t>(connectorViews.size());
        for (const ChunkPart& part : chunk.parts) {
            mimita_worldgen::PartV1 p{};
            std::snprintf(p.id, sizeof(p.id), "%s", part.id.c_str());
            std::snprintf(p.kind, sizeof(p.kind), "%s", part.kind.c_str());
            p.position[0] = part.position.x;
            p.position[1] = part.position.y;
            p.position[2] = part.position.z;
            p.size[0] = part.size.x;
            p.size[1] = part.size.y;
            p.size[2] = part.size.z;
            p.rotation[0] = part.rotationEuler.x;
            p.rotation[1] = part.rotationEuler.y;
            p.rotation[2] = part.rotationEuler.z;
            p.collision = part.collision ? 1 : 0;
            p.visible = part.visible ? 1 : 0;
            partViews.push_back(p);
        }
        for (const AuthoredChunkConnector& connector : chunk.connectors) {
            mimita_worldgen::ConnectorV1 c{};
            std::snprintf(c.id, sizeof(c.id), "%s", connector.id.c_str());
            std::snprintf(c.side, sizeof(c.side), "%s", connector.side.c_str());
            std::snprintf(c.orientation, sizeof(c.orientation), "%s",
                          connector.orientation.c_str());
            std::snprintf(c.role, sizeof(c.role), "%s", connector.role.c_str());
            c.position[0] = connector.position.x;
            c.position[1] = connector.position.y;
            c.position[2] = connector.position.z;
            c.radius = connector.radius;
            std::string tags;
            for (const std::string& tag : connector.tags) {
                if (!tags.empty()) tags += "|";
                tags += tag;
            }
            std::snprintf(c.tags, sizeof(c.tags), "%s", tags.c_str());
            std::string links;
            for (const std::string& link : connector.links) {
                if (!links.empty()) links += "|";
                links += link;
            }
            std::snprintf(c.links, sizeof(c.links), "%s", links.c_str());
            if (parentChunk && parentConnector && chunk.id == parentChunk &&
                connector.id == parentConnector)
                parentConnectorIndex = static_cast<int>(connectorViews.size());
            if (childChunk && childConnector && chunk.id == childChunk &&
                connector.id == childConnector)
                childConnectorIndex = static_cast<int>(connectorViews.size());
            connectorViews.push_back(c);
        }
        view.partCount = static_cast<std::uint32_t>(partViews.size()) - view.partOffset;
        view.connectorCount = static_cast<std::uint32_t>(connectorViews.size()) -
                              view.connectorOffset;
        if (parentChunk && chunk.id == parentChunk)
            parentChunkIndex = static_cast<int>(chunkViews.size());
        if (childChunk && chunk.id == childChunk)
            childChunkIndex = static_cast<int>(chunkViews.size());
        chunkViews.push_back(view);
    }
    if (parentChunkIndex < 0 || parentConnectorIndex < 0 ||
        childChunkIndex < 0 || childConnectorIndex < 0) {
        if (reason && reasonCapacity)
            std::snprintf(reason, reasonCapacity, "chunk or connector not found");
        return 0;
    }
    mimita_worldgen::TablesV1 tables{};
    tables.chunks = chunkViews.data();
    tables.chunkCount = static_cast<std::uint32_t>(chunkViews.size());
    tables.parts = partViews.data();
    tables.partCount = static_cast<std::uint32_t>(partViews.size());
    tables.connectors = connectorViews.data();
    tables.connectorCount = static_cast<std::uint32_t>(connectorViews.size());
    mimita_worldgen::ResolveV1 request{};
    request.tables = &tables;
    request.requestKind = 2;
    request.parentChunk = parentChunkIndex;
    request.parentConnector = parentConnectorIndex;
    request.childChunk = childChunkIndex;
    request.childConnector = childConnectorIndex;
    if (!ModuleHost::instance().worldgenResolve(&request))
        return 0;
    if (!request.outValid) {
        if (reason && reasonCapacity)
            std::snprintf(reason, reasonCapacity, "%s", request.reason);
        return 0;
    }
    if (outTransform)
        std::memcpy(outTransform, request.outTransform, sizeof(request.outTransform));
    return 1;
}

const EditorServicesV1* editorServices()
{
    static const EditorServicesV1 services = {
        mimita_map_editor::ABI_VERSION,
        sizeof(EditorServicesV1),
        hostGetPrimarySelection,
        hostApplyPrimaryPosition,
        hostNotify,
        hostRevision,
        hostSelectObject,
        hostSelectKind,
        hostSelectAll,
        hostClearSelection,
        hostLogLine,
        hostCopySelection,
        hostPasteClipboard,
        hostCutSelection,
        hostDuplicateSelection,
        hostDeleteSelection,
        hostUndo,
        hostRedo,
        hostSave,
        hostReload,
        hostAutosave,
        hostRebuildCollision,
        hostSetFreezeWorld,
        hostFocusSelection,
        hostNudge,
        hostSetPosition,
        hostSetRotation,
        hostSetScale,
        hostSetSize,
        hostSetBombSiteRadius,
        hostRenameSelection,
        hostGetSelectionCount,
        hostGetSelectionItem,
        hostSetSelection,
        hostGetProfileFootprint,
        hostCreateChunk,
        hostCreateChunkPart,
        hostCreateConnector,
        hostPreviewConnections,
        hostEvaluateConnectorPair,
    };
    return &services;
}

const void* MIMITA_MODULE_CALL hostGetCapability(
    std::uint32_t capabilityId, std::uint32_t version)
{
    return ModuleHost::instance().capability(capabilityId, version);
}

const mimita_module::HostServicesV1* genericHostServices()
{
    static const mimita_module::HostServicesV1 services = {
        mimita_module::ABI_VERSION,
        sizeof(mimita_module::HostServicesV1),
        hostLogEvent,
        hostGetCapability,
        hostRevision,
    };
    return &services;
}

const mimita_world_rendering::HostServicesV1* worldRenderingServices()
{
    static const mimita_world_rendering::HostServicesV1 services = {
        mimita_world_rendering::ABI_VERSION,
        sizeof(mimita_world_rendering::HostServicesV1),
        hostLogEvent,
    };
    return &services;
}

const mimita_worldgen::HostServicesV1* worldgenServices()
{
    static const mimita_worldgen::HostServicesV1 services = {
        mimita_worldgen::ABI_VERSION,
        sizeof(mimita_worldgen::HostServicesV1),
        hostLogEvent,
    };
    return &services;
}

const mimita_chunk_collision::HostServicesV1* chunkCollisionServices()
{
    static const mimita_chunk_collision::HostServicesV1 services = {
        mimita_chunk_collision::ABI_VERSION,
        sizeof(mimita_chunk_collision::HostServicesV1),
        hostLogEvent,
    };
    return &services;
}

} // namespace

ModuleHost& ModuleHost::instance()
{
    static ModuleHost host;
    return host;
}

ModuleHost::~ModuleHost()
{
    shutdown();
}

void ModuleHost::startup()
{
    startup(false);
}

void ModuleHost::startupServerModules()
{
    startup(true);
}

void ModuleHost::startup(bool serverProcess)
{
    if (mStarted)
        return;
    mStarted = true;
    if (!loadCatalog(serverProcess)) {
        rememberError("module catalog could not be loaded");
        return;
    }

    for (ModuleEntry& entry : mModules) {
        std::error_code error;
        std::filesystem::create_directories(entry.outputDir, error);
        entry.newestInputTime = newestInputTime(entry);
        entry.lastObservedInputTime = entry.newestInputTime;
        entry.currentPath = entry.outputDir / "current.json";
        entry.resultPath = entry.outputDir / "result.json";
        entry.depsPath = entry.outputDir / "deps.json";
        entry.lastResultJsonTime = fileWriteTime(entry.resultPath);
        refreshDependencies(entry);
        if (!loadCurrent(entry)) {
            requestBuild(entry);
        }
    }
    writeEvent("module.host.started", "module host startup complete", "module-host",
               "{\"modules\":" + std::to_string(mModules.size()) + "}");
    mInStartup = false;
}

void ModuleHost::shutdown()
{
    for (ModuleEntry& entry : mModules) {
        if (entry.workerProcess) {
            CloseHandle(static_cast<HANDLE>(entry.workerProcess));
            entry.workerProcess = nullptr;
        }
        unloadEntry(entry);
    }
    for (const auto& path : mRetiredPaths) {
        std::error_code error;
        std::filesystem::remove(path, error);
    }
    mRetiredPaths.clear();
    mModules.clear();
    mStarted = false;
}

void ModuleHost::poll()
{
    if (!mStarted)
        return;
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    if (now - static_cast<long long>(mLastPollTime) <
        static_cast<long long>(kPollIntervalMs))
        return;
    mLastPollTime = static_cast<std::uint64_t>(now);

    for (ModuleEntry& entry : mModules) {
        if (entry.workerProcess) {
            HANDLE process = static_cast<HANDLE>(entry.workerProcess);
            if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0) {
                CloseHandle(process);
                entry.workerProcess = nullptr;
                entry.buildInFlight = false;
                refreshDependencies(entry);
            }
        }

        const std::uint64_t resultTime = fileWriteTime(entry.currentPath);
        if (resultTime != 0 && resultTime != entry.lastResultTime) {
            entry.lastResultTime = resultTime;
            loadCurrent(entry);
        }

        const std::uint64_t resultJsonTime = fileWriteTime(entry.resultPath);
        if (resultJsonTime != 0 && resultJsonTime != entry.lastResultJsonTime) {
            entry.lastResultJsonTime = resultJsonTime;
            const std::string resultText = readTextFile(entry.resultPath);
            nlohmann::json result =
                nlohmann::json::parse(resultText, nullptr, false);
            if (!result.is_discarded() && result.is_object() &&
                result.value("status", std::string{}) == "failed") {
                const std::string failureHash =
                    result.value("code_hash", std::string{});
                if (failureHash == entry.lastFailureHash)
                    continue;
                entry.lastFailureHash = failureHash;
                std::string output = result.value("compiler_output", std::string{});
                if (output.size() > 4000)
                    output = output.substr(output.size() - 4000);
                writeEvent("module.build.failed", "module compiler failed",
                           entry.name,
                           nlohmann::json{
                               {"module", entry.name},
                               {"generation", result.value("generation", 0u)},
                               {"code_hash", failureHash},
                               {"exit_code", result.value("exit_code", -1)},
                               {"compiler_output_tail", output}}.dump());
                if (!mInStartup)
                    notifyBuildFailure(entry, output);
            }
        }

        const std::uint64_t newest = newestInputTime(entry);
        if (newest > entry.lastObservedInputTime && !entry.buildInFlight) {
            entry.lastChangedPaths.clear();
            const std::uint64_t threshold = entry.lastObservedInputTime;
            auto recordPath = [&](const std::filesystem::path& path) {
                std::error_code error;
                const auto absolute = std::filesystem::absolute(path, error);
                entry.lastChangedPaths.push_back(
                    (error ? path : absolute).lexically_normal().string());
            };
            for (const auto& path : entry.sources)
                if (fileWriteTime(path) > threshold)
                    recordPath(path);
            for (const auto& path : entry.headers)
                if (fileWriteTime(path) > threshold)
                    recordPath(path);
            entry.lastObservedInputTime = newest;
            requestBuild(entry);
        }
    }
}

ModuleHost::ModuleEntry* ModuleHost::findModule(const char* name)
{
    if (!name)
        return nullptr;
    for (ModuleEntry& entry : mModules)
        if (entry.name == name)
            return &entry;
    return nullptr;
}

const ModuleHost::ModuleEntry* ModuleHost::findModule(const char* name) const
{
    if (!name)
        return nullptr;
    for (const ModuleEntry& entry : mModules)
        if (entry.name == name)
            return &entry;
    return nullptr;
}

const ModuleHost::ModuleEntry* ModuleHost::findEventOwner(
    std::uint32_t eventType) const
{
    for (const ModuleEntry& entry : mModules)
        if (std::find(entry.events.begin(), entry.events.end(), eventType) !=
            entry.events.end())
            return &entry;
    return nullptr;
}

bool ModuleHost::moduleLoaded(const char* moduleName) const
{
    const ModuleEntry* entry = findModule(moduleName);
    return entry && entry->api != nullptr;
}

bool ModuleHost::invokeModule(const char* moduleName, std::uint32_t eventType,
                              void* payload, std::uint32_t payloadSize,
                              std::uint64_t tick)
{
    ModuleEntry* entry = findModule(moduleName);
    if (!entry || !entry->api || !entry->api->onEvent)
        return false;
    if (!entry->events.empty() &&
        std::find(entry->events.begin(), entry->events.end(), eventType) ==
            entry->events.end())
        return false;
    ModuleEventV1 event{};
    event.typeId = eventType;
    event.structSize = sizeof(ModuleEventV1);
    event.tick = tick;
    event.payload = payload;
    event.payloadSize = payloadSize;
    event.handled = 0;
    const int result = entry->api->onEvent(&event);
    return result != 0 || event.handled != 0;
}

bool ModuleHost::invokeEvent(std::uint32_t eventType, void* payload,
                             std::uint32_t payloadSize, std::uint64_t tick)
{
    const ModuleEntry* owner = findEventOwner(eventType);
    if (!owner)
        return false;
    return invokeModule(owner->name.c_str(), eventType, payload, payloadSize, tick);
}

const void* ModuleHost::capability(std::uint32_t capabilityId,
                                   std::uint32_t version)
{
    switch (capabilityId) {
    case mimita_module::CAPABILITY_EDITOR:
        return version == mimita_map_editor::ABI_VERSION ? editorServices() : nullptr;
    case mimita_module::CAPABILITY_WORLDGEN:
        return version == mimita_worldgen::ABI_VERSION ? worldgenServices() : nullptr;
    case mimita_module::CAPABILITY_MESH:
        return version == mimita_world_rendering::ABI_VERSION
            ? worldRenderingServices() : nullptr;
    case mimita_module::CAPABILITY_COLLISION:
        return version == mimita_chunk_collision::ABI_VERSION
            ? chunkCollisionServices() : nullptr;
    default:
        return nullptr;
    }
}

bool ModuleHost::mapEditorLoaded() const
{
    return moduleLoaded("map-editor");
}

bool ModuleHost::mapEditorSetSelectedPosition(const float position[3])
{
    mimita_map_editor::EditorSetPositionV1 payload{};
    if (!position)
        return false;
    payload.position[0] = position[0];
    payload.position[1] = position[1];
    payload.position[2] = position[2];
    if (!invokeModule("map-editor", mimita_module::EVENT_EDITOR_SET_POSITION,
                      &payload, sizeof(payload)))
        return false;
    return payload.result != 0;
}

bool ModuleHost::mapEditorComputeDragDelta(const DragMathV1* input,
                                           float outDelta[3])
{
    if (!input || !outDelta)
        return false;
    mimita_map_editor::EditorDragDeltaV1 payload{};
    payload.input = *input;
    if (!invokeModule("map-editor", mimita_module::EVENT_EDITOR_DRAG_DELTA,
                      &payload, sizeof(payload)))
        return false;
    if (!payload.result)
        return false;
    outDelta[0] = payload.outDelta[0];
    outDelta[1] = payload.outDelta[1];
    outDelta[2] = payload.outDelta[2];
    return true;
}

bool ModuleHost::mapEditorPickGizmoAxis(const GizmoPickV1* input, int* outAxis)
{
    if (!input || !outAxis)
        return false;
    mimita_map_editor::EditorGizmoPickV1 payload{};
    payload.input = *input;
    if (!invokeModule("map-editor", mimita_module::EVENT_EDITOR_GIZMO_PICK,
                      &payload, sizeof(payload)))
        return false;
    if (!payload.result)
        return false;
    *outAxis = payload.outAxis;
    return true;
}

bool ModuleHost::mapEditorExecuteCommand(const char* command,
                                         const char* arguments)
{
    mimita_map_editor::EditorCommandV1 payload{};
    payload.command = command;
    payload.arguments = arguments;
    if (!invokeModule("map-editor", mimita_module::EVENT_EDITOR_COMMAND,
                      &payload, sizeof(payload)))
        return false;
    return payload.result != 0;
}

bool ModuleHost::mapEditorMapCursor(mimita_map_editor::EditorInputV1* input)
{
    if (!input)
        return false;
    if (!invokeModule("map-editor", mimita_module::EVENT_EDITOR_INPUT,
                      input, sizeof(*input)))
        return false;
    return input->result != 0;
}

bool ModuleHost::mapEditorResolveClick(mimita_map_editor::EditorClickV1* click)
{
    if (!click)
        return false;
    if (!invokeModule("map-editor", mimita_module::EVENT_EDITOR_CLICK,
                      click, sizeof(*click)))
        return false;
    return click->result != 0;
}

bool ModuleHost::mapEditorAddChunk(const char* requestedId, char* outId,
                                   std::uint32_t outCapacity)
{
    mimita_map_editor::EditorAddChunkV1 payload{};
    if (requestedId)
        std::snprintf(payload.id, sizeof(payload.id), "%s", requestedId);
    if (!invokeModule("map-editor", mimita_module::EVENT_EDITOR_ADD_CHUNK,
                      &payload, sizeof(payload)))
        return false;
    if (!payload.result || !outId || outCapacity == 0)
        return false;
    std::snprintf(outId, outCapacity, "%s", payload.outId);
    return true;
}

bool ModuleHost::mapEditorAddChunkPart(const char* kind, char* outId,
                                       std::uint32_t outCapacity)
{
    mimita_map_editor::EditorAddChunkPartV1 payload{};
    if (kind)
        std::snprintf(payload.kind, sizeof(payload.kind), "%s", kind);
    if (!invokeModule("map-editor", mimita_module::EVENT_EDITOR_ADD_CHUNK_PART,
                      &payload, sizeof(payload)))
        return false;
    if (!payload.result || !outId || outCapacity == 0)
        return false;
    std::snprintf(outId, outCapacity, "%s", payload.outId);
    return true;
}

bool ModuleHost::mapEditorAddConnector(const char* side)
{
    mimita_map_editor::EditorAddConnectorV1 payload{};
    if (side)
        std::snprintf(payload.side, sizeof(payload.side), "%s", side);
    if (!invokeModule("map-editor", mimita_module::EVENT_EDITOR_ADD_CONNECTOR,
                      &payload, sizeof(payload)))
        return false;
    return payload.result != 0;
}

bool ModuleHost::mapEditorPreviewConnections(char* report,
                                             std::uint32_t capacity)
{
    mimita_map_editor::EditorPreviewV1 payload{};
    if (!invokeModule("map-editor", mimita_module::EVENT_EDITOR_PREVIEW_CONNECTIONS,
                      &payload, sizeof(payload)))
        return false;
    if (report && capacity)
        std::snprintf(report, capacity, "%s", payload.report);
    return payload.result != 0;
}

bool ModuleHost::mapEditorEvaluateConnectorPair(
    const char* parentChunk, const char* parentConnector,
    const char* childChunk, const char* childConnector,
    float outTransform[16], char* reason, std::uint32_t reasonCapacity)
{
    return hostEvaluateConnectorPair(parentChunk, parentConnector, childChunk,
                                     childConnector, outTransform, reason,
                                     reasonCapacity) != 0;
}

bool ModuleHost::worldRenderingLoaded() const
{
    return moduleLoaded("world-rendering");
}

std::uint32_t ModuleHost::activeGeneration(const char* moduleName) const
{
    const ModuleEntry* entry = findModule(moduleName);
    return entry ? entry->activeGeneration : 0;
}

bool ModuleHost::worldRenderingAnalyzeFaces(
    const float transform[16], const float cameraPosition[3],
    mimita_world_rendering::FaceAuditV1* outFaces, std::uint32_t faceCapacity)
{
    if (!transform || !cameraPosition || !outFaces ||
        faceCapacity < mimita_world_rendering::FACE_COUNT)
        return false;
    mimita_world_rendering::AnalyzeFacesV1 payload{};
    std::memcpy(payload.transform, transform, sizeof(payload.transform));
    std::memcpy(payload.cameraPosition, cameraPosition,
                sizeof(payload.cameraPosition));
    payload.capacity = faceCapacity;
    if (!invokeModule("world-rendering",
                      mimita_module::EVENT_RENDER_ANALYZE_FACES, &payload,
                      sizeof(payload)))
        return false;
    if (!payload.result)
        return false;
    std::memcpy(outFaces, payload.faces,
                sizeof(mimita_world_rendering::FaceAuditV1) *
                    mimita_world_rendering::FACE_COUNT);
    return true;
}

bool ModuleHost::worldRenderingBuildBox(mimita_world_rendering::BoxMeshV1* request)
{
    if (!request)
        return false;
    return invokeModule("world-rendering", mimita_module::EVENT_RENDER_BUILD_BOX,
                        request, sizeof(*request));
}

bool ModuleHost::worldgenResolve(mimita_worldgen::ResolveV1* request)
{
    if (!request)
        return false;
    return invokeModule("infinite-worldgen",
                        mimita_module::EVENT_WORLDGEN_RESOLVE, request,
                        sizeof(*request));
}

bool ModuleHost::chunkCollisionBuild(
    mimita_chunk_collision::BuildChunkV1* request)
{
    if (!request)
        return false;
    return invokeModule("chunk-collision",
                        mimita_module::EVENT_COLLISION_BUILD_CHUNK, request,
                        sizeof(*request));
}

bool ModuleHost::collisionContactPolicy(
    mimita_collision_policy::ContactPolicyV1* policy)
{
    if (!policy)
        return false;
    return invokeModule("chunk-collision",
                        mimita_module::EVENT_COLLISION_CONTACT_POLICY, policy,
                        sizeof(*policy));
}

bool ModuleHost::collisionSurfacePolicy(
    mimita_collision_policy::SurfacePolicyV1* policy)
{
    if (!policy)
        return false;
    return invokeModule("chunk-collision",
                        mimita_module::EVENT_COLLISION_SURFACE_POLICY, policy,
                        sizeof(*policy));
}

bool ModuleHost::npcBehaviorDecision(
    mimita_npc_behavior::DecisionV1* decision)
{
    if (!decision)
        return false;
    return invokeModule("npc-behavior",
                        mimita_module::EVENT_NPC_BEHAVIOR_DECISION,
                        decision, sizeof(*decision), decision->tick);
}

bool ModuleHost::npcBehaviorLoaded() const
{
    return moduleLoaded("npc-behavior");
}

bool ModuleHost::loadCatalog(bool serverProcess)
{
    mModules.clear();
    const std::filesystem::path root = std::filesystem::current_path();
    const std::filesystem::path catalogPath =
        root / "config" / "modules" / "modules.json";
    const std::string text = readTextFile(catalogPath);
    if (text.empty()) {
        rememberError("missing config/modules/modules.json");
        return false;
    }

    nlohmann::json rootJson = nlohmann::json::parse(text, nullptr, false);
    if (rootJson.is_discarded() || !rootJson.is_object())
        return false;

    for (const auto& item : rootJson.value("modules", nlohmann::json::array())) {
        if (!item.is_object())
            continue;
        if (item.value("server_process", false) != serverProcess)
            continue;
        ModuleEntry entry;
        entry.name = item.value("name", std::string{});
        if (entry.name.empty())
            continue;
        entry.abi = item.value("abi", mimita_module::ABI_VERSION);
        entry.stateSchema = item.value("state_schema", 1u);
        entry.activation = item.value("activation", std::string{});
        entry.outputDir = root / "build" / "modules" / entry.name;
        entry.currentPath = entry.outputDir / "current.json";
        entry.resultPath = entry.outputDir / "result.json";
        entry.depsPath = entry.outputDir / "deps.json";
        for (const auto& source : item.value("sources", nlohmann::json::array()))
            if (source.is_string())
                expandPatterns(root, source.get<std::string>(), entry.sources);
        for (const auto& header : item.value("headers", nlohmann::json::array()))
            if (header.is_string())
                expandPatterns(root, header.get<std::string>(), entry.headers);
        for (const auto& event : item.value("events", nlohmann::json::array()))
            if (event.is_number_unsigned())
                entry.events.push_back(event.get<std::uint32_t>());
        for (const auto& capability :
             item.value("capabilities", nlohmann::json::array()))
            if (capability.is_number_unsigned())
                entry.capabilities.push_back(capability.get<std::uint32_t>());
        mModules.push_back(std::move(entry));
    }
    return !mModules.empty();
}

bool ModuleHost::loadCurrent(ModuleEntry& entry)
{
    const std::string text = readTextFile(entry.currentPath);
    if (text.empty())
        return false;
    nlohmann::json current = nlohmann::json::parse(text, nullptr, false);
    if (current.is_discarded() || !current.is_object())
        return false;
    if (current.value("status", std::string{}) != "ok")
        return false;
    const std::uint32_t generation = current.value("generation", 0u);
    const std::string codeHash = current.value("code_hash", std::string{});
    const std::filesystem::path dll = current.value("dll", std::string{});
    if (generation == 0 || codeHash.empty() || !std::filesystem::exists(dll))
        return false;
    if (entry.api && generation == entry.activeGeneration &&
        codeHash == entry.activeCodeHash)
        return true;
    return loadCandidate(entry, dll, generation, codeHash);
}

bool ModuleHost::loadCandidate(ModuleEntry& entry,
                               const std::filesystem::path& dll,
                               std::uint32_t generation,
                               const std::string& expectedHash)
{
    const std::filesystem::path tempDll = uniqueLoadPath(entry, generation);
    std::error_code error;
    std::filesystem::copy_file(
        dll, tempDll, std::filesystem::copy_options::overwrite_existing, error);
    if (error) {
        rememberError("module candidate copy failed: " + error.message());
        writeEvent("module.validation.failed", "candidate DLL copy failed",
                   entry.name, "{\"module\":\"" + entry.name + "\"}");
        return false;
    }

    HMODULE candidateModule = LoadLibraryW(tempDll.wstring().c_str());
    if (!candidateModule) {
        rememberError("module candidate LoadLibrary failed");
        std::filesystem::remove(tempDll, error);
        writeEvent("module.validation.failed", "candidate LoadLibrary failed",
                   entry.name,
                   "{\"module\":\"" + entry.name + "\",\"error\":\"" +
                       std::to_string(GetLastError()) + "\"}");
        return false;
    }

    auto getAPI = reinterpret_cast<GetModuleAPIFn>(
        GetProcAddress(candidateModule, mimita_module::GET_MODULE_API_SYMBOL));
    const BaseV1* base = getAPI ? getAPI(mimita_module::ABI_VERSION) : nullptr;
    if (!base || base->abiVersion != mimita_module::ABI_VERSION ||
        !base->name || entry.name != base->name ||
        base->generation != generation ||
        !base->codeHash || expectedHash != base->codeHash ||
        base->structSize != sizeof(ModuleV1)) {
        rememberError("module candidate ABI/hash/name validation failed");
        FreeLibrary(candidateModule);
        std::filesystem::remove(tempDll, error);
        writeEvent("module.validation.failed",
                   "candidate ABI, name, generation, hash, or struct size mismatch",
                   entry.name,
                   "{\"module\":\"" + entry.name + "\",\"generation\":" +
                       std::to_string(generation) + "}");
        return false;
    }

    const auto* candidateApi = reinterpret_cast<const ModuleV1*>(base);
    if (base->stateSchema != entry.stateSchema ||
        !candidateApi->initialize || !candidateApi->shutdown ||
        !candidateApi->onEvent) {
        rememberError("module interface validation failed");
        FreeLibrary(candidateModule);
        std::filesystem::remove(tempDll, error);
        writeEvent("module.validation.failed",
                   "module lifecycle interface or state schema mismatch",
                   entry.name, "{\"module\":\"" + entry.name + "\"}");
        return false;
    }
    for (std::uint32_t capabilityId : entry.capabilities) {
        if (!capability(capabilityId, entry.abi)) {
            rememberError("module required capability is unavailable");
            FreeLibrary(candidateModule);
            std::filesystem::remove(tempDll, error);
            writeEvent("module.validation.failed",
                       "required host capability unavailable", entry.name,
                       nlohmann::json{{"module", entry.name},
                                      {"capability", capabilityId}}.dump());
            return false;
        }
    }
    if (!candidateApi->initialize(genericHostServices())) {
        rememberError("module initialize rejected host services");
        FreeLibrary(candidateModule);
        std::filesystem::remove(tempDll, error);
        writeEvent("module.validation.failed", "module initialize failed",
                   entry.name, "{\"module\":\"" + entry.name + "\"}");
        return false;
    }

    return activateEntry(entry, candidateModule, candidateApi, tempDll,
                         generation, expectedHash);
}

bool ModuleHost::activateEntry(ModuleEntry& entry, void* candidateModule,
                               const ModuleV1* candidateApi,
                               const std::filesystem::path& tempDll,
                               std::uint32_t generation,
                               const std::string& codeHash)
{
    std::vector<std::uint8_t> state;
    if (entry.api && entry.api->exportState) {
        const std::uint32_t size = entry.api->exportState(nullptr, 0);
        state.resize(size);
        if (size > 0)
            entry.api->exportState(state.data(), size);
    }

    const bool imported = !candidateApi->importState ||
        candidateApi->importState(state.empty() ? nullptr : state.data(),
                                  static_cast<std::uint32_t>(state.size()));
    if (!imported) {
        rememberError("module state migration rejected");
        if (candidateApi->shutdown)
            candidateApi->shutdown();
        FreeLibrary(static_cast<HMODULE>(candidateModule));
        std::error_code error;
        std::filesystem::remove(tempDll, error);
        writeEvent("module.rollback", "state migration failed; old module retained",
                   entry.name,
                   "{\"module\":\"" + entry.name + "\",\"candidate_generation\":" +
                       std::to_string(generation) + "}");
        return false;
    }

    void* oldModule = entry.moduleHandle;
    const std::uint32_t oldGeneration = entry.activeGeneration;
    const std::filesystem::path oldTemp = entry.loadedTempDll;

    if (entry.api && entry.api->shutdown)
        entry.api->shutdown();

    entry.moduleHandle = candidateModule;
    entry.api = candidateApi;
    entry.loadedTempDll = tempDll;
    entry.activeGeneration = generation;
    entry.activeCodeHash = codeHash;

    if (oldModule)
        FreeLibrary(static_cast<HMODULE>(oldModule));
    if (!oldTemp.empty())
        mRetiredPaths.push_back(oldTemp);

    writeEvent("module.activated", "candidate activated at safe module boundary",
               entry.name,
               nlohmann::json{
                   {"module", entry.name},
                   {"generation", generation},
                   {"previous_generation", oldGeneration},
                   {"code_hash", codeHash},
                   {"boundary", entry.activation},
                   {"pid", GetCurrentProcessId()},
                   {"changed_paths", entry.lastChangedPaths}}.dump());
    if (!mInStartup)
        notifyActivation(entry, generation, oldGeneration);
    entry.lastChangedPaths.clear();
    return true;
}

void ModuleHost::unloadEntry(ModuleEntry& entry)
{
    if (entry.api && entry.api->shutdown)
        entry.api->shutdown();
    if (entry.moduleHandle)
        FreeLibrary(static_cast<HMODULE>(entry.moduleHandle));
    if (!entry.loadedTempDll.empty()) {
        std::error_code error;
        std::filesystem::remove(entry.loadedTempDll, error);
    }
    entry.moduleHandle = nullptr;
    entry.api = nullptr;
    entry.loadedTempDll.clear();
}

void ModuleHost::retirePath(const std::filesystem::path& path)
{
    if (!path.empty())
        mRetiredPaths.push_back(path);
}

bool ModuleHost::requestBuild(ModuleEntry& entry)
{
    if (entry.buildInFlight || entry.workerProcess)
        return false;

    wchar_t pythonPath[MAX_PATH]{};
    DWORD length = GetEnvironmentVariableW(L"MIMITA_PYTHON", pythonPath, MAX_PATH);
    std::wstring python = (length > 0 && length < MAX_PATH)
        ? std::wstring(pythonPath) : std::wstring(L"python");

    const std::filesystem::path root = std::filesystem::current_path();
    const std::filesystem::path worker =
        root / "devscripts" / "module-build-worker.py";
    std::wstring command = L"\"" + python + L"\" \"" +
                           worker.wstring() + L"\" --module \"" +
                           std::wstring(entry.name.begin(), entry.name.end()) +
                           L"\"";

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, root.wstring().c_str(),
                        &startup, &process)) {
        rememberError("module build worker launch failed");
        writeEvent("module.build.failed", "worker launch failed", entry.name,
                   "{\"module\":\"" + entry.name + "\",\"error\":\"" +
                       std::to_string(GetLastError()) + "\"}");
        return false;
    }

    CloseHandle(process.hThread);
    entry.workerProcess = process.hProcess;
    entry.buildInFlight = true;
    writeEvent("module.build.started", "background module build requested",
               entry.name,
               "{\"module\":\"" + entry.name + "\",\"pid\":" +
                   std::to_string(GetCurrentProcessId()) + "}");
    return true;
}

bool ModuleHost::pollBuild(ModuleEntry& entry)
{
    return entry.buildInFlight;
}

void ModuleHost::refreshDependencies(ModuleEntry& entry)
{
    const std::string text = readTextFile(entry.depsPath);
    if (text.empty())
        return;
    nlohmann::json deps = nlohmann::json::parse(text, nullptr, false);
    if (deps.is_discarded() || !deps.is_array())
        return;
    for (const auto& item : deps) {
        if (!item.is_string())
            continue;
        const std::filesystem::path path = item.get<std::string>();
        if (std::find(entry.headers.begin(), entry.headers.end(), path) ==
            entry.headers.end())
            entry.headers.push_back(path);
    }
}

std::uint64_t ModuleHost::newestInputTime(const ModuleEntry& entry) const
{
    std::uint64_t newest = 0;
    for (const auto& source : entry.sources)
        newest = (std::max)(newest, fileWriteTime(source));
    for (const auto& header : entry.headers)
        newest = (std::max)(newest, fileWriteTime(header));
    return newest;
}

std::filesystem::path ModuleHost::uniqueLoadPath(const ModuleEntry& entry,
                                                 std::uint32_t generation)
{
    const DWORD pid = GetCurrentProcessId();
    ++mTempGeneration;
    return entry.outputDir /
        ("live-" + std::to_string(pid) + "-" + std::to_string(generation) + "-" +
         std::to_string(mTempGeneration) + ".dll");
}

void ModuleHost::writeEvent(const char* event, const char* reason,
                            const std::string& correlation,
                            const std::string& fieldsJson)
{
    nlohmann::json fields = nlohmann::json::parse(fieldsJson, nullptr, false);
    if (fields.is_discarded())
        fields = nlohmann::json::object();
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        event ? event : "module.event",
        correlation,
        reason ? reason : "",
        0,
        fields,
        __FILE__, __LINE__, __FUNCTION__);
}

std::string ModuleHost::joinedPaths(const ModuleEntry& entry) const
{
    auto normalized = [](const std::filesystem::path& path) {
        std::error_code error;
        const auto absolute = std::filesystem::absolute(path, error);
        return (error ? path : absolute).lexically_normal().string();
    };
    const std::vector<std::string>* paths = &entry.lastChangedPaths;
    std::string joined;
    if (paths->empty())
        paths = nullptr;
    if (!paths) {
        for (const auto& path : entry.sources) {
            if (!joined.empty())
                joined += "\n";
            joined += normalized(path);
        }
        return joined;
    }
    for (const auto& path : *paths) {
        if (!joined.empty())
            joined += "\n";
        joined += path;
    }
    return joined;
}

void ModuleHost::notifyActivation(const ModuleEntry& entry,
                                  std::uint32_t generation,
                                  std::uint32_t previousGeneration)
{
    const std::string paths = joinedPaths(entry);
    std::string message =
        "Generation " + std::to_string(generation) + " active";
    if (previousGeneration > 0)
        message += " (replaced " + std::to_string(previousGeneration) + ")";
    if (!paths.empty())
        message += "\n" + paths;
    const std::string title = "Hot reload: " + entry.name;
    NotificationSystem::instance().pushHotReload(title, message);
    writeEvent("module.notice.shown", "successful hot reload notice",
               entry.name,
               nlohmann::json{{"module", entry.name},
                              {"generation", generation},
                              {"previous_generation", previousGeneration},
                              {"accent", "hot_reload"},
                              {"message", message}}.dump());
}

void ModuleHost::notifyBuildFailure(const ModuleEntry& entry,
                                    const std::string& details)
{
    std::string failedPath;
    std::string firstError;
    size_t cursor = 0;
    while (cursor < details.size()) {
        const size_t end = details.find('\n', cursor);
        const std::string line = details.substr(
            cursor, end == std::string::npos ? std::string::npos : end - cursor);
        if (failedPath.empty() &&
            (line.find(".cpp:") != std::string::npos ||
             line.find(".h:") != std::string::npos)) {
            failedPath = line;
        }
        if (firstError.empty() &&
            line.find("error:") != std::string::npos) {
            firstError = line;
        }
        if (end == std::string::npos)
            break;
        cursor = end + 1;
    }
    std::string message =
        "Candidate generation was not activated. Previous generation " +
        std::to_string(entry.activeGeneration) + " remains active.";
    if (!failedPath.empty())
        message += "\n" + failedPath;
    if (!firstError.empty() && firstError != failedPath)
        message += "\n" + firstError;
    if (failedPath.empty() && !details.empty())
        message += "\n" + details.substr(0, std::min<size_t>(details.size(), 1200));
    if (failedPath.empty())
        failedPath = joinedPaths(entry);
    const std::string title = "Hot reload failed: " + entry.name;
    NotificationSystem::instance().pushHotReloadFailure(title, message);
    writeEvent("module.notice.shown", "hot reload failure notice",
               entry.name,
               nlohmann::json{{"module", entry.name},
                              {"active_generation", entry.activeGeneration},
                              {"accent", "critical"},
                              {"failed_path", failedPath},
                              {"message", message}}.dump());
}

void ModuleHost::rememberError(const std::string& error)
{
    mLastError = error;
    std::printf("[MODULE HOST] %s\n", error.c_str());
}
