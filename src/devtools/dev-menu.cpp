#include "dev-menu.h"
#include "dev-commands.h"
#include "dev-npc-selection.h"
#include "dev-teleport.h"
#include "npc/npc.h"
#include "entities/player.h"
#include "gui/gui-button.h"
#include "gui/gui-label.h"
#include "gui/gui-back.h"
#include "gui/ui-system.h"
#include "debug/debug-visuals.h"
#include <cstdio>

static bool sDevVisuals = true;
static bool sDevPhysics = true;
static bool sShowNpcDebug = true;

DevMenuResult drawDevMenu(GLFWwindow* win, NpcSystem& npcSystem, Player& player) {
    DevMenuResult r{};
    
    guiLabel("DEV TOOLS", 50, 50);
    
    float y = 100;
    const float btnW = 260;
    const float btnH = 40;
    const float gap = 10;
    
    // NPC SELECTION
    guiLabel("NPC SELECTION", 50, y);
    y += 30;
    
    if (guiButton(win, "Select All NPCs", 50, y, btnW, btnH, {0.3f, 0.7f, 0.3f, 1.0f})) {
        NpcSelectionManager::instance().selectAll(npcSystem);
    }
    y += btnH + gap;
    
    if (guiButton(win, "Clear Selection", 50, y, btnW, btnH, {0.7f, 0.3f, 0.3f, 1.0f})) {
        NpcSelectionManager::instance().clear();
    }
    y += btnH + gap;
    
    if (guiButton(win, "Select Next NPC", 50, y, btnW, btnH, {0.5f, 0.5f, 0.9f, 1.0f})) {
        NpcSelectionManager::instance().selectNext(npcSystem);
    }
    y += btnH + gap;
    
    if (guiButton(win, "Select Prev NPC", 50, y, btnW, btnH, {0.5f, 0.5f, 0.9f, 1.0f})) {
        NpcSelectionManager::instance().selectPrev(npcSystem);
    }
    y += btnH + gap + 10;
    
    char selText[64];
    snprintf(selText, sizeof(selText), "Selected: %zu", NpcSelectionManager::instance().count());
    uiDrawText(selText, 50, y, 0.35f, {1.0f, 1.0f, 0.5f, 1.0f});
    y += 25;
    
    // NPC SPAWNING
    guiLabel("NPC SPAWNING", 50, y);
    y += 30;
    
    if (guiButton(win, "Spawn NPC (Diff 1)", 50, y, btnW, btnH, {0.9f, 0.7f, 0.3f, 1.0f})) {
        npcSystem.spawnNpc(1.0f, player.pos + glm::vec3(5.0f, 0.0f, 0.0f));
    }
    y += btnH + gap;
    
    if (guiButton(win, "Spawn NPC (Diff 5)", 50, y, btnW, btnH, {0.9f, 0.5f, 0.3f, 1.0f})) {
        npcSystem.spawnNpc(5.0f, player.pos + glm::vec3(-5.0f, 0.0f, 0.0f));
    }
    y += btnH + gap;
    
    if (guiButton(win, "Spawn NPC (Diff 10)", 50, y, btnW, btnH, {0.9f, 0.3f, 0.3f, 1.0f})) {
        npcSystem.spawnNpc(10.0f, player.pos + glm::vec3(0.0f, 5.0f, 0.0f));
    }
    y += btnH + gap + 10;
    
    // TELEPORT
    guiLabel("TELEPORT", 50, y);
    y += 30;
    
    auto& selection = NpcSelectionManager::instance();
    bool hasSelection = selection.count() > 0;
    glm::vec4 tpColor = hasSelection ? glm::vec4{0.3f, 0.5f, 0.9f, 1.0f} : glm::vec4{0.3f, 0.3f, 0.5f, 0.5f};
    
    if (guiButton(win, "TP Selected -> Me", 50, y, btnW, btnH, tpColor)) {
        if (hasSelection) {
            auto selected = selection.getSelected(npcSystem);
            TeleportSelectedToTarget(selected, player.pos);
        }
    }
    y += btnH + gap;
    
    if (guiButton(win, "TP Selected -> (0,0,50)", 50, y, btnW, btnH, tpColor)) {
        if (hasSelection) {
            auto selected = selection.getSelected(npcSystem);
            TeleportSelectedToCoords(selected, {0.0f, 0.0f, 50.0f});
        }
    }
    y += btnH + gap;
    
    if (guiButton(win, "TP Selected -> Spawn 0", 50, y, btnW, btnH, tpColor)) {
        if (hasSelection) {
            auto selected = selection.getSelected(npcSystem);
            TeleportSelectedToSpawnPoint(selected, 0);
        }
    }
    y += btnH + gap;
    
    if (guiButton(win, "TP Selected -> Spawn 1", 50, y, btnW, btnH, tpColor)) {
        if (hasSelection) {
            auto selected = selection.getSelected(npcSystem);
            TeleportSelectedToSpawnPoint(selected, 1);
        }
    }
    y += btnH + gap + 10;
    
    // DEBUG TOGGLES
    guiLabel("DEBUG TOGGLES", 50, y);
    y += 30;
    
    if (uiCheckbox(win, "Debug Visuals (F1-F11)", {50, y, 30, 30}, &sDevVisuals)) {
        DebugVis::init(win); // Re-init toggles
    }
    uiDrawText("Debug Visuals", 90, y + 6, 0.35f, {1,1,1,1});
    y += 40;
    
    if (uiCheckbox(win, "Show NPC Debug", {50, y, 30, 30}, &sShowNpcDebug)) {
        // Handled in main.cpp render
    }
    uiDrawText("Show NPC Debug", 90, y + 6, 0.35f, {1,1,1,1});
    y += 40;
    
    if (uiCheckbox(win, "Debug Physics", {50, y, 30, 30}, &sDevPhysics)) {
        // DebugVis doesn't have physics toggle exposed, would need to add
    }
    uiDrawText("Debug Physics", 90, y + 6, 0.35f, {1,1,1,1});
    y += 50;
    
    // COMMAND REGISTRY
    guiLabel("REGISTERED COMMANDS", 50, y);
    y += 30;
    
    std::vector<const DevCommand*> commands;
    DevCommandRegistry::instance().listAll(commands);
    
    for (const DevCommand* cmd : commands) {
        char cmdText[128];
        snprintf(cmdText, sizeof(cmdText), "%s", cmd->name.c_str());
        if (guiButton(win, cmdText, 50, y, btnW, 28, {0.4f, 0.6f, 0.4f, 1.0f})) {
            DevCommandRegistry::instance().execute(cmd->name, {});
        }
        y += 32;
    }
    
    if (guiBackButton(win)) {
        r.goBack = true;
    }
    
    return r;
}
