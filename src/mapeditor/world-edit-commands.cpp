// 2026-11-05
#include "mapeditor/world-edit-commands.h"

#include <cstdlib>
#include <exception>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "debug/structured-log.h"
#include "devtools/terminal.h"
#include "gamemode/map-edit-applier.h"
#include "gamemode/map-config.h"
#include "worldproject/world-edit-session.h"

namespace {

using namespace MimitaWorldProject;

WorldEditSession gSession;
uint64_t gNextOperationId = 1;

void log(const std::string& message)
{
    Terminal::instance().addLog(message);
}

bool parseTarget(const std::string& text, WorldEditTargetKind& out)
{
    if (text == "entity") out = WorldEditTargetKind::Entity;
    else if (text == "block") out = WorldEditTargetKind::Block;
    else if (text == "vehicle") out = WorldEditTargetKind::Vehicle;
    else if (text == "bombsite" || text == "bomb_site") out = WorldEditTargetKind::BombSite;
    else if (text == "payload") out = WorldEditTargetKind::Payload;
    else if (text == "npc" || text == "npc_preset") out = WorldEditTargetKind::NpcPreset;
    else if (text == "zone" || text == "generation_zone") out = WorldEditTargetKind::GenerationZone;
    else if (text == "interaction") out = WorldEditTargetKind::Interaction;
    else if (text == "world") out = WorldEditTargetKind::World;
    else return false;
    return true;
}

bool parseCapability(const std::string& text, WorldEditCapability& out)
{
    if (text == "geometry") out = WorldEditCapability::Geometry;
    else if (text == "chunks") out = WorldEditCapability::Chunks;
    else if (text == "npcs") out = WorldEditCapability::Npcs;
    else if (text == "weapons") out = WorldEditCapability::Weapons;
    else if (text == "interactions") out = WorldEditCapability::Interactions;
    else if (text == "undo") out = WorldEditCapability::Undo;
    else if (text == "publish") out = WorldEditCapability::Publish;
    else if (text == "all") out = WorldEditCapability::All;
    else return false;
    return true;
}

WorldEditCapability defaultCapability(WorldEditTargetKind target)
{
    switch (target) {
    case WorldEditTargetKind::NpcPreset: return WorldEditCapability::Npcs;
    case WorldEditTargetKind::GenerationZone: return WorldEditCapability::Chunks;
    case WorldEditTargetKind::Interaction: return WorldEditCapability::Interactions;
    default: return WorldEditCapability::Geometry;
    }
}

bool parseValue(const std::string& type, const std::vector<std::string>& args,
                size_t first, WorldEditValue& out)
{
    try {
        if (type == "bool") {
            if (first >= args.size()) return false;
            out = WorldEditValue::boolean(args[first] == "1" || args[first] == "true");
            return true;
        }
        if (type == "int") {
            if (first >= args.size()) return false;
            out = WorldEditValue::integer(std::stoll(args[first]));
            return true;
        }
        if (type == "float") {
            if (first >= args.size()) return false;
            out = WorldEditValue::number(std::stod(args[first]));
            return true;
        }
        if (type == "vec3") {
            if (first + 2 >= args.size()) return false;
            out = WorldEditValue::vector(std::stod(args[first]),
                                         std::stod(args[first + 1]),
                                         std::stod(args[first + 2]));
            return true;
        }
        if (type == "string") {
            if (first >= args.size()) return false;
            std::string joined = args[first];
            for (size_t i = first + 1; i < args.size(); ++i)
                joined += " " + args[i];
            out = WorldEditValue::text(std::move(joined));
            return true;
        }
    } catch (const std::exception&) {
        return false;
    }
    return false;
}

nlohmann::json valueToJson(const WorldEditValue& value)
{
    switch (value.type) {
    case WorldEditValueType::Bool: return value.boolValue;
    case WorldEditValueType::Int: return value.intValue;
    case WorldEditValueType::Float: return value.floatValue;
    case WorldEditValueType::Vec3:
        return nlohmann::json::array({value.vec3Value.x, value.vec3Value.y,
                                      value.vec3Value.z});
    case WorldEditValueType::String: return value.stringValue;
    default: return nullptr;
    }
}

const char* valueTypeName(WorldEditValueType type)
{
    switch (type) {
    case WorldEditValueType::Bool: return "bool";
    case WorldEditValueType::Int: return "int";
    case WorldEditValueType::Float: return "float";
    case WorldEditValueType::Vec3: return "vec3";
    case WorldEditValueType::String: return "string";
    default: return "none";
    }
}

void logResult(const WorldEditResult& result, const std::string& action,
               const std::string& target, const std::string& objectId,
               uint16_t propertyId)
{
    nlohmann::json fields = {
        {"action", action},
        {"accepted", result.accepted},
        {"conflict", result.conflict},
        {"reason", result.reason},
        {"sequence", result.sequence},
        {"revision", result.revision},
        {"current_property_revision", result.currentPropertyRevision},
        {"target", target},
        {"object_id", objectId},
        {"property_id", propertyId},
    };
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World,
        result.accepted ? StructuredLevel::Important : StructuredLevel::Errors,
        result.accepted ? "world-edit.accepted"
                        : (result.conflict ? "world-edit.conflict"
                                           : "world-edit.rejected"),
        "world-edit", "host edit-session decision", 0, fields,
        __FILE__, __LINE__, __func__);
    log(std::string("[WORLDEDIT] ") + (result.accepted ? "accepted" :
        (result.conflict ? "conflict" : "rejected")) +
        " seq=" + std::to_string(result.sequence) +
        " rev=" + std::to_string(result.revision) +
        (result.reason.empty() ? std::string() : " reason=" + result.reason));
}

} // namespace

void registerWorldEditCommands()
{
    Terminal::instance().registerCommand({
        "worldedit",
        "Local host-authoritative world-edit session (trial/error validation)",
        "worldedit <begin <project> <version>|grant <account> <cap>|revoke <account> <cap>|set <account> <target> <id> <property> <type> <value...> [base=<rev>]|get <target> <id> <property>|undo <account> <seq>|ack <account> <seq>|replay <after>|snapshot|status>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                log("[WORLDEDIT] active=" + std::string(gSession.active() ? "1" : "0") +
                    " revision=" + std::to_string(gSession.revision()) +
                    " sequence=" + std::to_string(gSession.lastSequence()));
                return;
            }
            const std::string& command = args[0];

            if (command == "begin") {
                if (args.size() < 3) {
                    log("[WORLDEDIT] begin <project> <version>");
                    return;
                }
                gSession.begin(args[1], args[2], "local");
                gSession.setTargetExistsCallback(
                    [](WorldEditTargetKind target, const std::string& id) {
                        MapConfigRegistry& registry = MapConfigRegistry::instance();
                        switch (target) {
                        case WorldEditTargetKind::Entity:
                            return registry.findEntity(id) != nullptr;
                        case WorldEditTargetKind::Block:
                            return registry.findBlock(id) != nullptr;
                        case WorldEditTargetKind::Vehicle:
                            return registry.findVehicle(id) != nullptr;
                        case WorldEditTargetKind::BombSite:
                            return registry.findSite(id) != nullptr;
                        case WorldEditTargetKind::NpcPreset:
                            for (const auto& item : registry.npcPresets())
                                if (item.id == id) return true;
                            return false;
                        case WorldEditTargetKind::GenerationZone:
                            for (const auto& item : registry.generationZones())
                                if (item.id == id) return true;
                            return false;
                        case WorldEditTargetKind::Interaction:
                            for (const auto& item : registry.interactions())
                                if (item.id == id) return true;
                            return false;
                        default:
                            return true;
                        }
                    });
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::World, StructuredLevel::Important,
                    "world-edit.begin", args[1], "edit session began", 0,
                    {{"project", args[1]}, {"version", args[2]},
                     {"owner", "local"}, {"identity_mode", "local-unsigned"}},
                    __FILE__, __LINE__, __func__);
                log("[WORLDEDIT] session " + args[1] + " @" + args[2] +
                    " owner=local identity=local-unsigned");
                return;
            }

            if (command == "grant" || command == "revoke") {
                if (args.size() < 3) {
                    log("[WORLDEDIT] " + command + " <account> <capability>");
                    return;
                }
                WorldEditCapability capability;
                if (!parseCapability(args[2], capability)) {
                    log("[WORLDEDIT] unknown capability " + args[2]);
                    return;
                }
                if (command == "grant")
                    gSession.grantCapabilities(args[1], capability);
                else
                    gSession.revokeCapabilities(args[1], capability);
                log("[WORLDEDIT] " + command + " " + args[1] + " -> " +
                    std::to_string(static_cast<uint32_t>(
                        gSession.capabilities(args[1]))));
                return;
            }

            if (command == "set") {
                if (args.size() < 7) {
                    log("[WORLDEDIT] set <account> <target> <id> <property> <type> <value...> [base=<rev>]");
                    return;
                }
                WorldEditTargetKind target;
                if (!parseTarget(args[2], target)) {
                    log("[WORLDEDIT] unknown target " + args[2]);
                    return;
                }
                uint16_t propertyId = 0;
                try {
                    propertyId = static_cast<uint16_t>(std::stoul(args[4]));
                } catch (const std::exception&) {
                    log("[WORLDEDIT] property id must be a number");
                    return;
                }
                const std::string propertyName = args[4];

                size_t valueEnd = args.size();
                uint32_t baseRevision = 0;
                for (size_t i = 6; i < args.size(); ++i) {
                    if (args[i].rfind("base=", 0) == 0) {
                        try { baseRevision = static_cast<uint32_t>(std::stoul(args[i].substr(5))); }
                        catch (const std::exception&) {}
                        valueEnd = i;
                        break;
                    }
                }
                std::vector<std::string> valueArgs(args.begin() + 6,
                                                   args.begin() + valueEnd);
                WorldEditValue value;
                if (!parseValue(args[5], valueArgs, 0, value)) {
                    log("[WORLDEDIT] invalid value for type " + args[5]);
                    return;
                }

                WorldEditOperation operation;
                operation.opId = gNextOperationId++;
                operation.baseRevision = baseRevision;
                operation.capability = defaultCapability(target);
                operation.kind = WorldEditOpKind::Set;
                operation.target = target;
                operation.objectId = args[3];
                operation.propertyId = propertyId;
                operation.value = value;
                WorldEditValue previousValue;
                bool hasPrevious = false;
                readWorldEditProperty(operation, previousValue, hasPrevious);
                operation.hasPrevious = hasPrevious;
                operation.previousValue = previousValue;

                const WorldEditResult result = gSession.submit(args[1], operation);
                logResult(result, "set", args[2], args[3], propertyId);
                if (!result.accepted) return;

                std::string reason;
                if (!applyWorldEditOperation(operation, &reason)) {
                    StructuredLogger::instance().writeEvent(
                        StructuredCategory::World, StructuredLevel::Errors,
                        "world-edit.apply-failed", args[3],
                        "accepted edit could not be applied", 0,
                        {{"reason", reason}, {"target", args[2]},
                         {"object_id", args[3]},
                         {"property_id", propertyId}},
                        __FILE__, __LINE__, __func__);
                    log("[WORLDEDIT] apply failed: " + reason);
                    return;
                }
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::World, StructuredLevel::Important,
                    "world-edit.applied", args[3],
                    "host-accepted edit applied to map data", 0,
                    {{"action", "set"},
                     {"sequence", result.sequence},
                     {"revision", result.revision},
                     {"target", args[2]},
                     {"object_id", args[3]},
                     {"property_id", propertyId},
                     {"before_exists", operation.hasPrevious},
                     {"before_type", valueTypeName(operation.previousValue.type)},
                     {"before", valueToJson(operation.previousValue)},
                     {"after_type", valueTypeName(operation.value.type)},
                     {"after", valueToJson(operation.value)}},
                    __FILE__, __LINE__, __func__);
                log("[WORLDEDIT] applied " + args[2] + " " + args[3] +
                    " property=" + propertyName);
                return;
            }

            if (command == "get") {
                if (args.size() < 4) {
                    log("[WORLDEDIT] get <target> <id> <property>");
                    return;
                }
                WorldEditTargetKind target;
                if (!parseTarget(args[1], target)) {
                    log("[WORLDEDIT] unknown target " + args[1]);
                    return;
                }
                uint16_t propertyId = 0;
                try {
                    propertyId = static_cast<uint16_t>(std::stoul(args[3]));
                } catch (const std::exception&) {
                    log("[WORLDEDIT] property id must be a number");
                    return;
                }
                WorldEditOperation operation;
                operation.target = target;
                operation.objectId = args[2];
                operation.propertyId = propertyId;
                WorldEditValue value;
                bool exists = false;
                const bool supported = readWorldEditProperty(operation, value,
                                                             exists);
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::World, StructuredLevel::Important,
                    "world-edit.read", args[2],
                    "read current authored property", 0,
                    {{"target", args[1]}, {"object_id", args[2]},
                     {"property_id", propertyId}, {"supported", supported},
                     {"exists", exists}, {"type", valueTypeName(value.type)},
                     {"value", valueToJson(value)}},
                    __FILE__, __LINE__, __func__);
                log(std::string("[WORLDEDIT] get ") +
                    (exists ? "exists" : "missing") +
                    " type=" + valueTypeName(value.type) +
                    " value=" + valueToJson(value).dump());
                return;
            }

            if (command == "undo") {
                if (args.size() < 3) {
                    log("[WORLDEDIT] undo <account> <sequence>");
                    return;
                }
                uint32_t sequence = 0;
                try { sequence = static_cast<uint32_t>(std::stoul(args[2])); }
                catch (const std::exception&) {
                    log("[WORLDEDIT] invalid sequence");
                    return;
                }
                const WorldEditResult result = gSession.submitUndo(args[1], sequence);
                logResult(result, "undo", "", "", 0);
                if (!result.accepted) return;

                WorldEditAccepted accepted;
                if (gSession.acceptedBySequence(result.sequence, accepted)) {
                    WorldEditOperation inverse;
                    inverse.kind = accepted.kind;
                    inverse.target = accepted.target;
                    inverse.objectId = accepted.objectId;
                    inverse.propertyId = accepted.propertyId;
                    inverse.value = accepted.value;
                    std::string reason;
                    if (!applyWorldEditOperation(inverse, &reason)) {
                        StructuredLogger::instance().writeEvent(
                            StructuredCategory::World, StructuredLevel::Errors,
                            "world-edit.apply-failed", accepted.objectId,
                            "accepted undo could not be applied", 0,
                            {{"reason", reason}, {"sequence", result.sequence},
                             {"target", static_cast<int>(accepted.target)},
                             {"object_id", accepted.objectId},
                             {"property_id", accepted.propertyId}},
                            __FILE__, __LINE__, __func__);
                        log("[WORLDEDIT] undo apply failed: " + reason);
                    } else {
                        StructuredLogger::instance().writeEvent(
                            StructuredCategory::World, StructuredLevel::Important,
                            "world-edit.applied", accepted.objectId,
                            "host-accepted undo applied to map data", 0,
                            {{"action", "undo"},
                             {"sequence", result.sequence},
                             {"revision", result.revision},
                             {"target", static_cast<int>(accepted.target)},
                             {"object_id", accepted.objectId},
                             {"property_id", accepted.propertyId},
                             {"before_type", valueTypeName(accepted.previousValue.type)},
                             {"before", valueToJson(accepted.previousValue)},
                             {"after_type", valueTypeName(accepted.value.type)},
                             {"after", valueToJson(accepted.value)}},
                            __FILE__, __LINE__, __func__);
                    }
                }
                return;
            }

            if (command == "ack") {
                if (args.size() < 3) {
                    log("[WORLDEDIT] ack <account> <sequence>");
                    return;
                }
                try {
                    const uint32_t sequence = static_cast<uint32_t>(std::stoul(args[2]));
                    log(std::string("[WORLDEDIT] ack ") +
                        (gSession.acknowledge(args[1], sequence) ? "ok" : "rejected") +
                        " last=" + std::to_string(gSession.acknowledged(args[1])));
                } catch (const std::exception&) {
                    log("[WORLDEDIT] invalid sequence");
                }
                return;
            }

            if (command == "replay") {
                const uint32_t after = args.size() >= 2
                    ? static_cast<uint32_t>(std::stoul(args[1])) : 0;
                std::vector<WorldEditAccepted> accepted;
                const bool retained = gSession.replay("local", after, accepted);
                log(std::string("[WORLDEDIT] replay ") +
                    (retained ? "retained" : "snapshot-required") +
                    " count=" + std::to_string(accepted.size()));
                return;
            }

            if (command == "snapshot") {
                WorldEditSnapshot snapshot;
                gSession.snapshot(snapshot);
                log("[WORLDEDIT] snapshot seq=" +
                    std::to_string(snapshot.lastSequence) +
                    " rev=" + std::to_string(snapshot.revision) +
                    " properties=" + std::to_string(snapshot.properties.size()) +
                    " hash=" + snapshot.stateHash.substr(0, 16));
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::World, StructuredLevel::Important,
                    "world-edit.snapshot", "world-edit",
                    "authoritative snapshot built", 0,
                    {{"sequence", snapshot.lastSequence},
                     {"revision", snapshot.revision},
                     {"properties", snapshot.properties.size()},
                     {"hash", snapshot.stateHash}},
                    __FILE__, __LINE__, __func__);
                return;
            }

            if (command == "status") {
                log("[WORLDEDIT] active=" + std::string(gSession.active() ? "1" : "0") +
                    " project=" + gSession.projectId() +
                    " version=" + gSession.projectVersion() +
                    " revision=" + std::to_string(gSession.revision()) +
                    " sequence=" + std::to_string(gSession.lastSequence()) +
                    " log=" + std::to_string(gSession.logSize()));
                return;
            }

            log("[WORLDEDIT] unknown command: " + command);
        }
    });
}
