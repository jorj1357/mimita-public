#pragma once

struct Engine;
struct Player;
struct Camera;
struct InputFrame;

void engineTickNet(Engine& engine, float dt);
void engineRenderGhost(const Player& localPlayer, const Camera& camera);

// The AI input sample applied to the local actor this fixed tick. Combat reads
// it so AI melee uses the same target-gated primary intent as ranged attacks.
const InputFrame& lastAiInputFrame();
