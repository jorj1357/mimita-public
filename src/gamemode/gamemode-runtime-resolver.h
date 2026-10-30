// 10 29 2026
/* purpose
* Single runtime owner that resolves and applies per-gamemode values to the
* feature systems. GamemodeRegistry stays the loader/validator; feature config
* singletons stay the base JSON owners. A mode never rewrites their files.
* Does NOT render, simulate, or own match lifecycle.
*/
#pragma once

struct Gamemode;

class GamemodeRuntimeResolver {
public:
    // Apply the whole client-side mode policy in one place. Call after local
    // role/weapon selection so ActorPresetWeapons is rebuilt first.
    static void applyClient(const Gamemode& mode);

    // Restore every base config value and clear all mode overrides.
    static void clearClient();
};
