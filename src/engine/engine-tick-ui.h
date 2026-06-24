#pragma once

struct Engine;

void engineTickUI(Engine& engine, float dt, bool worldPassRan);
void engineTickUIHUD(Engine& engine, float dt);
void engineTickUIReplayHUD(Engine& engine, float dt);
void engineTickUIGameHUD(Engine& engine, float dt);
void engineTickUIOverlays(Engine& engine, float dt, bool worldPassRan);
