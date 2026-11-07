// 2026-11-07
/* purpose
* implement the host side of the versioned module ABI: catalog loading,
* background build requests, candidate validation, activation, and rollback
* this file DOES NOT own editor behavior, map data, or rendering
*/
#include "hot-reload/module-host.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <system_error>
#include <vector>

#include <windows.h>

#include <nlohmann/json.hpp>

#include "debug/structured-log.h"
#include "gamemode/map-config.h"
#include "mapeditor/map-editor.h"
#include "modules/api/map-editor-api.h"
#include "modules/api/world-rendering-api.h"

namespace {

using mimita_map_editor::HostServicesV1;
using mimita_map_editor::ModuleV1;
using mimita_world_rendering::FaceAuditV1;
using WorldModuleV1 = mimita_world_rendering::ModuleV1;
using mimita_module::BaseV1;
using GetModuleAPIFn = mimita_module::GetModuleAPIFn;

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

const HostServicesV1* mapEditorHostServices()
{
    static const HostServicesV1 services = {
        mimita_map_editor::ABI_VERSION,
        sizeof(HostServicesV1),
        hostLogEvent,
        hostGetPrimarySelection,
        hostApplyPrimaryPosition,
        hostNotify,
        hostRevision,
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
    if (mStarted)
        return;
    mStarted = true;
    if (!loadCatalog()) {
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
        entry.lastResultJsonTime = fileWriteTime(entry.resultPath);
        if (!loadCurrent(entry)) {
            requestBuild(entry);
        }
    }
    writeEvent("module.host.started", "module host startup complete", "module-host",
               "{\"modules\":" + std::to_string(mModules.size()) + "}");
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
                std::string output = result.value("compiler_output", std::string{});
                if (output.size() > 4000)
                    output = output.substr(output.size() - 4000);
                writeEvent("module.build.failed", "module compiler failed",
                           entry.name,
                           nlohmann::json{
                               {"module", entry.name},
                               {"generation", result.value("generation", 0u)},
                               {"code_hash", result.value("code_hash", std::string{})},
                               {"exit_code", result.value("exit_code", -1)},
                               {"compiler_output_tail", output}}.dump());
            }
        }

        const std::uint64_t newest = newestInputTime(entry);
        if (newest > entry.lastObservedInputTime && !entry.buildInFlight) {
            entry.lastObservedInputTime = newest;
            requestBuild(entry);
        }
    }
}

bool ModuleHost::mapEditorLoaded() const
{
    for (const ModuleEntry& entry : mModules) {
        if (entry.name == "map-editor")
            return entry.api != nullptr;
    }
    return false;
}

bool ModuleHost::mapEditorSetSelectedPosition(const float position[3])
{
    for (ModuleEntry& entry : mModules) {
        if (entry.name != "map-editor" || !entry.api)
            continue;
        const auto* api = static_cast<const ModuleV1*>(entry.api);
        if (!api->setSelectedPosition)
            return false;
        return api->setSelectedPosition(position) != 0;
    }
    return false;
}

bool ModuleHost::worldRenderingLoaded() const
{
    for (const ModuleEntry& entry : mModules)
        if (entry.name == "world-rendering")
            return entry.api != nullptr;
    return false;
}

std::uint32_t ModuleHost::activeGeneration(const char* moduleName) const
{
    if (!moduleName)
        return 0;
    for (const ModuleEntry& entry : mModules)
        if (entry.name == moduleName)
            return entry.activeGeneration;
    return 0;
}

bool ModuleHost::worldRenderingAnalyzeFaces(
    const float transform[16], const float cameraPosition[3],
    FaceAuditV1* outFaces, std::uint32_t faceCapacity)
{
    for (ModuleEntry& entry : mModules) {
        if (entry.name != "world-rendering" || !entry.api)
            continue;
        auto* api = static_cast<WorldModuleV1*>(entry.api);
        if (!api->analyzeFaces)
            return false;
        return api->analyzeFaces(transform, cameraPosition, outFaces,
                                 faceCapacity) != 0;
    }
    return false;
}

bool ModuleHost::loadCatalog()
{
    mModules.clear();
    const std::filesystem::path root = std::filesystem::current_path();
    const std::filesystem::path catalogPath = root / "config" / "modules" / "modules.json";
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
        ModuleEntry entry;
        entry.name = item.value("name", std::string{});
        if (entry.name.empty())
            continue;
        entry.abi = item.value("abi", 1u);
        entry.stateSchema = item.value("state_schema", 1u);
        entry.outputDir = root / "build" / "modules" / entry.name;
        entry.currentPath = entry.outputDir / "current.json";
        entry.resultPath = entry.outputDir / "result.json";
        for (const auto& source : item.value("sources", nlohmann::json::array()))
            if (source.is_string())
                expandPatterns(root, source.get<std::string>(), entry.sources);
        for (const auto& header : item.value("headers", nlohmann::json::array()))
            if (header.is_string())
                expandPatterns(root, header.get<std::string>(), entry.headers);
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

bool ModuleHost::loadCandidate(ModuleEntry& entry, const std::filesystem::path& dll,
                               std::uint32_t generation, const std::string& expectedHash)
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
        !base->codeHash || expectedHash != base->codeHash) {
        rememberError("module candidate ABI/hash/name validation failed");
        FreeLibrary(candidateModule);
        std::filesystem::remove(tempDll, error);
        writeEvent("module.validation.failed",
                   "candidate ABI, name, generation, or hash mismatch",
                   entry.name,
                   "{\"module\":\"" + entry.name + "\",\"generation\":" +
                       std::to_string(generation) + "}");
        return false;
    }

    const bool isMapEditor = entry.name == "map-editor";
    const bool isWorldRendering = entry.name == "world-rendering";
    if (!isMapEditor && !isWorldRendering) {
        rememberError("module has no host adapter");
        FreeLibrary(candidateModule);
        std::filesystem::remove(tempDll, error);
        writeEvent("module.validation.failed", "module has no host adapter",
                   entry.name, "{\"module\":\"" + entry.name + "\"}");
        return false;
    }

    if (isMapEditor) {
        const auto* candidateApi = reinterpret_cast<const ModuleV1*>(base);
        if (base->structSize != sizeof(ModuleV1) ||
            base->stateSchema != entry.stateSchema ||
            !candidateApi->initialize || !candidateApi->shutdown ||
            !candidateApi->setSelectedPosition ||
            !candidateApi->initialize(mapEditorHostServices())) {
            rememberError("map-editor interface validation failed");
            FreeLibrary(candidateModule);
            std::filesystem::remove(tempDll, error);
            writeEvent("module.validation.failed",
                       "map-editor interface or state schema mismatch",
                       entry.name, "{\"module\":\"" + entry.name + "\"}");
            return false;
        }
    } else {
        const auto* candidateApi = reinterpret_cast<const WorldModuleV1*>(base);
        static const mimita_world_rendering::HostServicesV1 services = {
            mimita_world_rendering::ABI_VERSION,
            sizeof(mimita_world_rendering::HostServicesV1),
            hostLogEvent,
        };
        if (base->structSize != sizeof(WorldModuleV1) ||
            base->stateSchema != entry.stateSchema ||
            !candidateApi->initialize || !candidateApi->shutdown ||
            !candidateApi->analyzeFaces ||
            !candidateApi->initialize(&services)) {
            rememberError("world-rendering interface validation failed");
            FreeLibrary(candidateModule);
            std::filesystem::remove(tempDll, error);
            writeEvent("module.validation.failed",
                       "world-rendering interface or state schema mismatch",
                       entry.name, "{\"module\":\"" + entry.name + "\"}");
            return false;
        }
    }

    return activateMapEditor(entry, candidateModule, const_cast<void*>(static_cast<const void*>(base)),
                             tempDll, generation, expectedHash);
}

bool ModuleHost::activateMapEditor(ModuleEntry& entry, void* candidateModule,
                                   void* candidateApi, const std::filesystem::path& tempDll,
                                   std::uint32_t generation, const std::string& codeHash)
{
    const bool isMapEditor = entry.name == "map-editor";
    auto* nextMap = isMapEditor ? static_cast<ModuleV1*>(candidateApi) : nullptr;
    auto* nextWorld = isMapEditor ? nullptr : static_cast<WorldModuleV1*>(candidateApi);
    auto* previousMap = (isMapEditor && entry.api)
        ? static_cast<ModuleV1*>(entry.api) : nullptr;
    auto* previousWorld = (!isMapEditor && entry.api)
        ? static_cast<WorldModuleV1*>(entry.api) : nullptr;

    auto exportState = [&](std::vector<std::uint8_t>& state) {
        std::uint32_t size = 0;
        if (previousMap && previousMap->exportState)
            size = previousMap->exportState(nullptr, 0);
        else if (previousWorld && previousWorld->exportState)
            size = previousWorld->exportState(nullptr, 0);
        state.resize(size);
        if (size > 0) {
            if (previousMap && previousMap->exportState)
                previousMap->exportState(state.data(), size);
            else if (previousWorld && previousWorld->exportState)
                previousWorld->exportState(state.data(), size);
        }
    };

    std::vector<std::uint8_t> state;
    if (entry.api)
        exportState(state);

    const bool imported = isMapEditor
        ? (!nextMap->importState || nextMap->importState(
              state.empty() ? nullptr : state.data(),
              static_cast<std::uint32_t>(state.size())))
        : (!nextWorld->importState || nextWorld->importState(
              state.empty() ? nullptr : state.data(),
              static_cast<std::uint32_t>(state.size())));
    if (!imported) {
        rememberError("module state migration rejected");
        if (isMapEditor) {
            if (nextMap->shutdown) nextMap->shutdown();
        } else if (nextWorld->shutdown) {
            nextWorld->shutdown();
        }
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

    if (previousMap && previousMap->shutdown)
        previousMap->shutdown();
    if (previousWorld && previousWorld->shutdown)
        previousWorld->shutdown();

    entry.moduleHandle = candidateModule;
    entry.api = isMapEditor ? static_cast<void*>(nextMap)
                            : static_cast<void*>(nextWorld);
    entry.loadedTempDll = tempDll;
    entry.activeGeneration = generation;
    entry.activeCodeHash = codeHash;

    if (oldModule)
        FreeLibrary(static_cast<HMODULE>(oldModule));
    if (!oldTemp.empty())
        mRetiredPaths.push_back(oldTemp);

    writeEvent("module.activated", "candidate activated at main-thread boundary",
               entry.name,
               "{\"module\":\"" + entry.name + "\",\"generation\":" +
                   std::to_string(generation) + ",\"previous_generation\":" +
                   std::to_string(oldGeneration) + ",\"code_hash\":\"" +
                   codeHash + "\",\"boundary\":\"before_frame\",\"pid\":" +
                   std::to_string(GetCurrentProcessId()) + "}");
    return true;
}

void ModuleHost::unloadEntry(ModuleEntry& entry)
{
    if (entry.api) {
        if (entry.name == "map-editor") {
            auto* api = static_cast<ModuleV1*>(entry.api);
            if (api->shutdown)
                api->shutdown();
        } else {
            auto* api = static_cast<WorldModuleV1*>(entry.api);
            if (api->shutdown)
                api->shutdown();
        }
    }
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
    const std::filesystem::path worker = root / "devscripts" / "module-build-worker.py";
    std::wstring command = L"\"" + python + L"\" \"" +
                           worker.wstring() + L"\" --module \"" +
                           std::wstring(entry.name.begin(), entry.name.end()) + L"\"";

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

void ModuleHost::rememberError(const std::string& error)
{
    mLastError = error;
    std::printf("[MODULE HOST] %s\n", error.c_str());
}
