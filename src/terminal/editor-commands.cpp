#include <cstdio>
#include <string>
#include <vector>
#include "devtools/terminal.h"
#include "terminal/terminal-state.h"

void registerEditorCommands()
{
    Terminal::instance().registerCommand({
        "editormode", "Enter map editor mode", "editormode",
        [](const std::vector<std::string>&) {
            EDITOR_MODE = true;
            GAME_STATE = GAME_PLAYING;
            Terminal::instance().addLog("[EDITOR] editor mode enabled");
        }
    });
    Terminal::instance().registerCommand({
        "gamemode", "Return to play mode or select sandbox/tdm", "gamemode [sandbox|tdm]",
        [](const std::vector<std::string>& args) {
            EDITOR_MODE = false;
            if (!args.empty()) {
                if (args[0] != "sandbox" && args[0] != "tdm") {
                    Terminal::instance().addLog("[ERROR] gamemode must be sandbox or tdm");
                    return;
                }
                ACTIVE_GAME_MODE = args[0];
            }
            Terminal::instance().addLog("[GAME MODE] " + ACTIVE_GAME_MODE);
        }
    });
    Terminal::instance().registerCommand({
        "selectobject", "Select an editor block/triangle id", "selectobject <id>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: selectobject <id>");
                return;
            }
            SELECTED_EDITOR_OBJ = std::stoi(args[0]);
            size_t count = THE_WORLD.blocks.empty() ? THE_WORLD.collisionMesh.triangles.size() : THE_WORLD.blocks.size();
            if (SELECTED_EDITOR_OBJ < 0 || SELECTED_EDITOR_OBJ >= (int)count) {
                SELECTED_EDITOR_OBJ = -1;
                Terminal::instance().addLog("[ERROR] object id out of range");
                return;
            }
            Terminal::instance().addLog("[EDITOR] selected object id " + std::to_string(SELECTED_EDITOR_OBJ));
        }
    });
    Terminal::instance().registerCommand({
        "assignmaterial", "Assign a material name to a selected block", "assignmaterial <name>",
        [](const std::vector<std::string>& args) {
            if (SELECTED_EDITOR_OBJ < 0 || args.empty()) {
                Terminal::instance().addLog("[ERROR] select a block and provide a material name");
                return;
            }
            if (SELECTED_EDITOR_OBJ >= (int)THE_WORLD.blocks.size()) {
                Terminal::instance().addLog("[EDITOR] GLB triangle material assignment is a placeholder");
                return;
            }
            THE_WORLD.blocks[SELECTED_EDITOR_OBJ].texName = args[0];
            Terminal::instance().addLog("[EDITOR] material assigned: " + args[0]);
        }
    });
    Terminal::instance().registerCommand({
        "savemap", "Placeholder editor save", "savemap [name]",
        [](const std::vector<std::string>& args) {
            Terminal::instance().addLog("[EDITOR] would save map " + (args.empty() ? std::string("untitled") : args[0]));
        }
    });
}
