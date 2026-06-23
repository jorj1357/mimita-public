#pragma once

struct Engine;
struct SimContext;
namespace MimitaNet { struct MultiplayerContext; }

void engineTick(Engine& engine, SimContext& simContext, MimitaNet::MultiplayerContext& mpContext, double& simAccumulator);
