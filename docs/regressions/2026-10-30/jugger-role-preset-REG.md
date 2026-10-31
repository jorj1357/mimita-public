# Juggernaut Fighter Role Preset Regression

Time created: 2026-10-30T13:29:27Z
Time last updated: 2026-10-30T13:29:27Z

Status: ATTEMPTED FIX (1)

Related specification: `docs/specs/actor-presets/actor-presets.md`, `docs/specs/gamemodes/gamemodes.md`
Related issue: `docs/specs/open-gameplay-issues.md`
Related changelog: `docs/changelog/2026-10-30/20261030_132927-jugger-role-aim-fix.md`

## Regression Occurrence 1

### Observed

Time: 2026-10-30 (user report; exact time not supplied)

### Expected Behavior

The local Fighter in Juggernaut Horde uses the Fighter role's fast movement and 100-degree FOV. Juggernaut-style Revolver RMB aiming follows the same default arm-aim path as Sandbox while physical firing continues to follow the barrel.

### Actual Behavior

The user reported remaining at 50 FOV and Juggernaut movement as a Fighter, and reported Revolver aiming failed only in Juggernaut-style modes.

### Specification

`docs/specs/gamemodes/juggernaut.md` assigns Fighters `retrograd_fast` and 100-degree forced FOV, and records the Juggernaut-style physical aim policy. The prior aim-body value `hybrid` differed from Sandbox's inherited `default` path and was associated with the reported backward Revolver arm pose.

### Wrong Code

`src/network/server-gamemode.cpp` limited participant identities to the first 64 entries and broadcast the same packet to every client. In a 68-actor Horde roster, a client could receive no identity for itself.

`src/network/community-match-client.cpp` contained a local role-preset fallback guarded by `mMode == "juggernaut"`, so it did not run for `jugger_horde`. The replicated role and actor preset can also share the same ID; the registry lookup then returns the actor-preset entry, whose `actorPresetId` is empty.

`config/actor-presets/juggernaut_fighter.json` had `force_fov: false`, allowing the shared mode packet to leave the Fighter at 50 FOV.

Both Juggernaut-style mode JSON files selected `aim.body_mode: "hybrid"`, unlike Sandbox's `default` body-aim mode.

### Confirmed Cause

The live server roster contained the human Fighter and 67 NPCs, while the replicated identity array is capped at 64. The client did not receive the local role in the first attempt. After guaranteeing the recipient's identity, the client resolved the Fighter role but initially did not resolve its preset because role and preset IDs are shared. This was visible in the bounded `client.gamemode.local-actor-identity` event before and after the fixes.

### Attempted Fix 1

Time: 2026-10-30T13:29:27Z

Changed the server to include each recipient's local actor identity in its own state packet, replacing the last roster identity only when necessary. The client now resolves a preset from the replicated role generically, handles a role that is itself an actor preset, and does not reapply the shared mode preset over an already-resolved local role. The Fighter preset now forces 100 FOV. Juggernaut-style modes now select the same default body-aim mode as Sandbox while retaining physical weapon firing.

Proof: Build 3858 succeeded through the dev loop. In live run `20261030_132821`, client events reported local identity found with role `juggernaut_fighter`, team 0, preset `juggernaut_fighter`; the role-preset event reported forced FOV 100 and movement preset `retrograd_fast`. That preset event occurred once. `client.gamemode.presentation.applied` reported effective aim mode `physical` and body mode `default`. The dev loop reported matching client/server build identities.

Human visual/gameplay review of actual movement, FOV, and Revolver RMB arm orientation is still required. The attempted fix is not a confirmed solution until the user observes the behavior working.
