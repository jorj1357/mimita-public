// 2026-10-23
#include "mapeditor/map-editor-commands.h"

#include <algorithm>
#include <cstdio>
#include <exception>
#include <string>
#include <vector>

#include "debug/structured-log.h"
#include "devtools/terminal.h"
#include "hot-reload/module-host.h"
#include "mapeditor/map-editor.h"
#include "modules/api/map-editor-api.h"
#include "terminal/terminal-state.h"

void registerMapEditorCommands()
{
    auto& terminal = Terminal::instance();
    terminal.registerCommand({
        "mapeditor", "Enter/leave the freecam map editor or run an editor action",
        "mapeditor [0|1|status|save|reload|freeze <0|1>|rebuild_collision|autosave|focus|add <entity <type>|block|vehicle [template]|bombsite|route|cart|chunk <geometry>|zone <seed>> [id]|chunk_connector <chunkId> <connectorId> <side> <orientation> [tags...]|spawn_vehicle [id]|vehicle_wheel <index> <hub x y z|radius|width> [values]|route_checkpoint <0|1>|route_final <0|1>|route_enabled <0|1>|route_radius <m>|route_bonus <sec>|route_wait <ticks>|route_move <up|down>|route_validate|cart_origin_to_route_start|route_start_to_cart_origin|nudge <dx> <dy> <dz>|set_position <x> <y> <z>|set_radius <m>|select <id>|select_kind <entity|block|vehicle|bombsite|route|cart|glb|all>|select_route|selectall|copy|paste|cut|duplicate|delete|undo|redo|help]",
        [](const std::vector<std::string>& args) {
            MapEditor& editor = MapEditor::instance();
            if (args.empty()) {
                editor.toggle();
                return;
            }
            const std::string& command = args[0];
            if (command == "1" || command == "on") {
                editor.setActive(true);
                return;
            }
            if (command == "0" || command == "off") {
                editor.setActive(false);
                return;
            }
            if (command == "status") {
                Terminal::instance().addLog(
                    std::string("[MAPEDIT] active=") + (editor.active() ? "1" : "0") +
                    " map=" + editor.mapId() +
                    " dirty=" + (editor.dirty() ? "1" : "0") +
                    " freeze=" + (editor.freezeWorld() ? "1" : "0") +
                    " selection=" + std::to_string(editor.selectionCount()));
                return;
            }
            if (command == "save") {
                editor.save();
                return;
            }
            if (command == "reload") {
                editor.reload();
                return;
            }
            if (command == "rebuild_collision") {
                editor.flushCollisionNow();
                return;
            }
            if (command == "autosave") {
                editor.autosave();
                return;
            }
            if (command == "cart_origin_to_route_start") {
                if (!editor.active() && !editor.setActive(true)) return;
                editor.cartOriginToRouteStart();
                return;
            }
            if (command == "route_start_to_cart_origin") {
                if (!editor.active() && !editor.setActive(true)) return;
                editor.routeStartToCartOrigin();
                return;
            }
            const bool moduleCommand =
                command == "select" || command == "select_kind" ||
                command == "select_route" || command == "selectall" ||
                command == "clear_selection" || command == "select_none" ||
                command == "copy" || command == "paste" || command == "cut" ||
                command == "duplicate" || command == "delete" ||
                command == "undo" || command == "redo" ||
                command == "save" || command == "reload" ||
                command == "autosave" || command == "rebuild_collision" ||
                command == "focus" || command == "nudge" ||
                command == "set_position" || command == "set_pos" ||
                command == "set_rotation" || command == "set_rot" ||
                command == "set_scale" || command == "set_size" ||
                command == "resize" || command == "set_radius" ||
                command == "site_radius" || command == "rename";
            if (moduleCommand) {
                const bool needsActive =
                    command != "save" && command != "reload" &&
                    command != "autosave" && command != "rebuild_collision";
                if (needsActive && !editor.active() && !editor.setActive(true))
                    return;
                std::string moduleArgs;
                for (size_t i = 1; i < args.size(); ++i) {
                    if (!moduleArgs.empty()) moduleArgs += ' ';
                    moduleArgs += args[i];
                }
                const bool moduleHandled =
                    ModuleHost::instance().mapEditorLoaded() &&
                    ModuleHost::instance().mapEditorExecuteCommand(
                        command.c_str(), moduleArgs.c_str());
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::World, StructuredLevel::Important,
                    "map-editor.command", command,
                    "map editor command dispatched", 0,
                    nlohmann::json{{"command", command},
                                   {"arguments", moduleArgs},
                                   {"handled_by_module", moduleHandled},
                                   {"generation", ModuleHost::instance().activeGeneration(
                                        "map-editor")}},
                    __FILE__, __LINE__, __FUNCTION__);
                if (moduleHandled)
                    return;
            }
            if (command == "select") {
                if (!editor.active() && !editor.setActive(true)) return;
                if (args.size() < 2) {
                    Terminal::instance().addLog("[MAPEDIT] select <id>");
                    return;
                }
                editor.selectObject(args[1]);
                return;
            }
            if (command == "select_kind") {
                if (!editor.active() && !editor.setActive(true)) return;
                if (args.size() < 2) {
                    Terminal::instance().addLog(
                        "[MAPEDIT] select_kind <entity|block|vehicle|bombsite|route|cart|glb|all>");
                    return;
                }
                editor.selectKind(args[1]);
                return;
            }
            if (command == "select_route") {
                if (!editor.active() && !editor.setActive(true)) return;
                editor.selectKind("route");
                return;
            }
            if (command == "set_position" || command == "set_pos") {
                if (!editor.active() && !editor.setActive(true)) return;
                if (args.size() < 4) {
                    Terminal::instance().addLog("[MAPEDIT] set_position <x> <y> <z>");
                    return;
                }
                try {
                    editor.setSelectedPosition(glm::vec3(std::stof(args[1]),
                        std::stof(args[2]), std::stof(args[3])));
                } catch (const std::exception&) {
                    Terminal::instance().addLog("[MAPEDIT] invalid position");
                }
                return;
            }
            if (command == "set_radius" || command == "site_radius") {
                if (!editor.active() && !editor.setActive(true)) return;
                if (args.size() < 2) {
                    Terminal::instance().addLog("[MAPEDIT] set_radius <m>");
                    return;
                }
                try {
                    editor.setSelectedBombSiteRadius(std::stof(args[1]));
                } catch (const std::exception&) {
                    Terminal::instance().addLog("[MAPEDIT] invalid radius");
                }
                return;
            }
            if (command == "set_size" || command == "resize") {
                if (args.size() < 4) {
                    Terminal::instance().addLog("[MAPEDIT] set_size <x> <y> <z>");
                    return;
                }
                try {
                    editor.setSelectedSize(glm::vec3(std::stof(args[1]),
                        std::stof(args[2]), std::stof(args[3])));
                } catch (const std::exception&) {
                    Terminal::instance().addLog("[MAPEDIT] invalid size");
                }
                return;
            }
            if (command == "rename") {
                if (args.size() < 2) {
                    Terminal::instance().addLog("[MAPEDIT] rename <name>");
                    return;
                }
                if (!editor.active() && !editor.setActive(true)) return;
                editor.setSelectedName(args[1]);
                return;
            }
            if (command == "set_rotation" || command == "set_rot") {
                if (args.size() < 4) {
                    Terminal::instance().addLog(
                        "[MAPEDIT] set_rotation <x> <y> <z>");
                    return;
                }
                if (!editor.active() && !editor.setActive(true)) return;
                try {
                    editor.setSelectedRotation(glm::vec3(
                        std::stof(args[1]), std::stof(args[2]),
                        std::stof(args[3])));
                } catch (const std::exception&) {
                    Terminal::instance().addLog("[MAPEDIT] invalid rotation");
                }
                return;
            }
            if (command == "set_scale") {
                if (args.size() < 4) {
                    Terminal::instance().addLog("[MAPEDIT] set_scale <x> <y> <z>");
                    return;
                }
                if (!editor.active() && !editor.setActive(true)) return;
                try {
                    editor.setSelectedScale(glm::vec3(
                        std::stof(args[1]), std::stof(args[2]),
                        std::stof(args[3])));
                } catch (const std::exception&) {
                    Terminal::instance().addLog("[MAPEDIT] invalid scale");
                }
                return;
            }
            if (command == "nudge") {
                if (!editor.active() && !editor.setActive(true)) return;
                if (args.size() < 4) {
                    Terminal::instance().addLog("[MAPEDIT] nudge <dx> <dy> <dz>");
                    return;
                }
                try {
                    editor.nudgeSelection(glm::vec3(std::stof(args[1]),
                        std::stof(args[2]), std::stof(args[3])));
                } catch (const std::exception&) {
                    Terminal::instance().addLog("[MAPEDIT] invalid nudge");
                }
                return;
            }
            if (command == "chunk_connector") {
                if (args.size() < 5) {
                    Terminal::instance().addLog(
                        "[MAPEDIT] chunk_connector <chunkId> <connectorId> <side> <orientation> [tag...]");
                    return;
                }
                MapConfigRegistry& registry = MapConfigRegistry::instance();
                for (auto& chunk : registry.chunksMutable()) {
                    if (chunk.id != args[1]) continue;
                    AuthoredChunkConnector connector;
                    connector.id = args[2];
                    connector.side = args[3];
                    connector.orientation = args[4];
                    for (size_t i = 5; i < args.size(); ++i)
                        connector.tags.push_back(args[i]);
                    chunk.connectors.push_back(std::move(connector));
                    registry.markChanged();
                    StructuredLogger::instance().writeEvent(
                        StructuredCategory::World, StructuredLevel::Important,
                        "map-editor.chunk-connector-added", args[1],
                        "authored chunk connector added through map editor", 0,
                        {{"chunk", args[1]}, {"connector", args[2]},
                         {"side", args[3]}, {"orientation", args[4]}},
                        __FILE__, __LINE__, __func__);
                    Terminal::instance().addLog("[MAPEDIT] connector added to " +
                                                args[1]);
                    return;
                }
                Terminal::instance().addLog("[MAPEDIT] chunk not found");
                return;
            }
            if (command == "chunk_preview") {
                if (args.size() < 2) {
                    Terminal::instance().addLog(
                        "[MAPEDIT] chunk_preview <chunkId>");
                    return;
                }
                if (!editor.active() && !editor.setActive(true)) return;
                editor.previewChunk(args[1]);
                return;
            }
            if (command == "chunk_part_material" ||
                command == "chunk_material") {
                if (args.size() < 3) {
                    Terminal::instance().addLog(
                        "[MAPEDIT] chunk_part_material <chunkId/partId> <material>");
                    return;
                }
                if (!editor.active() && !editor.setActive(true)) return;
                editor.selectObject(args[1]);
                editor.setSelectedChunkPartMaterial(args[2]);
                return;
            }
            if (command == "chunk_part_uv" || command == "chunk_uv") {
                if (args.size() < 4) {
                    Terminal::instance().addLog(
                        "[MAPEDIT] chunk_part_uv <chunkId/partId> <u> <v>");
                    return;
                }
                if (!editor.active() && !editor.setActive(true)) return;
                try {
                    editor.selectObject(args[1]);
                    editor.setSelectedChunkPartUvScale(std::stof(args[2]),
                                                      std::stof(args[3]));
                } catch (const std::exception&) {
                    Terminal::instance().addLog("[MAPEDIT] invalid uv scale");
                }
                return;
            }
            if (command == "chunk_connector_field") {
                if (args.size() < 4) {
                    Terminal::instance().addLog(
                        "[MAPEDIT] chunk_connector_field <chunkId/connectorId> <side|role|orientation|tag> <value>");
                    return;
                }
                if (!editor.active() && !editor.setActive(true)) return;
                editor.selectObject(args[1]);
                editor.setSelectedChunkConnectorField(args[2], args[3]);
                return;
            }
            if (command == "focus") {
                if (!editor.active() && !editor.setActive(true)) return;
                editor.focusSelection();
                return;
            }
            if (command == "undo") {
                if (!editor.active() && !editor.setActive(true)) return;
                editor.undoEdit();
                return;
            }
            if (command == "redo") {
                if (!editor.active() && !editor.setActive(true)) return;
                editor.redoEdit();
                return;
            }
            if (command == "freeze") {
                if (args.size() < 2) {
                    Terminal::instance().addLog(std::string("[MAPEDIT] freeze=") +
                        (editor.freezeWorld() ? "1" : "0"));
                    return;
                }
                editor.setFreezeWorld(args[1] != "0");
                return;
            }
            if (command == "add") {
                if (!editor.active() && !editor.setActive(true)) return;
                if (args.size() < 2) {
                    Terminal::instance().addLog("[MAPEDIT] add <entity <type>|block|bombsite|route|cart> [id] [radius]");
                    return;
                }
                const std::string& kind = args[1];
                if (kind == "entity") {
                    if (args.size() < 3) {
                        Terminal::instance().addLog("[MAPEDIT] add entity <type> [id]");
                        return;
                    }
                    editor.addEntity(args[2], args.size() > 3 ? args[3] : "");
                } else if (kind == "block") {
                    editor.addBlock(args.size() > 2 ? args[2] : "");
                } else if (kind == "vehicle" || kind == "car") {
                    const std::string templateId =
                        args.size() > 2 ? args[2] : "car_sedan";
                    const std::string vehicleId =
                        args.size() > 3 ? args[3] : "";
                    editor.addVehicle(templateId, vehicleId);
                } else if (kind == "bombsite" || kind == "site") {
                    float radius = 4.0f;
                    if (args.size() > 3) {
                        try { radius = std::max(0.1f, std::stof(args[3])); }
                        catch (...) { radius = 4.0f; }
                    }
                    editor.addBombSite(args.size() > 2 ? args[2] : "", radius);
                } else if (kind == "route") {
                    editor.addRoutePoint(args.size() > 2 ? args[2] : "");
                } else if (kind == "cart") {
                    editor.addCartOrigin();
                } else if (kind == "chunk") {
                    char created[64] = {};
                    if (!ModuleHost::instance().mapEditorAddChunk(
                            args.size() > 2 ? args[2].c_str() : "", created,
                            sizeof(created)))
                        editor.addChunk(args.size() > 2 ? args[2] : "");
                    else
                        Terminal::instance().addLog(std::string(
                            "[MAPEDIT] module added chunk ") + created);
                } else if (kind == "chunk_part") {
                    char created[64] = {};
                    if (!ModuleHost::instance().mapEditorAddChunkPart(
                            args.size() > 2 ? args[2].c_str() : "box", created,
                            sizeof(created)))
                        editor.addChunkPart(args.size() > 2 ? args[2] : "box");
                    else
                        Terminal::instance().addLog(std::string(
                            "[MAPEDIT] module added chunk part ") + created);
                } else if (kind == "connector") {
                    if (!ModuleHost::instance().mapEditorAddConnector(
                            args.size() > 2 ? args[2].c_str() : "east"))
                        editor.addConnector(args.size() > 2 ? args[2] : "east");
                } else if (kind == "chunk_spawn" || kind == "spawn") {
                    editor.addChunkSpawn(args.size() > 2 ? args[2] : "any");
                } else if (kind == "zone") {
                    if (args.size() < 3) {
                        Terminal::instance().addLog(
                            "[MAPEDIT] add zone <seed> [id] [generatorVersion]");
                        return;
                    }
                    MapConfigRegistry& registry = MapConfigRegistry::instance();
                    GenerationZone zone;
                    zone.id = args.size() > 3 ? args[3]
                        : "zone_" + std::to_string(registry.generationZones().size() + 1);
                    try {
                        zone.seed = static_cast<uint32_t>(std::stoul(args[2]));
                    } catch (const std::exception&) {
                        Terminal::instance().addLog("[MAPEDIT] invalid seed");
                        return;
                    }
                    zone.generatorVersion = args.size() > 4 ? args[4] : "1";
                    zone.boundsMin = THE_PLAYER.pos + glm::vec3(-400.0f, -100.0f, -400.0f);
                    zone.boundsMax = THE_PLAYER.pos + glm::vec3(400.0f, 200.0f, 400.0f);
                    zone.chunkSize = 104.0f;
                    for (const auto& chunk : registry.chunks())
                        zone.chunkDependencies.push_back(chunk.id);
                    registry.generationZonesMutable().push_back(std::move(zone));
                    registry.markChanged();
                    StructuredLogger::instance().writeEvent(
                        StructuredCategory::World, StructuredLevel::Important,
                        "map-editor.zone-added", args[2],
                        "authored generation zone added through map editor", 0,
                        {{"seed", args[2]},
                         {"zone", registry.generationZones().back().id},
                         {"chunk_dependencies",
                          registry.generationZones().back().chunkDependencies}},
                        __FILE__, __LINE__, __func__);
                    Terminal::instance().addLog("[MAPEDIT] added zone " +
                                                registry.generationZones().back().id);
                } else {
                    Terminal::instance().addLog("[MAPEDIT] unknown add kind " + kind);
                }
                return;
            }
            if (command == "preview_connections") {
                char report[2048] = {};
                if (ModuleHost::instance().mapEditorPreviewConnections(
                        report, sizeof(report))) {
                    Terminal::instance().addLog(std::string("[MAPEDIT] ") +
                                                report);
                } else {
                    editor.previewConnections();
                }
                return;
            }
            if (command == "attach") {
                if (!editor.active() && !editor.setActive(true)) return;
                editor.createAttachment();
                return;
            }
            if (command == "attachment_anchor") {
                if (!editor.active() && !editor.setActive(true)) return;
                if (args.size() < 2) {
                    Terminal::instance().addLog(
                        "[MAPEDIT] attachment_anchor <0|1>");
                    return;
                }
                editor.setAttachmentAnchored(args[1] != "0");
                return;
            }
            if (command == "attachment_remove") {
                if (!editor.active() && !editor.setActive(true)) return;
                editor.removeAttachmentForSelection();
                return;
            }
            if (command == "attachment_set") {
                if (!editor.active() && !editor.setActive(true)) return;
                if (args.size() < 4) {
                    Terminal::instance().addLog(
                        "[MAPEDIT] attachment_set <mass> <density> <strength>");
                    return;
                }
                try {
                    editor.setSelectedAttachmentProperties(
                        std::stof(args[1]), std::stof(args[2]),
                        std::stof(args[3]));
                } catch (const std::exception&) {
                    Terminal::instance().addLog(
                        "[MAPEDIT] invalid attachment property");
                }
                return;
            }
            if (command == "spawn_vehicle") {
                if (!editor.active() && !editor.setActive(true)) return;
                if (args.size() > 1)
                    editor.selectObject(args[1]);
                editor.spawnSelectedVehicle();
                return;
            }
            if (command == "vehicle_wheel") {
                if (!editor.active() && !editor.setActive(true)) return;
                if (args.size() < 3) {
                    Terminal::instance().addLog(
                        "[MAPEDIT] vehicle_wheel <index> <hub x y z|radius|width> [values]");
                    return;
                }
                int wheelIndex = -1;
                try {
                    wheelIndex = std::stoi(args[1]);
                } catch (const std::exception&) {
                    Terminal::instance().addLog("[MAPEDIT] invalid wheel index");
                    return;
                }
                const std::string& field = args[2];
                try {
                    if (field == "hub") {
                        if (args.size() < 6) {
                            Terminal::instance().addLog(
                                "[MAPEDIT] vehicle_wheel <index> hub <x> <y> <z>");
                            return;
                        }
                        editor.editSelectedVehicleWheel(
                            wheelIndex, field,
                            glm::vec3(std::stof(args[3]), std::stof(args[4]),
                                      std::stof(args[5])));
                    } else {
                        if (args.size() < 4) {
                            Terminal::instance().addLog(
                                "[MAPEDIT] vehicle_wheel <index> radius|width <value>");
                            return;
                        }
                        editor.editSelectedVehicleWheel(
                            wheelIndex, field, std::stof(args[3]));
                    }
                } catch (const std::exception&) {
                    Terminal::instance().addLog("[MAPEDIT] invalid wheel value");
                }
                return;
            }
            if (command == "route_checkpoint" || command == "route_final" ||
                command == "route_enabled") {
                if (!editor.active() && !editor.setActive(true)) return;
                if (args.size() < 2) {
                    Terminal::instance().addLog("[MAPEDIT] " + command + " <0|1>");
                    return;
                }
                const bool value = args[1] != "0";
                if (command == "route_checkpoint") editor.setSelectedRouteCheckpoint(value);
                else if (command == "route_final") editor.setSelectedRouteFinal(value);
                else editor.setSelectedRouteEnabled(value);
                return;
            }
            if (command == "route_radius" || command == "route_bonus" ||
                command == "route_wait") {
                if (!editor.active() && !editor.setActive(true)) return;
                if (args.size() < 2) {
                    Terminal::instance().addLog("[MAPEDIT] " + command + " <value>");
                    return;
                }
                try {
                    const float value = std::stof(args[1]);
                    if (command == "route_radius") editor.setSelectedRouteRadius(value);
                    else if (command == "route_bonus") editor.setSelectedRouteTimeBonus(value);
                    else editor.setSelectedRouteWaitTicks((int)value);
                } catch (const std::exception&) {
                    Terminal::instance().addLog("[MAPEDIT] invalid number " + args[1]);
                }
                return;
            }
            if (command == "route_move") {
                if (!editor.active() && !editor.setActive(true)) return;
                if (args.size() < 2 || (args[1] != "up" && args[1] != "down")) {
                    Terminal::instance().addLog("[MAPEDIT] route_move <up|down>");
                    return;
                }
                editor.moveSelectedRoute(args[1] == "up" ? -1 : 1);
                return;
            }
            if (command == "route_validate") {
                const std::string report = editor.validatePayloadRoute();
                if (report.empty()) {
                    Terminal::instance().addLog("[MAPEDIT] no payload route on this map");
                    return;
                }
                size_t start = 0;
                while (start <= report.size()) {
                    const size_t end = report.find('\n', start);
                    const std::string line = report.substr(start,
                        end == std::string::npos ? std::string::npos : end - start);
                    Terminal::instance().addLog("[MAPEDIT] " + line);
                    if (end == std::string::npos) break;
                    start = end + 1;
                }
                return;
            }
            if (command == "drag_probe") {
                if (!ModuleHost::instance().mapEditorLoaded()) {
                    Terminal::instance().addLog("[MAPEDIT] drag_probe: no map-editor module");
                    return;
                }
                mimita_map_editor::DragMathV1 drag{};
                drag.mode = 0;
                drag.mouseDelta[0] = 10.0f;
                drag.mouseDelta[1] = -5.0f;
                drag.cameraRight[0] = 1.0f;
                drag.cameraUp[1] = 1.0f;
                drag.unitsPerPixel = 0.01f;
                float delta[3] = {0.0f, 0.0f, 0.0f};
                const bool dragOk = ModuleHost::instance().mapEditorComputeDragDelta(
                    &drag, delta);
                mimita_map_editor::GizmoPickV1 pick{};
                pick.cursor[0] = 60.0f;
                pick.cursor[1] = 50.0f;
                pick.center[0] = 50.0f;
                pick.center[1] = 50.0f;
                pick.axisEnd[0][0] = 100.0f;
                pick.axisEnd[0][1] = 50.0f;
                pick.axisEnd[1][0] = 50.0f;
                pick.axisEnd[1][1] = 100.0f;
                pick.axisEnd[2][0] = 50.0f;
                pick.axisEnd[2][1] = 50.0f;
                pick.scaleEnd[0][0] = 0.0f;
                pick.scaleEnd[0][1] = 50.0f;
                pick.scaleEnd[1][0] = 50.0f;
                pick.scaleEnd[1][1] = 0.0f;
                pick.scaleEnd[2][0] = 50.0f;
                pick.scaleEnd[2][1] = 50.0f;
                pick.allowScale = 1;
                pick.pickPixels = 20.0f;
                int axis = -1;
                const bool pickOk = ModuleHost::instance().mapEditorPickGizmoAxis(
                    &pick, &axis);
                char message[256];
                std::snprintf(message, sizeof(message),
                    "[MAPEDIT] drag_probe delta=(%.3f,%.3f,%.3f) axis=%d drag=%d pick=%d",
                    delta[0], delta[1], delta[2], axis,
                    dragOk ? 1 : 0, pickOk ? 1 : 0);
                Terminal::instance().addLog(message);
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::World, StructuredLevel::Important,
                    "map-editor.drag-probe", "map-editor",
                    "bounded module drag/gizmo math probe", 0,
                    nlohmann::json{{"delta", {delta[0], delta[1], delta[2]}},
                                   {"axis", axis},
                                   {"drag_ok", dragOk},
                                   {"pick_ok", pickOk},
                                   {"generation",
                                    ModuleHost::instance().activeGeneration("map-editor")}},
                    __FILE__, __LINE__, __FUNCTION__);
                return;
            }
            if (command == "selectall") {
                if (!editor.active() && !editor.setActive(true)) return;
                editor.selectAllEdit();
                return;
            }
            if (command == "copy") {
                if (!editor.active() && !editor.setActive(true)) return;
                editor.copyEdit();
                return;
            }
            if (command == "paste") {
                if (!editor.active() && !editor.setActive(true)) return;
                editor.pasteEdit();
                return;
            }
            if (command == "cut") {
                if (!editor.active() && !editor.setActive(true)) return;
                editor.cutEdit();
                return;
            }
            if (command == "duplicate") {
                if (!editor.active() && !editor.setActive(true)) return;
                editor.duplicateEdit();
                return;
            }
            if (command == "delete") {
                if (!editor.active() && !editor.setActive(true)) return;
                editor.deleteEdit();
                return;
            }
            if (command == "help") {
                Terminal::instance().addLog("[MAPEDIT] RMB look | LMB select/drag | Shift+click multi");
                Terminal::instance().addLog("[MAPEDIT] Ctrl+C/V/X | Ctrl+D duplicate | Del delete | Ctrl+A select all");
                Terminal::instance().addLog("[MAPEDIT] Ctrl+Z undo | Ctrl+Shift+Z redo | Ctrl+S save");
                Terminal::instance().addLog("[MAPEDIT] V vertex snap | G grid snap | F frame | Esc back");
                Terminal::instance().addLog("[MAPEDIT] mapeditor add entity <type> [id] | add block [id] | add vehicle [template] [id] | add route [id] | add cart");
                Terminal::instance().addLog("[MAPEDIT] mapeditor spawn_vehicle [id] | vehicle_wheel <index> hub <x> <y> <z> | radius <m> | width <m>");
                Terminal::instance().addLog("[MAPEDIT] select route: route_checkpoint/final/enabled <0|1> | route_radius <m>");
                Terminal::instance().addLog("[MAPEDIT] route_bonus <sec> | route_wait <ticks> | route_move <up|down> | route_validate");
                Terminal::instance().addLog("[MAPEDIT] mapeditor selectall | select_kind route | select_route | copy | paste | cut | duplicate | delete | undo | redo");
                Terminal::instance().addLog("[MAPEDIT] mapeditor set_position <x> <y> <z> | set_size <x> <y> <z> | nudge <dx> <dy> <dz>");
                Terminal::instance().addLog("[MAPEDIT] add chunk [id] | add chunk_part [box|wall|floor] | add connector [side] | preview_connections");
                Terminal::instance().addLog("[MAPEDIT] chunks: chunk_preview <chunk> | chunk_part_material <chunk/part> <material> | chunk_part_uv <chunk/part> <u> <v>");
                Terminal::instance().addLog("[MAPEDIT] chunks: chunk_connector_field <chunk/connector> <side|role|orientation|tag> <value>");
                Terminal::instance().addLog("[MAPEDIT] properties: click an outliner row; edit X/Y/Z fields, Enter applies");
                return;
            }
            Terminal::instance().addLog("[MAPEDIT] unknown action " + command);
        },
        "2026-10-23",
        CommandCategory::Editor
    });

    terminal.registerCommand({
        "mapeditor_camspeed", "Set the map-editor camera speed multiplier",
        "mapeditor_camspeed <multiplier>",
        [](const std::vector<std::string>& args) {
            MapEditor& editor = MapEditor::instance();
            if (args.empty()) {
                Terminal::instance().addLog("[MAPEDIT] camspeed=" +
                    std::to_string(editor.cameraSpeedMultiplier()));
                return;
            }
            try {
                editor.setCameraSpeedMultiplier(std::stof(args[0]));
                Terminal::instance().addLog("[MAPEDIT] camspeed=" +
                    std::to_string(editor.cameraSpeedMultiplier()));
            } catch (const std::exception&) {
                Terminal::instance().addLog("[MAPEDIT] invalid multiplier " + args[0]);
            }
        },
        "2026-10-25",
        CommandCategory::Editor
    });

    terminal.registerCommand({
        "mapeditor_freeze", "Freeze or unfreeze the world while the map editor is active",
        "mapeditor_freeze <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog(std::string("[MAPEDIT] freeze=") +
                    (MapEditor::instance().freezeWorld() ? "1" : "0"));
                return;
            }
            MapEditor::instance().setFreezeWorld(args[0] != "0");
        },
        "2026-10-23",
        CommandCategory::Editor
    });
}
