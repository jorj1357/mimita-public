# Dev-loop launch-stage diagnostics

- Time UTC: `2026-10-25T14:55:33Z`
- Display: `2026-10-25 10:55:33 EDT`
- Branch: `20261011zt`
- HEAD: `0472121b` plus the existing uncommitted worktree
- Result: `SOURCE_CHECKED_RUNTIME_PENDING`

## Scope

Make dashboard launch attempts explain their stage, result, reason, build/run
identity, and elapsed time in the same shared `events.jsonl` used by the game.
Preserve existing unrelated worktree edits.

## Source changes

- `devscripts/dev-loop.py`: launch-stage events now cover artifact selection
  and validation, map/config selection, firewall preflight, stable launch
  staging/activation, process reconciliation/reuse, versioninfo, server
  process/logger/gameplay readiness, client process/logger readiness, build
  identity comparison, and final outcome.
- Early launch attempts create or retain a canonical journal before preflight;
  launch-time server replacement preserves that attempt's journal even though
  the standalone server-stop path resets its active-run state.
- The dashboard preserves the actual failure message instead of replacing it
  with only `dashboard launch failed for build N`.
- Client logger readiness now watches the exact client process and can report
  an early exit immediately instead of waiting out the full timeout.
- `docs/specs/dev-loop/dev-loop.md`: corrected the implementation snapshot to
  match current `[1]` selection behavior and documented stage records.

## Investigation evidence

- The referenced `logs/10-25-2026/20261025_103325/events.jsonl` currently has
  13,080 bytes and contains server heartbeat/network timing events, but no
  client frame events. The supplied status path
  `logs/2026-10-25/20261025_134032/events.jsonl` was not present.
- `logs/2026-10-25/20261025_142116/events.jsonl` is 63,837,232 bytes and has
  three client PIDs. Performance events include a 2,373.743 ms frame whose
  largest measured stage is `Replay` (2,366.426 ms), and a 2,259.408 ms frame
  whose largest stage is `Network::AvatarTick` (2,245.760 ms). The stall
  watchdog also captured main-thread stalls of roughly 1.5–1.75 seconds in
  `State`, `Replay`, and `Networking`.
- The newer run journal `logs/2026-10-25/20261025_144729/events.jsonl`
  identifies client PID 23144. At `2026-10-25T14:47:34Z`, its one-second
  performance window reported 280.8 average FPS but a 1,957.7 ms maximum
  frame; physics max was 0.873 ms and networking max was 6.335 ms. The spike
  record attributes 1,749.7 ms to `State`. At `14:47:43Z`, a 2,218.3 ms
  frame attributes 2,215.6 ms to `Audio::StartSound::CacheLookup` in
  `getCachedSoundData` (`src/audio/audio.cpp:127`). That cache-miss path calls
  filesystem existence/open/read synchronously on the game thread. The exact
  sound/cache path is not recorded, so the owner is confirmed but the specific
  triggering asset is unknown.
- `mpAvatarNetworkTick` can synchronously read/hash avatar cache files and
  build the local avatar publication on the main thread. This is a measured
  additional stage-level lead; the exact file/manifest responsible for its
  captured worst stall is not recorded, so no performance fix is claimed.
- The existing uncommitted C++ network/game-mode changes were inspected but
  left untouched. They include per-snapshot duplicate-ID filtering and
  reassembly-failure logging; the log evidence does not establish those edits
  as the cause of the client stalls.

## Validation and limits

- `python -m py_compile devscripts/dev-loop.py`: passed.
- `git diff --check`: passed; Git printed line-ending normalization notices.
- No game build or launch was initiated by this task. The already-open loop
  automatically began a background build from newer source changes; its live
  status reported a compile error in `src/gamemode/map-config.h:106`
  (`PayloadConfig` does not name a type), with last successful artifact 3190
  from source generation 29 and active source generation 34. That C++ failure
  is outside this patch and no C++ source was changed here.
- The healthy build-3181 game pair was left running. The dev-loop runtime
  workflow defers replacing a live pair on a Python source change, so the new
  diagnostics still need a later controlled reload and real launch to verify
  their emitted JSONL records.
- No visual/human acceptance or FPS improvement is claimed.
