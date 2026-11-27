# Build why and postmortem

- **Status:** active policy
- **Recorded:** 2026-11-27
- **Owner:** root dev-loop / build coordination
- **Related:** `AGENTS.md` gate 6,
  `docs/specs/dev-loop/build-time-observability.md`,
  `docs/specs/dev-loop/change-to-evidence-latency.md`

## Purpose

Every whole-game DLL build must answer three questions without asking the
agent:

1. **Why did this build exist now?** — the note's `why`.
2. **Where did the time go?** — phase seconds, queue wait, slowest TUs, cache,
   resource pressure.
3. **What should we change to make the next one faster?** — concrete
   suggestions with the measured cause.

A build result without these is incomplete, even if it compiles.

## Build note (required)

Before saving compiler inputs:

```
python devscripts/build-note.py --changes "short change summary" \
  --why "why this build exists now" \
  --files path/to/file [--files ...] \
  [--header-fanout N] [--expected-seconds S] \
  [--risk "resource/cache/runtime risk"] \
  [--proof "runtime evidence this build must produce"]
```

Rules:

- `--why` is required by policy. The tool accepts a missing value so a build
  never blocks, but records `why="missing"` and the postmortem flags it as
  build friction.
- `--header-fanout` is required when a header changed. Get it with
  `python devscripts/dependency_graph.py --affected <header>`.
- `--expected-seconds` is the agent's honest estimate; it lets the next agent
  compare actual versus expected.
- `--risk` names resource, cache, or runtime risk (for example "full rebuild
  after object-cache quarantine" or "CPU saturated").
- `--proof` names the runtime command/event that must appear before the change
  is considered proven.

## Postmortem (automatic)

The dev-loop writes a `postmortem` object into:

- `.dev/build-timings/<attempt>.json`
- `dev-loop.game-generation.ready.postmortem` in the run `events.jsonl`
- the build result returned to the caller

Schema:

| Field | Meaning |
|---|---|
| `why` | build-note reason; `why_missing=true` if absent |
| `agent` | note agent label |
| `attempt_id`, `generation` | build identity |
| `changed_paths`, `headers` | what was built |
| `header_fanout` | affected source count, if the note recorded it |
| `seconds.total` | attempt wall time |
| `seconds.worker_total` | worker time |
| `seconds.compile` | compile wall time |
| `seconds.link` | link wall time |
| `seconds.publish` | publish time |
| `seconds.object_scan` | cache/object scan time |
| `seconds.queue_wait_max` / `queue_wait_mean` | per-TU worker queue wait |
| `cache.compiled`, `cache.skipped`, `cache.invalidations` | cache behavior |
| `slowest_compiles` | top 5 TUs by compile seconds |
| `resource.mean_host_cpu_percent`, `peak_host_cpu_percent`, `minimum_available_memory_mb` | pressure |
| `suggestions` | ordered, measured next actions |
| `metrics_to_watch` | stable list of fields to extend |

## How to act

Read the postmortem immediately after the build:

1. **Why missing?** Add `--why` before the next build.
2. **Compile dominates** → inspect `slowest_compiles[0]`; reduce includes,
   split the TU, or move types out of a shared header.
3. **Header fanout > 50** → split the header or batch dependent changes.
4. **Queue wait high / CPU > 90% / memory < 256 MB** → resource contention;
   move concurrent tests/builds off the critical path, reduce jobs, or schedule
   off-peak.
5. **Object scan high** → cache/index freshness; follow the build self-repair
   runbook before quarantining.
6. **Full rebuild (`compiled > 500`)** → batch related changes; check cache
   invalidations.
7. **Link high** → inspect link input count/bytes; split objects or use a
   targeted link path.

## Adding more useful information

Agents may extend the note and postmortem, provided the fields are stable,
typed, and recorded in this spec. Useful additions:

- per-header fanout and the exact dependent source count;
- expected seconds and actual delta;
- test/run dependency (which proof needs this build);
- cache miss reason from `cache_invalidations`;
- top competing processes and host logical CPU count during the build;
- compile command/cache flags;
- user-visible asset or config reload cost;
- whether the change invalidated the PCH or a generated packet table.

Do not add free-form-only notes. If a metric matters, add it to the
postmortem schema, the build-note fields, and this table.

## Acceptance

- Every published or failed build has `why` (or `why_missing=true`) and a
  postmortem in the timing summary and `game-generation.ready`.
- Postmortem suggestions name a measured cause, not a generic guess.
- A build with a changed header records `header_fanout`.
- Agents can compare expected versus actual seconds and see the top 5 slow TUs.
- Resource pressure is visible as mean/peak CPU and minimum free memory.
