# Gen576 resource attribution and process-ranking correction

## Task

- Task ID: work-18ca5fb3d92b
- Work session: `work-18ca5fb3d92b`
- Session ID: work-18ca5fb3d92b
- Owner: `resource-profiler`
- Issue: `issue-ed3ac775283d`
- Focus: Analyze the saved process-ranking capture and validate the root dev-loop's build resource sample.
- Summary: Corrected the earlier claim that no process-ranking artifact existed, parsed the completed generation576 resource sidecar, recorded its attribution and alignment limits in the existing CPU issue, and handed the evidence to the DLL latency owner.
- Status: `PARTIAL`
- Date: 2026-11-24 UTC
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Base commit: `186eebd23ce5f8f3f10377c47277ed58bd3b4d83`

## Scope and pre-existing state

The checkout already had numerous modified configuration, documentation, and C++ files owned by other work. They were preserved. This session made no game-source edits, started no builds, and changed no runtime process. The root build attempt was already active when inspected and later completed as generation576; a new Mode 3 batch remained pending afterward.

## Evidence and changes

Appended evidence to [the CPU investigation issue](../../issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md):

1. **Process ranking correction**, added at line 2632. The existing `.dev/perf-captures/runtime-top-processes-20261124T0019Z.csv` covers 2026-11-24 00:20:23.838–00:20:38.597 UTC, 12 logical CPUs. It is about 72 seconds after the fixed-PID pair sample, so it is nearby but not matched. Observed top rows include PID22184 MiMITA at 22.48% host CPU and PID13292 at 20.49%; additional MiMITA PIDs and processes are recorded in the issue. Top-eight selection and conditional row averages limit attribution.
2. **Build-aligned resource sample**, added at line 2638. Attempt `game-dll-20261124T003901444604Z-3c8d4e` ran 949.244 seconds and compiled 305 of 643 translation units. Its 670 samples show mean host CPU 99.995%, mean memory use 91.53%, mean available memory 1,334,419,994 bytes, and minimum available memory 229,376 bytes. During compile samples, tracked compiler/linker processes averaged 18.10% host capacity, MiMITA processes 15.72%, and other tracked processes 22.80%.

**Limits:** Sidecar runtime context mixed runs `20261124_001546`, `20261124_004333`, and `20261124_005311`; the summary reports matched server/client alignment unavailable. Host disk totals and GPU data were unavailable. These observations show coincident build/resource pressure; they do not prove that MiMITA CPU caused the long build or identify a source function responsible.

## Artifacts and handoff

- `.dev/perf-captures/runtime-top-processes-20261124T0019Z.csv`
- `.dev/build-timings/game-dll-20261124T003901444604Z-3c8d4e.resource.jsonl`
- `.dev/build-timings/game-dll-20261124T003901444604Z-3c8d4e.resource.json`
- `.dev/build-timings/game-dll-20261124T003901444604Z-3c8d4e.json`
- The exact metrics and uncertainty were sent to `hot-reload-latency-root` in thread `01a1feed-4f3f-7e22-a411-5340f67264f9`.

## Validation and remaining work

- Parsed the CSV rows and the JSONL sidecar with Python/PowerShell read-only commands; verified the appended issue entries at lines 2632 and 2638.
- No tests or builds were run or requested by this session.
- Next: keep function attribution on matched, journal-verified runtime runs; include build resource sidecars in latency analysis while preserving the sidecar's workload-alignment caveat. GPU and host disk accounting remain open measurement gaps.
