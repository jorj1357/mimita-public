# Live hot-reload: per-process builds, retry, identity, and generation agreement

- EST timestamp: 2026-09-12 13:32:42 EDT (UTC 2026-09-12T17:32:42Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS_WITH_HUMAN_REVIEW` (staged; one bootstrap cold build required)

## Problem

Repeated edits to `src/hot-reload/modules/rocket-behavior.cpp` sometimes showed a
new activation while the authoritative server stayed on an older value:
source `outDamage = 66666`, server journal `base_damage=1111 out_damage=2222
result=hot`.

## Evidence

- `build/hotreload/gen2` contained `mimita-live-p25116-g000002.dll` **and**
  `mimita-live-p27412-g000002.dll` (two processes, same generation directory),
  with a single shared `build-result.json`/`build.log`.
- Client journal `...164941.jsonl`: `compile_finished ok` for generations
  6,7,8,9. Server journal `...165200.jsonl`: `compile_finished failed` for the
  same generations; last server `code_activation` = generation 5.
- Server `hot_damage_policy_result` had `actor_id=server` but no
  generation/code_hash/pid.
- Notifications said only "new version active: generation N" from both processes.

## Root cause

1. **Shared build namespace across processes.** Both wrote
   `genN/{mimita-game.dll, build-result.json, build.log}` with identical
   generation counters; the single result file was overwritten by whichever
   process wrote last, and the server read the wrong outcome (for example gen 9
   `compiler exit 1` while its own JSON said ok).
2. **No retry of a failed hash.** `beginBuild` set `observedSourceHash_` before
   compiling, so a transient/raced failure was never retried; the server stalled
   on generation 5 while the client recovered.

## What changed

### Per-process build isolation and retry
- `src/hot-reload/hot-reload-system.*`: build output is now
  `build/hotreload/p<pid>/gen<gen>/` with per-generation `build-result.json` and
  `build.log`. `attemptedHash_`/`pendingHash_`/`attemptFailures_`/
  `nextRetryMonoMs_` added; `observedSourceHash_` (active) is updated only on a
  successful activation; a failed hash is retried with bounded backoff (2s to
  10s, indefinitely) and notifies on every attempt.

### Observability
- `src/live-code/live-identity.*`: process side, PID, session id, simulation
  tick.
- `src/live-code/live-journal.*`: stamps `process`, `pid`, `session_id` on every
  event.
- `src/network/server-damage-policy.cpp`: `hot_damage_policy_result` now records
  `generation`, `code_hash`, `module`, `source_file`, `distance`, `base_damage`,
  `out_damage`, `result`.
- `src/live-code/live-code-events.*`: notifications identify side/PID/session/
  generation/hash, candidate vs active generation, and cold-restart severity;
  new `notifyGenerationMismatch` (`CLIENT ONLY — SERVER STILL RUNNING GENERATION
  N`).
- `src/network/server.cpp`, `src/main-init.cpp`, `src/network/multiplayer-tick.cpp`:
  set process identity and simulation tick.

### Generation agreement protocol (seed)
- `src/network/packets.h`: `PACKET_CODE_GENERATION = 69` +
  `CodeGenerationPacket` (`direction`, `phase`, `generation`, `codeHash`,
  `moduleSetHash`, `switchTick`).
- `src/network/server-packets.cpp`: stores each client's reported generation.
- `src/network/server.cpp`: announces the server generation/hash and a
  provisional switch tick on activation.
- `src/network/multiplayer-tick.cpp`, `src/network/multiplayer-context.h`: client
  reports its generation every ~30 ticks, stores the server announcement, and
  warns on mismatch.
- `isKnownPacketType` range raised to the newest type (note the existing TODO to
  convert it to an explicit switch).

### Docs
- `docs/architecture/live-development/hot-kernel.md`: generation agreement seed;
  "Why a cold build is required (and what the next phase must remove)".
- `docs/architecture/live-development/hot-kernel-next-steps.md`: Round 2
  implemented list and remaining proof.
- `docs/regressions/regressions-v1.md`: entry for the shared-namespace/retry bug.

## Why a cold build is required

The hot loader, journal identity fields, notification schema, the authoritative
damage-policy call site, the server lifecycle, and the new packet are all
EXE-owned mechanisms. A running process cannot gain a new call site, struct
field, or lifecycle hook. The next phase moves behavior behind the hot ABI
(behavior bindings, component capabilities, kernel event queue) and grows a
generic kernel/bytecode so future changes are data/behavior rather than new C++
call sites.

## Evidence (source/build)

- `python build_game_dll.py` -> success (4 hot sources).
- `-fsyntax-only` clean for `live-identity.cpp`, `live-journal.cpp`,
  `live-code-events.cpp`, `hot-reload-system.cpp`, `server-damage-policy.cpp`,
  `server.cpp`, `server-packets.cpp`, `multiplayer-tick.cpp`, `main-init.cpp`.
- Full cold build **not** performed: two `mimita.exe` processes were running and
  the invariant forbids killing them.

## Still required

- One cold build (no process running) to install the bridge.
- Running-server proof: server journal `hot_damage_policy_result
  process=server generation=<current> code_hash=<current> base_damage=1520
  out_damage=999999 result=hot` matched by `code_activation`, repeated edits,
  compile-failure retention, restore activation, and stable PID/session/EntityIds.
- Client/server generation agreement evidence.

## Files

New: `src/live-code/live-identity.h/.cpp`.

Changed: `src/hot-reload/hot-reload-system.h/.cpp`,
`src/live-code/live-journal.h/.cpp`, `src/live-code/live-code-events.h/.cpp`,
`src/network/server-damage-policy.cpp`, `src/network/server.cpp`,
`src/network/server-packets.cpp`, `src/network/packets.h`,
`src/network/server.h`, `src/network/multiplayer-tick.cpp`,
`src/network/multiplayer-context.h`, `src/main-init.cpp`,
`docs/architecture/live-development/hot-kernel.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`,
`docs/regressions/regressions-v1.md`.

## Pre-existing edits preserved

Unrelated working-tree changes were not reverted or claimed.
