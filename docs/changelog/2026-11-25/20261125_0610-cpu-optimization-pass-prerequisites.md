# CPU optimization pass prerequisites

## User direction

Continue the existing CPU-first resource goal. Each CPU optimization build must
carry one named code treatment selected from evidence, with matched before and
after measurements from the running MiMITA executable and its canonical
journals. Preserve the 240 FPS client frame-time and 60 Hz server tick gates.

The current goal already includes code changes, runtime proof, lower CPU use,
and those behavior gates. `docs/specs/dev-loop/agent-self-improvement.md`
already requires one evidence-selected code change and matched runtime
measurements for every build in an explicitly iterative optimization goal.
No duplicate policy edit was needed.

## Current observation

At 2026-11-25 06:08–06:12 UTC, Windows process inventory showed no live
`mimita.exe` processes. Root dev-loop had no registered runtime or active
build; it reported `lifecycle=starting`, `published_stale`, source generation
0 versus latest successful source generation 58, and no run. The older runtime
registry rows marked several test runs online, but their processes were gone.
The root-mode3-latency owner independently confirmed the old runs were not
theirs and no pair was available. One point-in-time OS memory snapshot showed
6,845 MB available; this is not a performance sample.

I used `dev-loop.py --ensure-running`; it reported the watchdog and daemon
running, but status remained `starting` and `published_stale`. No game process,
build, profiler command, or source compiler input was started by this session.

## Result and next discriminating work

No CPU code treatment or before/after optimization was tested in this pass:
there was no live pair, and the root executable/source state was not current
enough to pin a trustworthy baseline. The registry/process mismatch and this
limitation were appended to the existing CPU issue. The user request is linked
once in coordination context.

Next pass: resolve the active CPU-pass owner's source and runtime ownership;
restore an accurate root dev-loop state; establish a single pinned connected
pair with authoritative mode, actor counts, EXE/DLL generation and fresh
journals; capture bounded CPU/process and client/server callsite or stack
evidence; then select exactly one CPU code change. Measure the same workload
before and after, including process and whole-host CPU, per-frame client
budget, server tick budget and correctness. Do not treat profiler-only or
unrelated feature builds as CPU optimization passes.
