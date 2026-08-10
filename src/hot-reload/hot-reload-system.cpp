#include "hot-reload/hot-reload-system.h"

#include <cstdio>
#include <cstdlib>
#include <system_error>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <dlfcn.h>
#endif
#include <cstdint>
#include <filesystem>
#include <chrono>
namespace {

void MIMITA_GAME_CALL platformLog(const char* message)
{
    if (message)
        std::printf("%s\n", message);
}

std::uint64_t fileWriteTime(const std::filesystem::path& path)
{
    try {
        auto time = std::filesystem::last_write_time(path);
        return static_cast<std::uint64_t>(
            time.time_since_epoch().count()
        );
    } catch (...) {
        return 0;
    }
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
        std::printf("[HOT RELOAD] reload failed: missing %s\n", sourceDLL_.string().c_str());
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
#ifdef _WIN32
    void* candidateModule = static_cast<void*>(LoadLibraryW(tempDLL.wstring().c_str()));
#else
    void* candidateModule = dlopen(tempDLL.string().c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
    if (!candidateModule) {
#ifdef _WIN32
        std::printf("[HOT RELOAD] reload failed: LoadLibrary error=%lu\n", GetLastError());
#else
        std::printf("[HOT RELOAD] reload failed: dlopen: %s\n", dlerror());
#endif
        std::filesystem::remove(tempDLL, error);
        return false;
    }

#ifdef _WIN32
    auto getGameAPI = reinterpret_cast<GetGameAPIFn>(
        GetProcAddress(static_cast<HMODULE>(candidateModule), "GetGameAPI"));
#else
    auto getGameAPI = reinterpret_cast<GetGameAPIFn>(
        dlsym(candidateModule, "GetGameAPI"));
#endif
    GameAPI candidateAPI{};
    if (!getGameAPI ||
        !getGameAPI(MIMITA_GAME_API_VERSION, &candidateAPI) ||
        candidateAPI.version != MIMITA_GAME_API_VERSION ||
        candidateAPI.structSize != sizeof(GameAPI) ||
        !candidateAPI.updateEffects) {
        std::printf("[HOT RELOAD] reload failed: incompatible or incomplete GameAPI\n");
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(candidateModule));
#else
        dlclose(candidateModule);
#endif
        std::filesystem::remove(tempDLL, error);
        return false;
    }

    if (candidateAPI.onReload && !candidateAPI.onReload(&memory_)) {
        std::printf("[HOT RELOAD] reload failed: onReload rejected persistent memory\n");
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(candidateModule));
#else
        dlclose(candidateModule);
#endif
        std::filesystem::remove(tempDLL, error);
        return false;
    }

    void* previousModule = module_;
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
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(previousModule));
#else
        dlclose(previousModule);
#endif
        retiredTempDLLs_.push_back(previousTempDLL);
    }

    deleteRetiredTempDLLs();
    std::printf("[HOT RELOAD] reload success generation=%u\n", memory_.reloadCount);
    return true;
}void HotReloadSystem::unloadGameDLL()
{
    if (!module_)
        return;
    std::printf("[HOT RELOAD] unloading old DLL\n");
    if (api_.beforeUnload)
        api_.beforeUnload(&memory_);
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(module_));
#else
    dlclose(module_);
#endif
    retiredTempDLLs_.push_back(loadedTempDLL_);
    module_ = nullptr;
    api_ = {};
    loadedTempDLL_.clear();
    deleteRetiredTempDLLs();
}

bool HotReloadSystem::reloadGameDLLIfChanged()
{
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
#ifdef _WIN32
    const DWORD processId = GetCurrentProcessId();
#else
    const pid_t processId = getpid();
#endif
    ++tempGeneration_;
#ifdef _WIN32
    const char* extension = ".dll";
#else
    const char* extension = ".so";
#endif
    return sourceDLL_.parent_path() /
        ("mimita-game-live-" + std::to_string(processId) + "-" +
         std::to_string(tempGeneration_) + extension);
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
