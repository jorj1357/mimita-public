// 09 12 2026
/* purpose
* Implements the creation-mode terminal commands. Terminal-first so the same
* data can drive a GUI later.
* Does NOT own creation state.
*/
#include "terminal/creation-commands.h"

#include "camera.h"
#include "devtools/terminal.h"
#include "editor/creation-mode.h"
#include "terminal/terminal-state.h"
#include "world/world.h"

#include <cstdlib>
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace {

Editor::WorldObjectRef gLastPicked;
Editor::WorldObjectRef gClipboard;
bool gHasClipboard = false;

void pickAndStore()
{
    Editor::WorldObjectRef ref;
    if (Editor::CreationMode::instance().pick(THE_WORLD, THE_CAMERA.pos,
                                              THE_CAMERA.front, ref)) {
        gLastPicked = ref;
        Editor::CreationMode::instance().setSelected(ref.entity);
        Terminal::instance().addLog(Editor::CreationMode::instance().describe(ref));
    } else {
        Terminal::instance().addLog("[CREATE] no object under crosshair");
    }
}

} // namespace

void registerCreationCommands()
{
    Terminal::instance().registerCommand(
        {
            "modecreate", "Toggle creation/inspection mode (0 normal, 1 create)",
            "modecreate [0|1]",
            [](const std::vector<std::string>& args) {
                const bool on = args.empty() ? !Editor::CreationMode::instance().enabled()
                                             : (args[0] != "0");
                Editor::CreationMode::instance().setEnabled(on);
                Terminal::instance().addLog(on ? "[CREATE] creation mode ON"
                                               : "[CREATE] creation mode OFF");
            },
        },
        "2026-09-12", CommandCategory::Editor);

    Terminal::instance().registerCommand(
        {
            "create_inspect", "Inspect the world object under the crosshair",
            "create_inspect",
            [](const std::vector<std::string>&) { pickAndStore(); },
        },
        "2026-09-12", CommandCategory::Editor);

    Terminal::instance().registerCommand(
        {
            "create_copy", "Copy the last inspected (or pointed) object",
            "create_copy",
            [](const std::vector<std::string>&) {
                if (gLastPicked.entity == 0)
                    pickAndStore();
                if (gLastPicked.entity == 0) {
                    Terminal::instance().addLog("[CREATE] nothing to copy");
                    return;
                }
                gClipboard = gLastPicked;
                gHasClipboard = true;
                Terminal::instance().addLog("[CREATE] copied entity " +
                    std::to_string(gClipboard.entity));
            },
        },
        "2026-09-12", CommandCategory::Editor);

    Terminal::instance().registerCommand(
        {
            "create_paste", "Duplicate the copied object (optional offset)",
            "create_paste [dx dy dz]",
            [](const std::vector<std::string>& args) {
                if (!gHasClipboard) {
                    Terminal::instance().addLog("[CREATE] clipboard empty");
                    return;
                }
                glm::vec3 offset(0.0f);
                if (args.size() >= 3)
                    offset = glm::vec3((float)std::atof(args[0].c_str()),
                                       (float)std::atof(args[1].c_str()),
                                       (float)std::atof(args[2].c_str()));
                const std::uint64_t newId =
                    Editor::CreationMode::instance().duplicate(gClipboard, offset);
                Terminal::instance().addLog("[CREATE] duplicated to entity " +
                    std::to_string(newId) + " forkHash=" +
                    Editor::CreationMode::instance().forkHash());
            },
        },
        "2026-09-12", CommandCategory::Editor);

    Terminal::instance().registerCommand(
        {
            "create_delete", "Delete the selected object from the local fork",
            "create_delete",
            [](const std::vector<std::string>&) {
                if (gLastPicked.entity == 0) {
                    Terminal::instance().addLog("[CREATE] nothing selected");
                    return;
                }
                Editor::CreationMode::instance().remove(gLastPicked.entity);
                Terminal::instance().addLog("[CREATE] deleted entity " +
                    std::to_string(gLastPicked.entity));
                gLastPicked.entity = 0;
            },
        },
        "2026-09-12", CommandCategory::Editor);

    Terminal::instance().registerCommand(
        {
            "create_move", "Move the selected object to a position",
            "create_move <x> <y> <z>",
            [](const std::vector<std::string>& args) {
                if (args.size() < 3) {
                    Terminal::instance().addLog("[CREATE] usage: create_move <x> <y> <z>");
                    return;
                }
                Editor::CreationMode::instance().transform(gLastPicked.entity,
                    glm::vec3((float)std::atof(args[0].c_str()),
                              (float)std::atof(args[1].c_str()),
                              (float)std::atof(args[2].c_str())),
                    gLastPicked.rotation, gLastPicked.scale);
                Terminal::instance().addLog("[CREATE] moved entity " +
                    std::to_string(gLastPicked.entity));
            },
        },
        "2026-09-12", CommandCategory::Editor);

    Terminal::instance().registerCommand(
        {
            "create_rotate", "Rotate the selected object",
            "create_rotate <x> <y> <z>",
            [](const std::vector<std::string>& args) {
                if (args.size() < 3) {
                    Terminal::instance().addLog("[CREATE] usage: create_rotate <x> <y> <z>");
                    return;
                }
                gLastPicked.rotation = glm::vec3((float)std::atof(args[0].c_str()),
                                                 (float)std::atof(args[1].c_str()),
                                                 (float)std::atof(args[2].c_str()));
                Editor::CreationMode::instance().transform(gLastPicked.entity,
                    gLastPicked.position, gLastPicked.rotation, gLastPicked.scale);
                Terminal::instance().addLog("[CREATE] rotated entity " +
                    std::to_string(gLastPicked.entity));
            },
        },
        "2026-09-12", CommandCategory::Editor);

    Terminal::instance().registerCommand(
        {
            "create_scale", "Scale the selected object",
            "create_scale <x> <y> <z>",
            [](const std::vector<std::string>& args) {
                if (args.size() < 3) {
                    Terminal::instance().addLog("[CREATE] usage: create_scale <x> <y> <z>");
                    return;
                }
                gLastPicked.scale = glm::vec3((float)std::atof(args[0].c_str()),
                                              (float)std::atof(args[1].c_str()),
                                              (float)std::atof(args[2].c_str()));
                Editor::CreationMode::instance().transform(gLastPicked.entity,
                    gLastPicked.position, gLastPicked.rotation, gLastPicked.scale);
                Terminal::instance().addLog("[CREATE] scaled entity " +
                    std::to_string(gLastPicked.entity));
            },
        },
        "2026-09-12", CommandCategory::Editor);

    Terminal::instance().registerCommand(
        {
            "create_patch", "Print the local authoring fork hash and ops",
            "create_patch",
            [](const std::vector<std::string>&) {
                Editor::CreationMode& mode = Editor::CreationMode::instance();
                Terminal::instance().addLog("[CREATE] baseMapHash=" + mode.baseMapHash() +
                    " forkHash=" + mode.forkHash() +
                    " ops=" + std::to_string(mode.patch().size()) +
                    " (base asset unchanged)");
            },
        },
        "2026-09-12", CommandCategory::Editor);
}
