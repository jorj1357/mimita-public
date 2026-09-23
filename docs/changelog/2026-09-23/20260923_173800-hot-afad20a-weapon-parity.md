# Hot afad20a weapon parity — presentation and shared-route slice

UTC: 2026-09-23T17:38:00Z  
Display timezone: America/New_York  
Branch: `8292026stash`

## Result

`PASS_WITH_HUMAN_REVIEW` for the hot weapon presentation and route-diagnostics
slice. Server lifetime and NPC avatar assignment were not changed.

## Changes made

- Added per-axis view/world scale to the shared `ToolVisualRecipeV1` result.
- Matched the hot C++ recipes for revolver, shotgun, rocket launcher, grenade
  launcher, swordsword, spyknife, and HAFS to the existing afad20a-era JSON
  viewmodel and right-arm attachment values.
- Added hot JSON presentation loading for model path, view position/rotation,
  non-uniform scale, and attachment position/rotation. Invalid JSON keeps the
  previous valid presentation recipe.
- Changed hot attachment recentering to use the old longest-axis grip endpoint
  and the opposite endpoint for muzzle placement. Rendering, muzzle traces, and
  the existing collision consumer now receive the same resolved attachment
  transform.
- Added the selected `cpp`/`json` weapon policy source to hot tool route
  diagnostics.

## Validation

- Hot DLL generation 28 built successfully:
  `build/hotreload/mimita-live-g000028.dll`.
- `git diff --check` completed without whitespace errors; only existing line
  ending normalization warnings were reported.
- The live build path did not write or replace `mimita.exe`.
- No cold executable build, live DLL activation, or in-game visual/gameplay
  acceptance was performed in this session.

## Human review still required

- Activate the candidate in the already running client.
- Equip each supported weapon and verify texture, right-arm alignment, pose,
  muzzle, collision visualization, firing, reload, melee, and rocket splash.
- Switch `config/weapons.json` between `behaviorSource: "cpp"` and
  `behaviorSource: "json"` while the session remains alive and confirm the
  same fixed-tick result.
