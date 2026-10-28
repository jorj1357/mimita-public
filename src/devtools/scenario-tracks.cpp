#include "devtools/scenario-tracks.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

namespace ScenarioTracks {
namespace {

std::string utcTimestamp()
{
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &now);
#else
    gmtime_r(&now, &tm);
#endif
    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buffer;
}

bool readLines(const std::string& path,
               std::vector<nlohmann::json>& lines,
               std::string* error)
{
    std::ifstream in(path);
    if (!in.is_open())
    {
        if (error) *error = "cannot_open_track_file";
        return false;
    }
    std::string line;
    while (std::getline(in, line))
    {
        if (line.empty())
            continue;
        try
        {
            lines.push_back(nlohmann::json::parse(line));
        }
        catch (const std::exception&)
        {
            // A crash can leave one partial trailing line. Ignore only the
            // final malformed line, never a malformed line in the middle.
            std::string rest;
            (void)std::getline(in, rest);
            if (!rest.empty())
            {
                if (error) *error = "invalid_json_line";
                return false;
            }
            break;
        }
    }
    return true;
}

const nlohmann::json* findHeader(const std::vector<nlohmann::json>& lines)
{
    for (const auto& line : lines)
        if (line.value("kind", "") == "header")
            return &line;
    return nullptr;
}

} // namespace

nlohmann::json vec3Json(const glm::vec3& value)
{
    return nlohmann::json::array({value.x, value.y, value.z});
}

glm::vec3 jsonVec3(const nlohmann::json& value)
{
    if (!value.is_array() || value.size() < 3)
        return glm::vec3(0.0f);
    return glm::vec3(value[0].get<float>(),
                     value[1].get<float>(),
                     value[2].get<float>());
}

nlohmann::json actorStateJson(const ActorState& state)
{
    nlohmann::json parts = nlohmann::json::array();
    for (const BodyPartBox& part : state.bodyParts)
    {
        parts.push_back({{"n", part.name},
                         {"c", vec3Json(part.center)},
                         {"h", vec3Json(part.half)}});
    }
    nlohmann::json shape = nullptr;
    if (state.shape.valid)
    {
        shape = {
            {"pa", vec3Json(state.shape.previousA)},
            {"pb", vec3Json(state.shape.previousB)},
            {"ca", vec3Json(state.shape.currentA)},
            {"cb", vec3Json(state.shape.currentB)},
            {"r", state.shape.radius}
        };
    }
    return {
        {"id", state.trackId},
        {"k", state.kind},
        {"p", vec3Json(state.position)},
        {"v", vec3Json(state.velocity)},
        {"a", vec3Json(state.aim)},
        {"yaw", state.yaw},
        {"size", state.sizeScale},
        {"hp", state.health},
        {"maxhp", state.maxHealth},
        {"team", state.team},
        {"dead", state.dead},
        {"ground", state.grounded},
        {"gen", state.spawnGeneration},
        {"epoch", state.transformEpoch},
        {"weapon", state.weaponId},
        {"slot", state.equippedSlot},
        {"wstate", state.weaponState},
        {"act", state.actionActive},
        {"aid", state.actionId},
        {"astart", state.actionStartTick},
        {"aphase", state.actionPhaseTick},
        {"next", state.nextAllowedTick},
        {"parts", std::move(parts)},
        {"shape", std::move(shape)}
    };
}

ActorState parseActorState(const nlohmann::json& value)
{
    ActorState state;
    state.trackId = value.value("id", "");
    state.kind = value.value("k", "");
    state.position = jsonVec3(value.value("p", nlohmann::json::array()));
    state.velocity = jsonVec3(value.value("v", nlohmann::json::array()));
    state.aim = jsonVec3(value.value("a", nlohmann::json::array()));
    state.yaw = value.value("yaw", 0.0f);
    state.sizeScale = value.value("size", 1.0f);
    state.health = value.value("hp", 100);
    state.maxHealth = value.value("maxhp", 100);
    state.team = value.value("team", -1);
    state.dead = value.value("dead", false);
    state.grounded = value.value("ground", false);
    state.spawnGeneration = value.value("gen", 0u);
    state.transformEpoch = static_cast<uint16_t>(value.value("epoch", 0u));
    state.weaponId = value.value("weapon", "");
    state.equippedSlot = value.value("slot", 0);
    state.weaponState = value.value("wstate", 0);
    state.actionActive = value.value("act", false);
    state.actionId = value.value("aid", "");
    state.actionStartTick = value.value("astart", 0u);
    state.actionPhaseTick = value.value("aphase", 0);
    state.nextAllowedTick = value.value("next", 0u);
    if (value.contains("parts") && value["parts"].is_array())
    {
        for (const auto& part : value["parts"])
        {
            BodyPartBox box;
            box.name = part.value("n", "");
            box.center = jsonVec3(part.value("c", nlohmann::json::array()));
            box.half = jsonVec3(part.value("h", nlohmann::json::array()));
            state.bodyParts.push_back(std::move(box));
        }
    }
    if (value.contains("shape") && value["shape"].is_object())
    {
        const auto& shape = value["shape"];
        state.shape.valid = true;
        state.shape.previousA = jsonVec3(shape.value("pa", nlohmann::json::array()));
        state.shape.previousB = jsonVec3(shape.value("pb", nlohmann::json::array()));
        state.shape.currentA = jsonVec3(shape.value("ca", nlohmann::json::array()));
        state.shape.currentB = jsonVec3(shape.value("cb", nlohmann::json::array()));
        state.shape.radius = shape.value("r", 0.0f);
    }
    return state;
}

nlohmann::json identityJson(const ActorIdentity& identity)
{
    return {
        {"id", identity.trackId},
        {"k", identity.kind},
        {"entity", identity.entityId},
        {"name", identity.name},
        {"avatar", identity.avatar},
        {"gen", identity.spawnGeneration},
        {"epoch", identity.transformEpoch}
    };
}

ActorIdentity parseIdentity(const nlohmann::json& value)
{
    ActorIdentity identity;
    identity.trackId = value.value("id", "");
    identity.kind = value.value("k", "");
    identity.entityId = value.value("entity", 0u);
    identity.name = value.value("name", "");
    identity.avatar = value.value("avatar", "");
    identity.spawnGeneration = value.value("gen", 0u);
    identity.transformEpoch = static_cast<uint16_t>(value.value("epoch", 0u));
    return identity;
}

nlohmann::json lifecycleJson(const LifecycleEvent& event)
{
    return {
        {"kind", "actor_lifecycle"},
        {"track", event.trackId},
        {"event", event.event},
        {"server_tick", event.serverTick},
        {"client_tick", event.clientTick},
        {"gen", event.spawnGeneration},
        {"epoch", event.transformEpoch},
        {"hp", event.health},
        {"pos", vec3Json(event.position)}
    };
}

LifecycleEvent parseLifecycle(const nlohmann::json& value)
{
    LifecycleEvent event;
    event.trackId = value.value("track", "");
    event.event = value.value("event", "");
    event.serverTick = value.value("server_tick", 0u);
    event.clientTick = value.value("client_tick", 0u);
    event.spawnGeneration = value.value("gen", 0u);
    event.transformEpoch = static_cast<uint16_t>(value.value("epoch", 0u));
    event.health = value.value("hp", 0);
    event.position = jsonVec3(value.value("pos", nlohmann::json::array()));
    return event;
}

nlohmann::json meleeRevisionsJson(const std::vector<MeleeRevision>& revisions)
{
    nlohmann::json out = nlohmann::json::array();
    for (const MeleeRevision& revision : revisions)
    {
        out.push_back({
            {"weapon", revision.weaponId},
            {"revision", revision.revision},
            {"hash", revision.hash}
        });
    }
    return out;
}

std::vector<MeleeRevision> parseMeleeRevisions(const nlohmann::json& value)
{
    std::vector<MeleeRevision> out;
    if (!value.is_array())
        return out;
    for (const auto& entry : value)
    {
        MeleeRevision revision;
        revision.weaponId = entry.value("weapon", "");
        revision.revision = entry.value("revision", 0u);
        revision.hash = entry.value("hash", 0u);
        if (!revision.weaponId.empty())
            out.push_back(std::move(revision));
    }
    return out;
}

bool validateScenarioPath(const std::string& path, std::string* error)
{
    if (path.empty())
    {
        if (error) *error = "empty_scenario_path";
        return false;
    }
    if (path.find("..") != std::string::npos)
    {
        if (error) *error = "path_traversal_rejected";
        return false;
    }
    if (path.size() < 6 || path.substr(path.size() - 6) != ".jsonl")
    {
        if (error) *error = "expected_jsonl_scenario";
        return false;
    }
    return true;
}

bool mergeClientAndServer(const std::string& clientPath,
                          const std::string& serverPath,
                          const std::string& outputPath,
                          std::string* error)
{
    std::vector<nlohmann::json> clientLines;
    std::vector<nlohmann::json> serverLines;
    if (!readLines(clientPath, clientLines, error))
        return false;
    if (!readLines(serverPath, serverLines, error))
        return false;

    const nlohmann::json* clientHeader = findHeader(clientLines);
    const nlohmann::json* serverHeader = findHeader(serverLines);
    if (!clientHeader)
    {
        if (error) *error = "missing_client_header";
        return false;
    }
    if (!serverHeader)
    {
        if (error) *error = "missing_server_header";
        return false;
    }

    std::vector<nlohmann::json> serverRecords;
    nlohmann::json serverFooter = nullptr;
    for (const auto& line : serverLines)
    {
        const std::string kind = line.value("kind", "");
        if (kind == "server_tick" || kind == "actor_lifecycle")
            serverRecords.push_back(line);
        else if (kind == "footer")
            serverFooter = line;
    }
    if (serverRecords.empty())
    {
        if (error) *error = "server_track_empty";
        return false;
    }
    std::stable_sort(serverRecords.begin(), serverRecords.end(),
        [](const nlohmann::json& a, const nlohmann::json& b) {
            const uint32_t at = a.value("server_tick", a.value("tick", 0u));
            const uint32_t bt = b.value("server_tick", b.value("tick", 0u));
            return at < bt;
        });

    nlohmann::json header = *clientHeader;
    header["schema"] = kSchemaV2;
    header["tracks"] = nlohmann::json::array({"client_input", "client_actors", "server_actors"});
    header["server"] = {
        {"build_id", serverHeader->value("build_id", "")},
        {"start_server_tick", serverHeader->value("start_server_tick", 0u)},
        {"map", serverHeader->value("map", "")},
        {"mode", serverHeader->value("mode", "")},
        {"weapon_set", serverHeader->value("weapon_set", 0)},
        {"frame_count", serverHeader->value("frame_count", 0u)}
    };
    if (serverHeader->contains("melee_revisions"))
        header["melee_revisions"] = (*serverHeader)["melee_revisions"];

    std::map<std::string, nlohmann::json> identities;
    auto addIdentity = [&](const nlohmann::json& value) {
        if (!value.is_object())
            return;
        const std::string id = value.value("id", "");
        if (!id.empty())
            identities[id] = value;
    };
    auto addIdentities = [&](const nlohmann::json& source) {
        if (!source.is_array())
            return;
        for (const auto& value : source)
            addIdentity(value);
    };
    if (header.contains("actors"))
        addIdentities(header["actors"]);
    if (serverHeader->contains("actors"))
        addIdentities((*serverHeader)["actors"]);
    for (const auto& line : clientLines)
        if (line.value("kind", "") == "actor_identity")
            addIdentity(line);
    for (const auto& line : serverLines)
        if (line.value("kind", "") == "actor_identity")
            addIdentity(line);
    header["actors"] = nlohmann::json::array();
    for (const auto& entry : identities)
        header["actors"].push_back(entry.second);

    const std::filesystem::path output(outputPath);
    std::filesystem::path temp = output;
    temp += ".tmp";
    std::ofstream out(temp, std::ios::out | std::ios::trunc);
    if (!out.is_open())
    {
        if (error) *error = "cannot_open_merged_scenario";
        return false;
    }
    out << header.dump() << '\n';

    std::size_t serverIndex = 0;
    auto serverKey = [](const nlohmann::json& value) {
        return value.value("server_tick", value.value("tick", 0u));
    };
    auto flushServerThrough = [&](uint32_t key) {
        while (serverIndex < serverRecords.size() &&
               serverKey(serverRecords[serverIndex]) <= key)
        {
            out << serverRecords[serverIndex].dump() << '\n';
            ++serverIndex;
        }
    };

    uint32_t clientFrames = 0;
    uint32_t serverTicks = 0;
    nlohmann::json clientFooter = nullptr;
    for (const auto& line : clientLines)
    {
        const std::string kind = line.value("kind", "");
        if (kind == "header")
            continue;
        if (kind == "footer")
        {
            clientFooter = line;
            continue;
        }
        if (kind == "tick")
            ++clientFrames;
        const uint32_t key = line.value("server_tick", 0u);
        flushServerThrough(key);
        out << line.dump() << '\n';
    }
    flushServerThrough(0xFFFFFFFFu);
    for (const auto& line : serverRecords)
        if (line.value("kind", "") == "server_tick")
            ++serverTicks;

    if (!clientFooter.is_object())
        clientFooter = nlohmann::json::object();
    clientFooter["kind"] = "footer";
    clientFooter["tracks_complete"] = true;
    clientFooter["client_frames"] = clientFrames;
    clientFooter["server_ticks"] = serverTicks;
    clientFooter["server_footer"] = serverFooter;
    clientFooter["merged_utc"] = utcTimestamp();
    out << clientFooter.dump() << '\n';
    out.flush();
    out.close();
    if (!out.good())
    {
        if (error) *error = "merged_scenario_write_failed";
        return false;
    }

    std::error_code ec;
    std::filesystem::remove(output, ec);
    ec.clear();
    std::filesystem::rename(temp, output, ec);
    if (ec)
    {
        std::filesystem::remove(temp, ec);
        if (error) *error = "merged_scenario_replace_failed";
        return false;
    }
    return true;
}

} // namespace ScenarioTracks
