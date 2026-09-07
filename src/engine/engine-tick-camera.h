#pragma once

struct Engine;
struct RocketLauncherState;

void engineTickCamera(Engine& engine, float dt);
const RocketLauncherState& replayRocketState();
