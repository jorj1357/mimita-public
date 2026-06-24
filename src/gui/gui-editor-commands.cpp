#include <cstdio>
#include <string>
#include <vector>
#include "devtools/terminal.h"
#include "gui/gui-editor.h"
#include "gui/ui-system.h"
#include "gui/gui-layout.h"
#include "debug/debug-log.h"
#include "input/input-commands.h"

void registerGuiEditorCommands()
{
    Terminal::instance().registerCommand({
        "gui_edit", "Toggle GUI editor mode", "gui_edit [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                GuiEditor::instance().toggle();
            } else {
                GuiEditor::instance().setEnabled(args[0] == "1");
            }
            const bool on = GuiEditor::instance().isEnabled();
            printf("[GUI EDIT] %s\n", on ? "enabled" : "disabled");
            Terminal::instance().addLog(on
                ? "[GUI] Edit mode ON: buttons disabled, drag layout items only"
                : "[GUI] Edit mode OFF: buttons work normally");
        }
    });
    Terminal::instance().registerCommand({
        "gui_save", "Save all GUI layout positions to JSON files in config/gui/", "gui_save",
        [](const std::vector<std::string>&) {
            GuiLayoutManager::instance().saveAll();
            const std::vector<std::string> unsaved = GuiLayoutManager::instance().unsavedLayouts();
            if (unsaved.empty() && !GuiLayoutManager::instance().hasUnsaved()) {
                Terminal::instance().addLog("[GUI] no unsaved layouts");
            }
        }
    });
    Terminal::instance().registerCommand({
        "gui_save_menu", "Save only the current menu's layout", "gui_save_menu",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[GUI] usage: gui_save_menu <filepath>");
                Terminal::instance().addLog("[GUI] example: gui_save_menu config/gui/main-menu.json");
                return;
            }
            if (GuiLayoutManager::instance().saveLayout(args[0])) {
                Terminal::instance().addLog("[GUI] saved " + args[0]);
            } else {
                Terminal::instance().addLog("[GUI] failed to save " + args[0]);
            }
        }
    });
    Terminal::instance().registerCommand({
        "gui_load", "Reload a GUI layout JSON from disk", "gui_load <filepath>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[GUI] usage: gui_load <filepath>");
                return;
            }
            if (GuiLayoutManager::instance().reloadLayout(args[0])) {
                Terminal::instance().addLog("[GUI] reloaded " + args[0]);
            } else {
                Terminal::instance().addLog("[GUI] failed to reload " + args[0]);
            }
        }
    });
    Terminal::instance().registerCommand({
        "gui_reset", "Reset a menu to built-in defaults", "gui_reset <filepath>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[GUI] usage: gui_reset <filepath>");
                Terminal::instance().addLog("[GUI] example: gui_reset config/gui/main-menu.json");
                return;
            }
            GuiLayoutManager::instance().resetLayout(args[0]);
            Terminal::instance().addLog("[GUI] reset " + args[0] + " to defaults");
        }
    });
    Terminal::instance().registerCommand({
        "gui_reset_element", "Reset selected element to defaults (position=0, size=100x30)",
        "gui_reset_element",
        [](const std::vector<std::string>&) {
            auto& editor = GuiEditor::instance();
            if (!editor.hasSelection()) {
                Terminal::instance().addLog("[GUI] no element selected");
                return;
            }
            std::string file = editor.activeLayout();
            std::string id = editor.selectedElement();
            if (file.empty() || id.empty()) {
                Terminal::instance().addLog("[GUI] no active layout or element");
                return;
            }
            GuiLayout& layout = GuiLayoutManager::instance().getLayout(file);
            GuiElement* el = const_cast<GuiElement*>(layout.get(id));
            if (!el) {
                Terminal::instance().addLog("[GUI] element not found");
                return;
            }
            el->x = 100; el->y = 100; el->w = 100; el->h = 30;
            el->textOffsetX = 8.0f; el->textOffsetY = 4.0f;
            el->fontSize = 0.0f; el->padding = 8.0f; el->margin = 0.0f;
            el->visible = true; el->enabled = true; el->opacity = 1.0f;
            el->rotation = 0.0f; el->hoverScale = 1.0f;
            el->textColor = {1,1,1,1};
            el->backgroundColor = {0.2f,0.2f,0.3f,1.0f};
            el->hoverColor.clear(); el->pressedColor.clear(); el->outlineColor.clear();
            el->text.clear();
            layout.setElement(*el);
            Terminal::instance().addLog("[GUI] reset element \"" + id + "\" to defaults");
        }
    });
    Terminal::instance().registerCommand({
        "gui_reset_all", "Reset all menus to built-in defaults", "gui_reset_all",
        [](const std::vector<std::string>&) {
            GuiLayoutManager::instance().resetAll();
            Terminal::instance().addLog("[GUI] all layouts reset to defaults");
        }
    });
    Terminal::instance().registerCommand({
        "gui_debug_coords", "Toggle coordinate debug overlay", "gui_debug_coords [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                uiSetCoordDebug(!uiCoordDebugEnabled());
            } else {
                uiSetCoordDebug(args[0] == "1");
            }
            const bool on = uiCoordDebugEnabled();
            printf("[GUI COORD DEBUG] %s\n", on ? "enabled" : "disabled");
            Terminal::instance().addLog(on
                ? "[GUI] Coord debug ON"
                : "[GUI] Coord debug OFF");
        }
    });
    Terminal::instance().registerCommand({
        "gui_debug_overlap", "Toggle overlap debug visualization", "gui_debug_overlap [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                uiSetOverlapDebug(!uiOverlapDebugEnabled());
            } else {
                uiSetOverlapDebug(args[0] == "1");
            }
            const bool on = uiOverlapDebugEnabled();
            printf("[GUI OVERLAP DEBUG] %s\n", on ? "enabled" : "disabled");
            Terminal::instance().addLog(on
                ? "[GUI] Overlap debug ON: overlapping widgets highlighted in red"
                : "[GUI] Overlap debug OFF");
        }
    });
    Terminal::instance().registerCommand({
        "gui_debug_layout", "Show layout file paths and hot-reload status", "gui_debug_layout",
        [](const std::vector<std::string>&) {
            auto& mgr = GuiLayoutManager::instance();
            Terminal::instance().addLog("[GUI] Layout files in config/gui/:");
            for (const auto& path : {
                "config/gui/main-menu.json",
                "config/gui/play-menu.json",
                "config/gui/settings-menu.json",
                "config/gui/sandbox-map-menu.json",
                "config/gui/duel-config-menu.json",
                "config/gui/server-info-menu.json",
                "config/gui/sign-in-menu.json",
                "config/gui/help-menu.json",
                "config/gui/community-menu.json",
                "config/gui/practice-menu.json",
                "config/gui/replay-menu.json",
                "config/gui/graphics-menu.json",
                "config/gui/debug-menu.json",
                "config/gui/duel-match-end.json"
            }) {
                auto& layout = mgr.getLayout(path);
                int count = (int)layout.elementIds().size();
                Terminal::instance().addLog(std::string("  ") + path + " (" +
                    std::to_string(count) + " elements)");
            }
            Terminal::instance().addLog("[GUI] Hot reload: active (pollReload called each frame)");
            Terminal::instance().addLog("[GUI] Edit a JSON file, Ctrl+S, changes apply immediately");
        }
    });
    Terminal::instance().registerCommand({
        "input_debug", "Toggle input debug overlay", "input_debug [0|1]",
        [](const std::vector<std::string>& args) {
            auto& cmd = InputCommandSystem::instance();
            if (args.empty()) {
                cmd.setInputDebug(!cmd.inputDebug());
            } else {
                cmd.setInputDebug(args[0] == "1");
            }
            const bool on = cmd.inputDebug();
            Terminal::instance().addLog(on
                ? "[INPUT] Debug ON: showing key states and buffers"
                : "[INPUT] Debug OFF");
        }
    });
    Terminal::instance().registerCommand({
        "gui_debug", "Toggle all GUI debug overlays (bounds, anchors, guides)",
        "gui_debug [0|1]",
        [](const std::vector<std::string>& args) {
            bool on = args.empty() ? !uiDebugEnabled() : (args[0] == "1");
            uiSetDebug(on);
            uiSetOverlapDebug(on);
            uiSetCoordDebug(on);
            Terminal::instance().addLog(on
                ? "[GUI] Debug ON: showing bounds, anchors, guides"
                : "[GUI] Debug OFF");
        }
    });
}
