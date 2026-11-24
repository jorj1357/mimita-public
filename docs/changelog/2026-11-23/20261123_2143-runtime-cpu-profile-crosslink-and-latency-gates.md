# Runtime CPU profile cross-link and latency gates

- Session ID: work-ce4acfed5bf6

## Completed

- Rechecked the root dev-loop: no build was active and no Mode 3 batch was pending. Root server PID 10164 and daemon PID 30924 remained alive. The isolated test pair PIDs 22184/32832 remained alive, although the root status view lists no client for its own server-only run.
- Coordinated with the CPU/resource profiler. Its bounded sample and 1/64 callsite probe finished; the profile window is released. The profile is runtime CPU evidence only, not a build-latency measurement.
- Independently recalculated the target-only sample `.dev/perf-captures/runtime-attribution-20261123T2137Z-{host,server,client}.csv` and appended a concise cross-reference to `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md`.

## Evidence

- Run `20261123_195307`, Juggernaut Horde on `dust2cyberiav4`, server PID 22184/client PID 32832, 67 NPCs/1 player, 12 logical CPUs. Sample interval was approximately 21:37:28–21:37:43 UTC (14 target rows). Host CPU averaged 97.63% (maximum 99.88%). Available RAM averaged 3.98 GB and bottomed at 3.91 GB; memory load was 75–76%.
- Server CPU averaged 301.99% of one logical CPU (25.17% whole host); client averaged 354.73% (29.56% host); pair totaled about 54.73% host. The capture tracked the target pair, not the top competing processes. No build/compiler was active, so this cannot show how much host pressure slows compilation.
- The server journal still showed host game DLL generation 571 while NPC module generation was 570. The probe estimated `updateOneNpc` at 12.40% of server process cycles and `doCollisions` at 5.33%; these are 1/64 sampled-cycle estimates, may overlap, and the marker source lines do not identify internal hot instructions. Probe-era server tick times were reported around 21–29 ms with p95 around 38–64 ms; the workload was not sustaining a 16.667 ms tick.
- The previous measured DLL attempt remains 829.795 seconds total, with 754.586 seconds in compile and 12.657 seconds in link. The new per-process build sampler has not yet been exercised by a root build with an aligned Juggernaut baseline.

## Next gates

1. Get explicit READY/ownership confirmation for the other agents' modified C++ compiler inputs before requesting a root Mode 3 batch.
2. Capture the entire next root build with the native per-process provider and a matched runtime baseline; record compiler/linker PID CPU and memory, host load, competing processes, and exact changed/compiled/skipped translation units.
3. Resolve the resident ModuleHost local API rebind across game-DLL commit. That likely requires a one-time resident/EXE build and controlled process restart before NPC DLL activation can be trusted.
4. After activation is correct, reduce the measured compile and link critical path; a 12.657-second whole-game link alone already exceeds the <=10-second total target.

## Final status for this session

At 21:44 UTC, root status changed to `batch_pending=true` with `src/npc/npc.cpp` and `src/network/server-gamemode.cpp` as the only pending game inputs. The active NPC owner task `npc-elite-fcs-3` had not declared READY or provided a build-note ID. I asked for explicit readiness and did not seal/request this batch. This two-file batch would be useful for exercising the process sampler but would not satisfy the 15–20-file acceptance target. It also cannot prove fresh NPC module activation while the host/module generation mismatch remains.

## Changed files

- `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md`: appended the 21:37 no-build sample and its limitations under `2026-11-23T21:43Z — matched no-build resource profile, separate from build evidence`. There was no prior section for this capture; the new text includes host CPU, process-normalized CPU, memory, run identity, and the missing compiler/top-process attribution caveat.
- `docs/changelog/2026-11-23/20261123_2143-runtime-cpu-profile-crosslink-and-latency-gates.md`: new session record; there was no prior file. This line establishes the session ID: `- Session ID: work-ce4acfed5bf6`.
