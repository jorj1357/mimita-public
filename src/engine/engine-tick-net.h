#pragma once

struct Engine;
struct Player;
struct Camera;

void engineTickNet(Engine& engine, float dt);
void engineRenderGhost(const Player& localPlayer, const Camera& camera);
