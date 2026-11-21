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
