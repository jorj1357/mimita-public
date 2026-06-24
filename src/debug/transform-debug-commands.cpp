#include <cstdio>
#include <string>
#include <vector>
#include "devtools/terminal.h"
#include "debug/transform-debug.h"

void registerTransformDebugCommands()
{
    Terminal::instance().registerCommand({
        "transform_debug", "Enable/disable transform write logging (0=off, 1=on)", "transform_debug <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                TransformDebug::instance().setEnabled(!TransformDebug::instance().isEnabled());
            } else {
                TransformDebug::instance().setEnabled(args[0] != "0");
            }
            Terminal::instance().addLog(TransformDebug::instance().isEnabled()
                ? "[OK] transform debug enabled"
                : "[OK] transform debug disabled");
        }
    }, "2026-06-13");
    Terminal::instance().registerCommand({
        "entity_debug", "Filter transform logging to a specific entity ID", "entity_debug <entityId>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                TransformDebug::instance().setTargetEntity("");
                Terminal::instance().addLog("[OK] entity debug filter cleared");
                return;
            }
            TransformDebug::instance().setTargetEntity(args[0]);
            Terminal::instance().addLog("[OK] entity debug filter set to: " + args[0]);
        }
    }, "2026-06-13");
    Terminal::instance().registerCommand({
        "transform_history", "Show recent transform writes for an entity", "transform_history <entityId>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] usage: transform_history <entityId>");
                return;
            }
            const auto* history = TransformDebug::instance().getHistory(args[0]);
            if (!history || history->empty()) {
                Terminal::instance().addLog("[ERROR] no history for: " + args[0]);
                return;
            }
            char buf[512];
            snprintf(buf, sizeof(buf), "=== TRANSFORM HISTORY for %s (%zu events) ===",
                     args[0].c_str(), history->size());
            Terminal::instance().addLog(buf);
            int i = 0;
            for (const auto& ev : *history) {
                snprintf(buf, sizeof(buf), "  %d. %s  pos=(%.1f %.1f %.1f)->(%.1f %.1f %.1f)",
                         ++i, ev.system.c_str(),
                         ev.oldPos.x, ev.oldPos.y, ev.oldPos.z,
                         ev.newPos.x, ev.newPos.y, ev.newPos.z);
                Terminal::instance().addLog(buf);
            }
        }
    }, "2026-06-13");
}
