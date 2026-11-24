# Automatic build resource attribution and gen571 evidence

## Why this changed

The gen571 Mode3 attempt now had an automatic resource sidecar, but its provider was `windows_kernel32_host_only`: it recorded whole-host CPU and memory every second while leaving `top_processes` and `tracked_processes` empty. The user asked that CPU/RAM be measured alongside every build so we can tell whether the machine load slows compilation. The missing process view prevented that comparison.

## Generation571 attempt evidence

Attempt `game-dll-20261123T200924195125Z-7401c3` started at 20:09:24.238Z and completed successfully at20:23:13.847Z as game-DLL generation571. The attempt summary reports829.795s total (~13m50s), 642 source/link inputs, 271 compiled,372 skipped, and642 link object inputs. Compiled plus skipped totals643, one more than the manifest/link-input count; that accounting discrepancy is still open.

The eight changed paths were seven `.cpp` files plus `src/config.h`. The cache JSONL contains263 invalidation events, all `dependency newer than object` for `config.h` through normalized path variants. Together with the eight direct paths, those263 dependents explain271 compiled translation units. This is why unrelated-looking translation units compiled: their objects depend on the changed shared header. The remaining372 units were skipped.

Stage wall times were changed-source preflight46.412s, object-cache scan2.052s, compile754.586s, link12.657s, coherence validation0.601s, and publish1.009s. The slowest single source was `src/audio/audio.cpp` at89.075s. The machine was at mean99.986% CPU across12 logical CPUs, with mean available RAM1.08GiB, minimum651,264 bytes (0.62MiB), peak memory use99%, and final pagefile use31.57GiB. These show strong resource pressure during the long build, but the host-only sidecar cannot attribute it to compiler jobs, MiMITA, or other apps. A separate no-build pre-baseline was also already saturated, so this is not a causal A/B.

## Implementation

`devscripts/dev_loop/build_resource_sampler.py` now has a native Windows fallback that uses Toolhelp process enumeration, `GetProcessTimes`, and `GetProcessMemoryInfo` when `psutil` is unavailable. It records process role/name/PID, CPU percent per logical CPU and normalized to total host capacity, working set, private bytes, and pagefile usage. It samples host metrics at the normal interval and discovers processes on the existing bounded cadence; memory calls are limited to top CPU processes and relevant MiMITA/build processes. Process discovery wall time is measured. A discovery error no longer suppresses the host sample. Captures missing process metrics now finish as `partial`, and summaries explicitly list missing process CPU-rate observations during short-run warmup.

This supplements the existing one-second build sidecar; it does not change build authority, compiler, job limit, C++ inputs, or build timing. The issue tracker now has an explicit shared-brain proposal to surface a missing process provider as an incomplete capture.

## Verification

- `python -m py_compile devscripts/dev_loop/build_resource_sampler.py devscripts/quality/test_build_resource_sampler.py` passed.
- `python -m unittest devscripts.quality.test_build_resource_sampler -v` passed:4 focused tests.
- Direct Win32 process enumeration found354 processes and read the current Python PID's CPU counters and memory.
- A forced-host-fallback sampler smoke run at20:47:23.720–20:47:26.008Z produced six rows with provider `windows_kernel32_host_and_process`, status `captured`, no errors, and populated top/tracked process rows. Setup wall time was15ms and process discovery took45.012ms total wall time. The thread CPU timer reported0ms due to its coarse resolution, so it does not prove zero overhead.
- In that no-build smoke interval, host CPU was100%, available RAM3.11GiB (80% used), and pagefile use28.43GiB. The isolated client PID32832 used28.06% whole-host CPU and server PID22184 used21.71%, for49.77% combined; root server PID10164 used4.17%. Their working sets were282/552MiB and private bytes1262/968MiB. The build issue records that the exact workload journal had no events inside this short interval; the nearest later hotness row confirmed generation571,67 NPCs/1 player. These data show process attribution working, not a build-time comparison or a CPU reduction.
- The documented root-daemon reload was performed to load the updated Python module. Old daemon PID28440 exited through its control socket; existing root server PID10164 and isolated pair PIDs22184/32832 remained alive. The watchdog stayed PID5936 and started new daemon PID28388. After the final process-discovery-age correction, a second controlled reload replaced PID28388 with PID30924. Latest daemon and disk script hashes both equal `b554829f6eeaebfb6ac0dc4217f909bacaf0be04dfece4dfa875acf8c65c41d7`. Watchdog, dashboard, root server, and isolated pair remain alive; no game process was restarted and no build was requested.

## Remaining work

The next real root-dev-loop build must verify the new sidecar provider and compiler/linker process attribution while the intended client/server workload is live. Then compare a matched no-build period and build period. Reconcile why the attempt reported643 compiled-plus-skipped units but642 source/link inputs. The <=10s edit-to-live target is not met; this work only closes an important measurement gap.
