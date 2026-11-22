# Dev-loop journal and status CPU reduction

## Live profile

- Sampled checkout-owned daemon PID 14344 with `py-spy` 0.4.4 for 15 seconds at 100 Hz in nonblocking mode. The raw profile contains 810 samples and 5 stack-read errors. WPR was not recording, but starting its CPU profile failed because this process lacks the system-performance profiling privilege; no ETW trace was left running.
- The main loop called `print_status()` at `devscripts/dev-loop.py:5516` on each 200 ms iteration. That function called `state()`, which reads server/client JSONL tails and decodes them through `_instance_records()` / `_tail_records()`.
- The loop also called `save_state()` at `:5519` once a second. It built another `state()` snapshot, and `_write_manifest()` called `_instance_records()` again, rereading the same instance journals.
- Raw stack counts: `DevLoop.state` frame in 105/810 samples (13.0%); the print-status call path in 73/810 (9.0%); the save-state call path in 71/810 (8.8%). These counts overlap and are not additive. Watcher snapshot frames were rare in this 15-second interval, so the full source scan was not the dominant daemon hot path during this particular sample.
- Thread identities from `py-spy dump`: TID 25116 was `MainThread`, TID 29684 was `Thread-1 (watch)`, and TID 31296 was `mimita-dev-control`. This corrects the earlier interpretation that the second busy thread was the watcher; the prior 10.4%-of-one-core thread was the control thread.

## Change

- `save_state()` now retains the state snapshot it just built. `print_status()` accepts/reuses this snapshot instead of reconstructing state five times a second.
- `_write_manifest()` now receives the same snapshot and reuses its `instances` rows rather than rereading and decoding all instance journals again.
- Immediate status saves still refresh the snapshot. The periodic run-loop snapshot/save interval remains one second, and status rendering still runs on the existing 200 ms loop using that latest snapshot.

## Verification

- `python -m py_compile devscripts/dev-loop.py` passed.
- `git diff --check -- devscripts/dev-loop.py` passed with the expected LF-to-CRLF working-copy notice.
- An isolated Python probe verified that `save_state()` passes the identical snapshot to the manifest writer, `_write_manifest()` uses supplied instance rows without calling `_instance_records()` again, and `print_status(snapshot=...)` does not recompute state.
- The current daemon still runs code loaded before these edits. No runtime before/after CPU comparison, new game process, build, or Juggernaut test was performed. The live profile was on the existing Payload session. Deployment and matched post-change sampling remain necessary to measure impact.

## Next measurement

After a safe controlled daemon replacement, repeat the 15-second `py-spy` sample and a 10-second `psutil` sample with the same game processes and mode. Compare journal tail reads, `state()` and `save_state()` stack shares, daemon CPU, client/server CPU, and frame/simulation health. Then measure the requested pinned Juggernaut/AI scenario separately; do not combine its CPU result with the Payload baseline.

## Follow-up measurement — 2026-11-22 UTC

### Measured change and root dev-loop attempt

The one-source G437 attempt from the prior section succeeded at 222.017 seconds and activated on the existing server process. This follow-up modified only Python build tooling. The root dev-loop request `2026-11-22T00:06:30.343Z-e98be874` ran attempt `game-dll-20261122T000630945648Z-c2c8ee`; it completed successfully as a no-op in 141.027 seconds, retained generation 437, and made no compile/link/publish change.

The latest no-op attempt's JSONL reports packet registry codegen at 2.414 seconds, down from 33.383 seconds in the prior measured one-source attempt (92.8% lower; not a controlled same-load comparison). `devscripts/packet_registry_codegen.py` now tokenizes `PACKET_*` identifiers once per source line and uses an owner lookup map rather than checking every packet name against every line. `python devscripts/packet_registry_codegen.py --check` passed for 78 packets and confirms the generated ledger/header remain current.

The no-op attempt took 48.565 seconds in manifest_snapshot, 0.135 seconds in manifest_validation, 55.995 seconds in source_snapshot, and 0.113 seconds in object_cache_scan. The exact cache scan saw 642/642 cached, zero stale. A direct warm-cache profile took 9.194 seconds for build_manifest (OwnerResolver 5.147s; C++ enumeration 1.794s; 2,225/2,225 source/header digest hits; zero file hashes; 314ms stat time). Direct source mirror update took 0.052s. Direct immutable snapshot creation took 6.586s for 2,899 hardlinks and zero copies. The 39.4s manifest difference and 49.4s snapshot difference between the full attempt and these focused calls are unresolved; the in-memory live daemon was older than the newly edited instrumentation. No attribution is claimed for those intervals.

### No-op metric correction

The no-op summary had copied old compile/link/publish values from `current.json`, which describes the last published generation. `devscripts/dev_loop/game_build.py` now builds no-op worker metrics from progress and last-attempt records only when their attempt ID matches. An isolated assertion using the actual attempt fixture reports 642 scanned/skipped, zero compiled, object scan 0.109s, zero compile/link/publish, and 0.381s worker time instead of stale G437 compile/link totals. Python syntax, focused assertions, codegen check, and `git diff --check` passed. This fix is not live-loaded: the documented daemon stop procedure closes its managed client, so I preserved the current game pair. The daemon/UI timer changes from this session also await a safe reload.

### CPU evidence and limits

After the no-op build, a 12-second process sample measured 99.8% host CPU average across 12 logical CPUs. Client PID 24192 averaged 298.38% of one core (24.86% machine), server PID 27816 101.78% core (8.48% machine), additional MiMITA server PID 15340 126.11% core (10.51% machine), daemon PID 14344 26.06% core (2.17% machine), dashboard PID 33128 44.99% core (3.75% machine). A six-second all-process sample also averaged 99.8%; ChatGPT, cpptools, Defender, VS Code, cpptools-srv, and computer-use processes were active, with remaining CPU unaccounted. These samples were after the build, not aligned to each build subphase.

The preserved run `20261121_224058` is Payload/map `payload1`, NPC count 1 (client 24192/server 27816), not Juggernaut/AI pressure. No matched low-load build or Juggernaut test was run. Host saturation is observed and can confound timing, but it is not proven to cause the long manifest/snapshot intervals. Active visible gameplay cannot meet a 0% CPU target; <=1% is only a possible idle/background target.

### Next work

Keep the live pair intact. The next useful measurements are (1) load subphase instrumentation into a daemon without terminating the managed client, or otherwise capture exact in-process phase timestamps; (2) explain the 39s/49s stage discrepancies with OS process/disk counters and a matched rerun; (3) profile OwnerResolver and repository enumeration; (4) verify the no-op summary correction through a live root dev-loop attempt after a safe code reload; and (5) run a pinned Juggernaut/AI profile only with a suitable existing authorized runtime. The <=10s edit-to-live target is not met; the latest attempt was a 141s no-op.

## Follow-up 2 — owner-resolution cache profile (2026-11-22 UTC)

A warm `OwnerResolver` cProfile recorded 1,095,481 calls in 6.129 seconds. `OwnerResolver.__init__` accumulated 6.129s; 21 `expand_patterns` calls took 4.460s, 1,255 pathlib glob/sort expansions took 3.701s, and 17,118 `nt.scandir` calls took 2.225s. Path canonicalization also cost 1.890s across 3,624 `Path.resolve()` calls, including 1.155s in `nt._getfinalpathname`. The repeated owner-rule globbing and directory walking are the specific warm-manifest cache miss identified by this profile.

`devscripts/source_manifest.py` now caches the resolved owner map per root process. It invalidates when the owner or module catalog contents change, the C++ inventory changes, override existence changes, or the resolved link-input set changes. A same-process measurement using the existing source hash cache produced identical manifest revision `a3ef72a08c291c3491e0e193eddbb21eb09e79f51e8648f22ac3da575c12fed5` and 1,008 sources / 1,217 headers twice: first pass 8.589s (resolver 4.859s, cache miss), second pass 3.543s (resolver 0.033s, cache hit). An isolated temporary-root test changed a source owner rule from `game` to `resident`; the cache invalidated and returned the new owner. This focused measurement saves about 4.8s on the repeat but remains above the subsecond target.

The active daemon still has older Python loaded, and restarting it through the documented stop command would close its managed client. The live Payload pair remains intact; this optimization has not been verified in a root dev-loop build. The latest end-to-end no-op remains 141.027s, with full-attempt manifest/snapshot times much larger than focused operations; the 39s/49s discrepancies remain open. No Juggernaut/AI run or matched low-load CPU comparison was performed.
