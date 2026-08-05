// 08 05 2026, 00 00
/* purpose
* Declares the in-game leaderboard menu surface.
* Fetches ranked rows from the website API once per open and renders them with
* full VIP name styling (badge, solid, rainbow, per-letter colors).
* Provides the `leaderboard` terminal command entry point.
* DOES NOT grant entitlements, verify VIP, or write leaderboard data.
* DOES NOT own website API transport or database state.
* DOES NOT render the website React UI.
*/

#pragma once

#include <GLFW/glfw3.h>

enum class LeaderboardMenuAction
{
    None,
    GoBack
};

LeaderboardMenuAction drawLeaderboardMenu(GLFWwindow* win);
void registerLeaderboardCommands();
