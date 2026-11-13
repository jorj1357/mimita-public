# Hot-reload multi-agent synchronization handoff (2026-11-13)

## Purpose

This is the copy-paste coordination brief for **Agent B** and **Agent C**. They
join the shared checkout and work alongside the integrator (Agent A) without
branching, creating a second worktree, or editing the same files at the same
time.

The target is one running `mimita.exe` that reloads ordinary game `.cpp` changes
from one `game.dll` with state, networking, and rendering preserved.

## Current checkpoint

- Repo: `C:\mimita-v9`
- Shared checkout only. No branches, worktrees, Mode 2 lanes, or second checkout.
- Static gate: **0 violations / 0 files**
- Whole-game link: **0 undefined / 0 unique**
- Manifest: **770 `.cpp` / 603 game / 28 resident / 4 bootstrap**
- Latest candidate: `build/game/generation-67/game-g67.dll`
- Latest successful whole-game load/initialize: generation 67, server PID
  **4772**, run `20261113_002726`
- Evidence: `docs/handoffs/cpp-whole-game-load-20261113.md`
- `gameTick` is still a no-op. No full tick/render, swap, or production cutover.

## Non-negotiable rules

1. **One writer per file.** Claim paths in
   `docs/handoffs/hot-reload-agent-claims-20261113.md` before editing.
2. **Do not edit another agent's claimed path.** If overlap is unavoidable,
   stop and ask the integrator.
3. **Do not create branches, worktrees, lanes, or a second checkout.**
4. **Do not run concurrent builds.** The dev loop and game worker are shared.
   Use the status/control path; wait for the current build to finish.
5. **Do not launch a competing dev-loop daemon or a stale executable.**
6. **Do not edit `AGENTS.md`, the hot-reload specification, the source-owner
   policy, or `src/host/abi/*` without Agent A.**
7. **Do not delete or merge lane data.**
8. **Do not claim runtime, visual, multiplayer, deployment, or human acceptance
   evidence without running and reading the exact live process journal.**
9. **Do not perform the Stage 5 ownership cutover.** Agent A prepares it; the
   owner/human approves it.
10. Preserve unrelated user edits and untracked files.

## Ownership split

### Agent A — integrator and resident boundary

- `src/host/**`
- `src/game/game-module.cpp`
- `src/game/*-bridge-module.*`, `src/game/*-module.cpp` except paths below
- `docs/specs/hotreload/**`
- `config/source-owners.json`
- final barrier, coordinator integration, cutover preparation

### Agent B — networking and transport

Claim prefix:

- `src/network/server-*.cpp`
- `src/network/server.h`
- `src/network/multiplayer-*.cpp`
- `src/network/client.cpp`
- `src/network/udp-*`
- transport/session adapter call sites

Task:

1. Replace remaining server packet-send raw `SOCKET` fallbacks with
   `CAP_TRANSPORT` send/send-to-player/broadcast operations.
2. Keep listener/session/socket ownership resident.
3. Preserve packet schemas, IDs, ordering, retransmit, reconnect, and error
   behavior.
4. Do not change packet IDs. Packet IDs are tombstones.

### Agent C — full tick/render and draw state

Claim prefix:

- `src/engine/**`
- `src/render/**`
- `src/renderer/**` only after Agent A confirms the boundary
- `src/input/**`, `src/video/**` only when required by the tick/render entry

Task:

1. Define the whole-game tick/render entry, including fixed 60 Hz simulation
   and render submission.
2. Move raw bind/upload/draw use toward `CAP_RENDER` draw-state ownership while
   keeping the resident device and resources.
3. Preserve VSync forced off. Do not add a VSync setting or enable path.
4. Preserve the cached broadphase and avoid per-query allocations in hot loops.

### Shared, but only by request

- `src/main.cpp`, `src/main-init.cpp`, `src/main-systems.cpp`
- `src/game/game-objects-module.cpp`
- `src/terminal/terminal-state.h`
- `docs/handoffs/*` evidence files (append, do not rewrite another agent's
  evidence)

## Claim protocol

Before editing, add a row to
`docs/handoffs/hot-reload-agent-claims-20261113.md`:

```text
| Agent | Path or prefix | Task | Started | Finished |
|---|---|---|---|---|
```

Claim the smallest possible prefix. Release it when the work is built and
verified, not when the first edit is saved.

If a file is not claimed, it is not yours. Never “quickly” edit a shared file.

## Build and verification loop

All agents share one dev loop. **Only one may request a build at a time.**

```powershell
# Read-only status
python devscripts/dev-loop.py --status

# Host build request; wait for completion
python devscripts/dev-loop.py --request build --wait-for-build --build-timeout 900

# Whole-game candidate build (owner-authorized build/run exception in this session)
python devscripts/source_manifest.py --write
$rev = python -c "import json; print(json.load(open('build/source-manifest.json'))['revision'])"
python devscripts/game_build_worker.py --manifest build/source-manifest.json --revision $rev --jobs 8

# Required gates
python devscripts/game_static_gate.py
python devscripts/source_manifest.py --check
```

Never run the host build and the game worker at the same time.

## How to prove a change

For gameplay/user-visible work:

1. Compile and link the candidate.
2. Load/initialize it by the owner-authorized probe path.
3. Exercise the real behavior.
4. Read the exact `events.jsonl`.
5. Report source, build, link, load, runtime, and human evidence separately.

Build success is not feature success.

## Exact handoff prompts

### Prompt for Agent B — networking/transport

```text
You are Agent B on the MiMITA hot-reload task. Work in the existing shared
checkout C:\mimita-v9. Do not create branches, worktrees, lanes, or a second
checkout. Read AGENTS.md, docs/ROUTER.md, and
docs/handoffs/hot-reload-multi-agent-sync-20261113.md first.

Your ownership is the networking/transport prefix:
src/network/server-*.cpp, src/network/server.h,
src/network/multiplayer-*.cpp, src/network/client.cpp, src/network/udp-*.
Claim your exact paths in docs/handoffs/hot-reload-agent-claims-20261113.md
before editing. Do not edit host ABI, the spec, or another agent's paths.

Current checkpoint: static 0, link 0, generation 67 loaded/initialized, but
server packet-send raw SOCKET fallbacks remain. Your task is to replace those
fallbacks with CAP_TRANSPORT send/send-to-player/broadcast operations, keep
listener/session/socket ownership resident, and preserve packet schemas, IDs,
ordering, retransmit, reconnect, and error behavior. Packet IDs are tombstones.

Use only the shared dev loop for builds; do not run concurrent builds. Report
compile/link evidence separately from runtime evidence. Do not perform the
ownership cutover. Stop and ask on any overlap or boundary decision.

Return: changed paths, claim release, exact build/link result, runtime/journal
evidence, and remaining risks.
```

### Prompt for Agent C — tick/render/draw state

```text
You are Agent C on the MiMITA hot-reload task. Work in the existing shared
checkout C:\mimita-v9. Do not create branches, worktrees, lanes, or a second
checkout. Read AGENTS.md, docs/ROUTER.md, and
docs/handoffs/hot-reload-multi-agent-sync-20261113.md first.

Your ownership is the tick/render prefix: src/engine/**, src/render/**.
Claim exact paths in docs/handoffs/hot-reload-agent-claims-20261113.md before
editing. Do not edit host ABI, networking, the spec, or another agent's paths.

Current checkpoint: static 0, link 0, generation 67 loaded/initialized, but
gameTick is still a no-op and game code still uses raw GL ids for draw state.
Your task is to define the whole-game fixed-tick/render entry, preserve 60 Hz
simulation, and move raw bind/upload/draw use toward CAP_RENDER ownership while
keeping the resident device and resources. VSync must remain forced off.

Use only the shared dev loop for builds; do not run concurrent builds. Report
compile/link evidence separately from runtime evidence. Do not perform the
ownership cutover. Stop and ask on any overlap or boundary decision.

Return: changed paths, claim release, exact build/link result, runtime/journal
evidence, and remaining risks.
```

## What remains after Agent B/C start

- Server packet-send `CAP_TRANSPORT` completion.
- Whole-game tick/render entry.
- `CAP_RENDER` draw-state ownership.
- Full barrier/callback/job proof at whole-game scale.
- Coordinator-driven save → build → activate.
- Stage 5 cutover.
- Runtime campaign: NPC behavior, JSON recoil, AA/4D geometry, networking,
  add/remove/header edits, failure/rollback, human acceptance.

## Stop conditions

Stop and ask the integrator/owner when:

- two agents need the same file;
- a build is already running;
- an ABI/ownership decision changes;
- a packet ID or schema would change;
- VSync, fixed-tick, collision, or resource-lifetime invariants are at risk;
- the change cannot be proven without a process restart or cutover.


## Frame ownership contract (2026-11-13, after first attempt)

The owner decision is: **the game DLL owns the Engine; the stable host does not.**

The first `MIMITA_GAME_OWN_FRAME=1` client attempt loaded generation 71 and then
crashed in the menu with an unhandled-exception dialog (`UnhandledExceptionFilter`
/ `MessageBoxA` stack). The current C entry constructs a second `Engine` while
the host still constructs and initializes its own. Do not re-enable
`api.frameCapable = 1` until this is fixed.

Required C/A contract:

1. In DLL-owned frame mode, the host must not construct or initialize an
   `Engine` of its own.
2. The DLL must own `Engine`, window/renderer acquisition, `gameInit`, input/UI
   binding, fixed simulation stepping, and frame presentation.
3. The host supplies only resident services, startup data, and the safe frame
   call boundary.
4. A must not remove the host `Engine` path until the DLL bootstrap entry is
   available and can run without the host object.
5. Re-enable `frameCapable` only after one menu frame and one gameplay frame are
   proven in one process, with barrier entry/leave around both calls.

Agent C owns `src/engine/whole-game-entry.*`; Agent A owns
`src/game/game-module.cpp`, `src/host/**`, and `main.cpp` transition wiring.
