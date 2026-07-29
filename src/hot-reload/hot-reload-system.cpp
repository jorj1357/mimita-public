#include "hot-reload/hot-reload-system.h"

#include <cstdio>
#include <cstdlib>
#include <system_error>

#include <windows.h>

namespace {

void MIMITA_GAME_CALL platformLog(const char* message)
{
    if (message)
        std::printf("%s\n", message);
}

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

}

HotReloadSystem& HotReloadSystem::instance()
{
    static HotReloadSystem system;
    return system;
}

HotReloadSystem::HotReloadSystem()
{
    sourceDLL_ = std::filesystem::current_path() / "build" / "mimita-game.dll";
    memory_.apiVersion = MIMITA_GAME_API_VERSION;
    memory_.platform.version = MIMITA_GAME_API_VERSION;
    memory_.platform.log = platformLog;
}

HotReloadSystem::~HotReloadSystem()
{
    unloadGameDLL();
    deleteRetiredTempDLLs();
}

bool HotReloadSystem::loadGameDLL()
{
    if (!std::filesystem::exists(sourceDLL_)) {
        return false;
    }
    return loadCandidate(sourceDLL_);
}

bool HotReloadSystem::loadCandidate(const std::filesystem::path& sourceDLL)
{
    const std::filesystem::path tempDLL = makeUniqueTempDLLPath();
    std::error_code error;
    std::filesystem::copy_file(
        sourceDLL, tempDLL, std::filesystem::copy_options::overwrite_existing, error);
    if (error) {
        std::printf("[HOT RELOAD] reload failed: DLL copy: %s\n", error.message().c_str());
        return false;
    }

    std::printf("[HOT RELOAD] loading new DLL %s\n", tempDLL.string().c_str());
    HMODULE candidateModule = LoadLibraryW(tempDLL.wstring().c_str());
    if (!candidateModule) {
        std::printf("[HOT RELOAD] reload failed: LoadLibrary error=%lu\n", GetLastError());
        std::filesystem::remove(tempDLL, error);
        return false;
    }

    auto getGameAPI = reinterpret_cast<GetGameAPIFn>(
        GetProcAddress(candidateModule, "GetGameAPI"));
    GameAPI candidateAPI{};
    if (!getGameAPI ||
        !getGameAPI(MIMITA_GAME_API_VERSION, &candidateAPI) ||
        candidateAPI.version != MIMITA_GAME_API_VERSION ||
        candidateAPI.structSize != sizeof(GameAPI) ||
        !candidateAPI.updateEffects) {
        std::printf("[HOT RELOAD] reload failed: incompatible or incomplete GameAPI\n");
        FreeLibrary(candidateModule);
        std::filesystem::remove(tempDLL, error);
        return false;
    }

    if (candidateAPI.onReload && !candidateAPI.onReload(&memory_)) {
        std::printf("[HOT RELOAD] reload failed: onReload rejected persistent memory\n");
        FreeLibrary(candidateModule);
        std::filesystem::remove(tempDLL, error);
        return false;
    }

    HMODULE previousModule = static_cast<HMODULE>(module_);
    GameAPI previousAPI = api_;
    std::filesystem::path previousTempDLL = loadedTempDLL_;

    module_ = candidateModule;
    api_ = candidateAPI;
    loadedTempDLL_ = tempDLL;
    loadedDLLWriteTime_ = fileWriteTime(sourceDLL);
    ++memory_.reloadCount;

    if (previousModule) {
        std::printf("[HOT RELOAD] unloading old DLL\n");
        if (previousAPI.beforeUnload)
            previousAPI.beforeUnload(&memory_);
        FreeLibrary(previousModule);
        retiredTempDLLs_.push_back(previousTempDLL);
    }

    deleteRetiredTempDLLs();
    std::printf("[HOT RELOAD] reload success generation=%u\n", memory_.reloadCount);
    return true;
}

void HotReloadSystem::unloadGameDLL()
{
    if (!module_)
        return;

    std::printf("[HOT RELOAD] unloading old DLL\n");
    if (api_.beforeUnload)
        api_.beforeUnload(&memory_);
    FreeLibrary(static_cast<HMODULE>(module_));
    retiredTempDLLs_.push_back(loadedTempDLL_);
    module_ = nullptr;
    api_ = {};
    loadedTempDLL_.clear();
    deleteRetiredTempDLLs();
}

bool HotReloadSystem::reloadGameDLLIfChanged()
{
    if (!module_) {
        loadGameDLL();
        return module_ != nullptr;
    }
    rebuildIfSourcesChanged();

    const std::uint64_t writeTime = getDLLWriteTime();
    if (writeTime == 0 || writeTime == loadedDLLWriteTime_)
        return false;

    std::printf("[HOT RELOAD] detected source change\n");
    return loadCandidate(sourceDLL_);
}

bool HotReloadSystem::rebuildIfSourcesChanged()
{
    if (rebuildInProgress_)
        return false;

    const std::uint64_t newest = newestSourceWriteTime();
    if (observedSourceWriteTime_ == 0) {
        observedSourceWriteTime_ = newest;
        return false;
    }
    if (newest <= observedSourceWriteTime_)
        return false;

    observedSourceWriteTime_ = newest;
    rebuildInProgress_ = true;
    std::printf("[HOT RELOAD] detected source change\n");
    std::printf("[HOT RELOAD] rebuilding DLL\n");
    const int result = std::system("python build_game_dll.py");
    rebuildInProgress_ = false;
    if (result != 0) {
        std::printf("[HOT RELOAD] reload failed: DLL rebuild exit=%d\n", result);
        return false;
    }
    return true;
}

std::uint64_t HotReloadSystem::newestSourceWriteTime() const
{
    const std::filesystem::path root = std::filesystem::current_path();
    const std::filesystem::path sources[] = {
        root / "src" / "effects" / "effect-part.cpp",
        root / "src" / "hot-reload" / "game-api.h",
    };

    std::uint64_t newest = 0;
    for (const auto& source : sources)
        newest = (std::max)(newest, fileWriteTime(source));
    return newest;
}

std::filesystem::path HotReloadSystem::makeUniqueTempDLLPath()
{
    const DWORD processId = GetCurrentProcessId();
    ++tempGeneration_;
    return sourceDLL_.parent_path() /
        ("mimita-game-live-" + std::to_string(processId) + "-" +
         std::to_string(tempGeneration_) + ".dll");
}

void HotReloadSystem::deleteRetiredTempDLLs()
{
    std::error_code error;
    for (auto it = retiredTempDLLs_.begin(); it != retiredTempDLLs_.end();) {
        error.clear();
        if (it->empty() || std::filesystem::remove(*it, error) || !std::filesystem::exists(*it))
            it = retiredTempDLLs_.erase(it);
        else
            ++it;
    }
}

std::uint64_t HotReloadSystem::getDLLWriteTime() const
{
    return fileWriteTime(sourceDLL_);
}

const GameAPI* HotReloadSystem::gameAPI() const
{
    return module_ ? &api_ : nullptr;
}

GameMemory& HotReloadSystem::gameMemory()
{
    return memory_;
}

bool HotReloadSystem::loaded() const
{
    return module_ != nullptr;
}
