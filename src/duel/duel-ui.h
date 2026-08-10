// 08 10 2026, 14 34
/* purpose
* Renders the duels queue and in-match HUD: the wait timer, match-found banner,
* scoreboard, countdown, win/lose screen, rematch countdown, recent-duels panel,
* and the enemy-spawn tracer.
* Reads state from DuelQueue and DuelHistory; draws with the shared UI system.
* Does NOT run matchmaking, connect to servers, or mutate queue state.
* Does NOT render the PvE DuelManager HUD (that lives in game/duel.cpp).
*/

#pragma once

struct GLFWwindow;
class Camera;

void renderDuelQueueHud(GLFWwindow* win, float dt);
void renderDuelMatchHud(GLFWwindow* win, float dt);
void renderDuelTracer(const Camera& camera);
