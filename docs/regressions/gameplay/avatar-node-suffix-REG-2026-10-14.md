# Avatar Node-Name Suffix Detection Regression

Time created: 2026-10-14T01:03:37Z
Time last updated: 2026-10-14T01:03:37Z

Status: ATTEMPTED FIX (1)

Related specification:
`docs/specs/debug-logging/AI-fix-live.md`,
`docs/gold/2026-10-06-avatar-bind-anchors-and-jsonl-evidence.md`

Related changelog:
`docs/changelog/2026-10-14/20261014_003000-avatar-limb-render-origin.md`

---

## Regression Occurrence 1

### Observed

Time: `2026-10-14T00:52Z`

A custom avatar GLB whose body-part nodes were named with Blender duplicate
suffixes — `plrOrigin.091`, `head.001`, `torso.184`, `leftArm.002`,
`rightArm.003`, `leftLeg.004`, `rightLeg.005` — did not work in the engine. It
worked only when the nodes were named exactly `plrOrigin`, `head`, `torso`,
`leftArm`, `rightArm`, `leftLeg`, `rightLeg`.

### Expected Behavior

The loader identifies body parts by their base name regardless of the Blender
`.NNN` duplicate suffix. A model exported with suffixed node names loads,
canonicalizes, and animates the same as one with exact names.

### Actual Behavior

`isPlayerBodyPart` used exact string equality, so every suffixed node failed the
match. `normalizePlayerModelTransforms` then saw `partIndices.size() != 6` and
rejected canonicalization (event
`avatar.model.normalized-node` with `reason=root_or_required_parts`), and the
part identity names downstream (`physicalBody.parts`, `partIdFromName`, config
lookups, weapons, ragdoll) never matched.

### Why This Is Bad

A valid Blender export is unusable unless the author manually renames every
node, which is easy to miss and silently breaks the avatar.

### Specification

`docs/gold/2026-10-06-avatar-bind-anchors-and-jsonl-evidence.md`

Relevant requirement: the loader derives body-part placement from the avatar's
actual bind pose and the six named body parts must be recognized for hybrid,
ragdoll, and hitbox paths.

### Wrong Code

File:

`src/entities/player-loader.cpp`

```cpp
bool isPlayerBodyPart(const std::string& name)
{
    return name == "head" || name == "torso" ||
           name == "leftArm" || name == "rightArm" ||
            name == "leftLeg" || name == "rightLeg";
}
```

`src/network/server-body-template.cpp` had the same exact-match `isBodyPartName`.

### Confirmed Cause

Blender appends `.<digits>` to duplicate datablock names. The engine compared
raw node names to the six base names, so any suffixed name was treated as an
unknown node.

### Corrected Code

File:

`src/entities/player-loader.cpp` (declared in `src/entities/player.h`)

```cpp
std::string playerBodyPartBaseName(const std::string& name)
{
    const size_t dot = name.rfind('.');
    if (dot == std::string::npos || dot + 1 >= name.size())
        return name;
    for (size_t i = dot + 1; i < name.size(); ++i)
        if (name[i] < '0' || name[i] > '9')
            return name;
    return name.substr(0, dot);
}

bool isPlayerBodyPart(const std::string& name)
{
    const std::string base = playerBodyPartBaseName(name);
    return base == "head" || base == "torso" ||
           base == "leftArm" || base == "rightArm" ||
           base == "leftLeg" || base == "rightLeg";
}
```

The base name is used for body-part detection, `partIndices`, stored
`BodyPart`/`PhysicalBodyPart`/`Collider` names, bodyparts.json override lookups,
the editor skeleton override match (`avatar.cpp`), and the server-side
`isBodyPartName` / body-part classification (`server-body-template.cpp`).

### Fix

All body-part name matching and storage strips a trailing `.<digits>` suffix,
so suffixed exports are treated identically to exact-named exports.

### Proof

Human review:

Not yet performed.

Automated proof:

- Build `2268`, run `C:\mimita-v9\logs\2026-10-14\20261014_010035\events.jsonl`.
- A suffixed copy of `mimita-stabber-v7.glb` (`plrOrigin.091`, `head.001`,
  `torso.184`, `leftArm.002`, `rightArm.003`, `leftLeg.004`, `rightLeg.005`)
  loaded as a temporary avatar.
- `avatar.model.normalized-node`: six `normalized` records plus `success`
  with `root_index=6` (previously rejected).
- `avatar.model.transform-source`: `body_frame_policy=player_canonical`,
  `source=model`.
- `AIMBODY_AVATAR_BIND_PART` `bind_node_pivot_error ~1e-4` for all six parts.
- After cleanup, `monsterstabber` (exact names) still normalizes and binds with
  `~1e-4` error.

### Solution

`ATTEMPTED FIX (1)` — runtime proof confirms suffixed names are recognized;
awaiting human confirmation with the real avatar.
