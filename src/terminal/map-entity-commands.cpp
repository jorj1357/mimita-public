#include "terminal/map-entity-commands.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "config.h"
#include "debug/debug-visuals.h"
#include "devtools/terminal.h"
#include "entities/player.h"
#include "gamemode/map-config.h"
#include "network/server-gamemode.h"
#include "terminal/terminal-state.h"

namespace {

std::string activeMapId()
{
    const std::string& serverMap = MimitaNet::serverGamemodeState().mapId;
    if (!serverMap.empty()) return serverMap;
    if (gpActiveMapPath && !gpActiveMapPath->empty())
        return std::filesystem::path(*gpActiveMapPath).stem().string();
    return "zombietower1";
}

MapConfigRegistry& registry()
{
    auto& reg = MapConfigRegistry::instance();
    const std::string mapId = activeMapId();
    if (reg.current().mapId != mapId) reg.load(mapId);
    return reg;
}

std::string selectedId;

std::string argOrSelection(const std::vector<std::string>& args, size_t index = 0)
{
    if (args.size() > index) return args[index];
    return selectedId;
}

glm::vec3 placement()
{
    if (gpCamera && gpPlayer)
        return gpPlayer->pos + gpCamera->front * 5.0f;
    return glm::vec3(0.0f);
}

void printEntity(const MapEntity& entity)
{
    Terminal::instance().addLog(
        "[ENTITY] " + entity.id + " type=" + entity.type +
        " pos=(" + std::to_string(entity.position.x) + " " +
        std::to_string(entity.position.y) + " " + std::to_string(entity.position.z) +
        ") size=(" + std::to_string(entity.size.x) + " " +
        std::to_string(entity.size.y) + " " + std::to_string(entity.size.z) +
        ") radius=" + std::to_string(entity.radius) +
        " enabled=" + (entity.enabled ? "1" : "0"));
}

void ensureEntityConfig()
{
    auto& reg = registry();
    if (reg.current().mapId.empty())
        Terminal::instance().addLog("[ENTITY] no active map; using " + activeMapId());
}

} // namespace

void registerMapEntityCommands()
{
    auto& terminal = Terminal::instance();
    terminal.registerCommand({
        "entity_help", "List Map Entity Editor commands", "entity_help",
        [](const std::vector<std::string>&) {
            Terminal::instance().addLog("[ENTITY] entity_add <type> [id]");
            Terminal::instance().addLog("[ENTITY] entity_list | entity_info [id] | entity_select <id>");
            Terminal::instance().addLog("[ENTITY] entity_move_here [id] | entity_set_position <id> <x> <y> <z>");
            Terminal::instance().addLog("[ENTITY] entity_set <id> <radius|size|spawnCount|maxAlive|spawnCooldownTicks|monsterPool|oneShot|enabled> <value>");
            Terminal::instance().addLog("[ENTITY] entity_delete [id] | entity_visibility <on|off>");
            Terminal::instance().addLog("[ENTITY] entity_save | entity_reload");
        }, "", CommandCategory::Editor
    });
    terminal.registerCommand({
        "entity_add", "Add a map gameplay entity at the camera-forward placement", 
        "entity_add <monster_zone|checkpoint|pickup|damage_volume|boss_trigger|spawnpoint> [id]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) { Terminal::instance().addLog("[ENTITY] usage: entity_add <type> [id]"); return; }
            ensureEntityConfig();
            const std::string id = registry().createEntity(args[0], placement(), args.size() > 1 ? args[1] : "");
            if (id.empty()) { Terminal::instance().addLog("[ENTITY] id already exists or is invalid"); return; }
            selectedId = id;
            Terminal::instance().addLog("[ENTITY] created " + id + " (use entity_save)");
        }, "", CommandCategory::Editor
    });
    terminal.registerCommand({
        "entity_list", "List authored map entities", "entity_list",
        [](const std::vector<std::string>&) {
            ensureEntityConfig();
            const auto& entities = registry().current().entities;
            Terminal::instance().addLog("[ENTITY] map=" + registry().current().mapId + " count=" + std::to_string(entities.size()));
            for (const auto& entity : entities) printEntity(entity);
        }, "", CommandCategory::Editor
    });
    terminal.registerCommand({
        "entity_info", "Inspect one authored map entity", "entity_info [id]",
        [](const std::vector<std::string>& args) {
            ensureEntityConfig();
            const std::string id = argOrSelection(args);
            const MapEntity* entity = registry().findEntity(id);
            if (!entity) { Terminal::instance().addLog("[ENTITY] unknown id " + id); return; }
            printEntity(*entity);
            Terminal::instance().addLog("[ENTITY] pool=" + entity->monsterPool + " pickup=" + entity->pickupId + " boss=" + entity->bossId);
        }, "", CommandCategory::Editor
    });
    terminal.registerCommand({
        "entity_select", "Select an authored map entity", "entity_select <id>",
        [](const std::vector<std::string>& args) {
            ensureEntityConfig();
            if (args.empty() || !registry().findEntity(args[0])) { Terminal::instance().addLog("[ENTITY] unknown id"); return; }
            selectedId = args[0];
            Terminal::instance().addLog("[ENTITY] selected " + selectedId);
        }, "", CommandCategory::Editor
    });
    terminal.registerCommand({
        "entity_move_here", "Move an authored map entity to the current placement", "entity_move_here [id]",
        [](const std::vector<std::string>& args) {
            ensureEntityConfig();
            const std::string id = argOrSelection(args);
            MapEntity* entity = registry().findEntityMutable(id);
            if (!entity) { Terminal::instance().addLog("[ENTITY] unknown id " + id); return; }
            entity->position = placement();
            Terminal::instance().addLog("[ENTITY] moved " + id);
        }, "", CommandCategory::Editor
    });
    terminal.registerCommand({
        "entity_set_position", "Set an authored map entity position", "entity_set_position <id> <x> <y> <z>",
        [](const std::vector<std::string>& args) {
            try {
                if (args.size() < 4) throw std::runtime_error("expected id x y z");
                ensureEntityConfig();
                MapEntity* entity = registry().findEntityMutable(args[0]);
                if (!entity) throw std::runtime_error("unknown id");
                entity->position = glm::vec3(std::stof(args[1]), std::stof(args[2]), std::stof(args[3]));
                selectedId = entity->id;
                Terminal::instance().addLog("[ENTITY] position updated " + entity->id);
            } catch (const std::exception& e) { Terminal::instance().addLog(std::string("[ENTITY] ") + e.what()); }
        }, "", CommandCategory::Editor
    });
    terminal.registerCommand({
        "entity_set", "Set an authored map entity property", "entity_set <id> <radius|size|spawnCount|maxAlive|spawnCooldownTicks|monsterPool|oneShot|enabled> <value>",
        [](const std::vector<std::string>& args) {
            try {
                if (args.size() < 3) throw std::runtime_error("expected id property value");
                ensureEntityConfig();
                MapEntity* entity = registry().findEntityMutable(args[0]);
                if (!entity) throw std::runtime_error("unknown id");
                const std::string& property = args[1];
                if (property == "radius") entity->radius = std::stof(args[2]);
                else if (property == "spawnCount") entity->spawnCount = std::stoi(args[2]);
                else if (property == "maxAlive") entity->maxAlive = std::stoi(args[2]);
                else if (property == "spawnCooldownTicks") entity->spawnCooldownTicks = std::stoi(args[2]);
                else if (property == "monsterPool") entity->monsterPool = args[2];
                else if (property == "oneShot") entity->oneShot = args[2] != "0" && args[2] != "false";
                else if (property == "enabled") entity->enabled = args[2] != "0" && args[2] != "false";
                else if (property == "size") {
                    if (args.size() < 5) throw std::runtime_error("size requires x y z");
                    entity->size = glm::vec3(std::stof(args[2]), std::stof(args[3]), std::stof(args[4]));
                } else throw std::runtime_error("unknown property");
                selectedId = entity->id;
                Terminal::instance().addLog("[ENTITY] property updated " + entity->id + "." + property);
            } catch (const std::exception& e) { Terminal::instance().addLog(std::string("[ENTITY] ") + e.what()); }
        }, "", CommandCategory::Editor
    });
    terminal.registerCommand({
        "entity_delete", "Delete an authored map entity", "entity_delete [id]",
        [](const std::vector<std::string>& args) {
            ensureEntityConfig();
            const std::string id = argOrSelection(args);
            if (!registry().deleteEntity(id)) { Terminal::instance().addLog("[ENTITY] unknown id " + id); return; }
            if (selectedId == id) selectedId.clear();
            Terminal::instance().addLog("[ENTITY] deleted " + id + " (use entity_save)");
        }, "", CommandCategory::Editor
    });
    terminal.registerCommand({
        "entity_visibility", "Show or hide authored map entities", "entity_visibility <on|off>",
        [](const std::vector<std::string>& args) {
            const bool visible = args.empty() ? true : args[0] != "off" && args[0] != "0";
            ensureEntityConfig();
            registry().setEntityVisibility(visible);
            if (visible) {
                DebugVis::setMasterEnabled(true);
                DebugConfig::DEBUG_RENDER = true;
            }
            Terminal::instance().addLog(std::string("[ENTITY] visibility=") + (visible ? "on" : "off"));
        }, "", CommandCategory::Editor
    });
    terminal.registerCommand({
        "entity_save", "Save authored map entities to the active map JSON", "entity_save",
        [](const std::vector<std::string>&) {
            ensureEntityConfig();
            Terminal::instance().addLog(registry().save() ? "[ENTITY] saved" : "[ENTITY] save failed");
        }, "", CommandCategory::Editor
    });
    terminal.registerCommand({
        "entity_reload", "Reload the active map JSON, retaining last valid state on error", "entity_reload",
        [](const std::vector<std::string>&) {
            ensureEntityConfig();
            const bool ok = registry().load(activeMapId());
            Terminal::instance().addLog(ok ? "[ENTITY] reload success" : "[ENTITY] reload failed; last valid state retained");
        }, "", CommandCategory::Editor
    });
}
