# Path-aware watcher and CPU-pressure evidence

## User objective

Reduce MiMITA and dev-loop CPU pressure and cut `.cpp` edit-to-live latency. Keep the existing root dev-loop build authority and game processes intact; target roughly 10 seconds end-to-end, with warm scan/staging close to or below one second where practical.

## Findings

The earlier optimizations addressed the object-cache dependency scan, but the source discovery and source snapshot walks still exist:

- `devscripts/source_manifest.py` reuses file digests when size and `mtime_ns` match, but `enumerate_repository_cpp()` and `enumerate_repository_headers()` still recursively enumerate files each build and `build_manifest()` stats every candidate. This cache avoids many rereads; it does not cache the file list or eliminate the walk.
- `devscripts/dev_loop/game_build.py::_update_source_mirror()` already copies only paths that changed, but `_create_source_snapshot()` still recursively walks the mirror and creates a hardlink for every source/header/external file on every attempt.
- The root EXE source watcher previously used `FindFirstChangeNotificationW`, which only reported that something changed. Its consumer then rebuilt the full watched file map. If notifications failed, it repeated a full scan every 0.5 seconds. This can add CPU and repeated path-stat work during source-edit activity.

The whole-game DLL path and the root EXE watcher are separate. The watcher change below does not yet remove the DLL manifest walk or DLL source snapshot walk.

## Change made

Updated `devscripts/dev-loop.py` to use blocking `ReadDirectoryChangesW` events with exact changed paths. Normal file edits now refresh only that file’s signature. Added directories scan only that subtree; notification overflow or failure triggers a full correctness rescan. The fallback rescan interval is 2 seconds rather than 0.5 seconds, and a notification failure is surfaced in the console.

This change is in the shared checkout but is **not loaded in the already-running daemon**. It requires a controlled dev-loop restart before runtime CPU impact can be measured.

## Verification

- `python -m py_compile devscripts/dev-loop.py` passed.
- `git diff --check -- devscripts/dev-loop.py` passed; Git printed only the expected LF-to-CRLF working-copy notice.
- Temporary-directory integration using the actual `DevLoop.watch()` path delivered one exact `.cpp` file path with exactly one full snapshot (startup) and zero subtree snapshots for an ordinary file edit.
- Ten repeated temporary `.cpp` modifications produced exact-path notifications in 0.494–0.767 ms (median 0.647 ms), with zero full rescans. This measures filesystem notification delivery only; it is not end-to-end editor-to-game latency.
- No compiler, build worker, or game process was started by this session.

## CPU sample

A 10-second `psutil` sample observed 12 logical CPUs and 90.8% aggregate system CPU. Three `mimita.exe` processes used 91.6%, 239.8%, and 114.4% of one logical core respectively: about 37.15% of whole-machine capacity combined (roughly 4.46 logical cores). The root dev-loop daemon used 52.5% of one core and the dashboard Python process used 55.6% of one core, about 9.02% of total machine capacity combined.

The inspected processes were a Payload client/server pair plus an extra Bloodtest server process. They were not the requested Juggernaut/AI workload, and the sample cannot attribute the remaining host CPU to a specific process. The configured client frame cap is 999 FPS with VSync off. The active game also performs simulation and rendering; 0% CPU is not a realistic target for active play. A lower minimized/background render cap can reduce quiet-client work, but must be tested without changing fixed 60 Hz simulation or the forced-off VSync policy.

## Module wrapper invalidation

No generated module identity was frozen. The identity is currently consumed as runtime module metadata: the resident module host stores `base->generation` and `base->codeHash` and includes them in module-set hashing/agreement. Simply pinning those values would report stale code identity and could weaken client/server agreement. A safe no-rebuild design needs a runtime generation+code-hash identity from the resident host for folded modules, while standalone module DLLs keep compile-time identity, followed by an ABI review and live module-set agreement proof. This remains open on the DLL-latency issue.

## Remaining work and constraints

- The running daemon has not loaded the watcher change. Another agent has an active Bloodtest game process; its owner was asked whether a restart preserving all processes is safe, and no reply arrived during this work period. Do not claim a runtime or CPU improvement yet.
- The current shared dev-loop run is Payload, not Juggernaut. No Juggernaut stress run was started because other live game work is active and the machine measured above 90% CPU.
- The actual DLL manifest walk, hardlink snapshot walk, changed-TU compilation, wrapper identity fan-out, full DLL link, publish, load, and live activation still need matched measurements. The prior G436 build's 409.4s manifest, 111.6s snapshot, 148.0s changed-source preflight, 30.7s link, and 8.6s publish remain the latest detailed reference used here; G436 activation was not found in the inspected journal.
- Next: restart the existing daemon through the documented control path while preserving all owned game processes; compare watcher CPU and notification counts. Then implement a daemon-owned manifest index updated from exact events with overflow/full-audit fallback, and reuse immutable snapshots incrementally. Separately prototype a runtime identity contract for folded modules. Once conflicting runtime work ends, run matched no-op, one `.cpp`, header-fanout, and pinned Juggernaut/AI scenarios through the root dev-loop and measure edit/build/publish/load/activation/behavior separately.

This session reduces one watcher walk path and proves exact event delivery in an isolated probe. It does not yet meet the requested per-stage or 10-second build targets.

