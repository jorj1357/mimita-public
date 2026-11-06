// 2026-11-05
#include "worldgen/worldgen-commands.h"

#include <exception>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "debug/structured-log.h"
#include "devtools/terminal.h"
#include "gamemode/map-config.h"
#include "worldgen/worldgen-library.h"

using MimitaWorldGen::WorldGenLibrary;
using MimitaWorldGen::WorldGenMode;

namespace {

void log(const std::string& message)
{
    Terminal::instance().addLog(message);
}

bool parseVec3(const std::vector<std::string>& args, size_t first,
               glm::vec3& out)
{
    if (first + 2 >= args.size()) return false;
    try {
        out = glm::vec3(std::stof(args[first]), std::stof(args[first + 1]),
                        std::stof(args[first + 2]));
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool parseInt3(const std::vector<std::string>& args, size_t first,
               glm::ivec3& out)
{
    if (first + 2 >= args.size()) return false;
    try {
        out = glm::ivec3(std::stoi(args[first]), std::stoi(args[first + 1]),
                         std::stoi(args[first + 2]));
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

AuthoredChunk* findChunk(const std::string& id)
{
    for (auto& chunk : MapConfigRegistry::instance().chunksMutable())
        if (chunk.id == id) return &chunk;
    return nullptr;
}

GenerationZone* findZone(const std::string& id)
{
    for (auto& zone : MapConfigRegistry::instance().generationZonesMutable())
        if (zone.id == id) return &zone;
    return nullptr;
}

int faceIndexFromName(const std::string& name)
{
    if (name == "+x" || name == "x+") return 0;
    if (name == "-x" || name == "x-") return 1;
    if (name == "+y" || name == "y+") return 2;
    if (name == "-y" || name == "y-") return 3;
    if (name == "+z" || name == "z+") return 4;
    if (name == "-z" || name == "z-") return 5;
    return -1;
}

void logEvent(const char* name, const std::string& subject,
              const nlohmann::json& fields)
{
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important, name, subject,
        "worldgen authoring", 0, fields, __FILE__, __LINE__, __func__);
}

} // namespace

void registerWorldGenCommands()
{
    Terminal& terminal = Terminal::instance();

    terminal.registerCommand({
        "worldmode",
        "Manage per-mode world-generation JSON profiles",
        "worldmode <list|ensure|load <id>|save <id>|grid <id> <plane>|cell <id> <x> <y> <z>|size <id> <x> <y> <z>|radius <id> <x> <y> <z>|spacing <id> <center|connector>|chunks <id> [file...]|branch <id> <depth>|cycles <id> <0|1>>",
        [](const std::vector<std::string>& args) {
            WorldGenLibrary& lib = WorldGenLibrary::instance();
            if (args.empty() || args[0] == "list") {
                std::vector<std::string> modes;
                std::string error;
                lib.listModes(modes, &error);
                for (const auto& mode : modes) log("[WORLDMODE] " + mode);
                return;
            }
            if (args[0] == "ensure") {
                std::string error;
                if (lib.ensureDefaults(&error)) log("[WORLDMODE] defaults ready");
                else log("[WORLDMODE] " + error);
                return;
            }
            if (args[0] == "load" && args.size() >= 2) {
                WorldGenMode mode;
                std::string error;
                if (lib.loadMode(args[1], mode, &error)) {
                    log("[WORLDMODE] loaded " + mode.id + " chunks=" +
                        std::to_string(mode.chunkFiles.size()) + " plane=" +
                        mode.grid.plane + " spacing=" + mode.grid.spacingMode);
                } else log("[WORLDMODE] " + error);
                return;
            }
            if (args.size() < 3) {
                log("[WORLDMODE] see help");
                return;
            }
            const std::string& action = args[0];
            const std::string& id = args[1];
            WorldGenMode mode;
            std::string error;
            if (!lib.loadMode(id, mode, &error)) {
                mode.id = id;
                mode.grid.cellSize = glm::vec3(10.0f);
                mode.grid.chunkSize = glm::vec3(10.0f);
                mode.grid.streamRadius = glm::ivec3(2, 2, 0);
            }
            bool changed = false;
            if (action == "grid") {
                mode.grid.plane = args[2];
                mode.grid.repeatAxes = glm::ivec3(1, 1, 0);
                changed = true;
            } else if (action == "cell") {
                changed = parseVec3(args, 2, mode.grid.cellSize);
            } else if (action == "size") {
                changed = parseVec3(args, 2, mode.grid.chunkSize);
            } else if (action == "radius") {
                changed = parseInt3(args, 2, mode.grid.streamRadius);
            } else if (action == "spacing") {
                mode.grid.spacingMode = args[2] == "connector" ? "connector" : "center";
                changed = true;
            } else if (action == "branch") {
                try { mode.maxBranchDepth = std::stoi(args[2]); changed = true; }
                catch (const std::exception&) {}
            } else if (action == "cycles") {
                mode.allowCycles = args[2] != "0";
                changed = true;
            } else if (action == "chunks") {
                mode.chunkFiles.clear();
                for (size_t i = 2; i < args.size(); ++i)
                    mode.chunkFiles.push_back(WorldGenLibrary::chunkPath(args[i]));
                changed = true;
            }
            if (changed && lib.saveMode(mode, &error)) {
                logEvent("worldgen.mode.saved", id, {{"mode", id}});
                log("[WORLDMODE] saved " + id);
            } else {
                log("[WORLDMODE] no change or save failed: " + error);
            }
        },
        "2026-11-05", CommandCategory::Debug
    });

    terminal.registerCommand({
        "worldchunk",
        "Author a reusable primitive chunk and save it to the shared library",
        "worldchunk <add <id>|defaults|list|save <id>|part <chunk> <partId> <kind> <x> <y> <z> <sx> <sy> <sz> [material]|face <chunk> <partId> <+x|-x|+y|-y|+z|-z> <material>>",
        [](const std::vector<std::string>& args) {
            WorldGenLibrary& lib = WorldGenLibrary::instance();
            MapConfigRegistry& registry = MapConfigRegistry::instance();
            if (args.empty()) { log("[WORLDCHUNK] add|defaults|list|save|part|face"); return; }
            if (args[0] == "defaults") {
                std::string error;
                if (lib.ensureDefaults(&error)) log("[WORLDCHUNK] defaults ready");
                else log("[WORLDCHUNK] " + error);
                return;
            }
            if (args[0] == "list") {
                std::vector<std::string> files;
                std::string error;
                lib.listChunkFiles(files, &error);
                for (const auto& file : files) log("[WORLDCHUNK] " + file);
                return;
            }
            if (args[0] == "save" && args.size() >= 2) {
                AuthoredChunk* chunk = findChunk(args[1]);
                if (!chunk) { log("[WORLDCHUNK] chunk not found"); return; }
                std::string error;
                if (lib.saveChunk(*chunk, &error)) log("[WORLDCHUNK] saved " + args[1]);
                else log("[WORLDCHUNK] " + error);
                return;
            }
            if (args[0] == "add" && args.size() >= 2) {
                AuthoredChunk chunk;
                chunk.id = args[1];
                chunk.boundsMin = glm::vec3(-5.0f, -5.0f, 0.0f);
                chunk.boundsMax = glm::vec3(5.0f, 5.0f, 4.0f);
                registry.chunksMutable().push_back(chunk);
                registry.markChanged();
                logEvent("worldgen.chunk.added", args[1], {{"chunk", args[1]}});
                log("[WORLDCHUNK] added " + args[1]);
                return;
            }
            if (args[0] == "part" && args.size() >= 9) {
                AuthoredChunk* chunk = findChunk(args[1]);
                if (!chunk) { log("[WORLDCHUNK] chunk not found"); return; }
                ChunkPart part;
                part.id = args[2];
                part.kind = args[3];
                if (!parseVec3(args, 4, part.position)) { log("[WORLDCHUNK] bad pos"); return; }
                if (!parseVec3(args, 7, part.size)) { log("[WORLDCHUNK] bad size"); return; }
                if (args.size() >= 10) part.material = args[9];
                chunk->parts.push_back(std::move(part));
                registry.markChanged();
                logEvent("worldgen.chunk.part-added", args[1],
                         {{"chunk", args[1]}, {"part", args[2]}, {"kind", args[3]}});
                log("[WORLDCHUNK] part " + args[2] + " added to " + args[1]);
                return;
            }
            if (args[0] == "face" && args.size() >= 5) {
                AuthoredChunk* chunk = findChunk(args[1]);
                if (!chunk) { log("[WORLDCHUNK] chunk not found"); return; }
                int face = faceIndexFromName(args[3]);
                if (face < 0) { log("[WORLDCHUNK] bad face"); return; }
                for (auto& part : chunk->parts) {
                    if (part.id == args[2]) {
                        part.faceMaterials[face] = args[4];
                        registry.markChanged();
                        logEvent("worldgen.chunk.face-material", args[1],
                                 {{"chunk", args[1]}, {"part", args[2]},
                                  {"face", face}, {"material", args[4]}});
                        log("[WORLDCHUNK] face material set");
                        return;
                    }
                }
                log("[WORLDCHUNK] part not found");
                return;
            }
            log("[WORLDCHUNK] unknown command");
        },
        "2026-11-05", CommandCategory::Debug
    });

    terminal.registerCommand({
        "worldconnector",
        "Author a chunk connection point",
        "worldconnector <add <chunk> <id> <side> <orientation> <x> <y> <z> <role> [tag...]|list <chunk>>",
        [](const std::vector<std::string>& args) {
            MapConfigRegistry& registry = MapConfigRegistry::instance();
            if (args.size() < 2) { log("[WORLDCONNECTOR] add <chunk> ..."); return; }
            if (args[0] == "list") {
                AuthoredChunk* chunk = findChunk(args[1]);
                if (!chunk) { log("[WORLDCONNECTOR] chunk not found"); return; }
                for (const auto& c : chunk->connectors)
                    log("[WORLDCONNECTOR] " + c.id + " side=" + c.side +
                        " role=" + c.role + " orientation=" + c.orientation);
                return;
            }
            if (args[0] == "add" && args.size() >= 10) {
                AuthoredChunk* chunk = findChunk(args[1]);
                if (!chunk) { log("[WORLDCONNECTOR] chunk not found"); return; }
                AuthoredChunkConnector connector;
                connector.id = args[2];
                connector.side = args[3];
                connector.orientation = args[4];
                if (!parseVec3(args, 5, connector.position)) {
                    log("[WORLDCONNECTOR] bad position"); return;
                }
                connector.role = args[8];
                for (size_t i = 9; i < args.size(); ++i)
                    connector.tags.push_back(args[i]);
                chunk->connectors.push_back(std::move(connector));
                registry.markChanged();
                logEvent("worldgen.connector.added", args[1],
                         {{"chunk", args[1]}, {"connector", args[2]},
                          {"side", args[3]}, {"role", args[8]}});
                log("[WORLDCONNECTOR] added " + args[2] + " to " + args[1]);
                return;
            }
            log("[WORLDCONNECTOR] usage");
        },
        "2026-11-05", CommandCategory::Debug
    });

    terminal.registerCommand({
        "worldzone",
        "Author and tune a generation zone",
        "worldzone <add <id> <seed> <version>|grid <id> <xy|xz|yz|all>|origin <id> <x> <y> <z>|cell <id> <x> <y> <z>|size <id> <x> <y> <z>|radius <id> <x> <y> <z>|spacing <id> <center|connector>|mode <id> <modeId>|axes <id> <x> <y> <z>|list|save>",
        [](const std::vector<std::string>& args) {
            MapConfigRegistry& registry = MapConfigRegistry::instance();
            if (args.empty() || args[0] == "list") {
                for (const auto& zone : registry.generationZones())
                    log("[WORLDZONE] " + zone.id + " plane=" + zone.plane +
                        " seed=" + std::to_string(zone.seed) +
                        " mode=" + zone.modeId + " spacing=" + zone.spacingMode);
                return;
            }
            if (args[0] == "add" && args.size() >= 4) {
                GenerationZone zone;
                zone.id = args[1];
                try { zone.seed = static_cast<uint32_t>(std::stoul(args[2])); }
                catch (const std::exception&) { log("[WORLDZONE] bad seed"); return; }
                zone.generatorVersion = args[3];
                zone.plane = "xy";
                zone.cellSize = glm::vec3(10.0f);
                zone.chunkSizeVec = glm::vec3(10.0f, 10.0f, 4.0f);
                zone.streamRadius = glm::ivec3(2, 2, 0);
                registry.generationZonesMutable().push_back(std::move(zone));
                registry.markChanged();
                logEvent("worldgen.zone.added", args[1], {{"zone", args[1]}});
                log("[WORLDZONE] added " + args[1]);
                return;
            }
            if (args.size() < 3) { log("[WORLDZONE] usage"); return; }
            const std::string& action = args[0];
            GenerationZone* zone = findZone(args[1]);
            if (!zone) { log("[WORLDZONE] zone not found"); return; }
            bool changed = false;
            if (action == "grid") {
                zone->plane = args[2];
                changed = true;
            } else if (action == "axes") {
                changed = parseInt3(args, 2, zone->repeatAxes);
            } else if (action == "origin") {
                changed = parseVec3(args, 2, zone->origin);
            } else if (action == "cell") {
                changed = parseVec3(args, 2, zone->cellSize);
            } else if (action == "size") {
                changed = parseVec3(args, 2, zone->chunkSizeVec);
            } else if (action == "radius") {
                changed = parseInt3(args, 2, zone->streamRadius);
            } else if (action == "spacing") {
                zone->spacingMode = args[2] == "connector" ? "connector" : "center";
                changed = true;
            } else if (action == "mode") {
                zone->modeId = args[2];
                changed = true;
            }
            if (changed) {
                registry.markChanged();
                logEvent("worldgen.zone.saved", zone->id,
                         {{"zone", zone->id}, {"plane", zone->plane},
                          {"cell", {zone->cellSize.x, zone->cellSize.y,
                                    zone->cellSize.z}},
                          {"spacing", zone->spacingMode}});
                log("[WORLDZONE] updated " + zone->id);
            }
        },
        "2026-11-05", CommandCategory::Debug
    });
}
