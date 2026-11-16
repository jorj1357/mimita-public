// 2026-11-16
// Game-owned replay export entry for the resident export subprocess.

#include "host/abi/game-host-abi.h"

#include <cstdint>

#include "engine/engine.h"
#include "main-init.h"
#include "replay/replay-export.h"
#include "replay/replay-export-subprocess.h"

namespace mimita::game {

int runGameExport(const char* clipPath, const char* outputPath, int width,
                  int height, int visible, int verbose) {
    (void)visible;
    gReplayExportVerbose = verbose != 0;
    Engine engine;
    gameInit(0, nullptr, engine);
    runExportSubprocess(engine, clipPath, outputPath, width, height);
    return getReplayExportJob().state == ReplayExportJob::Done ? 0 : 1;
}

}  // namespace mimita::game

extern "C" __declspec(dllexport) const mimita::host::GameExportV1*
GetMimitaGameExportV1(std::uint32_t abiVersion) {
    if (abiVersion != mimita::host::kGameExportAbiVersion) {
        return nullptr;
    }
    static const mimita::host::GameExportV1 api = [] {
        mimita::host::GameExportV1 exportApi{};
        exportApi.abiVersion = mimita::host::kGameExportAbiVersion;
        exportApi.structSize = static_cast<std::uint32_t>(sizeof(exportApi));
        exportApi.run_export = &mimita::game::runGameExport;
        return exportApi;
    }();
    return &api;
}
