// 2026-10-27

#include "interaction/interaction-commands.h"

#include <cstdio>
#include <string>
#include <vector>

#include "devtools/terminal.h"
#include "interaction/interaction-system.h"
#include "terminal/terminal-state.h"

void registerInteractionCommands()
{
    Terminal::instance().registerCommand({
        "interaction_list",
        "List every registered F-interaction target",
        "interaction_list",
        [](const std::vector<std::string>&) {
            const auto& targets = InteractionSystem::instance().targets();
            Terminal::instance().addLog("[INTERACTION] count=" +
                                        std::to_string(targets.size()));
            for (const InteractionTarget& target : targets)
            {
                char buffer[320];
                std::snprintf(
                    buffer, sizeof(buffer),
                    "[INTERACTION] id=%u owner=%s label=\"%s\" radius=%.2f "
                    "priority=%d enabled=%d prompt=(%.2f %.2f %.2f)",
                    target.id, target.owner.c_str(), target.label.c_str(),
                    target.radius, target.priority,
                    target.enabled ? 1 : 0, target.promptOrigin.x,
                    target.promptOrigin.y, target.promptOrigin.z);
                Terminal::instance().addLog(buffer);
    Terminal::instance().registerCommand({
        "interaction_activate",
        "Activate the currently selected interaction target (same action as F)",
        "interaction_activate",
        [](const std::vector<std::string>&) {
            const bool ok =
                InteractionSystem::instance().activateSelected(THE_CAMERA);
            Terminal::instance().addLog(
                ok ? "[INTERACTION] activated selected target"
                   : "[INTERACTION] no target in range");
        }
    });
}
        }
    });
}
