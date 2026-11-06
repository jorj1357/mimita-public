// 2026-11-05
#include "interaction/world-interaction-runtime.h"

#include <algorithm>
#include <exception>
#include <string>

#include "debug/structured-log.h"
#include "devtools/terminal.h"
#include "interaction/interaction-system.h"
#include "terminal/terminal-state.h"

namespace {

constexpr const char* kOwner = "world-map-interactions";

glm::vec3 interactionPosition(const MapObjectiveConfig& config,
                              const AuthoredInteraction& interaction)
{
    if (interaction.targetKind == "entity") {
        for (const auto& entity : config.entities)
            if (entity.id == interaction.targetId)
                return entity.position;
    } else if (interaction.targetKind == "block") {
        for (const auto& block : config.blocks)
            if (block.id == interaction.targetId)
                return block.position;
    } else if (interaction.targetKind == "bomb_site" ||
               interaction.targetKind == "bombsite") {
        for (const auto& site : config.bombSites)
            if (site.id == interaction.targetId)
                return site.position;
    } else if (interaction.targetKind == "vehicle") {
        for (const auto& vehicle : config.vehicles)
            if (vehicle.id == interaction.targetId)
                return vehicle.position;
    } else if (interaction.targetKind == "payload") {
        return config.payload.cartOrigin;
    } else if (interaction.targetKind == "player") {
        return THE_PLAYER.pos;
    }
    return glm::vec3(0.0f);
}

} // namespace

WorldInteractionRuntime& WorldInteractionRuntime::instance()
{
    static WorldInteractionRuntime runtime;
    return runtime;
}

void WorldInteractionRuntime::clear()
{
    InteractionSystem::instance().clearOwner(kOwner);
    mTargetIds.clear();
    mHasSynced = false;
}

uint32_t WorldInteractionRuntime::targetIdFor(
    const std::string& interactionId) const
{
    const auto it = mTargetIds.find(interactionId);
    return it == mTargetIds.end() ? 0 : it->second;
}

void WorldInteractionRuntime::syncIfChanged(const MapObjectiveConfig& config,
                                            uint64_t revision)
{
    if (mHasSynced && mLastRevision == revision)
        return;
    mHasSynced = true;
    mLastRevision = revision;

    registerBuiltInWorldInteractionActions();
    InteractionSystem& interactions = InteractionSystem::instance();
    interactions.clearOwner(kOwner);
    mTargetIds.clear();

    const InteractionActionRegistry& actions =
        InteractionActionRegistry::instance();
    for (const auto& definition : config.interactions) {
        if (!definition.enabled || definition.actionId.empty())
            continue;
        const glm::vec3 position = interactionPosition(config, definition);
        InteractionTarget target;
        target.owner = kOwner;
        target.label = definition.prompt.empty()
            ? definition.actionId : definition.prompt;
        target.promptOrigin = position + glm::vec3(0.0f, 0.0f, 1.2f);
        target.interactOrigin = position;
        target.radius = definition.range;
        target.priority = 1;
        target.enabled = true;
        target.inputMode = definition.inputMode == "hold"
            ? InteractionInputMode::Hold : InteractionInputMode::Press;
        target.holdTicks = definition.inputMode == "hold"
            ? definition.holdTicks : 0;

        const std::string actionId = definition.actionId;
        const std::string targetId = definition.targetId;
        target.activate = [actionId, targetId](const InteractionContext& context) {
            return InteractionActionRegistry::instance().invoke(
                actionId, context, targetId);
        };

        if (!actions.has(actionId)) {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::General, StructuredLevel::Errors,
                "interaction.action.missing", actionId,
                "map interaction references an unregistered action", 0,
                {{"action", actionId}, {"target", targetId},
                 {"map", config.mapId}},
                __FILE__, __LINE__, __func__);
        }
        const uint32_t createdTargetId = interactions.add(std::move(target));
        mTargetIds[definition.id] = createdTargetId;
    }
}

void registerWorldInteractionCommands()
{
    Terminal::instance().registerCommand({
        "worldinteraction",
        "Author a map interaction and exercise its shared press/hold action",
        "worldinteraction <add <id> <action> <targetKind> <targetId> <prompt> <range> <press|hold> [holdTicks]|hold <id>|release|list>",
        [](const std::vector<std::string>& args) {
            Terminal& terminal = Terminal::instance();
            if (args.empty()) {
                terminal.addLog("[WORLDINTERACTION] add|hold|release|list");
                return;
            }
            MapConfigRegistry& registry = MapConfigRegistry::instance();
            if (args[0] == "add") {
                if (args.size() < 8) {
                    terminal.addLog("[WORLDINTERACTION] add <id> <action> <targetKind> <targetId> <prompt> <range> <press|hold> [holdTicks]");
                    return;
                }
                try {
                    AuthoredInteraction interaction;
                    interaction.id = args[1];
                    interaction.actionId = args[2];
                    interaction.targetKind = args[3];
                    interaction.targetId = args[4];
                    interaction.prompt = args[5];
                    interaction.range = std::max(0.25f, std::stof(args[6]));
                    interaction.inputMode = args[7] == "hold" ? "hold" : "press";
                    interaction.holdTicks = args.size() >= 9
                        ? std::max(0, std::stoi(args[8])) : 0;
                    if (interaction.inputMode == "hold" &&
                        interaction.holdTicks <= 0)
                        interaction.holdTicks = 30;
                    registry.interactionsMutable().push_back(
                        std::move(interaction));
                    registry.markChanged();
                    WorldInteractionRuntime::instance().syncIfChanged(
                        registry.current(), registry.revision());
                    const uint32_t targetId =
                        WorldInteractionRuntime::instance().targetIdFor(args[1]);
                    StructuredLogger::instance().writeEvent(
                        StructuredCategory::General, StructuredLevel::Important,
                        "world-interaction.authored", args[1],
                        "map-authored interaction registered", 0,
                        {{"interaction", args[1]}, {"action", args[2]},
                         {"target_kind", args[3]}, {"target_id", args[4]},
                         {"prompt", args[5]}, {"range", args[6]},
                         {"mode", args[7]}, {"hold_ticks",
                            args.size() >= 9 ? args[8] : std::string("0")},
                         {"interaction_target_id", targetId}},
                        __FILE__, __LINE__, __func__);
                    terminal.addLog("[WORLDINTERACTION] added " + args[1] +
                                    " targetId=" + std::to_string(targetId));
                } catch (const std::exception&) {
                    terminal.addLog("[WORLDINTERACTION] invalid number");
                }
                return;
            }
            if (args[0] == "hold") {
                if (args.size() < 2) {
                    terminal.addLog("[WORLDINTERACTION] hold <id>");
                    return;
                }
                const uint32_t targetId =
                    WorldInteractionRuntime::instance().targetIdFor(args[1]);
                const bool started = targetId != 0 &&
                    InteractionSystem::instance().beginHold(targetId, THE_CAMERA);
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::General, StructuredLevel::Important,
                    "world-interaction.hold-command", args[1],
                    "runtime hold command", 0,
                    {{"interaction", args[1]}, {"target_id", targetId},
                     {"started", started}, {"player_pos",
                        {THE_PLAYER.pos.x, THE_PLAYER.pos.y, THE_PLAYER.pos.z}}},
                    __FILE__, __LINE__, __func__);
                terminal.addLog(std::string("[WORLDINTERACTION] hold ") +
                                (started ? "started" : "rejected"));
                return;
            }
            if (args[0] == "release") {
                InteractionSystem::instance().releaseHold();
                terminal.addLog("[WORLDINTERACTION] released");
                return;
            }
            if (args[0] == "list") {
                terminal.addLog("[WORLDINTERACTION] authored=" +
                    std::to_string(registry.interactions().size()) +
                    " targets=" + std::to_string(
                        InteractionSystem::instance().count()));
                return;
            }
            terminal.addLog("[WORLDINTERACTION] unknown command");
        },
        "2026-11-05", CommandCategory::Debug
    });
}

void registerBuiltInWorldInteractionActions()
{
    InteractionActionRegistry& registry = InteractionActionRegistry::instance();
    if (registry.has("world.debug"))
        return;

    registry.registerAction("world.debug",
        [](const InteractionContext&, const std::string& targetId) {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::General, StructuredLevel::Important,
                "interaction.action.executed", "world.debug",
                "built-in authored interaction action executed", 0,
                {{"action", "world.debug"}, {"target", targetId}},
                __FILE__, __LINE__, __func__);
            return true;
        });
}
