# Agent Lane Integration and Pending Validation

Status: source integration is written; compile, runtime validation, and stable
promotion are pending.

This document is the obvious starting point for:

- finishing the world-streaming/navmesh/reconnect work;
- compiling and testing it through the lane dev-loop;
- merging all reviewed agent lanes into one EXE.

Read `docs/specs/dev-loop/agent-lanes-build-authority.md` and
`docs/specs/dev-loop/dev-loop.md` before running any command. The build
authority rules still apply.

## Short version

1. Commit each reviewed lane change. `promote` accepts commits, not dirty
   worktrees.
2. Create one integration worktree with
   `python devscripts/agent-lane.py promote`.
3. Run that integration lane through `ensure`/`run`; its dev-loop builds the
   one EXE. Do not use `agent-lane.py build`, `build.py`, or a compiler.
4. Use the integration EXE for the test matrix below.
5. Publish a stable artifact only after a clean build and runtime acceptance.

## Current work

Integration lane:

```text
lane:     world-streaming-nav-reconnect-20261108
worktree: C:\mimita-v9-agent-lanes\world-streaming-nav-reconnect-20261108
branch:   codex/agent-world-streaming-nav-reconnect-20261108
base:     dc7dea21889f84d6a7d7ad1a05d8563c63a39825
```

Preserved predecessor lane:

```text
lane:     npc-collision-hot-20261108
worktree: C:\mimita-v9-agent-lanes\npc-collision-hot-20261108
```

The current lane is dirty and has not been compiled in this session. No
successful build ID, compile result, runtime journal, or human acceptance is
claimed.

Implemented source changes are listed in the integration lane's changelog:
`C:\mimita-v9-agent-lanes\world-streaming-nav-reconnect-20261108\docs\changelog\2026-11-08\20261108_223500-world-streaming-nav-reconnect-integration.md`.
Read that changelog before changing the lane.

## Pending code work

Complete these before calling the feature finished:

1. **Verify tiled navmesh connectivity.**
   `src/npc/recast-navigation.cpp` now builds 32 m Detour tiles and rebuilds
   only dirty tiles. Confirm paths cross tile boundaries and that a dirty tile
   rebuild does not disconnect neighbouring tiles. If links are missing, add
   the smallest tile-boundary fix; do not fall back to a full-world rebuild.
2. **Enforce the server phase budget.**
   `performance.server-phase-budget` currently reports over-budget server
   phases. Bind projectile/contact work to the same bounded budget so a busy
   chunk cannot stall the fixed 60 Hz tick. Keep diagnostics categorized and
   rate-limited.
3. **Finish combat diagnostics.**
   Add bounded owner-level evidence for equip grace and damage mismatch. These
   are diagnostics until runtime evidence proves a behavior change is needed.
4. **Add a combined streaming + NPC test mode if needed.**
   Modes `52`, `54`, and `55` are Connected World streaming modes with no NPCs.
   Mode `57` has 19 NPCs on Payload. There is currently no Connected World +
   streaming + NPC launch mode, so cross-tile NPC streaming cannot be validated
   end-to-end with one built-in mode.

Do not add a new test harness for these. Use the lane dev-loop and its
`events.jsonl` journal.

## Compile and build

### Rules

- Agents never build directly. Do not run `build.py`, `buildv2.py`,
  `build_agent.py`, `module-build-worker.py`, `ccache`, a compiler, or
  `dev-loop.py --request build`.
- `python devscripts/agent-lane.py build <lane>` is deprecated and refuses to
  run. Do not use it.
- The lane dev-loop is the builder. It auto-detects saved edits.
- Only a human may use `MIMITA_BUILD_OVERRIDE=1`.

### Sequence

From the repository root:

```powershell
python devscripts/agent-lane.py ensure world-streaming-nav-reconnect-20261108
python devscripts/agent-lane.py status world-streaming-nav-reconnect-20261108
```

Wait for the lane dev-loop to report a successful build of the current source
revision. Then inspect the lane's build result:

```text
<lane>/.dev/state.json
<lane>/build/build-result.json
<lane>/build/changelog.txt
```

A successful compile is source/build evidence only. It does not prove that
configuration loaded, packets arrived, or behavior changed.

If this document is being followed after the current no-build directive is
lifted, compile failures are fixed here, not in a separate checkout.

## Runtime test matrix

Run each test through the lane dev-loop. Read the lane's `events.jsonl`; do not
substitute a synthetic self-test for live evidence.

```powershell
python devscripts/agent-lane.py run world-streaming-nav-reconnect-20261108 --launch-mode 56
python devscripts/agent-lane.py run world-streaming-nav-reconnect-20261108 --launch-mode 57
python devscripts/agent-lane.py run world-streaming-nav-reconnect-20261108 --launch-mode 52
python devscripts/agent-lane.py run world-streaming-nav-reconnect-20261108 --launch-mode 54
python devscripts/agent-lane.py run world-streaming-nav-reconnect-20261108 --launch-mode 55
```

| Test | Mode | Required evidence |
|---|---|---|
| Payload spawn and hit authority | 56 | Client reaches playing state; no reconnect loop; accepted combat logs show matching weapon/ammo/cooldown; server remains near 60 Hz. |
| Payload NPC navigation | 57 | 19 NPCs; `npc.navmesh.bake-success` without multi-second rebuild; `npc.nav.result` query time bounded; NPCs move and fight; no `npc.slow-update` stall. |
| Connected World streaming | 52 | `world-stream.anchor`/`update`/`chunk.applied` for `zone=infinite_zone`, `whole_world=false`; collision sync stays under budget; no per-chunk base broadphase rebuild. |
| Streaming performance | 54, 55 | 60 Hz server, 240 FPS render goal measured on the same machine; no reconnect; no frame starvation from chunk collision. |
| Tiled navmesh boundary | current streaming mode plus NPCs | NPC routes cross at least two tiles; dirty rebuild leaves neighbouring paths connected. Add a launch mode if no built-in mode provides this. |
| Reconnect/session health | 56 and 52 | Duplicate `welcome`/`join_accept` does not reset map-ready for the same player/map/epoch; `network.map-sync.state` shows progress; ICE failure reaches a bounded timeout instead of an endless “synchronizing map”. |
| Session/combat policy | 56 | `network-policy` events `706`/`707`/`708` fire with expected decisions; no unexplained cooldown reject. |

Pass criteria must come from the journals and observed behavior. Report source,
build, runtime, and human acceptance separately.

## Merge all reviewed lanes into one EXE

### 1. Freeze and commit

Stop editing every lane that will be merged. Each lane must be committed:

```powershell
git -C C:\mimita-v9-agent-lanes\<lane> status --short
git -C C:\mimita-v9-agent-lanes\<lane> add <intended files>
git -C C:\mimita-v9-agent-lanes\<lane> commit -m "<reviewed change>"
```

Record the exact commit SHA for each lane. Do not include uncommitted edits,
build outputs, journals, secrets, or unrelated files. `promote` rejects dirty
integration content later.

Current known lanes include:

```text
build-boundary-20261108
grenade-collision-20261105
network-session-diagnostics-20261105
npc-collision-hot-20261108
vehicle-arena-20261105
world-projects-20261105
world-streaming-nav-reconnect-20261108
```

Only merge lanes whose commits have been reviewed. Dependency order matters:
shared ABI/config/module changes first, then feature lanes.

### 2. Create the integration worktree

Choose a clean base commit that already contains the shared prerequisites.
Then pass every reviewed commit in order:

```powershell
python devscripts/agent-lane.py promote integrate-all-20261108 `
  --base <clean-base-sha> `
  --commit <lane-1-sha> `
  --commit <lane-2-sha> `
  --commit <lane-3-sha>
```

`promote` creates
`C:\mimita-v9-agent-lanes\integrate-all-20261108` and cherry-picks the commits.
If a cherry-pick conflicts, resolve it in that integration worktree and commit
the resolution. Do not force a merge in a feature lane.

Do **not** pass `--build`. It calls the deprecated build path and will refuse.
The integration lane dev-loop builds the EXE in the next step.

### 3. Resolve integration-owned conflicts

Check these before building:

- `config/modules/modules.json`: merge event IDs as a union. Never assign the
  same event ID to two owners. The current lane owns `706`, `707`, `708`, and
  `800`.
- Module ABI and API headers: one combined ABI, not per-lane copies.
- `config/gamemodes/masterlist/gamemode-masterlist.json` and gamemode JSONs:
  re-run the gamemode JSON sync/check after merging.
- `docs/ROUTER.md`: keep one route per topic.
- Shared collision, tick, network, and module-host files: preserve the latest
  owner-level design and delete duplicate paths rather than keeping both.

Review the combined diff before building:

```powershell
git -C C:\mimita-v9-agent-lanes\integrate-all-20261108 status --short
git -C C:\mimita-v9-agent-lanes\integrate-all-20261108 diff --stat
```

The integration worktree must be clean before stable promotion.

### 4. Build and test the one EXE

```powershell
python devscripts/agent-lane.py ensure integrate-all-20261108
python devscripts/agent-lane.py status integrate-all-20261108
python devscripts/agent-lane.py run integrate-all-20261108 --launch-mode 56
```

The build and all runtime evidence must come from this integration worktree.
Re-run the full test matrix above against the integration EXE. A feature-lane
build is not evidence for the combined EXE.

### 5. Stable promotion

`agent-lane.py promote --build` currently refuses because it calls the
deprecated build path. Stable publication therefore needs either:

- a human-authorized build of the clean integration worktree followed by a
  stable-pointer publication step, or
- a future non-building `agent-lane.py publish <lane>` command that calls the
  existing `publish_stable` logic without invoking a compiler.

Until one of those exists, the integration lane's successfully built immutable
artifact is the one-EXE candidate; the stable pointer is not updated. Do not
hand-copy `mimita.exe` into a stable folder or replace a running executable.

### 6. Final report

Record:

```text
integration lane:
base revision:
merged commits:
build ID / executable SHA-256:
artifact path:
test modes:
runtime journal paths:
passed:
failed / still open:
human acceptance:
```

## Related documents

- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/specs/dev-loop/agent-lanes-build-authority.md`
- `docs/specs/dev-loop/dev-loop.md`
- `docs/workflows/runtime-scenario-validation.md`
- `C:\mimita-v9-agent-lanes\world-streaming-nav-reconnect-20261108\docs\changelog\2026-11-08\20261108_223500-world-streaming-nav-reconnect-integration.md`
