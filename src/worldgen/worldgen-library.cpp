// 2026-11-05
#include "worldgen/worldgen-library.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "debug/structured-log.h"
#include "utils/json-comments.h"

namespace MimitaWorldGen {
namespace {

namespace fs = std::filesystem;
using json = nlohmann::json;

bool fail(std::string* error, const std::string& message)
{
    if (error) *error = message;
    return false;
}

json parseObject(const fs::path& path, std::string* error)
{
    std::ifstream in(path);
    if (!in.is_open())
        return fail(error, "cannot read " + path.string()), json();
    try {
        json value = parseJsonConfig(in);
        if (!value.is_object())
            return fail(error, "JSON root must be an object"), json();
        return value;
    } catch (const std::exception& e) {
        return fail(error, std::string("invalid JSON: ") + e.what()), json();
    }
}

bool writeAtomic(const fs::path& path, const std::string& text,
                 std::string* error)
{
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    if (ec) return fail(error, ec.message());
    const fs::path temp = path.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return fail(error, "cannot write " + temp.string());
        out << text;
        if (!out.good()) return fail(error, "write failed " + temp.string());
    }
    fs::remove(path, ec);
    ec.clear();
    fs::rename(temp, path, ec);
    if (ec) return fail(error, ec.message());
    return true;
}

json vec3ToJson(const glm::vec3& v)
{
    return json::array({v.x, v.y, v.z});
}

glm::vec3 vec3FromJson(const json& value, const glm::vec3& fallback)
{
    if (!value.is_array() || value.size() < 3) return fallback;
    return glm::vec3(value[0].get<float>(), value[1].get<float>(),
                     value[2].get<float>());
}

glm::vec2 vec2FromJson(const json& value, const glm::vec2& fallback)
{
    if (value.is_number()) return glm::vec2(value.get<float>());
    if (!value.is_array() || value.size() < 2) return fallback;
    return glm::vec2(value[0].get<float>(), value[1].get<float>());
}

json ivec3ToJson(const glm::ivec3& v)
{
    return json::array({v.x, v.y, v.z});
}

glm::ivec3 ivec3FromJson(const json& value, const glm::ivec3& fallback)
{
    if (!value.is_array() || value.size() < 3) return fallback;
    return glm::ivec3(value[0].get<int>(), value[1].get<int>(),
                      value[2].get<int>());
}

json chunkToJson(const AuthoredChunk& chunk)
{
    json parts = json::array();
    for (const auto& part : chunk.parts) {
        parts.push_back({
            {"id", part.id}, {"kind", part.kind},
            {"position", vec3ToJson(part.position)},
            {"size", vec3ToJson(part.size)},
            {"rotation", vec3ToJson(part.rotationEuler)},
            {"material", part.material},
            {"faces", {part.faceMaterials[0], part.faceMaterials[1],
                       part.faceMaterials[2], part.faceMaterials[3],
                       part.faceMaterials[4], part.faceMaterials[5]}},
            {"uv_scale", {part.uvScale.x, part.uvScale.y}},
            {"face_uv_scale", {
                {part.faceUvScale[0].x, part.faceUvScale[0].y},
                {part.faceUvScale[1].x, part.faceUvScale[1].y},
                {part.faceUvScale[2].x, part.faceUvScale[2].y},
                {part.faceUvScale[3].x, part.faceUvScale[3].y},
                {part.faceUvScale[4].x, part.faceUvScale[4].y},
                {part.faceUvScale[5].x, part.faceUvScale[5].y}}},
            {"collision", part.collision}, {"visible", part.visible},
        });
    }
    json connectors = json::array();
    for (const auto& connector : chunk.connectors) {
        connectors.push_back({
            {"id", connector.id}, {"side", connector.side},
            {"orientation", connector.orientation}, {"role", connector.role},
            {"position", vec3ToJson(connector.position)},
            {"tags", connector.tags}, {"links", connector.links},
        });
    }
    return json{
        {"format", 1},
        {"id", chunk.id},
        {"bounds_min", vec3ToJson(chunk.boundsMin)},
        {"bounds_max", vec3ToJson(chunk.boundsMax)},
        {"parts", parts}, {"connectors", connectors},
        {"tags", chunk.tags}, {"enabled", chunk.enabled},
    };
}

AuthoredChunk chunkFromJson(const json& root)
{
    AuthoredChunk chunk;
    chunk.id = root.value("id", "");
    chunk.boundsMin = vec3FromJson(root.value("bounds_min", json()), chunk.boundsMin);
    chunk.boundsMax = vec3FromJson(root.value("bounds_max", json()), chunk.boundsMax);
    chunk.enabled = root.value("enabled", true);
    if (root.contains("tags") && root["tags"].is_array())
        for (const auto& tag : root["tags"])
            if (tag.is_string()) chunk.tags.push_back(tag.get<std::string>());
    if (root.contains("parts") && root["parts"].is_array()) {
        for (const auto& item : root["parts"]) {
            if (!item.is_object()) continue;
            ChunkPart part;
            part.id = item.value("id", "");
            if (part.id.empty()) continue;
            part.kind = item.value("kind", part.kind);
            part.position = vec3FromJson(item.value("position", json()), part.position);
            part.size = vec3FromJson(item.value("size", json()), part.size);
            part.rotationEuler = vec3FromJson(item.value("rotation", json()),
                                              part.rotationEuler);
            part.material = item.value("material", "");
            part.collision = item.value("collision", true);
            part.visible = item.value("visible", true);
            if (item.contains("faces") && item["faces"].is_array())
                for (size_t i = 0; i < 6 && i < item["faces"].size(); ++i)
                    if (item["faces"][i].is_string())
                        part.faceMaterials[i] = item["faces"][i].get<std::string>();
            if (item.contains("uv_scale"))
                part.uvScale = vec2FromJson(item["uv_scale"], part.uvScale);
            if (item.contains("face_uv_scale") &&
                item["face_uv_scale"].is_array()) {
                for (size_t i = 0; i < 6 && i < item["face_uv_scale"].size(); ++i)
                    part.faceUvScale[i] = vec2FromJson(
                        item["face_uv_scale"][i], part.faceUvScale[i]);
            }
            chunk.parts.push_back(std::move(part));
        }
    }
    if (root.contains("connectors") && root["connectors"].is_array()) {
        for (const auto& item : root["connectors"]) {
            if (!item.is_object()) continue;
            AuthoredChunkConnector connector;
            connector.id = item.value("id", "");
            connector.side = item.value("side", "");
            connector.orientation = item.value("orientation", "");
            connector.role = item.value("role", "bidirectional");
            connector.position = vec3FromJson(item.value("position", json()),
                                              connector.position);
            if (item.contains("tags") && item["tags"].is_array())
                for (const auto& tag : item["tags"])
                    if (tag.is_string()) connector.tags.push_back(tag.get<std::string>());
            if (item.contains("links") && item["links"].is_array())
                for (const auto& link : item["links"])
                    if (link.is_string()) connector.links.push_back(link.get<std::string>());
            chunk.connectors.push_back(std::move(connector));
        }
    }
    return chunk;
}

json modeToJson(const WorldGenMode& mode)
{
    return json{
        {"format", 1},
        {"mode", mode.id},
        {"grid", {
            {"plane", mode.grid.plane},
            {"repeat_axes", ivec3ToJson(mode.grid.repeatAxes)},
            {"origin", vec3ToJson(mode.grid.origin)},
            {"cell_size", vec3ToJson(mode.grid.cellSize)},
            {"chunk_size", vec3ToJson(mode.grid.chunkSize)},
            {"stream_radius", ivec3ToJson(mode.grid.streamRadius)},
            {"spacing_mode", mode.grid.spacingMode},
            {"connector_gap", mode.grid.connectorGap},
        }},
        {"chunk_files", mode.chunkFiles},
        {"max_branch_depth", mode.maxBranchDepth},
        {"allow_cycles", mode.allowCycles},
        {"allow_crossings", mode.allowCrossings},
    };
}

WorldGenMode modeFromJson(const json& root)
{
    WorldGenMode mode;
    mode.id = root.value("mode", root.value("id", ""));
    if (root.contains("grid") && root["grid"].is_object()) {
        const json& grid = root["grid"];
        mode.grid.plane = grid.value("plane", mode.grid.plane);
        mode.grid.repeatAxes = ivec3FromJson(grid.value("repeat_axes", json()),
                                             mode.grid.repeatAxes);
        mode.grid.origin = vec3FromJson(grid.value("origin", json()), mode.grid.origin);
        mode.grid.cellSize = vec3FromJson(grid.value("cell_size", json()),
                                          mode.grid.cellSize);
        mode.grid.chunkSize = vec3FromJson(grid.value("chunk_size", json()),
                                           mode.grid.chunkSize);
        mode.grid.streamRadius = ivec3FromJson(grid.value("stream_radius", json()),
                                               mode.grid.streamRadius);
        mode.grid.spacingMode = grid.value("spacing_mode", mode.grid.spacingMode);
        mode.grid.connectorGap = grid.value("connector_gap", mode.grid.connectorGap);
    }
    if (root.contains("chunk_files") && root["chunk_files"].is_array())
        for (const auto& file : root["chunk_files"])
            if (file.is_string()) mode.chunkFiles.push_back(file.get<std::string>());
    mode.maxBranchDepth = root.value("max_branch_depth", mode.maxBranchDepth);
    mode.allowCycles = root.value("allow_cycles", mode.allowCycles);
    mode.allowCrossings = root.value("allow_crossings", mode.allowCrossings);
    return mode;
}

AuthoredChunk makeTunnelChunk(const char* id, const char* tag,
                              std::vector<AuthoredChunkConnector> connectors)
{
    AuthoredChunk chunk;
    chunk.id = id;
    chunk.boundsMin = glm::vec3(-5.0f, -5.0f, 0.0f);
    chunk.boundsMax = glm::vec3(5.0f, 5.0f, 4.0f);
    chunk.tags = {tag};

    ChunkPart floor;
    floor.id = "floor";
    floor.kind = "floor";
    floor.position = glm::vec3(0.0f, 0.0f, -0.25f);
    floor.size = glm::vec3(10.0f, 10.0f, 0.5f);
    floor.material = "floorv2";
    chunk.parts.push_back(floor);

    if (std::string(id) != "chunk_cross") {
        ChunkPart left;
        left.id = "left_wall";
        left.kind = "wall";
        left.position = glm::vec3(-4.75f, 0.0f, 2.0f);
        left.size = glm::vec3(0.5f, 10.0f, 4.0f);
        left.material = "wallv2";
        chunk.parts.push_back(left);
        ChunkPart right = left;
        right.id = "right_wall";
        right.position.x = 4.75f;
        chunk.parts.push_back(right);
    } else {
        for (int i = 0; i < 4; ++i) {
            ChunkPart post;
            post.id = "post_" + std::to_string(i);
            post.kind = "wall";
            post.size = glm::vec3(0.5f, 0.5f, 4.0f);
            post.position = glm::vec3(-4.75f + (i % 2) * 9.5f,
                                      -4.75f + (i / 2) * 9.5f, 2.0f);
            post.material = "wallv2";
            chunk.parts.push_back(post);
        }
    }
    chunk.connectors = std::move(connectors);
    return chunk;
}

void addConnector(AuthoredChunk& chunk, const char* id, const char* side,
                  const char* role, glm::vec3 position)
{
    AuthoredChunkConnector connector;
    connector.id = id;
    connector.side = side;
    connector.orientation = "tunnel";
    connector.role = role;
    connector.position = position;
    connector.tags = {"tunnel"};
    chunk.connectors.push_back(std::move(connector));
}

std::uint64_t writeTime(const fs::path& path)
{
    std::error_code error;
    const auto time = fs::last_write_time(path, error);
    if (error)
        return 0;
    return static_cast<std::uint64_t>(time.time_since_epoch().count());
}

bool finiteVec(const glm::vec3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

bool validSide(const std::string& side)
{
    return side == "east" || side == "west" || side == "north" ||
           side == "south" || side == "up" || side == "down";
}

bool validateProfile(const WorldGenMode& mode, std::string* error)
{
    if (!finiteVec(mode.grid.cellSize) || !finiteVec(mode.grid.chunkSize) ||
        mode.grid.cellSize.x <= 0.0f || mode.grid.cellSize.y <= 0.0f ||
        mode.grid.cellSize.z <= 0.0f || mode.grid.chunkSize.x <= 0.0f ||
        mode.grid.chunkSize.y <= 0.0f || mode.grid.chunkSize.z <= 0.0f)
        return fail(error, "worldgen profile has non-positive or non-finite sizes");
    if (mode.grid.spacingMode != "connector" && mode.grid.spacingMode != "center")
        return fail(error, "worldgen spacing_mode must be connector or center");
    for (int axis = 0; axis < 3; ++axis) {
        if (mode.grid.repeatAxes[axis] < 0)
            return fail(error, "worldgen repeat_axes must be 0 or 1");
        if (mode.grid.streamRadius[axis] < 0)
            return fail(error, "worldgen stream_radius must be non-negative");
    }
    return true;
}

bool validateChunks(const std::vector<AuthoredChunk>& chunks, std::string* error)
{
    std::unordered_set<std::string> ids;
    for (const AuthoredChunk& chunk : chunks) {
        if (chunk.id.empty())
            return fail(error, "worldgen chunk has an empty id");
        if (!ids.insert(chunk.id).second)
            return fail(error, "duplicate worldgen chunk id: " + chunk.id);
        if (!finiteVec(chunk.boundsMin) || !finiteVec(chunk.boundsMax))
            return fail(error, "chunk " + chunk.id + " has non-finite bounds");
        if (chunk.boundsMax.x < chunk.boundsMin.x ||
            chunk.boundsMax.y < chunk.boundsMin.y ||
            chunk.boundsMax.z < chunk.boundsMin.z)
            return fail(error, "chunk " + chunk.id + " has inverted bounds");
        for (const ChunkPart& part : chunk.parts) {
            if (!finiteVec(part.position) || !finiteVec(part.size) ||
                part.size.x <= 0.0f || part.size.y <= 0.0f ||
                part.size.z <= 0.0f)
                return fail(error, "chunk " + chunk.id + " part " + part.id +
                                       " has invalid geometry");
        }
        for (const AuthoredChunkConnector& connector : chunk.connectors) {
            if (!validSide(connector.side))
                return fail(error, "chunk " + chunk.id + " connector " +
                                       connector.id + " has invalid side");
            if (!std::isfinite(connector.position.x) ||
                !std::isfinite(connector.position.y) ||
                !std::isfinite(connector.position.z))
                return fail(error, "chunk " + chunk.id + " connector " +
                                       connector.id + " has non-finite position");
        }
    }
    return true;
}

} // namespace

WorldGenLibrary& WorldGenLibrary::instance()
{
    static WorldGenLibrary library;
    return library;
}

std::string WorldGenLibrary::modePath(const std::string& id)
{
    return "config/worldgen/" + id + ".json";
}

std::string WorldGenLibrary::chunkPath(const std::string& id)
{
    return "config/worldgen/chunks/" + id + ".json";
}

bool WorldGenLibrary::loadMode(const std::string& id, WorldGenMode& out,
                               std::string* error)
{
    const json root = parseObject(modePath(id), error);
    if (root.is_null()) return false;
    out = modeFromJson(root);
    if (out.id.empty()) out.id = id;
    return true;
}

bool WorldGenLibrary::saveMode(const WorldGenMode& mode, std::string* error)
{
    return writeAtomic(modePath(mode.id), modeToJson(mode).dump(2) + "\n",
                       error);
}

WorldGenLibrary::WatchEntry* WorldGenLibrary::findWatch(const std::string& id)
{
    for (WatchEntry& entry : mWatches)
        if (entry.id == id)
            return &entry;
    WatchEntry entry;
    entry.id = id;
    mWatches.push_back(std::move(entry));
    return &mWatches.back();
}

bool WorldGenLibrary::pollReload(const std::string& id, WorldGenMode& out,
                                 std::vector<AuthoredChunk>& chunks,
                                 bool* changed, std::string* error)
{
    if (changed)
        *changed = false;
    if (id.empty())
        return false;

    WatchEntry* watch = findWatch(id);
    const fs::path modeFile = modePath(id);
    const std::uint64_t modeWrite = writeTime(modeFile);
    if (modeWrite == 0)
        return false;

    WorldGenMode candidate;
    if (!loadMode(id, candidate, error))
        return false;

    bool fileChanged = modeWrite != watch->modeWrite;
    const bool firstObservation = watch->modeWrite == 0;
    std::vector<std::pair<std::string, std::uint64_t>> chunkWrites;
    chunkWrites.reserve(candidate.chunkFiles.size());
    for (const std::string& file : candidate.chunkFiles) {
        const std::uint64_t write = writeTime(file);
        chunkWrites.emplace_back(file, write);
        if (write == 0)
            fileChanged = true;
        else {
            auto previous = std::find_if(
                watch->chunkWrites.begin(), watch->chunkWrites.end(),
                [&file](const std::pair<std::string, std::uint64_t>& item) {
                    return item.first == file;
                });
            if (previous == watch->chunkWrites.end() || previous->second != write)
                fileChanged = true;
        }
    }
    if (!fileChanged && !firstObservation)
        return false;

    watch->modeWrite = modeWrite;
    watch->chunkWrites = chunkWrites;

    if (!validateProfile(candidate, error)) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::World, StructuredLevel::Errors,
            "worldgen.profile.rejected", id,
            "invalid worldgen profile kept previous data active", 0,
            {{"profile", id}, {"reason", error ? *error : "validation failed"}},
            __FILE__, __LINE__, __func__);
        return false;
    }

    std::vector<AuthoredChunk> candidateChunks;
    std::string chunkError;
    if (!loadChunksForMode(candidate, candidateChunks, &chunkError)) {
        if (error)
            *error = chunkError;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::World, StructuredLevel::Errors,
            "worldgen.profile.rejected", id,
            "worldgen chunk load failed; previous data retained", 0,
            {{"profile", id}, {"reason", chunkError}},
            __FILE__, __LINE__, __func__);
        return false;
    }
    if (!validateChunks(candidateChunks, &chunkError)) {
        if (error)
            *error = chunkError;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::World, StructuredLevel::Errors,
            "worldgen.profile.rejected", id,
            "worldgen chunk validation failed; previous data retained", 0,
            {{"profile", id}, {"reason", chunkError}},
            __FILE__, __LINE__, __func__);
        return false;
    }

    out = std::move(candidate);
    chunks = std::move(candidateChunks);
    if (changed)
        *changed = true;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "worldgen.profile.reloaded", id,
        "validated worldgen profile and chunks applied", 0,
        {{"profile", id}, {"chunks", chunks.size()},
         {"cell_size", {out.grid.cellSize.x, out.grid.cellSize.y,
                        out.grid.cellSize.z}},
         {"chunk_size", {out.grid.chunkSize.x, out.grid.chunkSize.y,
                         out.grid.chunkSize.z}},
         {"stream_radius", {out.grid.streamRadius.x, out.grid.streamRadius.y,
                            out.grid.streamRadius.z}},
         {"spacing_mode", out.grid.spacingMode}},
        __FILE__, __LINE__, __func__);
    return true;
}

bool WorldGenLibrary::listModes(std::vector<std::string>& out,
                                std::string* error)
{
    out.clear();
    std::error_code ec;
    const fs::path dir("config/worldgen");
    if (!fs::is_directory(dir, ec)) return true;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) return fail(error, ec.message());
        if (entry.is_regular_file() && entry.path().extension() == ".json")
            out.push_back(entry.path().stem().string());
    }
    std::sort(out.begin(), out.end());
    return true;
}

bool WorldGenLibrary::loadChunkFile(const std::string& path, AuthoredChunk& out,
                                    std::string* error)
{
    const json root = parseObject(path, error);
    if (root.is_null()) return false;
    out = chunkFromJson(root);
    return !out.id.empty();
}

bool WorldGenLibrary::loadChunksForMode(const WorldGenMode& mode,
                                        std::vector<AuthoredChunk>& out,
                                        std::string* error)
{
    out.clear();
    for (const std::string& file : mode.chunkFiles) {
        AuthoredChunk chunk;
        if (!loadChunkFile(file, chunk, error))
            return false;
        out.push_back(std::move(chunk));
    }
    return true;
}

bool WorldGenLibrary::saveChunk(const AuthoredChunk& chunk, std::string* error)
{
    return writeAtomic(chunkPath(chunk.id), chunkToJson(chunk).dump(2) + "\n",
                       error);
}

bool WorldGenLibrary::listChunkFiles(std::vector<std::string>& out,
                                     std::string* error)
{
    out.clear();
    std::error_code ec;
    const fs::path dir("config/worldgen/chunks");
    if (!fs::is_directory(dir, ec)) return true;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) return fail(error, ec.message());
        if (entry.is_regular_file() && entry.path().extension() == ".json")
            out.push_back(entry.path().string());
    }
    std::sort(out.begin(), out.end());
    return true;
}

bool WorldGenLibrary::ensureDefaults(std::string* error)
{
    std::error_code ec;
    fs::create_directories("config/worldgen/chunks", ec);
    if (ec) return fail(error, ec.message());

    std::vector<AuthoredChunk> chunks;
    AuthoredChunk straight = makeTunnelChunk("chunk_straight", "straight", {});
    addConnector(straight, "west_entry", "west", "entry", {-5.0f, 0.0f, 2.0f});
    addConnector(straight, "east_exit", "east", "exit", {5.0f, 0.0f, 2.0f});
    chunks.push_back(std::move(straight));

    AuthoredChunk corner = makeTunnelChunk("chunk_corner", "corner", {});
    addConnector(corner, "west_entry", "west", "entry", {-5.0f, 0.0f, 2.0f});
    addConnector(corner, "north_exit", "north", "exit", {0.0f, 5.0f, 2.0f});
    chunks.push_back(std::move(corner));

    AuthoredChunk cross = makeTunnelChunk("chunk_cross", "cross", {});
    addConnector(cross, "west", "west", "bidirectional", {-5.0f, 0.0f, 2.0f});
    addConnector(cross, "east", "east", "bidirectional", {5.0f, 0.0f, 2.0f});
    addConnector(cross, "north", "north", "bidirectional", {0.0f, 5.0f, 2.0f});
    addConnector(cross, "south", "south", "bidirectional", {0.0f, -5.0f, 2.0f});
    chunks.push_back(std::move(cross));

    AuthoredChunk deadend = makeTunnelChunk("chunk_deadend", "deadend", {});
    addConnector(deadend, "west_entry", "west", "entry", {-5.0f, 0.0f, 2.0f});
    chunks.push_back(std::move(deadend));

    for (const AuthoredChunk& chunk : chunks) {
        const fs::path path(chunkPath(chunk.id));
        if (fs::exists(path, ec)) continue;
        if (!saveChunk(chunk, error)) return false;
    }

    WorldGenMode mode;
    mode.id = "tunnel";
    mode.grid.plane = "xy";
    mode.grid.repeatAxes = glm::ivec3(1, 1, 0);
    mode.grid.cellSize = glm::vec3(10.0f, 10.0f, 10.0f);
    mode.grid.chunkSize = glm::vec3(10.0f, 10.0f, 4.0f);
    mode.grid.streamRadius = glm::ivec3(2, 2, 0);
    mode.grid.spacingMode = "connector";
    mode.maxBranchDepth = 12;
    mode.allowCycles = true;
    mode.allowCrossings = true;
    for (const AuthoredChunk& chunk : chunks)
        mode.chunkFiles.push_back(chunkPath(chunk.id));
    if (!fs::exists(modePath(mode.id), ec) && !saveMode(mode, error))
        return false;

    WorldGenMode dungeon;
    dungeon.id = "infinite_dungeon_slayer";
    dungeon.grid.plane = "xy";
    dungeon.grid.cellSize = glm::vec3(104.0f, 104.0f, 104.0f);
    dungeon.grid.chunkSize = glm::vec3(104.0f, 104.0f, 103.0f);
    dungeon.grid.streamRadius = glm::ivec3(2, 2, 0);
    dungeon.grid.spacingMode = "connector";
    dungeon.chunkFiles = mode.chunkFiles;
    if (!fs::exists(modePath(dungeon.id), ec) && !saveMode(dungeon, error))
        return false;
    return true;
}

} // namespace MimitaWorldGen
