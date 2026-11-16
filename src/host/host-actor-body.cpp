// Resident fallback for the shared actor-body constructor.
//
// The host server entity adapter default-constructs ServerNpc/ServerPlayer
// containers. The game generation owns the real ActorBodyState implementation;
// this weak definition keeps the strict cold EXE linkable and is overridden by
// the game-owned strong symbol in the migration EXE.

#include "entities/actor-body.h"

#include <algorithm>

__attribute__((weak)) void ActorBodyState::reset() {
    partHealth.fill(100);
}
