# Development loop and ccache integration

## Final state

Implemented the repository-side development-loop MVP on branch `afad20a-rebuild`.
No gameplay ownership, NPC implementation, launcher production flow, or existing
hot-reload consumer was rewritten.

## Changed files

- `.gitignore`: ignores generated `.dev/` build publications and state.
- `build.py`: treats `src/pch.h.gch` as an explicit shared dependency so a
  changed precompiled header invalidates affected object files.
- `build_toolchain.py`: resolves the current shared MinGW and GLFW installs at
  `C:\important` after explicit environment variables and repo-local tools.
- `devscripts/dev-loop.py`: adds one external watcher/build queue, debouncing,
  source-generation tracking, numbered build publication, stale-build state,
  daemon-owned server/client restart, AUTO-RESTART, and one-key controls.
  The profile now follows the GUI room-file handshake and launches the client
  through the room-code/ICE path instead of localhost direct UDP. Idle status
  output is transition-driven rather than redrawn every poll. A successful
  non-stale publication now always performs the equivalent of pressing `1`,
  even when the AUTO-RESTART toggle is OFF.
- `devscripts/dev-profiles/npc-navigation.json`: adds the first NPC
  development scenario using the existing map, server, mode, gamemode,
  weapon-set, NPC-count, notification, and coordinator room-code options.
- `src/network/net_mode.h`, `src/network/net_mode.cpp`, and `src/main.cpp`:
  add the explicit `--room <code>` client launch option. It populates
  `MultiplayerConnectInfo.roomCode` without populating `directAddress`, which
  selects the same asynchronous ICE join owner used by the GUI.

## Build/runtime design

The daemon invokes the existing `build.py build-only` path instead of
`build_agent.py`. This preserves existing dependency tracking, PCH use, and
parallel compilation while avoiding `build_agent.py`'s image-wide
`taskkill /im mimita.exe` behavior. Successful root builds are copied to
`.dev/builds/<number>/`; runtime DLLs are copied beside each published EXE.
The working directory remains the repository root so existing relative
configuration and asset paths continue to work.

The daemon automatically passes `MIMITA_CCACHE` when ccache is discoverable.
ccache was installed by the human through Chocolatey and detected as
`C:\ProgramData\chocolatey\bin\ccache.EXE`.

NPC behavior selection remains owned by the existing NPC difficulty and
behavior-profile configuration. The new profile does not claim a behavior CLI
feature that does not yet exist.

## Validation

- `python -m py_compile devscripts/dev-loop.py build.py`: passed.
- `python -m py_compile build_toolchain.py devscripts/dev-loop.py`: passed.
- `python devscripts/dev-loop.py --help`: passed.
- Toolchain resolution found MinGW GCC, ccache 4.14, and GLFW from the current
  `C:\important` installation.
- `python build.py build-only`: passed with `Nothing changed` in 0.51 seconds.
- `python build.py build-only` after the room-code change: passed; the changed
  launch and connection files compiled and `mimita.exe` linked successfully.
- The dev loop published `.dev/builds/0003` and launched two daemon-owned MiMITA
  processes from that versioned executable; both were then cleanly stopped.
- A controlled loop start from `C:\mimita-v9` published `.dev/builds/0005`; its
  unchanged status menu printed once. The old direct-launch process was stopped
  before applying the room-code correction.
- Imported the dev loop and loaded `npc-navigation.json`: passed.
- Snapshot discovery found 874 watched inputs.
- `git diff --check`: no changed-line whitespace errors; repository emitted
  pre-existing line-ending warnings.
- No source-changing C++ compilation was required in this run.
- The corrected room-code server registration and ICE client join still need
  human runtime confirmation. The reported server-position error must be
  re-tested only after that path is active.

## Focused documents and skills

- `docs/ROUTER.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/specs/performance/performance.md`
- `docs/skills/efficiency-checker-v1.md`
- `docs/skills/terminal-command-checker-v1.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/regressions/regressions-v1.md`

## Pre-existing work preserved

The worktree already contained unrelated modifications in combat, effects,
configuration, hot reload, networking, and multiple changelog files. Those
changes were preserved and not attributed to this session.

## Human review still needed

Run the daemon with `python devscripts/dev-loop.py --profile npc-navigation`.
Human review must confirm compiler cache hits, rapid-save queue behavior,
versioned launch, server/client shutdown, direct NPC scenario recreation, and
AUTO-RESTART.
