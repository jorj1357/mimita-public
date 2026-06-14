#include "help-menu.h"
#include "../gui-back.h"
#include "../gui-layout.h"
#include "../ui-system.h"
#include <cstdio>

HelpMenuResult drawHelpMenu(GLFWwindow* win)
{
    HelpMenuResult r{};

    int w = 0, h = 0;
    glfwGetFramebufferSize(win, &w, &h);
    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/help-menu.json");

    uiDrawRect({0, 0, (float)w, (float)h}, {0.035f, 0.04f, 0.052f, 1.0f}, "help-menu-background");

    float cx = w * 0.5f;
    float x = cx - 340.0f;
    float y = 60.0f;
    float lineH = 26.0f;

    uiDrawText("HELP", cx - 28.0f, y, 0.7f, {0.95f, 0.98f, 1.0f, 1.0f});
    y += 50.0f;

    auto line = [&](const char* text, glm::vec4 color = {0.75f, 0.85f, 1.0f, 1.0f}) {
        uiDrawText(text, x, y, 0.32f, color);
        y += lineH;
    };

    auto heading = [&](const char* text) {
        uiDrawText(text, x, y, 0.38f, {0.95f, 0.98f, 1.0f, 1.0f});
        y += lineH + 4.0f;
    };

    heading("What is Mimita?");
    line("Mimita is a fast-paced arena shooter focused on movement, flow, and style.");
    line("\"Movement is more important than aim.\"");
    y += 8.0f;

    heading("How to start playing");
    line("1. Choose a game mode from the PLAY menu.");
    line("2. Pick a map and start playing immediately.");
    line("3. No lobbies, no waiting -- just instant action.");
    y += 8.0f;

    heading("Game Modes");
    line("Casual  -- Free-roam sandbox, explore and fight NPCs.");
    line("Duels   -- 1vNPC rounds, first to kills wins the match.");
    line("Multiplayer -- Connect to servers and fight other players.");
    line("Practice -- Spawn training NPCs to test weapons and movement.");
    y += 8.0f;

    heading("Basic Controls");
    line("WASD           -- Move");
    line("Space          -- Jump (hold for air jumps)");
    line("Left Shift     -- Dash (ground + air)");
    line("E              -- Freeze (hold to slow velocity)");
    line("Q              -- Down dash (slam to ground, 1 use per touch)");
    line("Left Click     -- Shoot weapon");
    line("R              -- Reload");
    line("1 / 2 / 3      -- Equip weapon slots");
    line("` (grave)      -- Open developer console");
    y += 8.0f;

    heading("Movement Abilities");
    line("Jump: Ground jump lifts you up. Hold space in air for air jumps.");
    line("Dash: Burst of speed in your movement direction. Resets on touch.");
    line("Freeze: Hold E to freeze velocity. Slowly regain control over time.");
    line("Down dash: Q to slam downward instantly. One use per touch.");
    line("All abilities reset when you touch the ground or walls.");
    y += 8.0f;

    heading("Shooting");
    line("Revolver (slot 1): Precise single-shot hitscan. Good at range.");
    line("Shotgun (slot 3): 15-pellet fixed spread. Devastating up close.");
    line("Each weapon has its own ammo pool, reload time, and recoil pattern.");
    y += 8.0f;

    heading("Replayability & Fun");
    line("Fast respawns, generous movement, no artificial cooldowns.");
    line("Master the flow of movement + shooting. Chain dashes, walls, and jumps.");
    line("Experiment with weapons, training NPCs, and the arena.");
    y += 8.0f;

    heading("Contribute");
    line("Mimita is an open project. Contributions are welcome:");
    line("3D Art -- Maps, weapons, player models, effects");
    line("Code -- Gameplay, physics, UI, networking, tools");
    line("Ideas -- Game modes, weapons, balance feedback");
    line("Music/Sound -- Audio, effects, soundtrack");
    line("Testing -- Find bugs, report issues, suggest improvements");
    line("Visit https://mimita.fun to get involved.");
    y += 8.0f;

    if (guiBackButton(win, layout.getRect("backButton", {40.0f, 40.0f, 120.0f, 50.0f})))
        r.goBack = true;

    return r;
}
