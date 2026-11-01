# Avatar outlines missing on fallback and local NPC bodies

Time created: 2026-11-01T03:22:00Z
Time last updated: 2026-11-01T03:22:00Z

Status: ATTEMPTED FIX (1)

Related specification:
`config/playervisuals.json`,
`docs/specs/gui/gui.md`

Related changelog:
`docs/changelog/2026-11-01/20261101_015253-fun-counterstrike-hud-deathball-damage.md`

---

## Regression Occurrence 1

### Observed

Time: human report 2026-10-31

The player enabled enemy and friendly outlines, but the girl body and the normal
zombie body still rendered without any outline.

### Expected Behavior

Every player and NPC avatar that is allowed by the active mode's outline policy
must draw the configured team outline, including avatars whose geometry comes
from the single-mesh fallback and NPCs rendered through the local NPC system.

### Actual Behavior

- Local NPCs called `renderPlayer(npc.body, camera)`, which classifies them as
  the local player and selects the disabled `self` outline.
- Avatars without physical body parts took the single-mesh `renderMesh` path,
  which ignored `outlinePass`, the outline thickness, and the outline color.
- The outline bounds guard computed bounds only from physical body parts, so
  fallback avatars were rejected as having no valid bounds even if the outline
  pass had run.
- Actors with unknown team identity lost the outline entirely when a mode had an
  outline override.

### Why This Is Bad

The silhouette is the main team-readability cue. Some avatars being invisible
under the outline layer breaks the visual contract and makes player/NPC
identification inconsistent across avatar types.

### Wrong Code

File:

`src/npc/npc.cpp`

```cpp
void NpcSystem::render(const Camera& camera) const
{
    for (const Npc& npc : npcs)
        renderPlayer(npc.body, camera);
}
```

File:

`src/entities/player-render.cpp`

The fallback `renderMesh` and capsule paths did not set `uOutlinePass`,
`uOutlineThickness`, or the outline color.

### Confirmed Cause

`renderPlayer(...)` always passes `isLocal=true`. The fallback mesh/capsule
paths ignored outline state. `computeActorVisualBounds` returned invalid bounds
when `physicalBody.parts` was empty. Unknown-team actors failed the
`modeForcesTeamOutline` check.

### Attempted Fix 1

Time: `2026-11-01T03:18:36Z`

Change:

- Local NPCs render through `renderNetworkPlayer` with `isLocal=false`, the
  local team, and the NPC team.
- The engine computes and passes the local outline team to `NpcSystem::render`.
- Fallback `renderMesh` and capsule paths honor the outline pass and color.
- `computeActorVisualBounds` derives bounds from fallback mesh batches.
- Unknown-team actors use the configured enemy outline when the mode allows
  player outlines.
- Enemy and teammate outlines are enabled at thickness `4.0`, alpha `1.0` in
  `config/playervisuals.json`.

### Corrected Code

File:

`src/npc/npc.cpp`

```cpp
void NpcSystem::render(const Camera& camera, int localTeam) const
{
    for (const Npc& npc : npcs)
        renderNetworkPlayer(npc.body, camera, npc.id, false,
                            localTeam, npc.body.matchTeam);
}
```

File:

`src/entities/player-render.cpp`

```cpp
if (meshOutlinePassLoc >= 0) glUniform1i(meshOutlinePassLoc, outlinePass ? 1 : 0);
if (meshOutlineThicknessLoc >= 0)
    glUniform1f(meshOutlineThicknessLoc, outlinePass ? outlineThickness : 0.0f);
glUniform1i(meshUseColorLoc, (whiteOverride || outlinePass) ? 1 : 0);
if (outlinePass)
    glUniform4f(meshColorLoc, outlineColor.r, outlineColor.g,
                outlineColor.b, outlineColor.a);
```

File:

`src/render/render-player.cpp`

```cpp
const bool configuredTeamOutline = !isLocal && outline->enabled &&
    (!teammate || visualsConfig.friendlyOutlinesEnabled()) &&
    (!visualsConfig.hasPlayerOutlinesOverride() || visualsConfig.playerOutlinesEnabled());
const bool outlineAllowed = visualLayersAllowed || modeForcesTeamOutline ||
    configuredTeamOutline || ...;
```

### Proof

Runtime journal `20261101_031926`, build 34:

- `player_visuals.outline` recorded `drawn:true`, `thickness:4.0`,
  `world_thickness:0.06`, `reason:"drawn:4.000000"`;
- teammates and enemies both drew, with `alpha:1.0`;
- the stale run before the config change logged `thickness:1.0`, proving the new
  value is active.

Build 34 succeeded, and `--canonical-contact-selftest`,
`--objective-selftest`, and `--map-config-selftest` pass on the build family.

### Human Review Still Required

Visually confirm that the girl body and normal zombie body now show the outline,
that the thickness is acceptable, and that unknown-team monsters use the
intended color.
