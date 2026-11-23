# Runtime-scoped CPU probe and matched evidence

## Scope

Added a command route for an already registered dev-loop runtime's client. This enabled the existing bounded `perf_callsite_cpu` diagnostic without a native build, a second runtime pair, or a direct process launch. The route uses the existing agent-command JSONL format and command packet path.

## Code changes

| File | Before | After |
| --- | --- | --- |
| `devscripts/dev_loop/parallel_test_runs.py:392` | `def _append_queue(queue_path: Path, request_id: str, command: str) -> None:` and a fixed `"scenario_id": "parallel_agent_test"` | `def append_command_queue(..., scenario_id: str = "parallel_agent_test") -> None:`; existing parallel test callers retain their old scenario ID |
| `devscripts/dev_loop/runtimes.py:100` | `RuntimeManager` had `status()` followed by `_build_server_args()` and no queue operation | Added `queue_command()` with alive-client, run-directory containment, and empty-command checks; runtime clients now receive `--agent-command-file`, and runtime state retains its run-local queue path |
| `devscripts/dev-loop.py:3610` | `elif action == "command": self.queue_agent_command(request)` always targeted the root primary client | When `runtime_id` is supplied, queue through `RuntimeManager`, return the run journal/queue metadata in `last_control.results`, and wait for that runtime's client journal. Calls without `--runtime` keep the prior path |
| `devscripts/dev-loop.py:8376` | No runtime result poll existed | Added status polling with the required control token, then wait for the queued request's completion event. The first live attempt exposed the missing token; that was corrected before final route proof |
| `devscripts/dev_loop/cli.py:80` | `help="runtime id for runtime actions"` | `help="runtime id for runtime actions or --command targeting"` |
| `devscripts/quality/test_runtime_command_routing.py:17` | No focused runtime queue coverage | Added three checks for runtime queue metadata/scenario identity, unchanged parallel-test default, and empty-command rejection |

## Validation

- `python devscripts/quality/test_runtime_command_routing.py`: 3 passed.
- `python -m py_compile` on the four changed Python modules and focused test: passed.
- `git diff --check` on changed implementation/test files: passed.
- No C++ build or WPR capture was run.
- Controlled root dev-loop restart changed daemon PID 24608 to 9580 while preserving registered runtime server/client PIDs 6944/15328. Reloaded only `mode3-latency-run`; new runtime PIDs are server 24692 and client 23980, using generation 560. Server journal then reported `module_generation=560`, 67 NPCs, and 1 player.
- The runtime command route was exercised live. `perf_callsite_cpu on 15` was delivered and completed. Its 15-second timer had already emitted `final_window=true` before the later `perf_callsite_cpu off` request; the off command itself returned `delivered=true completed=true`, but live mid-window cancellation was not tested.

## Matched resource and callsite evidence

The bounded sample ran 2026-11-23 14:00:06.331–14:00:22.541 UTC (16.210 s), with 12 logical CPUs and no build active. Artifacts are `.dev/perf-captures/mode3-probe-resource-20261123T140006Z.csv` and `.json`.

| Measure | Value |
| --- | ---: |
| Host CPU mean | 52.64% |
| Server PID 24692 | 124.35% of one core; 10.36% whole host |
| Client PID 23980 | 219.48% of one core; 18.29% whole host |
| Pair total | 28.65% whole host |
| Physical memory load mean / available RAM mean | 61.01% / 6.102 GiB |
| Server/client RSS | 335.6 MiB / 680.5–693.4 MiB |
| Sampler cost | 1.16% of one core, about 0.097% whole host |

All 15 probe windows named `updateOneNpc` at `src/npc/npc.cpp:1549`, with sample rate 1/64. Estimated share of the server process cycle counter averaged 25.97%, ranged 17.01–35.50%, and ended with `final_window=true`. This is a sampled cycle-share estimate; it is not CPU milliseconds or a function's share of whole-host CPU. At the same time, 16 client FPS windows were around 155–174 FPS; the first had 154/156 frames and the last 174/175 frames over the 4.167 ms budget.

Before reloading the pair, the older 60.423 s journal-aligned sample reported server module generation 556 while the main runtime hotness event reported generation 560. This explains why that earlier window could not prove the newly built NPC probe. The current pair now shows module generation 560. The old sample and sampler overhead are documented in `issue-ed3ac775283d` with their limitations.

## Interpretation and follow-up

`updateOneNpc` is a measurable server contributor. Multiplying its average 25.97% server-cycle share by this window's 10.36% whole-host server CPU gives an approximate 2.69% whole-host contribution. This multiplication is an inference from two measurements, not a direct per-function whole-host counter. It does not explain client CPU or the remaining server CPU. Next, profile additional high-cost server NPC/physics callsites and the client render/update path, then compare matched active and quiet runs before deciding on behavior changes.
