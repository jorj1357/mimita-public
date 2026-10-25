// 2026-10-23
#include "mapeditor/map-editor-commands.h"

#include <exception>
#include <string>
#include <vector>

#include "devtools/terminal.h"
#include "mapeditor/map-editor.h"

void registerMapEditorCommands()
{
    auto& terminal = Terminal::instance();
    terminal.registerCommand({
        "mapeditor", "Enter/leave the freecam map editor or run an editor action",
        "mapeditor [0|1|status|save|reload|freeze <0|1>|rebuild_collision|autosave|focus|add <entity <type>|block|route|cart> [id]|route_checkpoint <0|1>|route_final <0|1>|route_enabled <0|1>|route_radius <m>|route_bonus <sec>|route_wait <ticks>|route_move <up|down>|route_validate|selectall|copy|paste|cut|duplicate|delete|undo|redo|help]",
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
                    Terminal::instance().addLog("[MAPEDIT] add <entity <type>|block|route|cart> [id]");
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
                } else if (kind == "route") {
                    editor.addRoutePoint(args.size() > 2 ? args[2] : "");
                } else if (kind == "cart") {
                    editor.addCartOrigin();
                } else {
                    Terminal::instance().addLog("[MAPEDIT] unknown add kind " + kind);
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
                Terminal::instance().addLog("[MAPEDIT] mapeditor add entity <type> [id] | add block [id] | add route [id] | add cart");
                Terminal::instance().addLog("[MAPEDIT] select route: route_checkpoint/final/enabled <0|1> | route_radius <m>");
                Terminal::instance().addLog("[MAPEDIT] route_bonus <sec> | route_wait <ticks> | route_move <up|down> | route_validate");
                Terminal::instance().addLog("[MAPEDIT] mapeditor selectall | copy | paste | cut | duplicate | delete | undo | redo");
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
