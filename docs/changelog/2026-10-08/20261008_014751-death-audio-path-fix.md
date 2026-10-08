# Death audio path fix

time_utc=2026-10-08T01:47:51Z
display_timezone=America/New_York
display_time=2026-10-07 21:47:51 EDT
branch=working tree
commit=not committed

## Scope

Investigated why the existing `npc_death` MP3 did not play and implemented the
narrow path-resolution fix requested by the user. No gameplay position, packet,
volume, pitch, or death-event ownership changes were made.

## Pre-existing edits

The working tree already contained unrelated modifications and untracked files
before this session. They were preserved. This session changed only
`src/audio/audio.cpp` and added this changelog.

## Source change

Owner: `src/audio/audio.cpp`, `soundPath()`.

Old behavior:

```cpp
if (std::filesystem::exists(path))
    return path;
if (name == "npc_death") return "assets/sound/U mimita sound effects.wav - grunt kill madness combat.mp3";
```

New behavior:

```cpp
const auto resolveExisting = [](const std::string& candidate) {
    const std::string resolved = resolveAssetPath(candidate);
    return std::filesystem::exists(resolved) ? resolved : candidate;
};

const std::string resolvedPath = resolveAssetPath(path);
if (std::filesystem::exists(resolvedPath))
    return resolvedPath;
if (name == "npc_death") return resolveExisting("assets/sound/U mimita sound effects.wav - grunt kill madness combat.mp3");
```

Reason: the asset exists at the repository path, but the prior audio resolver
tested relative to the process current working directory. Runtime logs showed
`[SOUND] invalid path event=npc_death`. The existing `resolveAssetPath()` helper
resolves beside the executable and provides the repository's established asset
lookup behavior.

## Documents and focused review

- `docs/ROUTER.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/operations/asset-management/asset-management.md`
- `docs/architecture/live-development/hot-audio-contract.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/skills/asset-checker-v1.md`
- `docs/skills/logging-checker-v1.md`

## Validation

- Asset source confirmed present:
  `C:\mimita-v9\assets\sound\U mimita sound effects.wav - grunt kill madness combat.mp3`
- `python build_agent.py`: `BUILD SUCCESS`, compiled 1 translation unit,
  skipped 547, return code 0.
- Build output: `mimita.exe` was linked successfully.
- Runtime audible acceptance: not performed in this session.
- Runtime proof still required: launch the newly built client, cause an NPC
  death, and confirm the live log records a successful `npc_death` voice start
  and that a human can hear it.

## Regression status

No new regression record was created. This was a confirmed runtime failure
investigation and narrow fix, not a newly discovered regression.
