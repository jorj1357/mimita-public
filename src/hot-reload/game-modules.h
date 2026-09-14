#pragma once

#include "hot-reload/game-api.h"

// Module entry points implemented inside the replaceable DLL. Each returns a
// descriptor whose name and function table become part of GameAPI::modules.
// They are only linked into the MIMITA_GAME_DLL build.
const GameModuleDescriptor* MimitaGetEffectModule();
const GameModuleDescriptor* MimitaGetActorModule();
const GameModuleDescriptor* MimitaGetPresentationModule();
const GameModuleDescriptor* MimitaGetGameplayModule();
const GameModuleDescriptor* MimitaGetEditorModule();

// Generic runtime package descriptor for this DLL generation. The kernel
// registers its systems/events/schemas/capabilities/commands generically so new
// concepts do not require a new EXE slot.
const GamePackageDescriptorV1* MimitaGetPackageDescriptor();
