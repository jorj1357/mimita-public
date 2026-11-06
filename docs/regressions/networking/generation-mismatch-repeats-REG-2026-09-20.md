# Repeating client/server live-generation mismatch

Time created: 2026-09-20T16:50:00Z
Time last updated: 2026-09-20T16:50:00Z

Status: ATTEMPTED FIX (1)

Related specification:
`docs/specs/networking/networking.md`

Related changelog:
`docs/changelog/2026-09-20/20260920_165000-generation-sync-versioninfo.md`

---

## Regression occurrence 1

### Observed

The attached screenshot showed the client reporting `clientGeneration=3` while
the server reported generation `31`, with both processes still running. The
user reports the same class of mismatch recurring since approximately
2026-09-18, including much larger differences such as server generation 99 and
client generation 3.

Evidence image:
`C:\Users\guita\AppData\Local\Temp\codex-clipboard-be6f52d4-3414-408a-8437-970f9bb6a93d.png`

### Expected behavior

The client must either converge to the server's active logical generation, or
show a bounded, explicit bootstrap/download/verification failure. A persistent
mismatch must never look like a healthy running session.

### What the numbers mean

`clientGeneration` is the local hot-loader generation. The remote number is
`serverCodeGeneration`, received from `CodeGenerationPacket`. It is not always
the server's currently active generation: packet phase 0 can describe a
candidate/status, phase 2 is a pending coordinated switch, and phase 3 is an
active late-join bootstrap. Comparing only the two integers can therefore show
a real pending transition as if the server were already running that code.

### Confirmed source-level cause

The client mismatch branch in `src/network/multiplayer-tick.cpp` previously
only called `LiveCodeEvents::notifyGenerationMismatch()`. It did not, by itself,
prove artifact request, artifact commit, hash verification, candidate install,
safe-tick activation, or convergence. The earlier regression
`docs/regressions/2026-09-16-server-client-generation-mismatch-spawn-lock.md`
shows the same failure family: client generation 10 versus server generation
76, repeated build retries, and no convergence.

This recurrence is therefore a synchronization/evidence bug, not proof that
the screenshot alone identifies which executable or JSONL file is active.

### Fix attempted

- Record every generation announcement in `generation_sync_state` JSONL with
  local/server generations, hashes, logical/platform hashes, phase, bootstrap
  state, and verification failure.
- Label mismatch notifications as candidate/status, switch-pending, or active.
- Emit `generation_converged` when local active generation catches the server
  generation, with an in-game notification.
- Add the hot `versioninfo` command so the running session prints process ID,
  EXE path, session, uptime, client/server generation and hashes, server tick,
  room/server identity, and the authoritative `events.jsonl` path.

### Proof still required

Build proof exists, but a two-process live run is still required to prove that
the client downloads, verifies, activates, and visibly converges after a server
generation change. Until that human/runtime test passes, this record remains an
attempted fix.
